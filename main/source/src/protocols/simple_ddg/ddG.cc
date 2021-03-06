// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file protocols/simple_ddg/ddG.cc
/// @brief implementation of the ddG class for computing interface delta dGs
///
///
/// @author Sarel Fleishman (sarelf@u.washington.edu)
/// @author Sachko Honda (honda@apl.washington.edu)
/// @author Kyle Barlow (kb@kylebarlow.com)
/// Additional notes by Honda on 12/16/2012
/// When this mover is called with PB_elec (PB potential energy) in scorefxn,
/// it enables caching poses in two (bound/unbound) states so that subsequent calls can avoid
/// resolving the PDE over and over for the same conformation.
///
/// See also core/scoring/methods/PoissonBoltzmannEnergy, core/scoring/PoissonBoltzmannPotential
///
#include <core/types.hh>

#include <core/kinematics/FoldTree.hh>
#include <core/kinematics/MoveMap.hh>

#include <core/scoring/Energies.hh>
#include <core/scoring/ScoreFunction.hh>
#include <core/scoring/methods/EnergyMethodOptions.hh>
#include <core/scoring/hbonds/HBondOptions.hh>

#include <core/pack/pack_rotamers.hh>
#include <core/pack/task/PackerTask.hh>
#include <core/pack/task/ResidueLevelTask_.hh>
#include <core/pack/task/TaskFactory.hh>

#include <core/pack/task/operation/NoRepackDisulfides.hh>
#include <core/pack/task/operation/TaskOperations.hh>

#include <core/pose/Pose.hh>
#include <core/pose/util.hh>
#include <core/pose/chains_util.hh>
#include <core/pose/extra_pose_info_util.hh>
#include <core/pose/symmetry/util.hh>
#include <core/pose/datacache/CacheableDataType.hh>
#include <core/pose/PDBInfo.hh>

#include <core/chemical/ResidueTypeSet.hh>
#include <core/conformation/ResidueFactory.hh>
#include <core/conformation/symmetry/SymmetricConformation.hh>
#include <core/conformation/symmetry/SymmetryInfo.hh>
#include <core/scoring/methods/PoissonBoltzmannEnergy.hh>

#include <protocols/calc_taskop_movers/DesignRepackMover.hh>
#include <protocols/simple_task_operations/RestrictToInterface.hh>
#include <protocols/simple_ddg/ddG.hh>
#include <protocols/simple_ddg/ddGCreator.hh>
#include <protocols/minimization_packing/MinMover.hh>
#include <basic/datacache/DataMap.hh>
#include <protocols/rigid/RigidBodyMover.hh>
#include <protocols/rosetta_scripts/util.hh>
#include <protocols/filters/Filter.hh>
#include <protocols/simple_moves/WaterBoxMover.hh>

#include <numeric/xyzVector.hh>

#include <ObjexxFCL/format.hh>

#include <utility/vector0.hh>
#include <utility/vector1.hh>
#include <utility/tag/Tag.hh>
#include <basic/Tracer.hh>
#include <basic/options/keys/pb_potential.OptionKeys.gen.hh>

// C++ headers
#include <map>
#include <algorithm>

//Auto Headers
#include <utility/excn/Exceptions.hh>
#include <boost/functional/hash.hpp>
// XSD XRW Includes
#include <utility/tag/XMLSchemaGeneration.hh>
#include <protocols/moves/mover_schemas.hh>

