// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file protocols/rna/denovo/RNA_FragmentMonteCarlo.cc
/// @brief  Fragment Assembly of RNA (FARNA) with full-atom refinement (FARFAR).
/// @author Rhiju Das, rhiju@stanford.edu


#include <protocols/rna/denovo/RNA_FragmentMonteCarlo.hh>
#include <core/fragment/rna/RNA_FragmentHomologyExclusion.hh>
#include <protocols/rna/denovo/movers/RNA_FragmentMover.hh>
#include <core/import_pose/RNA_BasePairHandler.hh>
#include <core/import_pose/libraries/RNA_ChunkLibrary.hh>
#include <core/import_pose/libraries/RNA_LibraryManager.hh>
#include <core/import_pose/RNA_JumpMover.hh>
#include <protocols/rna/denovo/movers/RNA_HelixMover.hh>
#include <protocols/rna/denovo/movers/RNA_DeNovoMasterMover.hh>
#include <protocols/rna/denovo/movers/RNA_Minimizer.hh>
#include <protocols/rna/denovo/movers/RNA_Relaxer.hh>
#include <protocols/rna/denovo/movers/RNP_HighResMover.hh>
#include <protocols/rna/movers/RNA_LoopCloser.hh>
#include <protocols/rna/denovo/RNA_DeNovoPoseInitializer.hh>
#include <core/pose/rna/secstruct_legacy/RNA_SecStructLegacyInfo.hh>
#include <core/import_pose/libraries/BasePairStepLibrary.hh>
#include <protocols/rna/denovo/util.hh>
#include <protocols/moves/MonteCarlo.hh>
#include <protocols/rigid/RigidBodyMover.hh>
#include <protocols/stepwise/modeler/rna/checker/RNA_VDW_BinChecker.hh>
#include <protocols/stepwise/modeler/align/util.hh> //move this to toolbox/
#include <protocols/stepwise/modeler/align/StepWisePoseAligner.hh> //move this to toolbox/
#include <protocols/stepwise/modeler/util.hh> //move this to toolbox/
#include <protocols/stepwise/modeler/rna/util.hh>
#include <core/pose/toolbox/AtomLevelDomainMap.hh>
#include <core/util/SwitchResidueTypeSet.hh>
#include <core/kinematics/ShortestPathInFoldTree.hh>
#include <core/kinematics/Jump.hh>
#include <core/pose/Pose.hh>
#include <core/pose/PDBInfo.hh>
#include <core/chemical/ChemicalManager.hh>
#include <core/pose/rna/util.hh>
#include <core/pose/util.hh>
#include <core/pose/extra_pose_info_util.hh>
#include <core/import_pose/import_pose.hh>
#include <core/import_pose/libraries/ChunkSet.hh>
#include <core/import_pose/options/RNA_FragmentMonteCarloOptions.hh>
#include <basic/options/option.hh>
#include <core/scoring/ScoreFunctionFactory.hh>

#include <core/pose/copydofs/util.hh>
#include <core/scoring/constraints/ConstraintSet.hh>
#include <core/scoring/constraints/CoordinateConstraint.hh>
#include <core/scoring/rms_util.hh>
#include <core/scoring/rna/RNA_ScoringInfo.hh>
#include <basic/database/open.hh>

#include <basic/Tracer.hh>
#include <ctime>
#include <utility/tools/make_vector.hh>
#ifdef GL_GRAPHICS
#include <protocols/viewer/ConformationViewer.hh>
#include <protocols/viewer/viewers.hh>
#endif

static basic::Tracer TR( "protocols.rna.denovo.RNA_FragmentMonteCarlo" );

using namespace core;
using namespace ObjexxFCL::format; // AUTO USING NS
using namespace protocols::rna::denovo::movers;
using namespace core::pose;
using namespace core::pose::rna;
using namespace core::import_pose::options;
using namespace core::import_pose;
using namespace core::import_pose::libraries;
using utility::tools::make_vector1;
using utility::vector1;

#ifdef GL_GRAPHICS
namespace protocols { namespace viewer {
	typedef std::map< int, ConformationViewerOP > ConformationViewers;
	extern ConformationViewers conformation_viewers;
} }
#endif

