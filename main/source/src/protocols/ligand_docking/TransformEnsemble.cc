// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file   src/protocols/ligand_docking/TransformEnsemble.cc
/// @author Thomas Willcock and Darwin Fu
/// Adapted from code by Sam Deluca

// Testing

#include <protocols/ligand_docking/TransformEnsembleCreator.hh>
#include <protocols/ligand_docking/TransformEnsemble.hh>

#include <protocols/qsar/scoring_grid/GridManager.hh>
#include <protocols/qsar/scoring_grid/GridSet.hh>
#include <protocols/qsar/scoring_grid/schema_util.hh>
#include <protocols/ligand_docking/grid_functions.hh>
#include <protocols/jd2/util.hh>
#include <protocols/rigid/RigidBodyMover.hh>
#include <protocols/rigid/RB_geometry.hh>

#include <core/conformation/Conformation.hh>
#include <core/chemical/ResidueType.hh>
#include <core/conformation/UltraLightResidue.hh>
#include <core/pose/Pose.hh>
#include <core/pose/util.hh>
#include <core/pose/PDBInfo.hh>
#include <core/import_pose/import_pose.hh>
#include <core/pose/chains_util.hh>
#include <core/kinematics/Jump.hh>
#include <core/pack/task/PackerTask.hh>
#include <core/pack/task/TaskFactory.hh>
#include <core/pack/palette/NoDesignPackerPalette.hh>
#include <protocols/ligand_docking/ligand_scores.hh>

#include <basic/datacache/DataMap.hh>
#include <basic/Tracer.hh>

#include <sstream>
#include <utility>
#include <utility/string_util.hh>

#include <numeric/numeric.functions.hh>
#include <numeric/xyz.functions.hh>
#include <numeric/random/random.hh>
#include <numeric/random/random_xyz.hh>

#include <utility/tag/Tag.hh>
#include <utility/vector0.hh>
#include <utility/io/util.hh>
#include <utility/excn/Exceptions.hh>
#include <utility/vector1.hh>
#include <utility/map_util.hh>
#include <vector>

#include <ObjexxFCL/format.hh>
#include <utility/io/ozstream.hh>

// XSD XRW Includes
#include <utility/tag/XMLSchemaGeneration.hh>
#include <protocols/moves/mover_schemas.hh>