namespace protocols {
namespace simple_ddg {

using core::conformation::symmetry::SymmetricConformation;
using core::conformation::symmetry::SymmetryInfoCOP;

using basic::Error;
using basic::Warning;

static basic::Tracer TR( "protocols.protein_interface_design.movers.ddG" );

using namespace core;
using namespace protocols::simple_ddg;
using namespace core::scoring;

ddG::ddG() :
	moves::Mover(ddG::mover_name()),
	repeats_(0),
	rb_jump_(0),
	per_residue_ddg_(false),
	repack_unbound_(false),
	relax_mover_( /* NULL */ ),
	use_custom_task_(false),
	repack_bound_(true),
	relax_bound_(false),
	relax_unbound_(true),
	solvate_(false),
	pb_enabled_(false),
	translate_by_(1000)
{
	bound_energies_.clear();
	unbound_energies_.clear();
	bound_per_residue_energies_.clear();
	unbound_per_residue_energies_.clear();
}

ddG::ddG( core::scoring::ScoreFunctionCOP scorefxn_in,
	core::Size const jump/*=1*/) :
	moves::Mover(ddG::mover_name()),
	repeats_(1),
	rb_jump_(jump),
	per_residue_ddg_(false),
	repack_unbound_(true),
	relax_mover_( /* NULL */ ),
	use_custom_task_(false),
	repack_bound_(true),
	relax_bound_(false),
	relax_unbound_(true),
	solvate_(false),
	pb_enabled_(false),
	translate_by_(1000)
{
	scorefxn_ = scorefxn_in->clone();

	bound_energies_.clear();
	unbound_energies_.clear();
	bound_per_residue_energies_.clear();
	unbound_per_residue_energies_.clear();

	// Determine if this PB enabled.
	if ( scorefxn_->get_weight(core::scoring::PB_elec) != 0. ) {
		// Set this to PB enabled
		pb_enabled_ = true;
		TR << "PB enabled" << std::endl;
	} else {
		pb_enabled_ = false;
	}
}

ddG::ddG( core::scoring::ScoreFunctionCOP scorefxn_in,
	core::Size const jump/*=1*/,
	utility::vector1<core::Size> const & chain_ids) :
	moves::Mover(ddG::mover_name()),
	//bound_total_energy_(0.0),
	//unbound_total_energy_(0.0),
	repeats_(1),
	rb_jump_(jump),
	per_residue_ddg_(false),
	repack_unbound_(true),
	relax_mover_( /* NULL */ ),
	use_custom_task_(false),
	repack_bound_(true),
	relax_bound_(false),
	relax_unbound_(true),
	solvate_(false),
	pb_enabled_(false),
	translate_by_(1000)
{
	scorefxn_ = scorefxn_in->clone();
	chain_ids_ = chain_ids;

	bound_energies_.clear();
	unbound_energies_.clear();
	bound_per_residue_energies_.clear();
	unbound_per_residue_energies_.clear();

	// Determine if this PB enabled.
	if ( scorefxn_->get_weight(core::scoring::PB_elec) != 0. ) {
		// Set this to PB enabled
		pb_enabled_ = true;
		TR << "PB enabled" << std::endl;
	} else {
		pb_enabled_ = false;
	}
}

void ddG::parse_my_tag(
	utility::tag::TagCOP tag,
	basic::datacache::DataMap  & data,
	protocols::filters::Filters_map const & filters,
	protocols::moves::Movers_map const & movers,
	core::pose::Pose const& pose)
{
	rb_jump_ = tag->getOption<core::Size>("jump", 1);
	if ( tag->hasOption("symmetry") ) {
		TR << "Option 'symmetry' for ddG mover has no effect - symmetry is autodetected from pose." << std::endl;
	}
	per_residue_ddg_ = tag->getOption<bool>("per_residue_ddg",false);
	repack_unbound_ = tag->getOption<bool>("repack_unbound",false);
	repeats_ = tag->getOption<Size>("repeats",1);
	task_factory( protocols::rosetta_scripts::parse_task_operations( tag, data ) );
	use_custom_task( tag->hasOption("task_operations") );
	repack_bound_ = tag->getOption<bool>("repack_bound",true);
	relax_bound_ = tag->getOption<bool>("relax_bound",false);
	relax_unbound_ = tag->getOption<bool>("relax_unbound",false);
	solvate_ = tag->getOption<bool>("solvate",false);
	translate_by_ = tag->getOption<core::Real>("translate_by", 1000);

	if ( tag->hasOption( "relax_mover" ) ) {
		relax_mover( protocols::rosetta_scripts::parse_mover( tag->getOption< std::string >( "relax_mover" ), movers ) );
	}
	if ( tag->hasOption( "filter" ) ) {
		filter( protocols::rosetta_scripts::parse_filter( tag->getOption< std::string >( "filter" ), filters ) );
	}

	if ( tag->hasOption("chains") && tag->hasOption("symmetry") ) {
		throw CREATE_EXCEPTION(utility::excn::RosettaScriptsOptionError, "you cannot specify multiple chains and use symmetry mode in the ddG mover at the same time right now. Sorry");
	}


	if ( ( tag->hasOption("chain_num") || tag->hasOption("chain_name") ) && tag->hasOption("jump") ) {
		throw CREATE_EXCEPTION(utility::excn::RosettaScriptsOptionError, "you can specify either chains or jump in the ddG mover, but not both");
	}

	if ( tag->hasOption("chain_num") ) {
		chain_ids_ = utility::string_split(tag->getOption<std::string>("chain_num"),',',core::Size());
	}

	if ( tag->hasOption("chain_name") ) {
		utility::vector1<std::string> chain_names = utility::string_split(tag->getOption<std::string>("chain_name"),',',std::string());
		for ( auto & chain_name : chain_names ) {
			chain_ids_.push_back(core::pose::get_chain_id_from_chain(chain_name,pose));
		}
	}

	if ( std::find(chain_ids_.begin(), chain_ids_.end(), 1) != chain_ids_.end() ) {
		// Instead of throwing error in the case, we invert the chains to be moved to take care of this
		utility::vector1<core::Size> inverted_chain_ids_;
		for ( core::Size i=1 ; i <= pose.conformation().num_chains() ; i++ ) {
			if ( std::find(chain_ids_.begin(), chain_ids_.end(), i) == chain_ids_.end() ) {
				inverted_chain_ids_.push_back(i);
			}
		}
		chain_ids_ = inverted_chain_ids_;
	}

	// Construct score function.
	scorefxn_ = protocols::rosetta_scripts::parse_score_function( tag, data )->clone();

	// Determine if this PB enabled.
	if ( scorefxn_->get_weight(core::scoring::PB_elec) != 0. ) {
		// Set this to PB enabled
		pb_enabled_ = true;
		TR << "PB enabled.  Translation distance = " << translate_by_ << " A" << std::endl;
		if ( tag->hasOption("translate_by") && translate_by_ > 100 ) {
			TR.Warning << "Translation distance may be too large for PB-enabled scoring.  Consider 100 (default for PB enabled runs) if you run out of memory."<< std::endl;
			TR.Warning.flush();
		} else if ( !tag->hasOption("translate_by") ) {
			translate_by_ = 100;
			TR.Warning << "Translation distance set to 100 in order to save memory for the PB calculations."<<std::endl;
			TR.Warning.flush();
		}
	} else {
		pb_enabled_ = false;
	}
	TR.flush();
}

ddG::~ddG() = default;

void ddG::scorefxn( core::scoring::ScoreFunctionCOP scorefxn_in ) {
	scorefxn_ = scorefxn_in->clone();
}


void ddG::apply(Pose & pose)
{
	using namespace core::scoring::methods;

	// Check if jump setting is correct for monomer case
	if ( pose.fold_tree().num_jump() == 0 ) {
		rb_jump( 0 );
	}

	if ( per_residue_ddg_ ) {
		EnergyMethodOptionsOP energy_options( new core::scoring::methods::EnergyMethodOptions(scorefxn_->energy_method_options()) );
		energy_options->hbond_options().decompose_bb_hb_into_pair_energies(true);
		scorefxn_->set_energy_method_options(*energy_options);
	}
	Real average_ddg = 0.0;
	std::map<Size, Real> average_per_residue_ddgs;

	for ( Size repeat = 1; repeat <= repeats_; ++repeat ) {
		// calculate() attaches pb-elec related cache to the pose, but shall not change the conformation.
		calculate(pose);

		// Carry on the gathered PB energy info.
		if ( pb_cached_data_ != nullptr ) {
			pose.data().set(pose::datacache::CacheableDataType::PB_LIFETIME_CACHE, pb_cached_data_);
		}

		average_ddg += sum_ddG();
		report_ddG(TR);
		if ( per_residue_ddg_ ) {
			for ( core::Size i = 1; i <= pose.size(); ++i ) {
				core::Real bound_energy = bound_per_residue_energies_[i];
				core::Real unbound_energy = unbound_per_residue_energies_[i];
				core::Real residue_ddg = bound_energy - unbound_energy;
				if ( average_per_residue_ddgs.find(i) == average_per_residue_ddgs.end() ) {
					average_per_residue_ddgs[i] = residue_ddg;
				} else {
					average_per_residue_ddgs[i] += residue_ddg;
				}
			}
		}
	}

	average_ddg /= repeats_;
	for ( auto & average_per_residue_ddg : average_per_residue_ddgs ) {
		average_per_residue_ddg.second /= repeats_;
	}

	setPoseExtraScore(pose, "ddg",average_ddg);
	if ( per_residue_ddg_ ) {
		for ( core::Size i = 1; i <= pose.size(); ++i ) {
			std::string residue_string(utility::to_string<core::Size>(i));
			setPoseExtraScore(pose,"residue_ddg_"+residue_string,average_per_residue_ddgs[i]);
		}
	}
}

/// @details a private function for storing energy values
void
ddG::fill_energy_vector( pose::Pose const & pose, std::map< ScoreType, core::Real > & energy_map )
{
	energy_map.clear();
	if ( filter_ ) {
		filter_->report(TR, pose);
		energy_map[ core::scoring::total_score ] = filter_->report_sm( pose );
	} else {
		using namespace core::scoring;
		for ( int i=1; i<= n_score_types; ++i ) {
			if ( (*scorefxn_)[ ScoreType(i) ] != 0.0 && ScoreType(i) != pro_close ) {
				energy_map.insert( std::make_pair( ScoreType( i ), (*scorefxn_)[ ScoreType(i)] * pose.energies().total_energies()[ ScoreType( i ) ] ) );
			}
		}
	}
}

void ddG::fill_per_residue_energy_vector(pose::Pose const & pose, std::map<Size, Real> & energy_map)
{
	if ( filter_ ) {
		utility_exit_with_message("Cannot calculate per-residue ddG's with a specified filter.");
	}
	energy_map.clear();
	for ( core::Size resid = 1; resid <= pose.size(); ++resid ) {
		core::Real energy = 0.0;
		for ( int st = 1; st <= n_score_types; ++st ) {
			if ( (*scorefxn_)[ScoreType(st)] != 0.0 && ScoreType(st) != pro_close ) {
				energy += (*scorefxn_)[ScoreType(st)] * pose.energies().residue_total_energies(resid)[ScoreType(st)];
			}
		}
		energy_map.insert(std::make_pair(resid,energy));
	}
}

/// @details output the ddG values
void
ddG::report_ddG( std::ostream & out ) const
{

	using ObjexxFCL::format::F;
	using ObjexxFCL::format::LJ;
	out << "-----------------------------------------\n";
	out << " Scores                       Wghtd.Score\n";
	out << "-----------------------------------------\n";
	auto unbound_it=unbound_energies_.begin();
	for ( auto const & bound_energie : bound_energies_ ) {
		if ( unbound_it != unbound_energies_.end() ) {
			if ( std::abs( unbound_it->second ) > 0.001 || std::abs( bound_energie.second ) > 0.001 ) {
				out << ' ' << LJ( 24, bound_energie.first ) << ' ' << F( 9,3, bound_energie.second - unbound_it->second )<<'\n';
			}
			++unbound_it;
		} else {
			if ( std::abs( bound_energie.second ) > 0.001 ) {
				out << ' ' << LJ( 24, bound_energie.first ) << ' ' << F( 9,3, bound_energie.second )<<'\n';
			}
		}
	}
	out << "-----------------------------------------\n";
	out << "Sum ddg: "<< sum_ddG()<<std::endl;
}

/// @details returns the total ddG
Real
ddG::sum_ddG() const
{
	Real sum_energy(0.0);

	auto unbound_it=unbound_energies_.begin();
	for ( auto const & bound_energie : bound_energies_ ) {
		if ( unbound_it != unbound_energies_.end() ) {
			sum_energy += bound_energie.second - unbound_it->second;
			++unbound_it;
		} else {
			sum_energy += bound_energie.second;
		}
	}
	return sum_energy;
}


/// @brief compute the energy of the repacked complex in the bound and unbound states
/// @param pose_in  The base pose.
/// @return Nothing, but the base pose's cache be augmented with extra cached data if the scorefunction
/// contains non-zero PB_elec term.
void
ddG::calculate( pose::Pose const & pose_original )
{
	using namespace pack;
	using namespace protocols::moves;
	using namespace core::scoring::methods;

	// work on a copy, don't/can't modify the original comformation.
	pose::Pose pose = pose_original;
	pose.update_residue_neighbors(); // Neighbors needed for task operation calculations later

	// Dummy state as default (<= pb_enabled_=false)
	std::string original_state = "stateless";

	//----------------------------------
	// Save the original state if pb
	//----------------------------------
	// The state is marked in pose's data-cache.
	PBLifetimeCacheOP cached_data = nullptr;
	core::scoring::methods::EnergyMethodOptions emoptions = scorefxn_->energy_method_options();

	// First see if this method is called as part of PB-electrostatic computation.
	if ( pb_enabled_ ) {
		cached_data = static_cast< PBLifetimeCacheOP > (pose.data().get_ptr< PBLifetimeCache > ( pose::datacache::CacheableDataType::PB_LIFETIME_CACHE ));
		runtime_assert( cached_data != nullptr );
		original_state = cached_data->get_energy_state();
	}

	//---------------------------------
	// Bound state
	//---------------------------------
	if ( pb_enabled_ ) cached_data->set_energy_state(emoptions.pb_bound_tag());

	if ( repack_unbound_ || repack_bound_ ) {
		setup_task(pose);
	}

	// fd: now solvate the bound pose
	if ( solvate_ ) {
		if ( ! repack_bound_ ) {
			// make a "null" packer task
			task_ = core::pack::task::TaskFactory::create_packer_task( pose );
			for ( Size i(1); i <= pose.total_residue(); ++i ) {
				task_->nonconst_residue_task(i).prevent_repacking();
			}
		}

		core::scoring::ScoreFunctionOP sfwater = scorefxn_->clone();
		sfwater->set_weight( core::scoring::pointwater, 1.0 );
		setup_task(pose);
		protocols::simple_moves::WaterBoxMover wb(sfwater);
		wb.set_taskop( task_ );
		wb.apply( pose );
		setup_solvated_task( pose, true ); // true = we're bound
	}

	if ( repack_bound() || solvate_ ) {
		pack::pack_rotamers( pose, *scorefxn_, task_ );
		// if we're solvating, we also need to minimize
		if ( solvate_ ) do_minimize( pose );
	}

	if ( relax_bound() && relax_mover() ) {
		relax_mover()->apply( pose );
	}

	(*scorefxn_)( pose );
	fill_energy_vector( pose, bound_energies_ );
	if ( per_residue_ddg_ ) {
		fill_per_residue_energy_vector(pose, bound_per_residue_energies_);
	}

	//---------------------------------
	// Unbound state
	//---------------------------------
	if ( unbind(pose) ) {
		if ( pb_enabled_ ) cached_data->set_energy_state(emoptions.pb_unbound_tag());

		// solvation mode requires updating packer task
		if ( solvate_ ) {
			setup_solvated_task( pose, false ); // false = we're unbound
		}

		// if we solvate, even if we don't repack unbound,
		//    we need to give waters a chance to virtualize
		if ( solvate_ || repack_unbound_ ) {
			// Use the same task which was setup earlier
			pack::pack_rotamers( pose, *scorefxn_, task_ );
			// if we're solvating, we also need to minimize
			if ( solvate_ ) do_minimize( pose );
		}

		if ( relax_unbound() && relax_mover() ) {
			relax_mover()->apply( pose );
		}

		(*scorefxn_)( pose );
		fill_energy_vector( pose, unbound_energies_ );
		if ( per_residue_ddg_ ) {
			fill_per_residue_energy_vector(pose, unbound_per_residue_energies_);
		}

		//----------------------------------
		// Return to the original state
		//----------------------------------
		if ( pb_enabled_ ) {
			cached_data->set_energy_state( original_state );
			pb_cached_data_ = cached_data;
		}
	} // End unbind if
}

void
ddG::setup_task( pose::Pose const & pose) {
	if ( use_custom_task() ) {
		// Allows the user to define custom tasks that specify which residues
		// are allowed to repack if RestrictToInterface doesn't work for them.
		task_ = task_factory_->create_task_and_apply_taskoperations( pose );  //!!!!!
	} else {
		task_ = core::pack::task::TaskFactory::create_packer_task( pose );
		task_->initialize_from_command_line().or_include_current( true );
		core::pack::task::operation::RestrictToRepacking rpk;
		rpk.apply( pose, *task_ );
		core::pack::task::operation::NoRepackDisulfides nodisulf;
		nodisulf.apply( pose, *task_ );

		if ( chain_ids_.size() > 0 ) {
			if ( core::pose::symmetry::is_symmetric( pose ) ) {
				utility_exit_with_message("Use of chain IDs in ddG with symmetric poses is not yet supported.");
				// Mostly because I don't know if the following code is sensible with symmetric poses.
			}
			//We want to translate each chain the same direction, though it doesnt matter much which one
			core::Size first_jump = core::pose::get_jump_id_from_chain_id(chain_ids_[1],pose);
			protocols::simple_task_operations::RestrictToInterface rti( first_jump, 8.0 /*interface_distance_cutoff_*/ );
			if ( chain_ids_.size() > 1 ) {
				for ( core::Size chain_index = 2; chain_index <= chain_ids_.size(); ++chain_index ) {
					core::Size current_jump = core::pose::get_jump_id_from_chain_id(chain_ids_[chain_index],pose);
					rti.add_jump(current_jump);
				}
			}
			rti.apply(pose,*task_);
		} else if ( rb_jump_ != 0 ) {
			protocols::simple_task_operations::RestrictToInterface rti( rb_jump_, 8.0 /*interface_distance_cutoff_*/ );
			rti.apply( pose, *task_ );
		}
	} // use_custom_task
}

//fd: Set up the packertask for the unbound pose in "solvate" mode
//    - it duplicates the input task and only modifies water behavior
//      (since unbinding creates additional waters)
void
ddG::setup_solvated_task( Pose const & pose, bool bound ) {
	core::pack::task::PackerTaskOP task_new = core::pack::task::TaskFactory::create_packer_task( pose );
	task_new->or_include_current( true );

	for ( core::Size i=1; i<=task_new->total_residue(); ++i ) {
		if ( pose.residue(i).is_water() ) {
			// allow repacking
			task_new->nonconst_residue_task(i).restrict_to_repacking();
		} else {
			// use normal unbound behavior
			runtime_assert( i <= task_->total_residue() );
			if ( (bound && repack_bound_) || (!bound && repack_unbound_) ) {
				core::pack::task::ResidueLevelTask_ & newtask_i(
					dynamic_cast<core::pack::task::ResidueLevelTask_ &>(task_new->nonconst_residue_task(i) ) );
				newtask_i.update_commutative( task_->residue_task(i) );
			} else {
				task_new->nonconst_residue_task(i).prevent_repacking();
			}
		}
	}

	task_ = task_new;
}

// fd: Before the "unbind", we want to duplicate waters that are "near" both interfaces to be on both sides
void
ddG::duplicate_waters_across_jump( Pose & pose, Size jumpnum ) const {
	core::chemical::ResidueTypeSetCOP rsd_set( pose.residue_type_set_for_pose( core::chemical::FULL_ATOM_t ) );
	core::conformation::ResidueOP vrt_wat = core::conformation::ResidueFactory::create_residue( rsd_set->name_map("HOH_V") );

	core::Size nres = pose.total_residue();

	utility::vector1< bool > pose_split = pose.fold_tree().partition_by_jump(jumpnum);

	for ( core::Size i=nres; i>=1; --i ) {
		core::conformation::Residue const &res = pose.residue(i);

		if ( res.is_water() ) {
			core::Real mindist = 10.0;
			core::Size minres = 1;
			for ( core::Size j=1; j<=nres; ++j ) {
				if ( pose_split[i] == pose_split[j] ) continue;
				core::Vector xyz1 = pose.residue(i).xyz( pose.residue(i).nbr_atom() );
				core::Vector xyz2 = pose.residue(j).xyz( pose.residue(j).nbr_atom() );
				core::Real dist_i = (xyz1 - xyz2).length();
				if ( dist_i<mindist ) {
					mindist=dist_i;
					minres=j;
				}
			}

			if ( mindist<5.0 ) {
				core::conformation::ResidueOP new_res = utility::pointer::make_shared< core::conformation::Residue >( *vrt_wat );
				new_res->set_xyz(  "O", res.atom("O").xyz() );
				new_res->set_xyz( "H1", res.atom("H1").xyz() );
				new_res->set_xyz( "H2", res.atom("H2").xyz() );
				pose.append_residue_by_jump( *new_res, mindist );

				// this chain lettering is currently rejected -- need to fix  // fd: ???
				pose.pdb_info()->set_resinfo( pose.total_residue(), pose.pdb_info()->chain(minres), pose.total_residue(), ' ');
			}
		}
	} // main for loop water duplication
}

//fd: Perform a task-aware minimize (only called if we've packed with waters)
void
ddG::do_minimize( Pose & pose ) const
{
	protocols::minimization_packing::MinMoverOP min_mover(new protocols::minimization_packing::MinMover);
	min_mover->score_function( scorefxn_ );
	core::kinematics::MoveMapOP mm( new core::kinematics::MoveMap() );
	mm->set_jump( false );
	mm->set_chi( false );
	mm->set_bb( false );

	Size const nres( task_->total_residue() );
	for ( Size i(1); i <=nres; ++i ) {
		if ( pose.residue(i).is_water() ) {
			core::Size jump_i = pose.fold_tree().get_jump_that_builds_residue( i );
			mm->set_jump( jump_i, true );
		} else {
			if ( task_->design_residue( i ) || task_->pack_residue( i ) ) {
				mm->set_chi( i, true );
			}
		}
	}

	min_mover->movemap( mm );
	min_mover->apply( pose );
}


bool
ddG::unbind( pose::Pose & pose ) const
{
	if ( core::pose::symmetry::is_symmetric( pose ) ) {
		auto & symm_conf( dynamic_cast<SymmetricConformation & > ( pose.conformation()) );
		std::map< Size, core::conformation::symmetry::SymDof > dofs ( symm_conf.Symmetry_Info()->get_dofs() );

		if ( solvate_ ) {
			utility_exit_with_message("solvate_=True currently unsupported for symmetry");
		}
		rigid::RigidBodyDofSeqTransMoverOP translate( new rigid::RigidBodyDofSeqTransMover( dofs ) );
		translate->step_size( translate_by_ );
		translate->apply( pose );
	} else if ( chain_ids_.size() > 0 ) {
		//We want to translate each chain the same direction, though it doesnt matter much which one
		Vector translation_axis(1,0,0);
		for ( core::Size current_chain_id : chain_ids_ ) {
			core::Size current_jump_id = core::pose::get_jump_id_from_chain_id(current_chain_id,pose);
			if ( solvate_ ) {
				duplicate_waters_across_jump( pose, current_jump_id );
			}
			rigid::RigidBodyTransMoverOP translate( new rigid::RigidBodyTransMover( pose, current_jump_id) );

			// Commented by honda: APBS blows up grid > 500.  Just use the default just like bound-state.
			translate->step_size( translate_by_ );
			translate->trans_axis(translation_axis);
			translate->apply( pose );
		}
	} else if ( rb_jump_ != 0 ) {
		rigid::RigidBodyTransMoverOP translate( new rigid::RigidBodyTransMover( pose, rb_jump_ ) );
		if ( solvate_ ) {
			duplicate_waters_across_jump( pose, rb_jump_ );
		}

		// Commented by honda: APBS blows up grid > 500.  Just use the default just like bound-state.
		translate->step_size( translate_by_ );
		translate->apply( pose );
	} else {
		// We have no information about chains or jumps to move, so let's not try and unbind
		return false;
	}
	return true;
}

void
ddG::filter( protocols::filters::FilterOP f ) {
	filter_ = f;
}

protocols::filters::FilterOP
ddG::filter() const {
	return filter_;
}

protocols::moves::MoverOP
ddG::clone() const {
	return (protocols::moves::MoverOP) utility::pointer::make_shared< ddG >( *this );
}

std::string ddG::get_name() const {
	return mover_name();
}

std::string ddG::mover_name() {
	return "ddG";
}

utility::tag::XMLSchemaComplexTypeGeneratorOP
ddG::define_ddG_schema() {
	using namespace utility::tag;
	AttributeList attlist;

	rosetta_scripts::attributes_for_parse_score_function( attlist );

	attlist + XMLSchemaAttribute::attribute_w_default(
		"jump", xsct_non_negative_integer,
		"XSD XRW TO DO",
		"1");

	attlist + XMLSchemaAttribute::attribute_w_default(
		"per_residue_ddg", xsct_rosetta_bool,
		"XSD XRW TO DO",
		"false");

	attlist + XMLSchemaAttribute::attribute_w_default(
		"repack_unbound", xsct_rosetta_bool,
		"XSD XRW TO DO",
		"false");

	rosetta_scripts::attributes_for_parse_task_operations(attlist);

	attlist + XMLSchemaAttribute::attribute_w_default(
		"repack_bound", xsct_rosetta_bool,
		"XSD XRW TO DO",
		"true");

	attlist + XMLSchemaAttribute::attribute_w_default(
		"relax_bound", xsct_rosetta_bool,
		"Should we relax the bound state, if a relax mover is specified?  Default false.",
		"false");

	attlist + XMLSchemaAttribute::attribute_w_default(
		"relax_unbound", xsct_rosetta_bool,
		"Should we relax the unbound state, if a relax mover is specified?  Default true.",
		"true");

	attlist + XMLSchemaAttribute::attribute_w_default(
		"translate_by", xs_decimal,
		"XSD XRW TO DO",
		"1000");

	attlist + XMLSchemaAttribute(
		"relax_mover", xs_string,
		"XSD XRW TO DO");

	attlist + XMLSchemaAttribute(
		"filter", xs_string,
		"XSD XRW TO DO");

	attlist + XMLSchemaAttribute(
		"chain_num", xs_string,
		"XSD XRW TO DO");

	attlist + XMLSchemaAttribute(
		"chain_name", xs_string,
		"XSD XRW TO DO");

	// fd
	attlist + XMLSchemaAttribute::attribute_w_default(
		"solvate", xsct_rosetta_bool,
		"Solvate both bound and unbound pose (using WaterBox mover)",
		"false");

	XMLSchemaComplexTypeGeneratorOP ct_gen( new XMLSchemaComplexTypeGenerator );

	ct_gen->complex_type_naming_func(&moves::complex_type_name_for_mover)
		.element_name(mover_name())
		.description(
		"This mover is useful for reporting the total or per-residue "
		"ddgs in cases where you don't want to use the ddG filter for "
		"some reason. (also, the ddg filter can't currently do per-residue "
		"ddgs). Ddg scores are reported as string-real pairs in the output scorefile. "
		"The total ddg score has the tag \"ddg\" and the each per residue ddg has the tag "
		"\"residue_ddg_n\" where n is the residue number.")
		.add_attributes( attlist )
		.add_optional_name_attribute();
	return ct_gen;
}

void ddG::provide_xml_schema( utility::tag::XMLSchemaDefinition & xsd )
{
	auto ct_gen = define_ddG_schema();
	ct_gen->write_complex_type_to_schema(xsd);
}

std::string ddGCreator::keyname() const {
	return ddG::mover_name();
}

protocols::moves::MoverOP
ddGCreator::create_mover() const {
	return utility::pointer::make_shared< ddG >();
}

void ddGCreator::provide_xml_schema( utility::tag::XMLSchemaDefinition & xsd ) const
{
	ddG::provide_xml_schema( xsd );
}


} //movers
} //protocols
