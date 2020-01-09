// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file RNA de novo fragment assembly
/// @brief protocols that are specific to RNA_DeNovoProtocol
/// @details
/// @author Rhiju Das, Parin Sripakdeevong, Fang-Chieh Chou


// Unit headers
#include <protocols/rna/denovo/RNA_DeNovoProtocol.hh>
#include <core/import_pose/options/RNA_DeNovoProtocolOptions.hh>
#include <protocols/rna/denovo/RNA_FragmentMonteCarlo.hh>
#include <core/fragment/rna/FullAtomRNA_Fragments.hh>
#include <protocols/rna/movers/RNA_LoopCloser.hh>
#include <protocols/rna/denovo/movers/RNA_Minimizer.hh>
#include <core/import_pose/RNA_BasePairHandler.hh>
#include <core/import_pose/RNA_DeNovoParameters.hh>
#include <protocols/rna/denovo/RNA_DeNovoPoseInitializer.hh>
#include <protocols/rna/denovo/movers/RNA_Relaxer.hh>
#include <core/import_pose/libraries/RNA_ChunkLibrary.hh>
#include <protocols/rna/setup/RNA_MonteCarloJobDistributor.hh>
#include <protocols/rna/setup/RNA_CSA_JobDistributor.hh>

// for cloud
#include <protocols/network/cloud.hh>
#include <core/io/silent/ScoreFileSilentStruct.hh>

// Package headers
#include <core/pose/rna/RNA_BasePairClassifier.hh>
#include <protocols/rna/denovo/util.hh>
#include <protocols/stepwise/modeler/rna/checker/RNA_VDW_BinChecker.hh>
#include <protocols/scoring/VDW_CachedRepScreenInfo.hh>

// Project headers
#include <core/conformation/Residue.hh>
#include <core/scoring/rms_util.hh>
#include <core/scoring/ScoreFunction.hh>
#include <core/scoring/ScoreFunctionFactory.hh>
#include <core/scoring/ScoringManager.hh>
#include <core/scoring/ScoreType.hh>
#include <core/scoring/rna/RNA_ScoringInfo.hh>
#include <core/chemical/rna/util.hh>
#include <core/scoring/rna/chemical_shift/RNA_ChemicalShiftPotential.hh>
#include <core/id/AtomID_Map.hh>
#include <core/id/AtomID.hh>
#include <core/id/DOF_ID.hh>
#include <core/kinematics/MoveMap.hh>
#include <core/pose/Pose.hh>
#include <core/pose/rna/util.hh>
#include <core/pose/extra_pose_info_util.hh>
#include <core/io/silent/RNA_SilentStruct.hh>
#include <core/io/silent/BinarySilentStruct.hh>
#include <core/io/silent/SilentFileData.hh>
#include <core/io/silent/SilentFileOptions.hh>
#include <core/io/silent/util.hh>
#include <core/io/pdb/pdb_writer.hh>
#include <core/scoring/constraints/ConstraintSet.hh>
#include <core/pose/rna/RNA_BaseDoubletClasses.hh>
#include <core/util/SwitchResidueTypeSet.hh>

#include <core/scoring/Energies.hh>
#include <core/scoring/EnergyMap.hh>

#include <utility/file/file_sys_util.hh>
#include <numeric/MathNTensor.hh>

#include <core/types.hh>
#include <basic/Tracer.hh>

// option key includes
#include <basic/options/option.hh>
// A lot of these are just for register_options right now for boinc
#include <basic/options/keys/edensity.OptionKeys.gen.hh>
#include <basic/options/keys/in.OptionKeys.gen.hh>
#include <basic/options/keys/out.OptionKeys.gen.hh>
#include <basic/options/keys/score.OptionKeys.gen.hh>
#include <basic/options/keys/full_model.OptionKeys.gen.hh>
#include <basic/options/keys/constraints.OptionKeys.gen.hh>
#include <basic/options/keys/rna.OptionKeys.gen.hh>
#include <basic/options/keys/stepwise.OptionKeys.gen.hh>
// check cloud auth, slightly alter behavior (for scorefiles)
#include <basic/options/keys/cloud.OptionKeys.gen.hh>

// External library headers

//C++ headers
#include <iostream>