namespace protocols {
namespace ligand_docking {

static basic::Tracer transform_tracer("protocols.ligand_docking.TransformEnsemble", basic::t_debug);

TransformEnsemble::TransformEnsemble():
	Mover("TransformEnsemble")
	// & in declaration values
{}

TransformEnsemble::TransformEnsemble(TransformEnsemble const & ) = default;

TransformEnsemble::TransformEnsemble(
	protocols::qsar::scoring_grid::GridSetCOP grid_set_prototype,
	utility::vector1<std::string> const & chains,
	core::Real const & box_size,
	core::Real const & move_distance,
	core::Real const & angle,
	core::Size const & cycles,
	core::Real const & temp
) :
	Mover("TransformEnsemble"),
	grid_set_prototype_(std::move( grid_set_prototype ))
	// & in declaration default values
{
	transform_info_.chains = chains;
	transform_info_.box_size = box_size;
	transform_info_.move_distance = move_distance;
	transform_info_.angle = angle;
	transform_info_.cycles = cycles;
	transform_info_.temperature = temp;
}

TransformEnsemble::~TransformEnsemble() = default;

protocols::moves::MoverOP TransformEnsemble::clone() const
{
	return utility::pointer::make_shared< TransformEnsemble > (*this);
}

protocols::moves::MoverOP TransformEnsemble::fresh_instance() const
{
	return utility::pointer::make_shared< TransformEnsemble >();
}

void TransformEnsemble::parse_my_tag
(
	utility::tag::TagCOP const tag,
	basic::datacache::DataMap & data,
	protocols::filters::Filters_map const & /*filters*/,
	protocols::moves::Movers_map const & /*movers*/,
	core::pose::Pose const & pose
)
{
	if ( tag->getName() != "TransformEnsemble" ) {
		throw CREATE_EXCEPTION(utility::excn::RosettaScriptsOptionError, "This should be impossible");
	}
	//Divides by root(3) so the center can only move a total equal to move_distance in each step
	transform_info_.move_distance = (tag->getOption<core::Real>("move_distance")) /sqrt(3);
	transform_info_.box_size = tag->getOption<core::Real>("box_size");
	transform_info_.angle = tag->getOption<core::Real>("angle");
	transform_info_.cycles = tag->getOption<core::Size>("cycles");
	transform_info_.temperature = tag->getOption<core::Real>("temperature");
	transform_info_.repeats = tag->getOption<core::Size>("repeats",1);
	optimize_until_score_is_negative_ = tag->getOption<bool>("optimize_until_score_is_negative",false);

	initial_perturb_ = (tag->getOption<core::Real>("initial_perturb",0.0));

	use_conformers_ = tag->getOption<bool>("use_conformers",true);

	use_main_model_ = tag->getOption<bool>("use_main_model",false);

	std::string const all_chains_str = tag->getOption<std::string>("chains");
	transform_info_.chains = utility::string_split(all_chains_str, ',');

	for ( core::Size i=1; i <= transform_info_.chains.size(); ++i ) {
		core::Size current_chain_id(core::pose::get_chain_id_from_chain(transform_info_.chains[i], pose));
		transform_info_.chain_ids.push_back(current_chain_id);
		transform_info_.jump_ids.push_back(core::pose::get_jump_id_from_chain_id(current_chain_id, pose));
	}

	if ( tag->hasOption("ensemble_proteins") ) {

		ensemble_proteins_ = tag->getOption<std::string>("ensemble_proteins");
		utility::vector1<std::string> lines = utility::io::get_lines_from_file_data(ensemble_proteins_);

		for ( const auto & line : lines ) {

			core::pose::PoseOP extra_pose = core::import_pose::pose_from_file(line,false, core::import_pose::PDB_file);
			grid_set_poses_.insert(std::make_pair(line, *extra_pose));
		}

	}


	if ( tag->hasOption("sampled_space_file") ) {
		output_sampled_space_ = true;
		sampled_space_file_ = tag->getOption<std::string>("sampled_space_file");
	}

	grid_set_prototype_ = protocols::qsar::scoring_grid::parse_grid_set_from_tag(tag, data);
}

void TransformEnsemble::apply(core::pose::Pose & pose)
{
	//Setting up ligands and ligand center
	grid_sets_.clear();
	utility::vector1<core::conformation::ResidueOP> single_conformers;
	core::Vector original_center(0,0,0);

	debug_assert( grid_set_prototype_ != nullptr );

	//Grid setup: Use centroid of all chains as center of grid
	core::Vector const center(protocols::geometry::centroid_by_chains(pose, transform_info_.chain_ids));

	grid_sets_.push_back(std::make_pair("MAIN", qsar::scoring_grid::GridManager::get_instance()->get_grids( *grid_set_prototype_, pose, center, transform_info_.chains)));

	if ( ensemble_proteins_ != "" ) {
		make_multi_pose_grids(center);
	}

	core::pack::task::PackerTaskCOP the_task( core::pack::task::TaskFactory::create_packer_task(pose, utility::pointer::make_shared< core::pack::palette::NoDesignPackerPalette >() ) );

	for ( core::Size i=1; i <= transform_info_.chains.size(); ++i ) {
		core::Size const begin(pose.conformation().chain_begin(transform_info_.chain_ids[i]));

		core::conformation::Residue original_residue = pose.residue(begin);
		original_center = original_center + original_residue.xyz(original_residue.nbr_atom());

		single_conformers.clear();
		rotamers_for_trials(pose,begin,single_conformers, *the_task);
		ligand_conformers_.insert(std::pair<core::Size, utility::vector1< core::conformation::ResidueOP > >(i, single_conformers));

	}

	original_center = original_center/(transform_info_.chains.size());

	//Setup scores and move count

	core::Real last_score(10000.0);
	core::Real best_score(10000.0);
	core::Real current_score(10000.0);

	core::pose::Pose best_pose(pose);
	core::pose::Pose starting_pose(pose);
	core::Size accepted_moves = 0;
	core::Size rejected_moves = 0;


	for ( core::Size i=1; i <= transform_info_.chains.size(); ++i ) {
		transform_tracer << "Considering " << ligand_conformers_[i].size() << " conformers during sampling for chain " << i << std::endl;
	}

	utility::io::ozstream sampled_space;
	if ( output_sampled_space_ ) {
		sampled_space.open(sampled_space_file_);
	}


	for ( core::Size repeat = 1; repeat <= transform_info_.repeats; ++repeat ) {
		pose = starting_pose;
		core::Size cycle = 0;
		bool not_converged = true;
		best_ligands_.clear();
		ligand_residues_.clear();
		reference_residues_.clear();
		last_accepted_ligand_residues_.clear();
		last_accepted_reference_residues_.clear();
		for ( core::Size i=1; i <= transform_info_.chains.size(); ++i ) {
			core::Size begin(pose.conformation().chain_begin(transform_info_.chain_ids[i]));
			core::conformation::UltraLightResidue ligand_residue( pose.residue(begin).get_self_ptr() );
			ligand_residues_.push_back(ligand_residue);
			reference_residues_.push_back(ligand_residue);
		}

		last_accepted_ligand_residues_ = ligand_residues_;
		last_accepted_reference_residues_ = reference_residues_;

		//For benchmarking purposes it is sometimes desirable to translate/rotate the ligand
		//away from the starting point before beginning a trajectory.
		if ( initial_perturb_ > 0.0 ) {
			bool perturbed = false;
			while ( !perturbed )
					{
				translate_ligand(ligand_residues_,reference_residues_,initial_perturb_);

				//Also randomize starting conformer
				if ( ligand_conformers_.size() > 1 && use_conformers_ == true ) {

					for ( core::Size i=1; i <= ligand_residues_.size(); ++i ) {
						transform_tracer << "Doing a conformer change for ligand number: " << i << std::endl;
						change_conformer(ligand_residues_[i], reference_residues_[i], i);
					}
				}
				perturbed = true;

				//new_center calculated as the weighted atom count average of the center
				core::Vector new_center(0,0,0);
				new_center = weighted_center(ligand_residues_);
				core::Real distance = new_center.distance(original_center);

				//Not everything in grid, also checks center distance
				if ( !check_grid(ligand_residues_, distance) ) {
					ligand_residues_ = last_accepted_ligand_residues_;
					reference_residues_ = last_accepted_reference_residues_;
					perturbed = false;
				}
			}

			last_accepted_ligand_residues_ = ligand_residues_;
			last_accepted_reference_residues_ = reference_residues_;
		}


		last_score = score_ligands(ligand_residues_);
		//set post-perturb ligand as best model and score
		best_ligands_ = ligand_residues_;
		best_score = last_score;

		while ( not_converged )
				{
			if ( optimize_until_score_is_negative_ ) {
				if ( cycle >= transform_info_.cycles && last_score <= 0.0 ) {
					not_converged= false;
				} else if ( cycle % 2*transform_info_.cycles == 0 ) { // Print every time we're twice the requested cycles.
					// Print a warning, so at least we can see if we're in an infinite loop.
					transform_tracer.Warning << "optimized for " << cycle << " cycles and the score (" << last_score << ") is still not negative." << std::endl;
				}
			} else {
				if ( cycle >= transform_info_.cycles ) {
					not_converged= false;
				}
			}


			//Incrementer
			cycle++;
			transform_tracer << "Cycle Number " << cycle << std::endl;
			bool move_accepted = false;

			//during each move either move the ligand or try a new conformer (if there is more than one conformer)
			//Consider each conformer change scores individually but consider moves as the ensemble score.
			if ( ligand_conformers_.size() > 1 && use_conformers_ == true && numeric::random::uniform() >= 0.5 ) {

				for ( core::Size i=1; i <= ligand_residues_.size(); ++i ) {
					transform_tracer << "Doing a conformer change for ligand number: " << i << std::endl;

					change_conformer(ligand_residues_[i], reference_residues_[i], i);

					//Not everything in grid
					if ( !check_grid(ligand_residues_) ) {
						move_accepted = false;
						ligand_residues_[i] = last_accepted_ligand_residues_[i];
						continue;
					}

					current_score = score_ligands(ligand_residues_);

					//If monte_carlo rejected
					if ( !monte_carlo(current_score, last_score) ) {
						move_accepted = false;
						ligand_residues_[i]=last_accepted_ligand_residues_[i];
					} else {
						//If monte carlo accepted, check vs. best score
						move_accepted = true;
						last_accepted_ligand_residues_[i] = ligand_residues_[i];
					}

				}

				current_score = score_ligands(ligand_residues_);
				if ( current_score < best_score ) {
					best_score = current_score;
					best_ligands_ = ligand_residues_;
				}

			} else {
				transform_ligand(ligand_residues_,reference_residues_);
				//new_center calculated as the weighted atom count average of the center
				core::Vector new_center(0,0,0);
				new_center = weighted_center(ligand_residues_);
				core::Real distance = new_center.distance(original_center);

				//Not everything in grid, also checks center distance
				if ( !check_grid(ligand_residues_, distance) ) {
					ligand_residues_ = last_accepted_ligand_residues_;
					reference_residues_ = last_accepted_reference_residues_;
					move_accepted = false;
				} else {

					current_score = score_ligands(ligand_residues_);

					//If monte_carlo rejected
					if ( !monte_carlo(current_score, last_score) ) {
						ligand_residues_ = last_accepted_ligand_residues_;
						reference_residues_ = last_accepted_reference_residues_;
						move_accepted = false;
					} else {
						//If monte carlo accepted, check vs. best score
						last_accepted_ligand_residues_ = ligand_residues_;
						last_accepted_reference_residues_ = reference_residues_;
						move_accepted = true;
						if ( current_score < best_score ) {
							best_score = current_score;
							best_ligands_ = ligand_residues_;
							transform_tracer << "accepting new best pose" << std::endl;
						}
					}
				}

			}

			if ( move_accepted ) {
				accepted_moves++;
			} else {
				rejected_moves++;
			}

			if ( output_sampled_space_ ) {
				for ( core::conformation::UltraLightResidue const & ligand_residue: ligand_residues_ ) {
					dump_conformer(ligand_residue,sampled_space);
				}
			}

		}


		core::Real accept_ratio =(core::Real)accepted_moves/((core::Real)accepted_moves
			+(core::Real)rejected_moves);
		transform_tracer <<"percent acceptance: "<< accepted_moves << " " << accept_ratio<<" " << rejected_moves <<std::endl;

		protocols::jd2::add_string_real_pair_to_current_job("Transform_accept_ratio", accept_ratio);

		std::string transform_ensemble = "TransformEnsemble";
		std::string tag;

		if ( output_sampled_space_ ) {
			sampled_space.close();
		}

		//Set pose to best scored model - report score of a single grid (the best one) if using multiple poses
		//Tracks which model was the best using core::Size best_pose_count
		core::Size best_pose_count;
		best_score = convert_to_full_pose(pose, best_pose_count);

		transform_tracer << "Accepted pose with grid score: " << best_score << std::endl;
		protocols::jd2::add_string_real_pair_to_current_job("Grid_score", best_score);


		std::map< std::string, core::Real > grid_scores;
		for ( core::Size i=1; i<=transform_info_.jump_ids.size(); ++i ) {
			core::Size jump = transform_info_.jump_ids[i];

			//Score all ligands using the best pose and report score
			utility::map_merge( grid_scores, get_ligand_grid_scores( *(grid_sets_[best_pose_count].second), jump, pose, "" ) );
		}

		for ( auto const & entry : grid_scores ) {
			protocols::jd2::add_string_real_pair_to_current_job( entry.first, entry.second );
		}

	}
}

bool TransformEnsemble::check_grid(utility::vector1<core::conformation::UltraLightResidue> & ligand_residues, core::Real distance) //distance=0 default
{

	if ( distance > transform_info_.box_size ) {
		transform_tracer << "Distance from original center: " << distance << std::endl;
		transform_tracer << "Pose rejected because the center has moved outside the box" << std::endl;
		return false;
	}

	//The score is meaningless if any atoms are outside of the grid
	for ( const auto & grid_set : grid_sets_ ) {
		if ( !grid_set.second->is_in_grid(ligand_residues) ) { //Reject the pose

			transform_tracer << "Pose rejected because atoms are outside of the grid" << std::endl;
			return false;
		}
	}
	//Everything is awesome!
	return true;

}

core::Real TransformEnsemble::score_ligands(utility::vector1<core::conformation::UltraLightResidue> & ligand_residues)
{
	core::Real score_total = 0;

	//Ligand residue level (00B, 00C, 00D...etc.) averaging taken care of in average_score function
	//Grid level averaging taken care of here (Grid 1 average score of all ligands, Grid 2 average score of all ligands...etc.)

	for ( const auto & grid_set : grid_sets_ ) {
		core::Real score = grid_set.second->average_score(ligand_residues);
		score_total = score_total + score;
		transform_tracer << "Scored ligands for " << grid_set.first << " and got " << score << std::endl;
	}
	return score_total / grid_sets_.size();
}

core::Real TransformEnsemble::convert_to_full_pose(core::pose::Pose & pose, core::Size & best_pose_count)
{

	core::Real best_score = 10000;
	best_pose_count = 1;
	std::string best_pose_hash = "";

	//Main model should be first grid set
	if ( use_main_model_ ) {
		best_pose_count = 1;

		best_pose_hash = grid_sets_[1].first;
		debug_assert( best_pose_hash == "MAIN" );

		best_score = grid_sets_[1].second->average_score(best_ligands_);
	} else {
		for ( core::Size i = 1; i <= grid_sets_.size(); ++i ) {
			core::Real current_score = grid_sets_[i].second->average_score(best_ligands_);

			transform_tracer << "Looked for best protein-ligand at " << grid_sets_[i].first << " and got " << current_score << std::endl;

			//This has a bias of order you put poses in if scores equal. Could shuffle but unlikely to two decimals
			if ( current_score <= best_score ) {
				best_pose_count = i;
				best_score = current_score;
				best_pose_hash = grid_sets_[i].first;
			}
		}

	}
	transform_tracer << "best hash is " << best_pose_hash << std::endl;

	if ( best_pose_hash == "MAIN" ) {
		for ( core::Size i=1; i <= best_ligands_.size(); ++i ) {
			best_ligands_[i].update_conformation(pose.conformation());
		}

		return best_score;
	} else {
		for ( core::Size i=1; i <= transform_info_.chain_ids.size(); ++i ) {

			core::conformation::Residue original_residue = pose.residue(pose.conformation().chain_begin(transform_info_.chain_ids[i]));
			grid_set_poses_[best_pose_hash].append_residue_by_jump(original_residue, grid_set_poses_[best_pose_hash].size(),"","",true);
			core::pose::PDBInfoOP pdb_info( grid_set_poses_[best_pose_hash].pdb_info() );
			pdb_info->obsolete(false);
			pdb_info->copy(*pose.pdb_info(),pose.conformation().chain_begin(transform_info_.chain_ids[i]),pose.conformation().chain_end(transform_info_.chain_ids[i]),grid_set_poses_[best_pose_hash].size());
		}

		for ( core::Size i=1; i <= best_ligands_.size(); ++i ) {
			best_ligands_[i].update_conformation(pose.conformation());
		}

		pose = grid_set_poses_[best_pose_hash];
		return best_score;
	}

}


void TransformEnsemble::make_multi_pose_grids(core::Vector center)
{

	qsar::scoring_grid::GridManager* grid_manager = qsar::scoring_grid::GridManager::get_instance();
	for ( const auto & grid_set_pose : grid_set_poses_ ) {

		grid_sets_.push_back(std::make_pair(grid_set_pose.first, grid_manager->get_grids( *grid_set_prototype_, grid_set_pose.second, center, transform_info_.chains)));
	}
}

bool TransformEnsemble::monte_carlo(core::Real & current, core::Real & last)
{

	core::Real const boltz_factor((last-current)/transform_info_.temperature);
	core::Real const probability = std::exp( boltz_factor);


	if ( probability < 1 && numeric::random::uniform() >= probability ) {  //reject the new pose
		transform_tracer << "Pose rejected because it didn't meet Metropolis criterion" << std::endl;
		return false;

	} else if ( probability < 1 ) {  // Accept the new pose
		last = current;
		transform_tracer << "Pose accepted because it did meet Metropolis criterion" << std::endl;
		return true;

	} else {  //Accept the new pose
		last = current;

		transform_tracer << "Pose accepted because it improved the score" << std::endl;
		return true;

	}

}

void TransformEnsemble::translate_ligand(utility::vector1<core::conformation::UltraLightResidue> & ligand_residues, utility::vector1<core::conformation::UltraLightResidue> & reference_residues, core::Real distance)
{
	//Random uniformly sampled translation sphere with radius equal to distance
	core::Vector translation = numeric::random::uniform_vector_sphere(distance);
	core::Real angle = 360;

	numeric::xyzMatrix<core::Real> rotation(
		numeric::z_rotation_matrix_degrees( angle*numeric::random::uniform() ) * (
		numeric::y_rotation_matrix_degrees( angle*numeric::random::uniform() ) *
		numeric::x_rotation_matrix_degrees( angle*numeric::random::uniform() ) ));

	core::Vector group_center = weighted_center(ligand_residues);
	core::Vector ref_group_center = weighted_center(reference_residues);

	for ( core::Size i=1; i <= ligand_residues.size(); ++i ) {
		ligand_residues[i].transform(rotation,translation,group_center);
	}

	for ( core::Size i=1; i <= reference_residues.size(); ++i ) {
		reference_residues[i].transform(rotation,translation,ref_group_center);
	}

}

void TransformEnsemble::transform_ligand(utility::vector1<core::conformation::UltraLightResidue> & ligand_residues, utility::vector1<core::conformation::UltraLightResidue> & reference_residues)
{
	if ( transform_info_.angle ==0 && transform_info_.move_distance == 0 ) {
		transform_tracer.Warning <<"angle and distance are both 0.  Transform will do nothing" <<std::endl;
		return;
	}

	core::Vector translation(
		transform_info_.move_distance*numeric::random::gaussian(),
		transform_info_.move_distance*numeric::random::gaussian(),
		transform_info_.move_distance*numeric::random::gaussian());

	numeric::xyzMatrix<core::Real> rotation(
		numeric::z_rotation_matrix_degrees( transform_info_.angle*numeric::random::gaussian() ) * (
		numeric::y_rotation_matrix_degrees( transform_info_.angle*numeric::random::gaussian() ) *
		numeric::x_rotation_matrix_degrees( transform_info_.angle*numeric::random::gaussian() ) ));

	core::Vector group_center = weighted_center(ligand_residues);
	core::Vector ref_group_center = weighted_center(reference_residues);

	for ( core::Size i=1; i <= ligand_residues.size(); ++i ) {

		ligand_residues[i].transform(rotation,translation,group_center);

	}

	for ( core::Size i=1; i <= reference_residues.size(); ++i ) {

		reference_residues[i].transform(rotation,translation,ref_group_center);
	}

}

void TransformEnsemble::change_conformer(core::conformation::UltraLightResidue & ligand_residue, const core::conformation::UltraLightResidue & reference_residue, core::Size resid)
{

	core::Size index_to_select;

	debug_assert(ligand_conformers_[resid].size());
	index_to_select = numeric::random::random_range(1,ligand_conformers_[resid].size());
	transform_tracer <<"Conformer is number: " << index_to_select << std::endl;
	core::conformation::UltraLightResidue new_residue(ligand_conformers_[resid][index_to_select]);
	new_residue.align_to_residue(reference_residue);
	ligand_residue = new_residue;

}

void TransformEnsemble::change_conformers(utility::vector1<core::conformation::UltraLightResidue> & ligand_residues, const utility::vector1<core::conformation::UltraLightResidue> & reference_residues)
{

	for ( core::Size i=1; i <= ligand_residues.size(); ++i ) {
		change_conformer(ligand_residues[i], reference_residues[i], i);

	}

}

void TransformEnsemble::dump_conformer(core::conformation::UltraLightResidue const & residue, utility::io::ozstream & output)
{
	using namespace ObjexxFCL::format;
	for ( core::Size atom_index = 1; atom_index <= residue.natoms(); ++atom_index ) {
		core::PointPosition coords = residue[atom_index];
		std::string outline( "HETATM" + I( 5,  1 ) + "  V   AAA A"
			+ I( 4, 1 ) + "    "
			+ F( 8, 3, coords.x() ) + F( 8, 3, coords.y() ) + F( 8, 3, coords.z() )
			+ F( 6, 2, 1.0 ) + ' ' + F( 5, 2, 1.0 ));
		output <<outline <<std::endl;
	}
}

void TransformEnsemble::print_xyz(core::Vector vector)
{
	transform_tracer <<"X-coordinate is: " << vector[0] << std::endl;
	transform_tracer <<"Y-coordinate is: " << vector[1] << std::endl;
	transform_tracer <<"Z-coordinate is: " << vector[2] << std::endl;

}

core::Vector TransformEnsemble::weighted_center(utility::vector1<core::conformation::UltraLightResidue> & residues)
{
	core::Vector center(0,0,0);
	core::Size total_atoms = 0;

	for ( core::Size i=1; i <=residues.size(); ++i ) {
		center = center + residues[i].center()*residues[i].natoms();
		total_atoms = total_atoms + residues[i].natoms();
	}

	center = center / total_atoms;
	return center;
}

void TransformEnsemble::provide_xml_schema( utility::tag::XMLSchemaDefinition & xsd )
{
	using namespace utility::tag;

	// AMW: In order to make non_negative_real possible with the least pain
	// it's easiest to make it come from decimal.
	// This means we're limited to 12 digits and no scientific notation.

	// To do: change this.
	XMLSchemaRestriction restriction;
	restriction.name( "non_negative_real" );
	restriction.base_type( xs_decimal );
	restriction.add_restriction( xsr_minInclusive, "0" );
	xsd.add_top_level_element( restriction );

	AttributeList attlist;
	attlist
		+ XMLSchemaAttribute::required_attribute("chains", xs_string, "Ligand chains, specified as the PDB chain IDs")
		+ XMLSchemaAttribute("sampled_space_file", xs_string, "XRW TO DO")
		+ XMLSchemaAttribute::required_attribute("move_distance", xsct_real, "Maximum translation performed per step in the monte carlo search.")
		+ XMLSchemaAttribute::required_attribute("box_size", xsct_real, "Maximum translation that can occur from the ligand starting point.")
		+ XMLSchemaAttribute::required_attribute("angle", xsct_real, "Maximum rotation angle performed per step in the monte carlo search.")
		+ XMLSchemaAttribute::required_attribute("temperature", xsct_real, "Boltzmann temperature for the monte carlo simulation.")
		+ XMLSchemaAttribute::required_attribute("cycles", xsct_non_negative_integer,
		"Total number of steps to be performed in the monte carlo simulation.")
		+ XMLSchemaAttribute::attribute_w_default("repeats", xsct_non_negative_integer,
		"Total number of repeats of the monte carlo simulation to be performed.", "1")
		+ XMLSchemaAttribute::attribute_w_default("optimize_until_score_is_negative", xsct_rosetta_bool,
		"Continue sampling beyond \"cycles\" if score is positive", "false")
		+ XMLSchemaAttribute::attribute_w_default("use_main_model", xsct_rosetta_bool,
		"Use the primary input protein model regardless of scores", "false")
		+ XMLSchemaAttribute::attribute_w_default("use_conformers", xsct_rosetta_bool,
		"Use ligand conformations while sampling", "true")
		+ XMLSchemaAttribute::attribute_w_default("ensemble_proteins", xs_string,
		"File to read protein ensemble options from", "")
		+ XMLSchemaAttribute::attribute_w_default("initial_perturb", "non_negative_real" ,
		"Make an initial, unscored translation and rotation "
		"Translation will be selected uniformly in a sphere of the given radius (Angstrom)."
		"Automatically triggers 360 degrees randomized rotation", "0.0");

	protocols::qsar::scoring_grid::attributes_for_parse_grid_set_from_tag(attlist, "The Scoring Grid set to use with TransformEnsemble scoring");

	protocols::moves::xsd_type_definition_w_attributes( xsd, mover_name(),
		"Performs a monte carlo search of the ligand ensemble binding site using precomputed scoring grids. "
		"Replaces the Translate, Rotate, and SlideTogether movers.", attlist );
}


protocols::moves::MoverOP TransformEnsembleCreator::create_mover() const
{
	return utility::pointer::make_shared< TransformEnsemble >();
}

std::string TransformEnsemble::mover_name()
{
	return "TransformEnsemble";
}

std::string TransformEnsemble::get_name() const {
	return mover_name();
}

std::string TransformEnsembleCreator::keyname() const {
	return TransformEnsemble::mover_name();
}

void TransformEnsembleCreator::provide_xml_schema( utility::tag::XMLSchemaDefinition & xsd ) const
{
	TransformEnsemble::provide_xml_schema( xsd );
}


}
}
