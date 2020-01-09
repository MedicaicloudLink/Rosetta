// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file protocols/rna/setup/RNA_CSA_JobDistributor.cc
/// @brief
/// @details
/// @author Rhiju Das, rhiju@stanford.edu


#include <protocols/rna/setup/RNA_CSA_JobDistributor.hh>
#include <protocols/stepwise/monte_carlo/StepWiseMonteCarlo.hh>
#include <protocols/rna/denovo/RNA_FragmentMonteCarlo.hh>
#include <protocols/stepwise/monte_carlo/util.hh>
#include <protocols/stepwise/modeler/align/util.hh>
#include <core/io/silent/util.hh>
#include <core/pose/Pose.hh>
#include <core/io/silent/SilentStruct.hh>
#include <core/io/silent/SilentFileData.hh>
#include <core/io/silent/SilentFileOptions.hh>
#include <core/io/silent/EnergyNames.hh>
#include <core/io/silent/util.hh>
#include <core/scoring/Energies.hh>
#include <core/pose/extra_pose_info_util.hh>
#include <utility/string_util.hh>
#include <utility/file/file_sys_util.hh>
#include <utility/sys_util.hh>

#include <basic/Tracer.hh>
#include <numeric/random/random.hh>

static basic::Tracer TR( "protocols.rna.setup.RNA_CSA_JobDistributor" );