//Auto Headers
#include <protocols/viewer/GraphicsState.hh>
#include <utility/vector1.hh>

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// The original protocol for Fragment Assembly of RNA (FARNA), first developed in rosetta++ in 2006.
//
// Refactored in 2015 so that actual monte carlo sampling is encapsulated in *RNA_FragmentMonteCarlo*, along with minimize/relax --
//   this allows call of fragment assembly from within other protocols like stepwise.
//
// Setup of options has moved into RNA_DeNovoProtocolOptions and RNA_FragmentMonteCarloOptions.
//
// So the jobs remaining of RNA_DeNovoProtocol are simply:
//
//   1. Setup of various libraries and movers that might be used by RNA_DeNovoProtocol.
//   2. Compute metrics on final poses (RMSD, etc.)
//   3. Output to silent files
//
// These remaining functionalities could be subsumed into a JobDistributor with appropriate PoseMetrics.
//
//                       -- Rhiju, 2015
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

using namespace ObjexxFCL::format; // AUTO USING NS
using namespace core;
using namespace core::pose::rna;
using namespace core::import_pose;
using namespace core::import_pose::libraries;

namespace protocols {
namespace rna {
namespace denovo {

static basic::Tracer TR( "protocols.rna.denovo.RNA_DeNovoProtocol" );

RNA_DeNovoProtocol::RNA_DeNovoProtocol( core::import_pose::options::RNA_DeNovoProtocolOptionsCOP options,
	RNA_DeNovoParametersCOP params):
	Mover(),
	options_(std::move( options )),
	rna_params_(std::move( params ))
{
	if ( rna_params_ == nullptr ) {
		if ( !options_->rna_params_file().empty() ) {
			rna_params_ = utility::pointer::make_shared< core::import_pose::RNA_DeNovoParameters >( options_->rna_params_file() );
		} else {
			rna_params_ = utility::pointer::make_shared< core::import_pose::RNA_DeNovoParameters >();
		}
	}
	Mover::type("RNA_DeNovoProtocol");
}

/// @brief Clone this object
protocols::moves::MoverOP RNA_DeNovoProtocol::clone() const {
	return utility::pointer::make_shared< RNA_DeNovoProtocol >(*this);
}

//////////////////////////////////////////////////
RNA_DeNovoProtocol::~RNA_DeNovoProtocol() = default;

// Maybe use an OptionsCollection here?
// static
void
RNA_DeNovoProtocol::register_options() {
	using namespace basic::options;
	using namespace basic::options::OptionKeys;
	option.add_relevant( in::file::fasta );
	option.add_relevant( in::file::native );
	option.add_relevant( in::file::input_res );
	option.add_relevant( out::file::silent );
	option.add_relevant( out::nstruct );
	option.add_relevant( out::overwrite );
	option.add_relevant( score::weights );
	option.add_relevant( score::set_weights );
	option.add_relevant( basic::options::OptionKeys::rna::denovo::sequence );
	option.add_relevant( basic::options::OptionKeys::rna::denovo::secstruct );
	option.add_relevant( basic::options::OptionKeys::rna::denovo::secstruct_file );
	option.add_relevant( basic::options::OptionKeys::rna::denovo::minimize_rna );
	option.add_relevant( basic::options::OptionKeys::rna::denovo::rounds );
	option.add_relevant( basic::options::OptionKeys::rna::denovo::fixed_stems );
	option.add_relevant( basic::options::OptionKeys::rna::denovo::obligate_pair );
	option.add_relevant( basic::options::OptionKeys::rna::denovo::obligate_pair_explicit );
	// option.add_relevant( basic::options::OptionKeys::rna::denovo::relax_rna );
	// option.add_relevant( basic::options::OptionKeys::rna::denovo::simple_relax );
	// option.add_relevant( basic::options::OptionKeys::rna::denovo::ignore_secstruct );
	option.add_relevant( basic::options::OptionKeys::rna::denovo::lores_scorefxn );
	option.add_relevant( basic::options::OptionKeys::rna::denovo::set_lores_weights);
	option.add_relevant( basic::options::OptionKeys::rna::denovo::cycles );
	option.add_relevant( basic::options::OptionKeys::rna::denovo::temperature );
	option.add_relevant( basic::options::OptionKeys::rna::denovo::jump_change_frequency );
	option.add_relevant( basic::options::OptionKeys::rna::denovo::close_loops );
	option.add_relevant( basic::options::OptionKeys::rna::denovo::close_loops_after_each_move );
	option.add_relevant( basic::options::OptionKeys::rna::denovo::heat );
	option.add_relevant( basic::options::OptionKeys::rna::denovo::staged_constraints );
	// option.add_relevant( basic::options::OptionKeys::rna::denovo::jump_library_file );
	option.add_relevant( basic::options::OptionKeys::rna::denovo::params_file );
	option.add_relevant( basic::options::OptionKeys::rna::denovo::filter_lores_base_pairs );
	option.add_relevant( basic::options::OptionKeys::rna::denovo::filter_lores_base_pairs_early );
	option.add_relevant( basic::options::OptionKeys::rna::denovo::filter_chain_closure );
	option.add_relevant( basic::options::OptionKeys::rna::denovo::filter_chain_closure_halfway );
	option.add_relevant( basic::options::OptionKeys::rna::denovo::filter_chain_closure_distance );
	option.add_relevant( basic::options::OptionKeys::rna::denovo::output_filters );
	option.add_relevant( basic::options::OptionKeys::rna::denovo::autofilter );
	option.add_relevant( basic::options::OptionKeys::rna::denovo::no_filters );
	option.add_relevant( basic::options::OptionKeys::rna::denovo::vall_torsions );
	option.add_relevant( basic::options::OptionKeys::rna::denovo::use_1jj2_torsions );
	option.add_relevant( basic::options::OptionKeys::rna::denovo::rna_lores_chainbreak_weight );
	option.add_relevant( basic::options::OptionKeys::rna::denovo::rna_lores_linear_chainbreak_weight );
	option.add_relevant( basic::options::OptionKeys::rna::denovo::allow_bulge  );
	option.add_relevant( basic::options::OptionKeys::rna::denovo::allowed_bulge_res );
	option.add_relevant( basic::options::OptionKeys::rna::denovo::allow_consecutive_bulges );
	option.add_relevant( basic::options::OptionKeys::rna::denovo::move_first_rigid_body );
	option.add_relevant( basic::options::OptionKeys::rna::denovo::root_at_first_rigid_body );
	option.add_relevant( basic::options::OptionKeys::rna::denovo::suppress_bp_constraint );
	option.add_relevant( basic::options::OptionKeys::rna::denovo::output_res_num );
	option.add_relevant( basic::options::OptionKeys::rna::denovo::offset );
	option.add_relevant( basic::options::OptionKeys::rna::denovo::tag );
	option.add_relevant( basic::options::OptionKeys::rna::denovo::refine_silent_file );
	option.add_relevant( basic::options::OptionKeys::rna::denovo::refine_native );
	option.add_relevant( basic::options::OptionKeys::rna::denovo::bps_moves );
	option.add_relevant( basic::options::OptionKeys::rna::denovo::out::dump );
	option.add_relevant( basic::options::OptionKeys::rna::denovo::out::output_lores_silent_file );
	option.add_relevant( basic::options::OptionKeys::rna::denovo::out::binary_output );
	option.add_relevant( basic::options::OptionKeys::rna::denovo::minimize::minimizer_use_coordinate_constraints );
	option.add_relevant( basic::options::OptionKeys::rna::denovo::minimize::extra_minimize_res );
	option.add_relevant( basic::options::OptionKeys::rna::denovo::minimize::extra_minimize_chi_res );
	option.add_relevant( basic::options::OptionKeys::rna::denovo::minimize::minimize_bps );
	option.add_relevant( full_model::cyclize );
	option.add_relevant( full_model::cutpoint_closed );
	option.add_relevant( full_model::cutpoint_open );
	option.add_relevant( full_model::rna::block_stack_above_res );
	option.add_relevant( full_model::rna::block_stack_below_res );
	option.add_relevant( basic::options::OptionKeys::rna::vary_geometry );
	option.add_relevant( basic::options::OptionKeys::rna::data_file );

	option.add_relevant( constraints::cst_file );

	// see if we can get density scoring to work
	option.add_relevant( basic::options::OptionKeys::edensity::mapfile );
}

/// @details  Apply the RNA de novo modeling protocol to a pose.
///
void RNA_DeNovoProtocol::apply( core::pose::Pose & pose ) {

	using namespace core::pose;
	using namespace core::scoring;
	using namespace core::io::pdb;
	using namespace core::io::silent;
	using namespace protocols::rna::denovo;

	// bps_moves and new_fold_tree_initializer are mutually exclusive
	if ( options_->bps_moves() && options_->new_fold_tree_initializer() ) {
		utility_exit_with_message( "If you want to supply -new_fold_tree_initializer true, as for an electron density case, you MUST supply -bps_moves false." );
	}

	///////////////////////////////////////////////////////////////////////////
	// A bunch of initialization
	///////////////////////////////////////////////////////////////////////////
	if ( options_->dump_pdb() ) pose.dump_pdb( "init.pdb" );

	// Set up the cached vdw rep screen info in the pose
	// requires further setup later (once user input fragments have been inserted in the pose)
	if ( options_->filter_vdw() ) {
		protocols::scoring::fill_vdw_cached_rep_screen_info_from_command_line( pose );
		vdw_grid_ = utility::pointer::make_shared< protocols::stepwise::modeler::rna::checker::RNA_VDW_BinChecker >( pose );
	}

	// RNA score function (both low-res and high-res).
	initialize_scorefxn( pose );

	// Some other silent file setup
	if ( options_->overwrite() ) remove_silent_file_if_it_exists( options_->silent_file() );
	initialize_lores_silent_file();
	initialize_tag_is_done();

	protocols::rna::denovo::RNA_DeNovoPoseInitializerOP rna_de_novo_pose_initializer( new protocols::rna::denovo::RNA_DeNovoPoseInitializer( *rna_params_ )  );
	rna_de_novo_pose_initializer->set_bps_moves( options_->bps_moves() );
	rna_de_novo_pose_initializer->set_root_at_first_rigid_body( options_->root_at_first_rigid_body() );
	rna_de_novo_pose_initializer->set_dock_each_chunk( options_->dock_each_chunk() );
	rna_de_novo_pose_initializer->set_dock_each_chunk_per_chain( options_->dock_each_chunk_per_chain() );
	rna_de_novo_pose_initializer->set_center_jumps_in_single_stranded( options_->center_jumps_in_single_stranded() );
	rna_de_novo_pose_initializer->set_new_fold_tree_initializer( options_->new_fold_tree_initializer() );
	rna_de_novo_pose_initializer->set_model_with_density( options_->model_with_density() );
	bool refine_pose( refine_pose_list_.size() > 0 || options_->refine_pose() );
	if ( !refine_pose || options_->refine_native_get_good_FT() ) rna_de_novo_pose_initializer->initialize_for_de_novo_protocol( pose, options_->ignore_secstruct() ); // virtualize phosphates, but no chainbreaks -- PUT HIGHER?

	//Keep a copy for resetting after each decoy.
	Pose start_pose = pose;
	// This copy of the pose does not have its grid_vdw stuff set up
	// But it will be set up in RNA_FragmentMonteCarlo before any sampling happens

	///////////////////////////////////////////////////////////////////////////
	// Main Loop.
	///////////////////////////////////////////////////////////////////////////
	Size refine_pose_id( 1 );
	std::list< core::Real > all_lores_score_final; // used for filtering.

	using namespace basic::options;
	using namespace basic::options::OptionKeys;
	using namespace basic::options::OptionKeys::stepwise::monte_carlo;

	std::string const silent_file = option[ out::file::silent ]();

	RNA_ChunkLibraryOP user_input_chunk_library( new RNA_ChunkLibrary( options_->chunk_pdb_files(), options_->chunk_silent_files(), pose,
		options_->input_res(), rna_params_->allow_insert_res() ) );
	RNA_BasePairHandlerOP rna_base_pair_handler( refine_pose ? new RNA_BasePairHandler( pose ) : new RNA_BasePairHandler( *rna_params_ ) );

	// main loop
	rna_fragment_monte_carlo_ = utility::pointer::make_shared< RNA_FragmentMonteCarlo >( options_ );

	rna_fragment_monte_carlo_->set_user_input_chunk_library( user_input_chunk_library );

	if ( options_->initial_structures_provided() ) {
		utility::vector1< std::string > silent_files_empty;
		RNA_ChunkLibraryOP user_input_chunk_initialization_library( new RNA_ChunkLibrary( options_->chunk_initialization_pdb_files(),
			silent_files_empty, pose, options_->input_res_initialize(), rna_params_->allow_insert_res() ) );
		rna_fragment_monte_carlo_->set_user_input_chunk_initialization_library( user_input_chunk_initialization_library );
	}
	rna_fragment_monte_carlo_->set_rna_base_pair_handler( rna_base_pair_handler ); // could later have this look inside pose's sec_struct_info

	//the jd will take care of outfile tagging
	//rna_fragment_monte_carlo_->set_out_file_tag( out_file_tag );
	rna_fragment_monte_carlo_->set_native_pose( get_native_pose() );
	rna_fragment_monte_carlo_->set_denovo_scorefxn( denovo_scorefxn_ );
	rna_fragment_monte_carlo_->set_hires_scorefxn( hires_scorefxn_ );


	protocols::rna::setup::RNA_JobDistributorOP denovo_job_distributor;
	if ( option[ csa::csa_bank_size ].user() ) {
		denovo_job_distributor = utility::pointer::make_shared< protocols::rna::setup::RNA_CSA_JobDistributor >(
			rna_fragment_monte_carlo_,
			silent_file,
			option[ out::nstruct ](),
			option[ csa::csa_bank_size ](),
			option[ csa::csa_rmsd ](),
			option[ csa::csa_output_rounds ](),
			option[ csa::annealing ]() );
	} else {
		denovo_job_distributor = utility::pointer::make_shared< protocols::rna::setup::RNA_MonteCarloJobDistributor >(
			rna_fragment_monte_carlo_,
			silent_file,
			options_->nstruct() );
	}
	denovo_job_distributor->set_native_pose( get_native_pose() );
	denovo_job_distributor->set_superimpose_over_all( option[ OptionKeys::stepwise::superimpose_over_all ]() );
	denovo_job_distributor->initialize( pose ); // start_pose_ gets saved.

	output::RNA_FragmentMonteCarloOutputterOP outputter;
	//for ( Size n = 1; n <= options_->nstruct(); n++ ) {
	Size n = 1;
	while ( denovo_job_distributor->has_another_job() ) {

		std::string const out_file_tag = "S_"+lead_zero_string_of( n++, 6 );
		if ( tag_is_done_[ out_file_tag ] ) continue;

		//std::clock_t start_time = clock();

		if ( refine_pose_list_.size() > 0 ) {
			pose = *refine_pose_list_[ refine_pose_id ];
			++refine_pose_id;
			if ( refine_pose_id > refine_pose_list_.size() ) refine_pose_id = 1;
		} else {
			pose = start_pose;
		}

		rna_fragment_monte_carlo_->set_refine_pose( refine_pose );
		rna_fragment_monte_carlo_->set_is_rna_and_protein( rna_params_->is_rna_and_protein() ); // need to know this for the high resolution stuff
		if ( options_->filter_vdw() ) rna_fragment_monte_carlo_->set_vdw_grid( vdw_grid_ );
		if ( !refine_pose || options_->refine_native_get_good_FT() ) rna_fragment_monte_carlo_->set_rna_de_novo_pose_initializer( rna_de_novo_pose_initializer ); // only used for resetting fold-tree & cutpoints on each try.
		rna_fragment_monte_carlo_->set_all_lores_score_final( all_lores_score_final );  // used for filtering.
		if ( outputter != nullptr ) rna_fragment_monte_carlo_->set_outputter( outputter); // accumulate stats in histogram.

		// This calls apply on the rna_fragment_monte_carlo_ object, to which it
		// has a shared_ptr
		denovo_job_distributor->apply( pose );

		all_lores_score_final = rna_fragment_monte_carlo_->all_lores_score_final(); // might have been updated, used for filtering.
		if ( options_->output_lores_silent_file() ) align_and_output_to_silent_file( *(rna_fragment_monte_carlo_->lores_pose()), lores_silent_file_, out_file_tag );
		outputter = rna_fragment_monte_carlo_->outputter();

		std::string const out_file_name = out_file_tag + ".pdb";
		if ( options_->dump_pdb() ) dump_pdb( pose,  out_file_name );

		//if ( options_->save_times() ) setPoseExtraScore( pose, "time", static_cast< Real >( clock() - start_time ) / CLOCKS_PER_SEC );

		align_and_output_to_silent_file( pose, options_->silent_file(), out_file_tag );

		// AMW: This is a nice opportunity to push data to the cloud. Right now we don't
		// have any need to support really general, JD-compatible options for it; it's
		// just a luxury feature. Hence why we're just using some static const bags of
		// holding for speed...
		if ( basic::options::option[ basic::options::OptionKeys::cloud::auth ].user() ) {
			static const SilentFileOptions scorefileopts{};
			static const SilentFileData sfd( "score.sc", scorefileopts );

			protocols::network::post_decoy("decoy.pdb", pose);
			ScoreFileSilentStruct sfss( scorefileopts, pose, out_file_tag );
			sfd.write_silent_struct( sfss, "score.sc" );
			std::ifstream f( "score.sc" );
			// adapted from https://stackoverflow.com/questions/2912520/read-file-contents-into-a-string-in-c
			std::string content( ( std::istreambuf_iterator<char>( f ) ),
				( std::istreambuf_iterator< char >() ) );
			protocols::network::post_file( "score.sc", content );
		}

	}

	//} //nstruct
}


///////////////////////////////////////////////////////////////////////////////////////////////////////
std::string
RNA_DeNovoProtocol::get_name() const {
	return "RNA_DeNovoProtocol";
}


///////////////////////////////////////////////////////////////////////////////////////////////////////
void
RNA_DeNovoProtocol::show(std::ostream & output) const
{
	Mover::show(output);
	output <<   "nstruct:                   " << options_->nstruct()  <<
		"\nUser defined MC cycles:        " << (options_->user_defined_cycles()  ? "True" : "False") <<
		"\nAll RNA fragment file:         " << options_->all_rna_fragments_file() <<
		"\nDump pdb:                      " << (options_->dump_pdb() ? "True" : "False") <<
		"\nMinimize structure:            " << (options_->minimize_structure() ? "True" : "False") <<
		"\nRelax structure:               " << (options_->relax_structure() ? "True" : "False") <<
		"\nIgnore secstruct:              " << (options_->ignore_secstruct() ? "True" : "False") <<
		"\nClose loops at end:            " << (options_->close_loops() ? "True" : "False") <<
		"\nClose loops in last round:     " << (options_->close_loops() ? "True" : "False") <<
		"\nClose loops after each move:   " << (options_->close_loops_after_each_move() ? "True" : "False") <<
		"\nSimple rmsd cutoff relax:      " << (options_->simple_rmsd_cutoff_relax() ? "True" : "False") <<
		"\nAllow bulges:                  " << (options_->allow_bulge() ? "True" : "False") <<
		"\nAllow consecutive bulges:      " << (options_->allow_consecutive_bulges() ? "True" : "False") <<
		"\nUse chem shift data:           " << (options_->use_chem_shift_data() ? "True" : "False") <<
		"\nDefault temperature for MC:    " << options_->temperature() <<
		"\nInput rna params file?:        " << ((options_->rna_params_file() == "" ) ? "No" : "Yes") <<
		"\nJump library file:             " << options_->jump_library_file() <<
		"\nOutput lores silent file:      " << (options_->output_lores_silent_file() ? "True" : "False") <<
		"\nFilter lores base pairs:       " << (options_->filter_lores_base_pairs() ? "True" : "False") <<
		"\nFilter lores base pairs early: " << (options_->filter_lores_base_pairs_early() ? "True" : "False") <<
		"\nFilter chain closure:          " << (options_->filter_chain_closure() ? "True" : "False") <<
		"\nFilter chain closure distance: " << options_->filter_chain_closure_distance() <<
		"\nFilter chain closure halfway:  " << (options_->filter_chain_closure_halfway() ? "True" : "False") <<
		"\nVary bond geometry:            " << (options_->vary_bond_geometry() ? "True" : "False") <<
		"\nBinary RNA output:             " << (options_->binary_rna_output() ? "True" : "False") <<
		"\nStaged constraints:            " << (options_->staged_constraints() ? "True" : "False") <<
		"\nTitrate stack bonus:           " << (options_->titrate_stack_bonus() ? "True" : "False") <<
		"\nMove first rigid body:         " << (options_->move_first_rigid_body() ? "True" : "False") <<
		"\nRoot at first rigid body:      " << (options_->root_at_first_rigid_body() ? "True" : "False") <<
		"\nOutput Filters:                " << (options_->output_filters() ? "True" : "False") <<
		"\nAutofilter:                    " << (options_->autofilter() ? "True" : "False") <<
		"\nAutofilter score quantile:     " << options_->autofilter_score_quantile() <<
		"\nBase pair step moves:          " << (options_->bps_moves() ? "True" : "False");

}

///////////////////////////////////////////////////////////////////////////////////////////////////////
void
RNA_DeNovoProtocol::initialize_scorefxn( core::pose::Pose & pose ) {

	using namespace core::scoring;
	using namespace basic::options;

	// RNA low-resolution score function.
	denovo_scorefxn_ = ScoreFunctionFactory::create_score_function( options_->lores_scorefxn() );
	apply_set_weights(  denovo_scorefxn_, option[ OptionKeys::rna::denovo::set_lores_weights ]() );

	if ( core::scoring::rna::nonconst_rna_scoring_info_from_pose( pose ).rna_data_info().rna_reactivities().size() > 0 ) {
		denovo_scorefxn_->set_weight( core::scoring::rna_chem_map_lores, option[ OptionKeys::score::rna::rna_chem_map_lores_weight ]() );
	}

	if ( options_->filter_vdw() ) {
		denovo_scorefxn_->set_weight( core::scoring::grid_vdw, options_->grid_vdw_weight() );
	}


	// initial_denovo_scorefxn_ = denovo_scorefxn_->clone();
	if ( options_->chainbreak_weight() > -1.0 ) denovo_scorefxn_->set_weight( chainbreak, options_->chainbreak_weight() );
	if ( options_->linear_chainbreak_weight() > -1.0 ) denovo_scorefxn_->set_weight( linear_chainbreak, options_->linear_chainbreak_weight() );

	initialize_constraints( pose );

	// RNA high-resolution score function.
	hires_scorefxn_ = get_rna_hires_scorefxn( rna_params_->is_rna_and_protein() );//->clone();

	if ( options_->model_with_density() ) {
		// Then we should turn on the density score terms in the low-res and the high-res score functions
		if ( denovo_scorefxn_->get_weight( core::scoring::elec_dens_fast ) <= 0 ) {
			denovo_scorefxn_->set_weight( core::scoring::elec_dens_fast, 10.0 );
		}
		if ( hires_scorefxn_->get_weight( core::scoring::elec_dens_fast ) <= 0 ) {
			hires_scorefxn_->set_weight( core::scoring::elec_dens_fast, 10.0 );
		}
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
void
RNA_DeNovoProtocol::initialize_constraints( core::pose::Pose & pose ) {

	using namespace core::scoring;

	if ( pose.constraint_set()->has_constraints() ) {
		denovo_scorefxn_->set_weight( atom_pair_constraint, 1.0 );
		denovo_scorefxn_->set_weight( coordinate_constraint, 1.0 ); // now useable in RNA denovo!
		denovo_scorefxn_->set_weight( base_pair_constraint, 1.0 );
	}
	if ( options_->rmsd_screen() > 0.0 && !denovo_scorefxn_->has_nonzero_weight( coordinate_constraint) ) {
		denovo_scorefxn_->set_weight( coordinate_constraint, 1.0 ); // now useable in RNA denovo!
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////
void
RNA_DeNovoProtocol::initialize_tag_is_done()
{
	tag_is_done_ = core::io::silent::initialize_tag_is_done( options_->silent_file() );
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////
void
RNA_DeNovoProtocol::initialize_lores_silent_file() {

	if ( !options_->output_lores_silent_file() ) return;
	lores_silent_file_ = core::io::silent::get_outfile_name_with_tag( options_->silent_file(), "_LORES" );

}


//////////////////////////////////////////////////////////////////////
void
RNA_DeNovoProtocol::calc_rmsds( core::io::silent::SilentStruct & s, core::pose::Pose & pose,
	std::string const & out_file_tag ) const
{
	Real const rmsd = rna_fragment_monte_carlo_->get_rmsd_no_superimpose( pose );
	TR << "All atom rmsd: " << rmsd << " for " << out_file_tag << std::endl;
	s.add_energy( "rms",  rmsd );

	Real const rmsd_stems = rna_fragment_monte_carlo_->get_rmsd_stems_no_superimpose( pose );
	if ( rmsd_stems > 0.0 ) {
		TR << "All atom rmsd over stems: " << rmsd_stems << " for " << out_file_tag << std::endl;
		s.add_energy( "rms_stem", rmsd_stems );
	}
}

///////////////////////////////////////////////////////////////////////////////////////////
void
RNA_DeNovoProtocol::output_silent_struct(
	core::io::silent::SilentStruct & s, core::io::silent::SilentFileData & silent_file_data,
	std::string const & silent_file, pose::Pose & pose, std::string const & out_file_tag,
	bool const score_only /* = false */ ) const
{
	using namespace core::io::silent;
	using namespace core::scoring;
	using namespace core::pose::rna;

	if ( get_native_pose() ) calc_rmsds( s, pose, out_file_tag );

	add_number_base_pairs( pose, s );
	if ( get_native_pose() ) {
		add_number_native_base_pairs( pose, *get_native_pose(), s );
	}

	// hopefully these will end up in silent file...
	if ( options_->output_filters() && ( rna_fragment_monte_carlo_ != nullptr ) ) {
		s.add_energy(  "lores_early", rna_fragment_monte_carlo_->lores_score_early() );
		if ( options_->minimize_structure() ) s.add_energy( "lores_final", rna_fragment_monte_carlo_->lores_score_final() );
	}

	TR << "Outputting to silent file: " << silent_file << std::endl;
	silent_file_data.write_silent_struct( s, silent_file, score_only );
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////
void
RNA_DeNovoProtocol::output_to_silent_file(
	core::pose::Pose & pose,
	std::string const & silent_file,
	std::string const & out_file_tag,
	bool const score_only /* = false */ ) const
{

	using namespace core::io::silent;
	using namespace core::scoring;

	if ( rna_params_->is_rna_and_protein() && (!options_->minimize_structure() || options_->output_lores_silent_file() ) ) {
		// convert back to full atom (should give stupid coords... ok for now b/c protein doesn't move)
		// if protein sidechains have moved, then the pose should already be in full atom by now (?!)
		// if the structure is getting minimized, then it should already be converted back to full atom
		core::util::switch_to_residue_type_set( pose, core::chemical::FA_STANDARD, false /* no sloppy match */, true /* only switch protein residues */, true /* keep energies! */ );
		// but as soon as I score again, it tries to recalculate rnp scores (so they get set to 0)
	}

	// Silent file setup?
	SilentFileOptions opts;
	SilentFileData silent_file_data( opts );

	// What is all this rigamarole, making the silent struct data?
	// Why do I need to supply the damn file name? That seems silly.
	TR << "Making silent struct for " << out_file_tag << std::endl;

	SilentStructOP s = ( options_->binary_rna_output() ) ?
		SilentStructOP( utility::pointer::make_shared< BinarySilentStruct >( opts, pose, out_file_tag ) ) :
		SilentStructOP( utility::pointer::make_shared< RNA_SilentStruct >( opts, pose, out_file_tag ) );

	if ( options_->use_chem_shift_data() ) add_chem_shift_info( *s, pose);

	output_silent_struct( *s, silent_file_data, silent_file, pose, out_file_tag, score_only );
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////
void
RNA_DeNovoProtocol::align_and_output_to_silent_file( core::pose::Pose & pose, std::string const & silent_file, std::string const & out_file_tag ) const
{
	if ( options_->align_output() ) { // true unless density map is supplied
		rna_fragment_monte_carlo_->align_pose( pose, true /*verbose*/ );
	}
	if ( rna_params_->is_rna_and_protein() && !options_->minimize_structure() ) {
		// copy the pose because we'll switch the residue type set when we output to silent file
		// this is a stupid hack b/c right now the pose has both centroid and full atom residues!
		core::pose::Pose pose_copy =  pose;
		output_to_silent_file( pose_copy, silent_file, out_file_tag, false /*score_only*/ );
	} else {
		output_to_silent_file( pose, silent_file, out_file_tag, false /*score_only*/ );
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////
void
RNA_DeNovoProtocol::add_chem_shift_info(core::io::silent::SilentStruct & silent_struct,
	core::pose::Pose const & const_pose) const {

	using namespace core::scoring;
	using namespace core::pose;

	runtime_assert( options_->use_chem_shift_data() );

	pose::Pose chem_shift_pose = const_pose; //HARD COPY SLOW!
	core::scoring::ScoreFunctionOP temp_scorefxn( new ScoreFunction );
	temp_scorefxn->set_weight( rna_chem_shift  , 1.00 );
	(*temp_scorefxn)(chem_shift_pose);

	EnergyMap const & energy_map=chem_shift_pose.energies().total_energies();

	Real const rosetta_chem_shift_score= energy_map[ rna_chem_shift ];

	//This statement should be very fast except possibly the 1st call.
	core::scoring::rna::chemical_shift::RNA_ChemicalShiftPotential const &
		rna_chemical_shift_potential( core::scoring::ScoringManager::
		get_instance()->get_RNA_ChemicalShiftPotential() );

	Size const num_chem_shift_data_points=rna_chemical_shift_potential.get_total_exp_chemical_shift_data_points();

	//rosetta_chem_shift_score --> Sum_square chemical_shift deviation.

	Real const chem_shift_RMSD=sqrt( rosetta_chem_shift_score /
		float(num_chem_shift_data_points) );

	silent_struct.add_energy( "chem_shift_RMSD", chem_shift_RMSD);

	silent_struct.add_energy( "num_chem_shift_data",
		float(num_chem_shift_data_points) );

	if ( silent_struct.has_energy("rna_chem_shift")==false ) {
		//If missing this term, then the rna_chem_shift weight is probably
		//zero in the weight_file.
		silent_struct.add_energy( "rna_chem_shift", 0.0);
	}
}

std::ostream & operator<< ( std::ostream &os, RNA_DeNovoProtocol const & mover )
{
	mover.show(os);
	return os;
}

} //denovo
} //rna
} //protocols