namespace protocols {
namespace rna {
namespace denovo {

////////////////////////////////////////////////////////////////////////////////////
/// @details
///  Holds the main loop for Fragment assembly of RNA (FARNA).
///
///  Two major modes:
///    in classic mode, assumes pose is unfolded, and tries to fold from scratch;
///       several stages gradually turn on chain closure, apply filters, etc.
///
///  in refine_pose mode, carries out the last stage of classic modeling, with all
///       score terms on, all constraints on, and loop closing active.
///
///  Can be called by two wrapper classes --
///   RNA_DeNovoProtocol    (setup through .params file or command lines in 'classic'
///                           rna_denovo application)
///   RNA_DeNovoOptimizer   (better encapsulation, used inside stepwise)
///
///
/// TODO
///  - simulated tempering as an alternative to gradual turn on of score terms
///  - factor out alignment stuff, unify with stepwise. [Move StepWisePoseAligner to toolbox?]
///  - encapsulate filters into function outside of main loop.
///
////////////////////////////////////////////////////////////////////////////////////

//Constructor
RNA_FragmentMonteCarlo::RNA_FragmentMonteCarlo( RNA_FragmentMonteCarloOptionsCOP options ):
	options_(std::move( options )),
	monte_carlo_cycles_max_default_( 100000 ),
	monte_carlo_cycles_( 0 ), // will be updated later based on options
	rounds_( 0 ), // will be updated later based on options
	refine_pose_( false ),
	lores_score_early_( 0.0 ),
	lores_score_final_( 0.0 ),
	is_rna_and_protein_( false )
{}

//Destructor
RNA_FragmentMonteCarlo::~RNA_FragmentMonteCarlo() = default;

////////////////////////////////////////////////////////////////////////////
void
RNA_FragmentMonteCarlo::initialize( pose::Pose & pose ) {
	initialize_parameters();
	initialize_libraries( pose );
	initialize_movers( pose ); // need the pose here for the RNP high res mover - Kalli
	initialize_score_functions();
}

////////////////////////////////////////////////////////////////////////////
void
RNA_FragmentMonteCarlo::apply( pose::Pose & pose ){

	std::clock_t start_time = clock();

	using namespace stepwise::modeler::align;

	initialize( pose );

	pose::Pose start_pose = pose;

	// The pose gets scored when this object is set up: if using grid_vdw, will see a warning that the term was not computed
	// because the alignment info is not yet set up here (although won't see the warning the first time this function is called, b/c
	// the vdw_bin will be empty as well, also causing the score term to not be computed, but without a warning
	// However, this warning is not an issue
	protocols::moves::MonteCarlo monte_carlo( pose, *denovo_scorefxn_, options_->temperature() );

	setup_monte_carlo_cycles( pose );

	Size max_tries( 1 );
	if ( options_->filter_lores_base_pairs() || options_->filter_chain_closure() )  max_tries = 10;

	vector1< Size > moving_res_list; // used for alignment

	for ( Size ntries = 1; ntries <= max_tries; ++ntries ) {

		time_t pdb_start_time = time(nullptr);

		if ( ntries > 1 ) TR << TR.Red << "Did not pass filters. Trying the model again: trial " << ntries << " out of " << max_tries << TR.Reset << std::endl;

		pose = start_pose;

		// save constraints
		core::scoring::constraints::ConstraintSetOP pose_constraints = pose.constraint_set()->clone();

		kinematics::FoldTree initial_structures_ft;
		Pose initial_structure;

		// set up initial conformations, or initial rigid body arrangements
		// (differs from refine_native, because a partial structure may be provided)
		if ( options_->initial_structures_provided() ) {
			setup_full_initial_structure( pose );
			initial_structure = pose;
		}

		Pose save_structure;
		if ( refine_pose_ && options_->refine_native_get_good_FT() ) {
			save_structure = pose;
		}

		if ( (!refine_pose_ || options_->refine_native_get_good_FT()) && rna_de_novo_pose_initializer_ != nullptr ) rna_de_novo_pose_initializer_->setup_fold_tree_and_jumps_and_variants( pose, *(rna_denovo_master_mover_->rna_jump_mover()), atom_level_domain_map_, *rna_chunk_library_ );

		// the new fold tree initializer changes the structure of the pose, but especially with density
		// we really need to use it to get a reasonable fold tree
		// but if we're refining native, really all we want from the pose initializer is a nice fold tree
		if ( refine_pose_ && options_->refine_native_get_good_FT() ) {
			copy_structure_keep_fold_tree( pose, save_structure );
		}

		if ( options_->initial_structures_provided() ) {
			copy_structure_keep_fold_tree( pose, initial_structure );
		}

		pose.constraint_set( pose_constraints );

		if ( !options_->bps_moves() ) rna_base_pair_handler_->setup_base_pair_constraints( pose, atom_level_domain_map_, options_->suppress_bp_constraint() ); // needs to happen after setting cutpoint variants, etc.

		constraint_set_ = pose.constraint_set()->clone();

		if ( align_pose_ != nullptr ) moving_res_list = reroot_pose_before_align_and_return_moving_res( pose );

		rna_chunk_library_->initialize_random_chunks( pose, options_->dump_pdb() ); //actually not random if only one chunk in each region.

		if ( refine_pose_ ) core::pose::copydofs::copy_dofs_match_atom_names( pose, start_pose );

		if ( options_->initial_structures_provided() ) core::pose::copydofs::copy_dofs_match_atom_names( pose, initial_structure );

		rna_denovo_master_mover_->set_close_loops( options_->close_loops_after_each_move() );
		if ( options_->close_loops_after_each_move() ) rna_loop_closer_->apply( pose );

		// Could probably set up VDW filter in a way that doesn't require setup every time that this apply function is called
		// But would require changes to the RNA_VDW_BinChecker object first.
		if ( options_->filter_vdw() ) {
			vdw_grid_->FARFAR_setup_using_user_input_VDW_pose( options_->VDW_rep_screen_info(),
				pose, options_->vdw_rep_screen_include_sidechains() );
		}

		if ( !refine_pose_ && !options_->initial_structures_provided() ) {
			if ( is_rna_and_protein_ ) {
				// putting this here to be sure to affect ONLY rna-protein runs
				rna_denovo_master_mover_->do_random_moves( pose, monte_carlo_cycles_, true /* set heat cycles based on number of RNA res only */  );
			} else {
				rna_denovo_master_mover_->do_random_moves( pose, monte_carlo_cycles_ );
			}
		}

		// this works here I think, doesn't create atom mapping issues, but it may create some issues with
		// the score function
		if ( options_->convert_protein_centroid() /* default true */ && is_rna_and_protein_ ) {
			core::util::switch_to_residue_type_set( pose, core::chemical::CENTROID, false /* no sloppy match */, true /* only switch protein residues */, false /* keep energies! */ );
		}

		if ( is_rna_and_protein_ && options_->rna_protein_docking() && options_->rna_protein_docking_legacy() ) {
			rna_denovo_master_mover_->setup_rnp_fold_tree( pose, refine_pose_, options_->randomize_init_rnp() /* default true*/ );
			// try doing initial search of rigid body positions
			// make an option
			// for now let's only do this if we have density
			if ( options_->model_with_density() && options_->randomize_init_rnp() ) {
				rna_denovo_master_mover_->search_rigid_body_orientation( pose );
			}
		}

		monte_carlo.reset( pose );

		if ( options_->verbose() ) TR << "Beginning main loop... " << std::endl;

		bool found_solution( true );
		for ( Size r = 1; r <= rounds_; r++ ) {

			if ( options_->verbose() ) TR << TR.Blue << "Beginning round " << r << " of " << rounds_ << TR.Reset << std::endl;

#ifdef GL_GRAPHICS
			for ( auto const & elem : protocols::viewer::conformation_viewers ) {
				elem.second->set_center_vector( core::pose::get_center_of_mass( pose ) );
			}
#endif

			if ( r == rounds_ && options_->close_loops() ) rna_denovo_master_mover_->set_close_loops( true );

			//Keep score function coarse for early rounds.
			update_denovo_scorefxn_weights( r );

			monte_carlo.score_function( *working_denovo_scorefxn_ );

			pose = monte_carlo.lowest_score_pose();

			// Introduce constraints in stages.
			update_pose_constraints( r, pose );
			if ( align_pose_ != nullptr && !options_->disallow_realign() ) align_pose_and_add_rmsd_constraints( pose, align_pose_, moving_res_list, options_->rmsd_screen() );
			monte_carlo.reset( pose );

			update_rna_denovo_master_mover( r, pose );

			////////////////////////////////
			// This is it ... do the loop.
			////////////////////////////////
			for ( Size i = 1; i <= monte_carlo_cycles_ / rounds_; ++i ) {
				rna_denovo_master_mover_->apply( pose, i );
				if ( rna_denovo_master_mover_->success() ) monte_carlo.boltzmann( pose, rna_denovo_master_mover_->move_type() );
				outputter_->output_running_info( r, i, pose, working_denovo_scorefxn_ );
			}

			if ( get_native_pose() && options_->verbose() ) TR << get_rmsd( pose ) << " is all atom rmsd." << std::endl;

			monte_carlo.recover_low( pose );
			if ( options_->verbose() ) monte_carlo.show_counters();
			monte_carlo.reset_counters();

			if ( r == 2 || rounds_ == 1 ) { //special 'early' stage
				lores_score_early_ = (*working_denovo_scorefxn_)( pose );
				if ( options_->filter_lores_base_pairs_early() ) {
					bool const base_pairs_OK = rna_base_pair_handler_->check_base_pairs( pose, atom_level_domain_map_ );
					if ( options_->verbose() ) TR << "Checking base pairs early! Result: " << base_pairs_OK << std::endl;
					if ( !base_pairs_OK ) {
						found_solution = false;
						break;
					}
				}
			}

			if ( r == rounds_ / 2 || rounds_ == 1 ) { // halfway point
				if ( options_->filter_chain_closure_halfway() ) {
					Real const filter_chain_closure_distance_halfway = 2 * options_->filter_chain_closure_distance();
					bool const rna_loops_OK = rna_loop_closer_->check_closure( pose, filter_chain_closure_distance_halfway );
					if ( options_->verbose() ) TR << "Checking loop closure with tolerance of " << filter_chain_closure_distance_halfway << " Angstroms! Result: " << rna_loops_OK << std::endl;
					if ( !rna_loops_OK ) {
						found_solution = false;
						break;
					}
				}
			}
		}

		time_t pdb_end_time = time(nullptr);
		if ( options_->verbose() ) TR << "Finished fragment assembly of " << out_file_tag_ << " in " << (long)(pdb_end_time - pdb_start_time) << " seconds." << std::endl;

		if ( !found_solution ) { // Just try again if early exit from above
			if ( ntries == max_tries ) pose = monte_carlo.lowest_score_pose();
			continue;
		}
		pose = monte_carlo.lowest_score_pose();


		if ( options_->close_loops() ) rna_loop_closer_->apply( pose, rna_base_pair_handler_->connections() );

		// A bunch of filters
		if ( options_->filter_chain_closure() ) {
			if ( !rna_loop_closer_->check_closure( pose, options_->filter_chain_closure_distance() ) ) {
				if ( options_->verbose() ) TR << "Failed chain closure filter." << std::endl;
				continue;
			}
		}

		if ( options_->filter_lores_base_pairs() ) {
			if ( !rna_base_pair_handler_->check_base_pairs( pose, atom_level_domain_map_ ) ) {
				if ( options_->verbose() ) TR << "Failed base pairing filter." << std::endl;
				continue;
			}
		}

		lores_score_final_ = (*working_denovo_scorefxn_)( pose );
		if ( options_->autofilter() ) {
			if ( !check_score_filter( lores_score_final_, all_lores_score_final_ ) ) {
				if ( options_->verbose() ) TR << "Failed score filter." << std::endl;
				continue;
			}
		}
		break; //Pass all the filters, early exit
	} // ++ntries <= max_tries

	// Get the full strength constraint back. Next few lines repeat code from above -- would be better to stick in a function.
	update_denovo_scorefxn_weights( rounds_ );
	update_pose_constraints( rounds_, pose );
	if ( align_pose_ != nullptr && !options_->disallow_realign() ) {
		stepwise::modeler::align::align_pose_and_add_rmsd_constraints( pose, align_pose_, moving_res_list, options_->rmsd_screen() );
	}

	if ( options_->verbose() ) {
		working_denovo_scorefxn_->show( TR, pose ); TR << std::endl;
	}
	final_scorefxn_ = working_denovo_scorefxn_;

	lores_pose_ = pose.clone();

	// AMW From output...
	// Don't do this if we had been called in "legacy mode" -- best
	// clue to this is if there's a params file, as that is the only
	// circumstance that requires it.
	if ( options_->rna_params_file().empty() ) {
		align_pose( *lores_pose_, true );
	}

	if ( options_->minimize_structure() ) {
		if ( is_rna_and_protein_ ) {
			// convert the pose back to full atom
			if ( options_->convert_protein_centroid() ) {
				core::util::switch_to_residue_type_set( pose, core::chemical::FA_STANDARD, false /* no sloppy match */, true /* only switch protein residues */, false /* don't keep the energies! */ );
				rna_chunk_library_->insert_random_protein_chunks( pose ); //actually not random if only one chunk in each region.
			}

			if ( options_->rnp_min_first() ) {
				rna_minimizer_->set_atom_level_domain_map( atom_level_domain_map_ );
				rna_minimizer_->apply( pose );
			}

			// default true
			if ( options_->rnp_high_res_relax() ) {
				rnp_high_res_mover_->initialize( pose );
				rnp_high_res_mover_->apply( pose );
			}
			rna_minimizer_->set_atom_level_domain_map( atom_level_domain_map_ );
			rna_minimizer_->apply( pose );
		} else {
			rna_minimizer_->set_atom_level_domain_map( atom_level_domain_map_ );
			rna_minimizer_->apply( pose );
		}
		if ( options_->close_loops() ) rna_loop_closer_->apply( pose, rna_base_pair_handler_->connections() );
		final_scorefxn_ = hires_scorefxn_;
	}

	if ( options_->use_chem_shift_data() ) apply_chem_shift_data( pose );

	if ( options_->relax_structure() ) rna_relaxer_->apply( pose );

	if ( options_->allow_bulge() ) {
		//Identify and virtualize the bulge residues.
		virtualize_bulges(
			pose, options_->allowed_bulge_res(), final_scorefxn_, out_file_tag_,
			true /*allow_pre_virtualize*/, options_->allow_consecutive_bulges(),
			true /*verbose*/ );
	}

	outputter_->finalize( denovo_scorefxn_ );

	pose.constraint_set( constraint_set_ );

	final_score( pose ); // may include rna_chem_map score here.






	// From pose output
	// Similarly, these are code-paths that are also carried out in RNA_DeNovoProtocol,
	// which at this point is a wrapper only used in the legacy codepath.
	if ( options_->rna_params_file().empty() ) {
		align_pose( pose, true /*verbose*/ );

		// From pose output
		if ( is_rna_and_protein_ && !options_->minimize_structure() ) {
			// convert back to full atom (should give stupid coords... ok for now b/c protein doesn't move)
			// if protein sidechains have moved, then the pose should already be in full atom by now (?!)
			// if the structure is getting minimized, then it should already be converted back to full atom
			core::util::switch_to_residue_type_set( pose, core::chemical::FA_STANDARD, false, true, true  );
			// but as soon as I score again, it tries to recalculate rnp scores (so they get set to 0)
		}

		if ( get_native_pose() ) {
			setPoseExtraScore( pose, "rms", get_rmsd_no_superimpose( pose ) );
			setPoseExtraScore( pose, "rms_stem", get_rmsd_stems_no_superimpose( pose ) );
			setPoseExtraScore( *lores_pose_, "rms", get_rmsd_no_superimpose( *lores_pose_ ) );
			setPoseExtraScore( *lores_pose_, "rms_stem", get_rmsd_stems_no_superimpose( *lores_pose_ ) );
		}

		// hopefully these will end up in silent file...
		if ( options_->output_filters() ) { //}&& ( rna_fragment_monte_carlo_ != nullptr ) ) {
			//s.add_energy(  "lores_early", rna_fragment_monte_carlo_->lores_score_early() );
			setPoseExtraScore( pose, "lores_early", lores_score_early() );
			setPoseExtraScore( *lores_pose_, "lores_early", lores_score_early() );
			//if ( options_->minimize_structure() ) s.add_energy( "lores_final", rna_fragment_monte_carlo_->lores_score_final() );
			if ( options_->minimize_structure() ) setPoseExtraScore( pose, "lores_final", lores_score_final() );
			if ( options_->minimize_structure() ) setPoseExtraScore( *lores_pose_, "lores_final", lores_score_final() );
		}

		//TR << "Outputting to silent file: " << silent_file << std::endl;
		if ( options_->save_times() ) setPoseExtraScore( pose, "time", static_cast< Real >( clock() - start_time ) / CLOCKS_PER_SEC );

	}



	//if ( options_->use_chem_shift_data() ) add_chem_shift_info( *s, pose);

}

PoseOP
RNA_FragmentMonteCarlo::get_additional_output() {
	// we need to trigger "no lores output" somehow.
	if ( options_->output_lores_silent_file() ) {
		return lores_pose_;
	} else {
		return nullptr;
	}
}

////////////////////////////////////////////////////////////////////////////
void
RNA_FragmentMonteCarlo::initialize_parameters() {
	// parameters for run.
	//rounds_ = refine_pose_ ? 1 : options_->rounds();
	if ( refine_pose_ && !options_->override_refine_pose_rounds() ) {
		rounds_ = 1;
	} else {
		rounds_ = options_->rounds();
	}

	if ( options_->rmsd_screen() > 0 ) {
		// Ideally should be able to do alignment in general case (even without rmsd_screen),
		// but running into a funny issue with rerooting.
		align_pose_ = get_native_pose();
		if ( options_->align_pdb().size() > 0 ) {
			using namespace core::chemical;
			ResidueTypeSetCOP rsd_set = ChemicalManager::get_instance()->residue_type_set( FA_STANDARD );
			align_pose_ = core::import_pose::get_pdb_with_full_model_info( options_->align_pdb(), rsd_set );
		}
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////
// Sets up ChunkLibrary for the run, which holds information on where user-input
//  segments ('chunks') are in the pose, and where special base-pair-steps (stored
//  in the Rosetta database) might be added.
//
// The resulting atom_level_domain_map_ information will mark atoms where fragments and jumps
//  cannot be placed.
///////////////////////////////////////////////////////////////////////////////////////////////
void
RNA_FragmentMonteCarlo::initialize_libraries( pose::Pose & pose ) {
	if ( user_input_rna_chunk_library_ != nullptr ) {
		rna_chunk_library_ = user_input_rna_chunk_library_->clone();
	} else {
		rna_chunk_library_ = utility::pointer::make_shared< RNA_ChunkLibrary >( pose );
		// AMW: fixed a bad bug here! We should disallow movement of extra_min_res input_res
		// if bps_moves aren't on, as well!
		if ( !options_->bps_moves() || options_->disallow_bps_at_extra_min_res() ) rna_chunk_library_->atom_level_domain_map()->disallow_movement_of_input_res( pose );
	}

	// set up the initialization library, just used to provide initial rigid body orientations of chunks
	// especially useful with density maps
	if ( user_input_rna_chunk_initialization_library_ != nullptr ) {
		rna_chunk_initialization_library_ = user_input_rna_chunk_initialization_library_->clone();
	}

	if ( rna_base_pair_handler_ == nullptr ) rna_base_pair_handler_ = utility::pointer::make_shared< RNA_BasePairHandler >( pose );

	if ( options_->bps_moves() ) {
		rna_chunk_library_->setup_base_pair_step_chunks( pose, rna_base_pair_handler_->get_canonical_base_pair_steps(),
			RNA_LibraryManager::get_instance()->canonical_base_pair_step_library() );
		rna_chunk_library_->setup_base_pair_step_chunks( pose, rna_base_pair_handler_->get_noncanonical_base_pair_steps(),
			RNA_LibraryManager::get_instance()->general_base_pair_step_library() );
		if ( options_->allow_fragment_moves_in_bps() ) rna_chunk_library_->update_to_move_rosetta_library_chunks();
	}

	atom_level_domain_map_ = rna_chunk_library_->atom_level_domain_map(); // will be shared with other movers (!)
}

////////////////////////////////////////////////////////////////////////////
void
RNA_FragmentMonteCarlo::initialize_movers( core::pose::Pose const & pose ) {
	rna_loop_closer_ = utility::pointer::make_shared< protocols::rna::movers::RNA_LoopCloser >();
	rna_loop_closer_->set_atom_level_domain_map( atom_level_domain_map_ );

	if ( options_->initial_structures_provided() ) {
		rna_loop_closer_init_ = utility::pointer::make_shared< protocols::rna::movers::RNA_LoopCloser >();
		rna_loop_closer_init_->set_atom_level_domain_map( rna_chunk_initialization_library_->atom_level_domain_map() );
	}

	// MasterMover handles fragments, jumps, chunks, docking
	rna_denovo_master_mover_ = utility::pointer::make_shared< RNA_DeNovoMasterMover >( options_, atom_level_domain_map_,
		rna_base_pair_handler_, rna_loop_closer_,
		rna_chunk_library_,
		denovo_scorefxn_->clone()
	);

	if ( options_->initial_structures_provided() ) {
		rna_denovo_master_mover_init_ = utility::pointer::make_shared< RNA_DeNovoMasterMover >( options_,
			rna_chunk_initialization_library_->atom_level_domain_map(),
			rna_base_pair_handler_, rna_loop_closer_init_,
			rna_chunk_initialization_library_, denovo_scorefxn_->clone() );
	}

	if ( options_->dock_each_chunk_per_chain() && (options_->helical_substruct_res().size() > 0) ) {
		bool move_first_rigid_body = false;
		if ( options_->move_first_rigid_body() || options_->dock_into_density() ) {
			move_first_rigid_body = true;
		}
		rna_helix_mover_ = utility::pointer::make_shared< protocols::rna::denovo::movers::RNA_HelixMover >(
			options_->helical_substruct_res(), rna_base_pair_handler_, move_first_rigid_body );
		// check whether the helix mover actually has helices that it can move
		// if not, don't set it in the master mover (so it will never be called)
		rna_denovo_master_mover_->set_rna_helix_mover( rna_helix_mover_ );
	}

	if ( options_->relax_structure() || options_->minimize_structure() ) {
		rna_minimizer_ = utility::pointer::make_shared< RNA_Minimizer >( options_ );
		rna_minimizer_->set_score_function( hires_scorefxn_ );
		rna_minimizer_->set_atom_level_domain_map( atom_level_domain_map_ );

		rna_relaxer_ = utility::pointer::make_shared< RNA_Relaxer >( rna_denovo_master_mover_->rna_fragment_mover(), rna_minimizer_);
		rna_relaxer_->simple_rmsd_cutoff_relax( options_->simple_rmsd_cutoff_relax() );
	}

	// separate dependency on minimize_structure?
	if ( is_rna_and_protein_ && options_->minimize_structure() && options_->rnp_high_res_relax() ) {
		rnp_high_res_mover_ = utility::pointer::make_shared< RNP_HighResMover >( rna_denovo_master_mover_->rna_fragment_mover(), rna_loop_closer_, options_ );
		rnp_high_res_mover_->initialize( pose );
	}

	if ( outputter_ == nullptr ) {
		outputter_ = utility::pointer::make_shared< output::RNA_FragmentMonteCarloOutputter >( options_, align_pose_ );
	}
}
////////////////////////////////////////////////////////////////////////////
void
RNA_FragmentMonteCarlo::initialize_score_functions() {
	// scorefxns
	using namespace core::scoring;
	working_denovo_scorefxn_ = denovo_scorefxn_->clone();
	// RNA high-resolution score function + rna_chem_shift term
	if ( options_->use_chem_shift_data() ) {
		Real const CS_weight = 4.0; //hard-coded to 4.0 based on CS-ROSETTA-RNA work (Parin et al. 2012).
		core::scoring::ScoreFunctionOP chem_shift_scorefxn = hires_scorefxn_->clone();
		chem_shift_scorefxn->set_weight( rna_chem_shift, CS_weight );
		chem_shift_scorefxn_ = chem_shift_scorefxn;
	}

	if ( options_->initial_structures_provided() ) {
		chainbreak_sfxn_ = ScoreFunctionFactory::create_score_function( "empty" );
		chainbreak_sfxn_->set_weight( linear_chainbreak, 10.0 );
	}
}

////////////////////////////////////////////////////////////////////////////////////////
/// @brief moves should get finer as simulation progresses through rounds
void
RNA_FragmentMonteCarlo::update_rna_denovo_master_mover( Size const & r /*round number*/,
	pose::Pose const & pose )
{
	// Finer and finer fragments
	Real const frag_size_old = rna_denovo_master_mover_->frag_size();
	rna_denovo_master_mover_->set_frag_size( update_frag_size( r ) );
	if ( rna_denovo_master_mover_->frag_size() != frag_size_old ) {
		TR << "Fragment size: " << rna_denovo_master_mover_->frag_size()  << std::endl;
	}

	Real rot_mag, trans_mag;
	Real const rot_mag_init( 10.0 ),   rot_mag_final( 0.2 );
	Real const trans_mag_init( 5.0 ), trans_mag_final( 0.1 );
	get_rigid_body_move_mags( r, rot_mag, trans_mag, rot_mag_init, trans_mag_init,
		rot_mag_final, trans_mag_final );

	// finer rigid body moves
	// only set up the rigid body mover if we specified virtual anchor, RNA/protein docking, or docking into density
	if ( options_->docking() ) {
		rna_denovo_master_mover_->setup_rigid_body_mover( pose, rot_mag, trans_mag ); // needs to happen after fold_tree is decided...
	}

	if ( options_->dock_into_density() && options_->dock_into_density_legacy() ) {
		// Should we make rot_mag and trans_mag be different than for rigid body mover??
		// maybe it should also be an option?
		rna_denovo_master_mover_->setup_dock_into_density_mover( pose, rot_mag, trans_mag );
	}

	// set helix docking rotation and translation magnitudes
	if ( options_->dock_each_chunk_per_chain() && (options_->helical_substruct_res().size() > 0) ) {
		// max angle 10? max trans 2?
		Real const hel_rot_mag_init( 30.0 ), hel_rot_mag_final( 0.2 );
		Real const hel_trans_mag_init( 5.0 ), hel_trans_mag_final( 0.1 );
		//Real const hel_rot_mag_init( 10.0 ), hel_rot_mag_final( 0.2 );
		//Real const hel_trans_mag_init( 2.0 ), hel_trans_mag_final( 0.1 );
		Real helix_rot_mag, helix_trans_mag;
		get_rigid_body_move_mags( r, helix_rot_mag, helix_trans_mag,
			hel_rot_mag_init, hel_trans_mag_init,
			hel_rot_mag_final, hel_trans_mag_final );

		rna_denovo_master_mover_->set_helix_mover_magnitude( helix_rot_mag, helix_trans_mag );
	}

	// reset the magnitude of the RNP docking perturbation each round
	if ( is_rna_and_protein_ && options_->rna_protein_docking() && options_->rna_protein_docking_legacy() ) {
		rna_denovo_master_mover_->setup_rna_protein_docking_mover( pose, rot_mag, trans_mag );
	}

	rna_denovo_master_mover_->set_sfxn( working_denovo_scorefxn_ );
}

////////////////////////////////////////////////////////////////////////////////////////
/// @details Classic 3->2->1 RNA fragment size reduction, set by round r.
Size
RNA_FragmentMonteCarlo::update_frag_size( Size const & r ) const
{
	// user defined over-ride of fragment size.
	if ( options_->frag_size() > 0 )  return options_->frag_size();
	Size frag_size = 3;
	if ( r > 1.0 * ( rounds_ / 3.0 ) ) frag_size = 2;
	if ( r > 2.0 * ( rounds_ / 3.0 ) ) frag_size = 1;
	return frag_size;
}

////////////////////////////////////////////////////////////////////////////////////////
void
RNA_FragmentMonteCarlo::get_rigid_body_move_mags( Size const & r,
	Real & rot_mag,
	Real & trans_mag,
	Real const & rot_mag_init,
	Real const & trans_mag_init,
	Real const & rot_mag_final,
	Real const & trans_mag_final ) const
{

	Real suppress;
	if ( options_->small_docking_moves() ) {
		suppress = 1.0;
	} else {
		//Keep moves coarse for early rounds. For the last 1/4 of modeling, plateau to the finest moves.
		suppress = (1.0 - options_->docking_move_size()) + 3.0 / 4.0 * r / rounds_;
		//suppress = 3.0 / 4.0 * r / rounds_;
	}
	if ( suppress > 1.0 ) suppress = 1.0;

	// Real const rot_mag_init( 10.0 ),   rot_mag_final( 0.2 );
	// Real const trans_mag_init( 5.0 ), trans_mag_final( 0.1 );
	rot_mag   = rot_mag_init   +  (rot_mag_final - rot_mag_init ) * suppress;
	trans_mag = trans_mag_init +  (trans_mag_final - trans_mag_init ) * suppress;
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @details
// Totally ad hoc recipe for default # cycles, if not user-specified.
void
RNA_FragmentMonteCarlo::setup_monte_carlo_cycles( core::pose::Pose const & pose ){

	if ( options_->user_defined_cycles() || options_->monte_carlo_cycles() > 0 ) {
		monte_carlo_cycles_ = options_->monte_carlo_cycles();
		if ( options_->verbose() ) TR << "Using user-defined " << monte_carlo_cycles_ << " cycles in de novo modeling." << std::endl;
		return;
	}
	// figure out rough number of degrees of freedom.

	// first count up number of residues with atom_level_domain_map.
	Size const nres_move = get_moving_res( pose, atom_level_domain_map_ ).size();
	if ( options_->verbose() ) TR << "Number of moving residues: " << nres_move << std::endl;

	Size const nchunks = rna_chunk_library_->num_moving_chunks();
	if ( options_->verbose() ) TR << "Number of moving chunks: " << nchunks << std::endl;

	// then count up rigid bodies that need to be docked.
	Size nbody_move = get_rigid_body_jumps( pose ).size();
	if ( nbody_move > 1 ) nbody_move--; // first rigid body does not move, by convention.
	if ( nbody_move > 0 ) TR << "Number of moving bodies: " << nbody_move << std::endl;

	monte_carlo_cycles_ = 2000 * ( nres_move + nchunks ) + 20000 * nbody_move;

	if ( monte_carlo_cycles_ > monte_carlo_cycles_max_default_ ) {
		monte_carlo_cycles_ = monte_carlo_cycles_max_default_;
		if ( options_->verbose() ) TR << "Using maximum default Monte Carlo cycles: " <<  monte_carlo_cycles_ << ". Use -cycles option to change this." << std::endl;
	}

	if ( options_->verbose() ) TR << "Using " << monte_carlo_cycles_ << " cycles in de novo modeling." << std::endl;

}

////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// @details
///   Having this final_score function allows slow terms to not be applied during Monte Carlo but only
///    as an extra score at end.
void
RNA_FragmentMonteCarlo::final_score( core::pose::Pose & pose )
{
	core::scoring::ScoreFunctionOP final_scorefxn_plus_slow_terms = final_scorefxn_->clone();
	if ( core::scoring::rna::nonconst_rna_scoring_info_from_pose( pose ).rna_data_info().rna_reactivities().size() > 0 ) {
		final_scorefxn_plus_slow_terms->set_weight( core::scoring::rna_chem_map, 1.0 );
	}
	( *final_scorefxn_plus_slow_terms )( pose );
	if ( final_scorefxn_plus_slow_terms->has_nonzero_weight( core::scoring::rna_chem_map ) && options_->verbose() ) final_scorefxn_plus_slow_terms->show( pose );
}

////////////////////////////////////////////////////////////////////////////////////////
/// @details
///  Progressive upweighting of 'fine-scale' RNA modeling terms.
///  Stack geometry, chainbreak, and constraint terms are zero for first of 10 rna_denovo rounds,
///    then titrated up to final weight.
///  Analogous to abinitio protocol for proteins, where strand geometry terms are
///   turned on late.
void
RNA_FragmentMonteCarlo::update_denovo_scorefxn_weights( Size const r )
{
	using namespace core::scoring;

	Real const rna_base_axis_final_weight         = denovo_scorefxn_->get_weight( rna_base_axis );
	Real const rna_base_stagger_final_weight      = denovo_scorefxn_->get_weight( rna_base_stagger );
	Real const rna_base_stack_axis_final_weight   = denovo_scorefxn_->get_weight( rna_base_stack_axis );
	Real const linear_chainbreak_final_weight     = denovo_scorefxn_->get_weight( linear_chainbreak );
	Real const chainbreak_final_weight            = denovo_scorefxn_->get_weight( chainbreak );
	Real const atom_pair_constraint_final_weight  = denovo_scorefxn_->get_weight( atom_pair_constraint );
	Real const coordinate_constraint_final_weight = denovo_scorefxn_->get_weight( coordinate_constraint );
	Real const base_pair_constraint_final_weight  = denovo_scorefxn_->get_weight( base_pair_constraint );
	Real const rna_chem_map_lores_final_weight    = denovo_scorefxn_->get_weight( rna_chem_map_lores );
	Real const rnp_vdw_final_weight               = denovo_scorefxn_->get_weight( rnp_vdw );

	//Keep score function coarse for early rounds.
	// Real const suppress  = (r - 1.0) / (rounds - 1.0);
	Real const suppress  = static_cast<Real>( r ) / rounds_;

	working_denovo_scorefxn_->set_weight( rna_base_axis,      suppress*rna_base_axis_final_weight  );
	working_denovo_scorefxn_->set_weight( rna_base_stagger,   suppress*rna_base_stagger_final_weight  );
	if ( options_->titrate_stack_bonus() ) working_denovo_scorefxn_->set_weight( rna_base_stack_axis,suppress*rna_base_stack_axis_final_weight  );

	if ( options_->ramp_rnp_vdw() ) working_denovo_scorefxn_->set_weight( rnp_vdw, suppress*rnp_vdw_final_weight );

	if ( options_->gradual_constraints() ) {
		working_denovo_scorefxn_->set_weight( atom_pair_constraint,  suppress*atom_pair_constraint_final_weight  );
		working_denovo_scorefxn_->set_weight( coordinate_constraint,  suppress*coordinate_constraint_final_weight  );
	}
	working_denovo_scorefxn_->set_weight( base_pair_constraint, suppress*base_pair_constraint_final_weight );
	working_denovo_scorefxn_->set_weight( rna_chem_map_lores,   suppress*rna_chem_map_lores_final_weight  );

	// keep chainbreak extra low for early rounds... seems to be important for rigid body modeler.
	//Real suppress_chainbreak  = ( r - ( rounds_/3.0 ) )/ ( static_cast<Real>( rounds_ ) - ( rounds_ / 3.0 ) );
	//Real const suppress_chainbreak_min = 1 / static_cast< Real >( rounds_ );
	//if ( suppress_chainbreak < suppress_chainbreak_min ) suppress_chainbreak = suppress_chainbreak_min;

	working_denovo_scorefxn_->set_weight( linear_chainbreak,  suppress * linear_chainbreak_final_weight  );
	working_denovo_scorefxn_->set_weight( chainbreak,  suppress * chainbreak_final_weight  );

}

////////////////////////////////////////////////////////////////////////////////////////
/// @details Used in staged_constraints only.
Size
RNA_FragmentMonteCarlo::figure_out_constraint_separation_cutoff( Size const r, Size const max_dist )
{
	//Keep score function coarse for early rounds.
	Real const suppress  = 5.0 / 3.0 * r / rounds_;

	Size const separation_cutoff ( static_cast<Size>( suppress * max_dist ) + 2 );
	if ( separation_cutoff > max_dist ) return max_dist;

	return separation_cutoff;
}

////////////////////////////////////////////////////////////////////////////////////////
/// @details
///   Only does something if -staged_constraints flag is on.
///   Progressively introduces constraints, with shortest range first (as determined by
///    distance in fold-tree).
///   Inspired by classic fold_constraints protocol for proteins.
void
RNA_FragmentMonteCarlo::update_pose_constraints( Size const r, core::pose::Pose & pose )
{
	using namespace core::scoring::constraints;

	if ( !options_->staged_constraints() ) return;

	if ( !constraint_set_ ) return;

	static core::kinematics::ShortestPathInFoldTree shortest_path_in_fold_tree( pose.fold_tree() );
	Size const separation_cutoff = figure_out_constraint_separation_cutoff( r, shortest_path_in_fold_tree.max_dist() );
	TR << "Round " << r << " out of " << rounds_ << std::endl;
	TR << "Fold tree current separation cutoff " << separation_cutoff << " out of " << shortest_path_in_fold_tree.max_dist() << std::endl;
	// Fang: apparently separation_cutoff can be smaller than shortest_path_in_fold_tree.dist( i , j )
	// Not sure why but do an early exit hack here
	if ( separation_cutoff >= shortest_path_in_fold_tree.max_dist() ) {
		pose.constraint_set( constraint_set_ );
	} else {
		ConstraintCOPs csts( constraint_set_->get_all_constraints() );
		ConstraintSetOP cst_set_new( new core::scoring::constraints::ConstraintSet );
		for ( ConstraintCOP const & cst : csts ) {
			if ( cst->natoms() == 2 )  { // currently only defined for pairwise distance constraints.
				Size const i = cst->atom( 1 ).rsd();
				Size const j = cst->atom( 2 ).rsd();
				Size const dist( shortest_path_in_fold_tree.dist( i , j ) );
				if ( dist  > separation_cutoff ) continue;
			}
			cst_set_new->add_constraint( cst );
		}
		pose.constraint_set( cst_set_new );
	}

	TR << "Num constraints " << pose.constraint_set()->get_all_constraints().size() << " out of " <<
		constraint_set_->get_all_constraints().size() << std::endl;

}

///////////////////////////////////////////////////////////////////////////////////////////////
bool
RNA_FragmentMonteCarlo::check_score_filter( Real const lores_score, std::list< Real > & all_lores_score ){

	all_lores_score.push_back( lores_score );

	all_lores_score.sort(); // nice -- can do this with a list!

	// note that if options_->autofilter_score_quantile_ = 0.20, the first decoy will be 'passed' for free.
	Size const n = all_lores_score.size();
	Size const cutoff_index = static_cast< Size >( n * options_->autofilter_score_quantile() ) + 1;

	// the one pain with lists -- need to iterate through to find the element corresponding to the quantile score.
	Real all_lores_score_cutoff = all_lores_score.front();
	Size i( 1 );
	for ( auto iter = all_lores_score.begin(), end = all_lores_score.end(); iter != end; ++iter, i++ ) {
		if ( i == cutoff_index ) all_lores_score_cutoff = *iter;
	}

	TR << "Comparing current lores score " << lores_score << " to automatically determined cutoff: " << all_lores_score_cutoff << " based on " << options_->autofilter_score_quantile() << " quantile from "  << n << " models so far" << std::endl;
	return ( lores_score <= all_lores_score_cutoff );
}

///////////////////////////////////////////////////////////////////////////////////////////////
void
RNA_FragmentMonteCarlo::apply_chem_shift_data( core::pose::Pose & pose ){

	using namespace core::scoring;

	runtime_assert( options_->use_chem_shift_data() );

	if ( options_->minimize_structure() ) {
		rna_minimizer_->set_score_function( chem_shift_scorefxn_ ); //use the chem_shift_scorefxn_
		rna_minimizer_->apply( pose );
		rna_minimizer_->set_score_function( hires_scorefxn_ ); //set back the original scorefxn.
		if ( options_->close_loops() ) rna_loop_closer_->apply( pose, rna_base_pair_handler_->connections() );
		final_scorefxn_ = chem_shift_scorefxn_;
	}

	(*chem_shift_scorefxn_)( pose );
}

//////////////////////////////////////////////////////////////////////////////////
void
RNA_FragmentMonteCarlo::align_pose( core::pose::Pose & pose, bool const verbose ) const
{
	bool loop_modeling_into_single_structure( false );
	if ( !options_->superimpose_over_all() ) {
		// if input pdbs were specified with -s or -silent, then automatic alignment to first of these input chunks.
		// otherwise, align to native pose, if specified.
		loop_modeling_into_single_structure = rna_chunk_library_->superimpose_to_single_user_input_chunk( pose );
	}

	if ( !loop_modeling_into_single_structure && get_native_pose() ) {

		Pose const & native_pose = *get_native_pose();

		//realign to native for ease of viewing.
		vector1< Size > superimpose_res;
		for ( Size n = 1; n <= pose.size(); n++ )  superimpose_res.push_back( n );

		id::AtomID_Map< id::AtomID > const & alignment_atom_id_map_native =
			protocols::stepwise::modeler::align::create_alignment_id_map_legacy( pose, native_pose, superimpose_res ); // perhaps this should move to toolbox.

		if ( verbose ) TR << "Aligning pose to native." << std::endl;

		core::scoring::superimpose_pose( pose, native_pose, alignment_atom_id_map_native );
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
// returns moving_res_list
// Andy: it would be better for this to SHARE CODE with stepwise/modeler/util.cc if possible.
// Andy: it would be better to remove any dependence of RNA_FragmentMonteCarlo on
//   full_model_info if possible.
//  -- rhiju
vector1< Size >
RNA_FragmentMonteCarlo::reroot_pose_before_align_and_return_moving_res( pose::Pose & pose ) const
{
	// find connection point to 'fixed res'
	Size moving_res_at_connection( 0 ), reference_res( 0 );
	vector1< Size > moving_res_list = get_moving_res( pose, atom_level_domain_map_ );
	for ( Size const moving_res : moving_res_list ) {
		Size const parent_res = pose.fold_tree().get_parent_residue( moving_res );
		if ( moving_res_list.has_value( parent_res ) ) continue;
		moving_res_at_connection = moving_res;
		reference_res            = parent_res;
		break;
	}

	bool const is_jump = pose.fold_tree().jump_nr( reference_res, moving_res_at_connection ) > 0 ;
	bool switched_moving_and_root_partitions = stepwise::modeler::revise_root_and_moving_res( pose, moving_res_at_connection );

	// revise_root_and_moving_res() handled revision of single residue only... need to translate this
	// switch to the whole list of residues.
	if ( switched_moving_and_root_partitions ) {
		vector1< Size > const moving_res_list_original = moving_res_list;
		moving_res_list = utility::tools::make_vector1( moving_res_at_connection );
		if ( ! is_jump ) {
			for ( Size const moving_res : moving_res_list_original ) {
				if ( moving_res_list_original.has_value( pose.fold_tree().get_parent_residue( moving_res ) ) ) moving_res_list.push_back( moving_res );
			}
		}
	}
	return moving_res_list;
}

//////////////////////////////////////////////////////////////////////////////////
core::Real
RNA_FragmentMonteCarlo::get_rmsd( core::pose::Pose const & const_pose ) const
{
	Pose pose = const_pose;
	align_pose( pose );
	return get_rmsd_no_superimpose( pose );
}

//////////////////////////////////////////////////////////////////////////////////
core::Real
RNA_FragmentMonteCarlo::get_rmsd_no_superimpose( core::pose::Pose const & pose ) const
{
	using namespace core::scoring;

	runtime_assert( get_native_pose() != nullptr );
	std::map< core::id::AtomID, core::id::AtomID > atom_id_map;
	setup_matching_heavy_atoms( *get_native_pose(), pose, atom_id_map ); // no virtuals, no hydrogens.
	check_for_loop_modeling_case( atom_id_map );

	return rms_at_corresponding_atoms_no_super( *get_native_pose(), pose, atom_id_map );
}

//////////////////////////////////////////////////////////////////////////////////
core::Real
RNA_FragmentMonteCarlo::get_rmsd_stems_no_superimpose ( core::pose::Pose const & pose ) const {
	using namespace core::scoring;

	runtime_assert( get_native_pose() != nullptr );
	vector1< Size > stem_residues( rna_base_pair_handler()->get_stem_residues( pose ) );
	if ( stem_residues.empty() ) return 0.0;

	std::map< core::id::AtomID, core::id::AtomID > atom_id_map;
	setup_matching_heavy_atoms( *get_native_pose(), pose, atom_id_map ); // no virtuals, no hydrogens.
	return rms_at_corresponding_atoms_no_super( *get_native_pose(), pose, atom_id_map, stem_residues );
}

//////////////////////////////////////////////////////////////////////
bool
RNA_FragmentMonteCarlo::loop_modeling() const {
	return ( rna_chunk_library_->single_user_input_chunk() );
}

//////////////////////////////////////////////////////////////////////
void
RNA_FragmentMonteCarlo::check_for_loop_modeling_case( std::map< core::id::AtomID, core::id::AtomID > & atom_id_map ) const
{
	// special case -- we only care about the loop(s). Pose has already been aligned to fixed residues.
	// this will be decided in align_and_output_to_silent_file.
	if ( ! loop_modeling() ) return;

	std::map< core::id::AtomID, core::id::AtomID > loop_atom_id_map;
	TR << "In loop modeling mode, since there is a single user-inputted pose" << std::endl;
	for ( auto const & elem : atom_id_map ) {
		Size domain( rna_chunk_library_->atom_level_domain_map()->get_domain( elem.second ) );
		if ( domain == 0 || domain == ROSETTA_LIBRARY_DOMAIN ) {
			loop_atom_id_map[ elem.first ] = elem.second;
			// TR << TR.Cyan << "Loop atom: " << atom_id_to_named_atom_id( elem.second, pose ) << TR.Reset << std::endl;
		}
	}
	atom_id_map = loop_atom_id_map;
}

//////////////////////////////////////////////////////////////////////////////////
void
RNA_FragmentMonteCarlo::setup_full_initial_structure( core::pose::Pose & pose ) const
{

	rna_de_novo_pose_initializer_->setup_fold_tree_and_jumps_and_variants( pose, *(rna_denovo_master_mover_init_->rna_jump_mover()), rna_chunk_initialization_library_->atom_level_domain_map(), *rna_chunk_initialization_library_, true /*enumerate for new fold tree initializer*/ );

	Size tries = 1;
	core::Real score = 100.;
	Pose start_pose = pose;
	while ( score > 0.5 && tries < 4 ) {
		pose = start_pose;
		score = randomize_and_close_all_chains( pose );
		tries += 1;
	}

}

//////////////////////////////////////////////////////////////////////////////////
core::Real
RNA_FragmentMonteCarlo::randomize_and_close_all_chains( core::pose::Pose & pose ) const
{

	/////////////////
	// do random moves and close chains //

	// randomize pose - we won't do this again later
	if ( is_rna_and_protein_ ) {
		rna_denovo_master_mover_init_->do_random_moves( pose, monte_carlo_cycles_, true );
	} else {
		rna_denovo_master_mover_init_->do_random_moves( pose, monte_carlo_cycles_ );
	}

	protocols::moves::MonteCarlo mc( pose, *chainbreak_sfxn_, options_->temperature() );
	mc.score_function( *chainbreak_sfxn_ );

	core::Real score = 100.0;
	Size i = 0;
	while ( score > 1.0 && i < 200 ) {
		++i;
		rna_denovo_master_mover_init_->rna_fragment_mover()->random_fragment_insertion( pose, 1 /*frag_size*/ );
		mc.boltzmann( pose );
		if ( mc.last_accepted_score() < 1.0 ) {
			rna_loop_closer_init_->apply( pose );
		}
		score = mc.last_accepted_score();
	}
	pose = mc.lowest_score_pose();
	rna_loop_closer_init_->apply( pose );

	options::RNA_FragmentMonteCarloOptionsOP options_chainbreak = options_->clone();
	options_chainbreak->set_dock_into_density( false );
	options_chainbreak->set_minimize_rounds( 1 );
	options_chainbreak->set_skip_o2prime_trials( true );
	options_chainbreak->set_protein_packing( false );

	RNA_MinimizerOP chainbreak_rna_min_ = utility::pointer::make_shared< RNA_Minimizer >( options_chainbreak );
	chainbreak_rna_min_->set_score_function( chainbreak_sfxn_ );
	chainbreak_rna_min_->set_atom_level_domain_map( rna_chunk_initialization_library_->atom_level_domain_map() );
	chainbreak_rna_min_->set_skip_chi_min( true );
	chainbreak_rna_min_->set_min_tol( 0.0001 ); // to keep it fast
	chainbreak_rna_min_->apply( pose );

	return (*chainbreak_sfxn_)( pose );

}

//////////////////////////////////////////////////////////////////////////////////
void
RNA_FragmentMonteCarlo::copy_structure_keep_fold_tree( core::pose::Pose & pose,
	core::pose::Pose const & pose_to_copy ) const
{

	// keep the fold tree from the pose
	kinematics::FoldTree save_ft = pose.fold_tree();

	// but get everything else from the pose_to_copy
	pose = pose_to_copy;
	pose.fold_tree( save_ft );

	// and then set up the chainbreak variants properly
	rna_de_novo_pose_initializer_->setup_chainbreak_variants( pose, atom_level_domain_map_ );

}

//////////////////////////////////////////////////////////////////////////////////
void
RNA_FragmentMonteCarlo::show(std::ostream & output) const
{
	Mover::show(output);
	output <<  "\nRounds:                        " << rounds_ <<
		"\nMonte Carlo cycles:            " << monte_carlo_cycles_ <<
		"\nMC cycle max default:          " << monte_carlo_cycles_max_default_ <<
		"\nLores score early:             " << (lores_score_early_) <<
		"\nLores score final:             " << (lores_score_final_);

}

} //denovo
} //rna
} //protocols