using namespace core;
using namespace core::io::silent;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Initial attempt at population-based optimization for rna monte carlo.
//
//  CSA = conformational space annealing. Not attempting to directly use that Scheraga/Lee method,
//         just recalling from memory what M. Tyka and I coded up a while back for Rosetta++.
//
//  nstruct       = # number of updates per number of structures in bank.
//  cycles        = # monte carlo cycles per update (specify as -cycles)
//  csa_bank_size = total # of cycles to carry out by all nodes over all calculation.
//
// So the total compute = total cycles = csa_bank_size * nstruct * cycles.
//
// Instead of a master/worker setup, each node knows the name of the silent file with the
//     "bank" of models and updates it after doing some monte carlo steps. Each node sets a lock
//     by creating a ".lock" file when it needs to read it in and update the bank.
//     Admittedly, kind of janky to use file server for communication, but hopefully will work,
//     and later can fixup for MPI, threading, or whatever.
//
// Each model in a bank has a "cycles" column (and its name is S_N, where N = cycles). This
//     is the total # cycles in the CSA calculation over all nodes completed at the time
//     the model is saved to disk.
//
// Currently decisions to replace models with 'nearby' models are based on RMSD -- note trick
//     below where one pose is "filled" in to be a complete pose. This may take some extra computation,
//     and if the nodes get jammed up waiting for RMSDs to be calculate, this could be optimized (e.g.,
//     by precomputing RMSDs ahead of time, and only locking the file at the last moment.)
//
// Could almost certainly define a "recombination" protocol, and make this into a reasonable
//     genetic algorithm.
//
//      -- rhiju, 2014
//
/////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace protocols {
namespace rna {
namespace setup {

//Constructor
RNA_CSA_JobDistributor::RNA_CSA_JobDistributor( stepwise::monte_carlo::StepWiseMonteCarloOP rna_monte_carlo,
	std::string const & silent_file,
	core::Size const nstruct,
	core::Size const csa_bank_size,
	core::Real const csa_rmsd,
	bool const output_round_silent_files,
	bool const annealing ):
	RNA_JobDistributor( rna_monte_carlo, silent_file, nstruct ),
	lock_file( silent_file + ".lock" ),
	csa_bank_size_( csa_bank_size ),
	total_updates_( csa_bank_size * nstruct ),
	csa_rmsd_( csa_rmsd ),
	output_round_silent_files_( output_round_silent_files ),
	total_updates_so_far_( 0 ),
	annealing_( annealing )
{}

RNA_CSA_JobDistributor::RNA_CSA_JobDistributor( denovo::RNA_FragmentMonteCarloOP rna_monte_carlo,
	std::string const & silent_file,
	core::Size const nstruct,
	core::Size const csa_bank_size,
	core::Real const csa_rmsd,
	bool const output_round_silent_files,
	bool const annealing ):
	RNA_JobDistributor( rna_monte_carlo, silent_file, nstruct ),
	lock_file( silent_file + ".lock" ),
	csa_bank_size_( csa_bank_size ),
	total_updates_( csa_bank_size * nstruct ),
	csa_rmsd_( csa_rmsd ),
	output_round_silent_files_( output_round_silent_files ),
	total_updates_so_far_( 0 ),
	annealing_( annealing )
{}

//Destructor
RNA_CSA_JobDistributor::~RNA_CSA_JobDistributor() = default;

void
RNA_CSA_JobDistributor::initialize( core::pose::Pose const & pose ) {
	start_pose_ = pose.clone();
	total_updates_so_far_ = 0;
}

////////////////////////////////////////////////////////////
bool
RNA_CSA_JobDistributor::has_another_job() {
	return ( total_updates_so_far_ < total_updates_ );
}

////////////////////////////////////////////////////////////
Size
RNA_CSA_JobDistributor::get_updates( core::io::silent::SilentStructCOP s ) const {
	runtime_assert( s->has_energy( "updates" ) );
	return static_cast< Size >( s->get_energy( "updates" ) );
}

void
RNA_CSA_JobDistributor::set_updates( core::io::silent::SilentStructOP s, Size const updates ) const {
	s->add_string_value( "updates", utility::to_string( updates ) );
	s->set_decoy_tag( "S_" +  ObjexxFCL::lead_zero_string_of( updates, 6 ) ); // redundant with updates column.
}

////////////////////////////////////////////////////////////
void
RNA_CSA_JobDistributor::apply( core::pose::Pose & pose ) {

	///////////////////////
	// "check out" a model
	///////////////////////
	if ( sfd_ == nullptr ) read_in_silent_file();
	if ( !has_another_job() ) return;

	////////////////////
	// run some cycles
	////////////////////

	if ( sfd_->structure_list().size() < csa_bank_size_ ) {
		// If bank is not yet full, start from scratch.
		pose = *start_pose_;
		if ( stepwise_monte_carlo_ ) {
			stepwise_monte_carlo_->set_model_tag( "NEW" );
		} else {
			rna_fragment_monte_carlo_->set_out_file_tag( "NEW" );
		}
	} else {
		if ( annealing_ ) {
			if ( total_updates_so_far_ == csa_bank_size_ ) {
				// time to compute d_cut
				// only have to calculate once so it's fine not to cache it.
				csa_rmsd_ = average_pairwise_distance() / 2.0;
			} else if ( total_updates_so_far_ % csa_bank_size_ == 0 ) {
				// time to update d_cut -- for now, fixed schedule
				// Lee 2003 Phys Rev Lett suggests going from dave/2 to dave/5
				// in a fixed ratio, 'slowly' (approximately 10 pool-sizes, it looked
				// like), then staying there.
				// multiplying by 0.9124 every update approximately does this.
				csa_rmsd_ *= 0.9124;
			}
		}


		// Start from a model in the bank.
		SilentStructOP s = numeric::random::rg().random_element( sfd_->structure_list() );
		TR << TR.Cyan << "Starting from model in bank " << s->decoy_tag() <<  TR.Reset << std::endl;
		s->fill_pose( pose );
		if ( stepwise_monte_carlo_ ) {
			stepwise_monte_carlo_->set_model_tag( s->decoy_tag() );
		} else {
			rna_fragment_monte_carlo_->set_out_file_tag( s->decoy_tag() );
		}
	}
	if ( stepwise_monte_carlo_ ) {
		stepwise_monte_carlo_->apply( pose );
	} else {
		rna_fragment_monte_carlo_->apply( pose );
	}

	/////////////////////
	// now update bank
	/////////////////////
	put_lock_on_silent_file();
	read_in_silent_file();
	update_bank( pose );
	write_out_silent_file();
	free_lock_on_silent_file();

	if ( output_round_silent_files_ ) write_out_round_silent_file();
}

Real
RNA_CSA_JobDistributor::average_pairwise_distance() const {
	runtime_assert( sfd_ != nullptr );

	// This is a big computation. If you have a bank of 200 structures, you are
	// taking 200 * 200 RMSDs. At some point -- maybe jd3 migration? -- let's be
	// clever and farm them out.
	utility::vector1< SilentStructOP > const & struct_list = sfd_->structure_list();

	// Oh, don't be dumb. It's "just" 99 * 200. Upper triangle.
	Real distance = 0;
	Size num = 0;
	for ( Size ii = 1; ii <= struct_list.size(); ++ii ) {
		core::pose::PoseOP ii_pose( new Pose );
		struct_list[ ii ]->fill_pose( *ii_pose );
		for ( Size jj = ii + 1; jj <= struct_list.size(); ++jj ) {
			core::pose::PoseOP jj_pose( new Pose );
			struct_list[ jj ]->fill_pose( *jj_pose );

			distance += stepwise::modeler::align::get_rmsd( *ii_pose, *jj_pose, false, false );
			++num;
		}
	}

	return distance / Real( num );
}

////////////////////////////////////////////////////////////////////////////////////////////////
void
RNA_CSA_JobDistributor::update_bank( core::pose::Pose & pose ) {

	runtime_assert( sfd_ != nullptr );

	SilentStructOP s; // this eventually most hold the silent struct that is *updated*
	// creates a mock version of the pose with all residues instantiated. used anyway for rms_fill calculation,
	// and below used to check for closeness with poses in bank.
	core::pose::PoseOP full_model_pose;

	if ( sfd_->size() >= csa_bank_size_ ) {
		// not in initial bank-filling phase -- need to make a choice about what to keep in bank
		runtime_assert( sfd_->size() == csa_bank_size_ ); // sanity check.

		// need to decide whether to insert into bank or not.
		sfd_->order_by_energy();
		SilentStructOP s_worst = sfd_->structure_list()[ sfd_->size() ]; // note that sfd readin always orders by energy.
		Real worst_score = s_worst->get_energy( "score" );
		Real score = pose.energies().total_energy();

		// if worse in energy than everything else, don't save. but record # updates in lowest scoring pose.
		if ( score > worst_score ) {
			s = sfd_->structure_list()[ 1 ];
		} else {
			// else if better in energy ... there's a chance this will get inserted.
			utility::vector1< SilentStructOP > const & struct_list = sfd_->structure_list();
			full_model_pose = stepwise::monte_carlo::build_full_model( pose ); // makes a full model with all residues.

			//  If within X RMSD of a model, replace it.
			Size kick_out_idx( struct_list.size() );
			for ( Size n = 1; n <= struct_list.size(); n++ ) {
				SilentStructOP s_test = struct_list[ n ];
				Pose pose_test;
				s_test->fill_pose( pose_test );
				if ( check_for_closeness( pose_test, *full_model_pose ) ) {
					Real score_test = s_test->get_energy( "score" );
					TR << TR.Magenta << "Found a pose that was close. " << score_test << " vs. " << score << TR.Reset << std::endl;
					if ( score_test > score ) {
						kick_out_idx = n; // replace the pose in the bank with this one.
					} else {
						kick_out_idx = 0;
						s = s_test; // keep the lower energy pose that is already in the bank
					}
					break;
				}
			}

			if ( kick_out_idx > 0 ) {
				runtime_assert( s == nullptr ); // s will be filled below.
				SilentFileOptions opts;
				SilentFileDataOP sfd_new( new SilentFileData(opts) );
				for ( Size n = 1; n <= struct_list.size(); n++ ) if ( n != kick_out_idx ) sfd_new->add_structure( struct_list[ n ] );
				sfd_ = sfd_new;
			}
		}
	}

	if ( s == nullptr ) {
		s = stepwise::monte_carlo::prepare_silent_struct( "S_0", pose, get_native_pose(),
			superimpose_over_all_, true /*do_rms_fill_calculation*/, full_model_pose );
		sfd_->add_structure( s );
	}

	// allows for checks on book-keeping
	runtime_assert( s != nullptr );
	total_updates_so_far_ += 1;
	set_updates( s, total_updates_so_far_ );
	TR << TR.Cyan << "Outputting silent structure: " << s->decoy_tag() << TR.Reset << std::endl;
	sfd_->order_by_energy();

}

/////////////////////////////////////////////////////////////////////////
void
RNA_CSA_JobDistributor::read_in_silent_file(){

	core::io::silent::SilentFileOptions opts;
	sfd_ = utility::pointer::make_shared< SilentFileData >(opts);
	total_updates_so_far_ = 0;
	if ( !utility::file::file_exists( silent_file_ ) ) return;

	sfd_->read_file( silent_file_ );
	sfd_->order_by_energy();
	utility::vector1< SilentStructOP > const & struct_list = sfd_->structure_list();
	for ( Size n = 1; n <= struct_list.size(); n++ ) {
		SilentStructOP s = struct_list[ n ];
		total_updates_so_far_ = std::max( total_updates_so_far_, get_updates( s ) );
	}

}

/////////////////////////////////////////////////////////////////////////
void
RNA_CSA_JobDistributor::write_out_silent_file( std::string const & silent_file_in ){
	std::string const silent_file = silent_file_in.size() == 0 ?  silent_file_ : silent_file_in;
	runtime_assert( sfd_ != nullptr );
	runtime_assert( sfd_->structure_list().size() > 0 );
	runtime_assert( sfd_->structure_list().size() <= csa_bank_size_ );
	core::io::silent::remove_silent_file_if_it_exists( silent_file );
	runtime_assert( !utility::file::file_exists( silent_file ) );
	sfd_->clear_shared_silent_data(); // kind of bananas, but otherwise getting header printed out twice and lots of issues.
	sfd_->write_all( silent_file );

}

////////////////////////////////////////////////////////////////////////////////////////
void
RNA_CSA_JobDistributor::write_out_round_silent_file(){
	// keep track of stages along the way -- every time we do a multiple of csa_bank_size is a 'round'.
	if ( ( total_updates_so_far_ % csa_bank_size_ ) == 0 ) {
		Size const nrounds = total_updates_so_far_ / csa_bank_size_;
		std::string const round_silent_file = utility::replace_in( silent_file_, ".out",
			".round" + ObjexxFCL::lead_zero_string_of( nrounds, 3 ) + ".out" );
		write_out_silent_file( round_silent_file );
		TR << "Done with output: " << round_silent_file << std::endl;
	}
}

////////////////////////////////////////////////////////////////////////////////////////
void
RNA_CSA_JobDistributor::put_lock_on_silent_file() {
	Size ntries( 0 );
	Size const MAX_TRIES( 1000 );
	while ( utility::file::file_exists( lock_file ) && ++ntries <= MAX_TRIES ) {
		TR << lock_file << " exists. Will try again in one second." << std::endl;
		utility::rand_sleep();
	}
	runtime_assert( ntries <= MAX_TRIES );
	std::ofstream ostream( lock_file.c_str() );
	ostream << "locking " << silent_file_ << " by creating: " << lock_file << std::endl;
	ostream.close();
}

////////////////////////////////////////////////////////////////////////////////////////
void
RNA_CSA_JobDistributor::free_lock_on_silent_file() {
	runtime_assert( utility::file::file_exists( lock_file ) );
	utility::file::file_delete( lock_file );
}


////////////////////////////////////////////////////////////////////////////////////////
// The reason to use full_model_pose, which has all residues filled in with A-form, is
//  that we don't need to do any crazy book-keeping to match the various sister poses in one
//  model to sister poses in the other model.
//
// Also we create full_model_pose anyway to calculate rms_fill.
//
bool
RNA_CSA_JobDistributor::check_for_closeness( pose::Pose & pose_test,
	pose::Pose const & full_model_pose ) const {
	Real const rms = stepwise::modeler::align::superimpose_with_stepwise_aligner( pose_test, full_model_pose, superimpose_over_all_ );
	TR << TR.Cyan << "Calculated RMS to model: " << tag_from_pose( pose_test ) << " to be: " << rms << " and compared to " << csa_rmsd_ << TR.Reset << std::endl;
	return ( rms < csa_rmsd_ );
}


} //setup
} //rna
} //protocols
