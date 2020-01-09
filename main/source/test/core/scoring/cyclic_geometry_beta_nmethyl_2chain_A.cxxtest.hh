// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.
/// @file test/core/scoring/cyclic_geometry_beta_nmethyl_2chain_A.cxxtest.hh
/// @brief Unit tests for cyclic peptide pose scoring with 2 chains in the peptide and the beta score function, with N-methylation.
/// @detials Cyclic permutations should score identically.  This test is split into an A suite and a B suite due to its runtime.
/// @author Vikram K. Mulligan (vmullig@uw.edu)

// Test headers
#include <cxxtest/TestSuite.h>
#include <test/core/init_util.hh>
#include <test/UTracer.hh>
#include <test/util/symmetry_funcs.hh>

// Unit headers
#include <core/scoring/ScoringManager.hh>
#include <core/scoring/ScoreFunction.fwd.hh>
#include <core/scoring/ScoreFunction.hh>
#include <core/scoring/ScoreFunctionFactory.hh>
#include <core/scoring/Energies.hh>
#include <core/chemical/ResidueType.hh>
#include <core/conformation/ResidueFactory.hh>
#include <core/chemical/ResidueTypeSet.hh>
#include <core/conformation/util.hh>

// Core headers
#include <core/types.hh>
#include <core/pose/Pose.hh>
#include <core/conformation/Residue.hh>
#include <core/chemical/VariantType.hh>
#include <core/import_pose/import_pose.hh>
#include <core/pose/annotated_sequence.hh>
#include <core/pose/variant_util.hh>
#include <core/chemical/AA.hh>

//Protocols headers
#include <protocols/simple_moves/MutateResidue.hh>

#include <test/core/scoring/cyclic_geometry_headers.hh>

using namespace std;

using core::Size;
using core::Real;
using core::pose::Pose;
using core::chemical::AA;


static basic::Tracer TR("core.scoring.CyclicGeometry_beta_Tests_A.cxxtest");

class CyclicGeometry_nmethyl_beta_TwoChainTests_A : public CxxTest::TestSuite {

public:

	void setUp() {
		core_init_with_additional_options( "-symmetric_gly_tables true -write_all_connect_info -connect_info_cutoff 0.0 -beta -score:weights beta.wts" );

		// Pull in the two-chain cyclic peptide pose (12 + 12 = 24 residues)
		core::pose::PoseOP initial_pose_2chain( new core::pose::Pose );
		core::import_pose::pose_from_file( *initial_pose_2chain, "core/scoring/twochain_cycpep.pdb", core::import_pose::PDB_file );

		// Strip off termini and connect the ends:
		core::pose::remove_variant_type_from_pose_residue( *initial_pose_2chain, core::chemical::LOWER_TERMINUS_VARIANT, 1 );
		core::pose::remove_variant_type_from_pose_residue( *initial_pose_2chain, core::chemical::CUTPOINT_LOWER, 1 );
		core::pose::remove_variant_type_from_pose_residue( *initial_pose_2chain, core::chemical::CUTPOINT_UPPER, 1 );
		core::pose::remove_variant_type_from_pose_residue( *initial_pose_2chain, core::chemical::UPPER_TERMINUS_VARIANT, 12 );
		core::pose::remove_variant_type_from_pose_residue( *initial_pose_2chain, core::chemical::CUTPOINT_LOWER, 12 );
		core::pose::remove_variant_type_from_pose_residue( *initial_pose_2chain, core::chemical::CUTPOINT_UPPER, 12 );
		core::pose::remove_variant_type_from_pose_residue( *initial_pose_2chain, core::chemical::LOWER_TERMINUS_VARIANT, 13 );
		core::pose::remove_variant_type_from_pose_residue( *initial_pose_2chain, core::chemical::CUTPOINT_LOWER, 13 );
		core::pose::remove_variant_type_from_pose_residue( *initial_pose_2chain, core::chemical::CUTPOINT_UPPER, 13 );
		core::pose::remove_variant_type_from_pose_residue( *initial_pose_2chain, core::chemical::UPPER_TERMINUS_VARIANT, 24 );
		core::pose::remove_variant_type_from_pose_residue( *initial_pose_2chain, core::chemical::CUTPOINT_LOWER, 24 );
		core::pose::remove_variant_type_from_pose_residue( *initial_pose_2chain, core::chemical::CUTPOINT_UPPER, 24 );
		initial_pose_2chain->conformation().declare_chemical_bond(1, "N", 24, "C");
		initial_pose_2chain->conformation().declare_chemical_bond(13, "N", 12, "C");

		remove_disulfides(initial_pose_2chain);
		form_disulfides(initial_pose_2chain);
		for ( core::Size ir=1, irmax=initial_pose_2chain->size(); ir<=irmax; ++ir ) {
			initial_pose_2chain->conformation().rebuild_polymer_bond_dependent_atoms_this_residue_only(ir);
		}

		// Add N-methylation:
		protocols::simple_moves::MutateResidueOP mutres3( new protocols::simple_moves::MutateResidue( 3, "TRP:N_Methylation" ) );
		mutres3->set_update_polymer_dependent( true );
		mutres3->apply(*initial_pose_2chain);
		protocols::simple_moves::MutateResidueOP mutres17( new protocols::simple_moves::MutateResidue( 17, "DTRP:N_Methylation" ) );
		mutres17->set_update_polymer_dependent( true );
		mutres17->apply(*initial_pose_2chain);

		initial_pose_2chain->set_chi( 1, 3, 68.0 );
		initial_pose_2chain->set_chi( 2, 3, 62.0 );
		initial_pose_2chain->set_chi( 3, 3, 72.0 );

		initial_pose_2chain->set_chi( 1, 17, -71.0 );
		initial_pose_2chain->set_chi( 2, 17, -63.0 );
		initial_pose_2chain->set_chi( 3, 17, -74.0 );

		initial_pose_2chain->update_residue_neighbors();

		poses_2chain_.clear();
		mirror_poses_2chain_.clear();

		poses_2chain_.push_back(initial_pose_2chain);
		mirror_poses_2chain_.push_back( mirror_pose_with_disulfides( poses_2chain_[1] ) );
		for ( core::Size i=1; i<=23; ++i ) {
			poses_2chain_.push_back( permute_with_disulfides( poses_2chain_[i] ) );
			mirror_poses_2chain_.push_back( mirror_pose_with_disulfides( poses_2chain_[i+1] ) );

			poses_2chain_[i+1]->conformation().rebuild_polymer_bond_dependent_atoms_this_residue_only(1);
			poses_2chain_[i+1]->conformation().rebuild_polymer_bond_dependent_atoms_this_residue_only(13);
			poses_2chain_[i+1]->conformation().rebuild_polymer_bond_dependent_atoms_this_residue_only(12);
			poses_2chain_[i+1]->conformation().rebuild_polymer_bond_dependent_atoms_this_residue_only(24);
			mirror_poses_2chain_[i+1]->conformation().rebuild_polymer_bond_dependent_atoms_this_residue_only(1);
			mirror_poses_2chain_[i+1]->conformation().rebuild_polymer_bond_dependent_atoms_this_residue_only(13);
			mirror_poses_2chain_[i+1]->conformation().rebuild_polymer_bond_dependent_atoms_this_residue_only(12);
			mirror_poses_2chain_[i+1]->conformation().rebuild_polymer_bond_dependent_atoms_this_residue_only(24);

			for ( core::Size j(1), jmax(mirror_poses_2chain_[i+1]->total_residue()); j<=jmax; ++j ) {
				for ( core::Size ichi(1), ichimax(mirror_poses_2chain_[i+1]->residue(j).type().nchi()); ichi<=ichimax; ++ichi ) {
					poses_2chain_[i+1]->set_chi(ichi, j, initial_pose_2chain->chi( ichi, (j + i > jmax ? j + i - jmax: j + i) ) );
					poses_2chain_[i+1]->update_residue_neighbors();
					mirror_poses_2chain_[i+1]->set_chi(ichi, j, -1.0*poses_2chain_[i+1]->chi( ichi, j ) );
					mirror_poses_2chain_[i+1]->update_residue_neighbors();
				}
			}
		}

	}

	void tearDown() {
	}

	/// @brief Tests cyclic permutation scoring with the fa_rep scorefunction.
	/// @author Vikram K. Mulligan (vmullig@uw.edu)
	void test_cyclic_permutation_fa_rep() {
		//Set up the scorefunction
		core::scoring::ScoreFunctionOP scorefxn( new core::scoring::ScoreFunction );
		scorefxn->set_weight( core::scoring::fa_rep, 1.0 );
		TR << "Testing fa_rep score term." << std::endl;
		CyclicGeometryTestHelper helper;
		helper.cyclic_pose_test(scorefxn, poses_2chain_, mirror_poses_2chain_);
		return;
	}

	/// @brief Tests cyclic permutation scoring with the fa_intra_rep scorefunction.
	/// @author Vikram K. Mulligan (vmullig@uw.edu)
	void test_cyclic_permutation_fa_intra_rep() {
		//Set up the scorefunction
		core::scoring::ScoreFunctionOP scorefxn( new core::scoring::ScoreFunction );
		scorefxn->set_weight( core::scoring::fa_intra_rep, 1.0 );
		TR << "Testing fa_intra_rep score term." << std::endl;
		CyclicGeometryTestHelper helper;
		helper.cyclic_pose_test(scorefxn, poses_2chain_, mirror_poses_2chain_);
		return;
	}

	/// @brief Tests cyclic permutation scoring with the fa_intra_rep_xover4 scorefunction.
	/// @author Vikram K. Mulligan (vmullig@uw.edu)
	void test_cyclic_permutation_fa_intra_rep_xover4() {
		//Set up the scorefunction
		core::scoring::ScoreFunctionOP scorefxn( new core::scoring::ScoreFunction );
		scorefxn->set_weight( core::scoring::fa_intra_rep_xover4, 1.0 );
		scorefxn->set_weight( core::scoring::fa_sol, 1.0 );
		TR << "Testing fa_intra_rep_xover4 score term." << std::endl;
		CyclicGeometryTestHelper helper;
		helper.cyclic_pose_test(scorefxn, poses_2chain_, mirror_poses_2chain_);
		return;
	}

	/// @brief Tests cyclic permutation scoring with the fa_intra_atr_xover4 scorefunction.
	/// @author Vikram K. Mulligan (vmullig@uw.edu)
	void test_cyclic_permutation_fa_intra_atr_xover4() {
		//Set up the scorefunction
		core::scoring::ScoreFunctionOP scorefxn( new core::scoring::ScoreFunction );
		scorefxn->set_weight( core::scoring::fa_intra_atr_xover4, 1.0 );
		scorefxn->set_weight( core::scoring::fa_sol, 1.0 );
		TR << "Testing fa_intra_atr_xover4 score term." << std::endl;
		CyclicGeometryTestHelper helper;
		helper.cyclic_pose_test(scorefxn, poses_2chain_, mirror_poses_2chain_);
		return;
	}

	/// @brief Tests cyclic permutation scoring with the lk_ball_bridge scorefunction.
	/// @author Vikram K. Mulligan (vmullig@uw.edu)
	void test_cyclic_permutation_lk_ball_bridge() {
		//Set up the scorefunction
		core::scoring::ScoreFunctionOP scorefxn( new core::scoring::ScoreFunction );
		scorefxn->set_weight( core::scoring::lk_ball_bridge, 1.0 );
		scorefxn->set_weight( core::scoring::fa_sol, 1.0 );
		TR << "Testing lk_ball_bridge score term." << std::endl;
		CyclicGeometryTestHelper helper;
		helper.cyclic_pose_test(scorefxn, poses_2chain_, mirror_poses_2chain_);
		return;
	}

	/// @brief Tests cyclic permutation scoring with the lk_ball_bridge_uncpl scorefunction.
	/// @author Vikram K. Mulligan (vmullig@uw.edu)
	void test_cyclic_permutation_lk_ball_bridge_uncpl() {
		//Set up the scorefunction
		core::scoring::ScoreFunctionOP scorefxn( new core::scoring::ScoreFunction );
		scorefxn->set_weight( core::scoring::lk_ball_bridge_uncpl, 1.0 );
		scorefxn->set_weight( core::scoring::fa_sol, 1.0 );
		TR << "Testing lk_ball_bridge_uncpl score term." << std::endl;
		CyclicGeometryTestHelper helper;
		helper.cyclic_pose_test(scorefxn, poses_2chain_, mirror_poses_2chain_);
		return;
	}

	/// @brief Tests cyclic permutation scoring with the pro_close scorefunction.
	/// @author Vikram K. Mulligan (vmullig@uw.edu)
	void test_cyclic_permutation_pro_close() {
		//Set up the scorefunction
		core::scoring::ScoreFunctionOP scorefxn( new core::scoring::ScoreFunction );
		scorefxn->set_weight( core::scoring::pro_close, 1.0 );
		TR << "Testing pro_close score term." << std::endl;
		CyclicGeometryTestHelper helper;
		helper.cyclic_pose_test(scorefxn, poses_2chain_, mirror_poses_2chain_);
		return;
	}

	/// @brief Tests cyclic permutation scoring with the dslf_fa13 scorefunction.
	/// @author Vikram K. Mulligan (vmullig@uw.edu)
	void test_cyclic_permutation_dslf_fa13() {
		//Set up the scorefunction
		core::scoring::ScoreFunctionOP scorefxn( new core::scoring::ScoreFunction );
		scorefxn->set_weight( core::scoring::dslf_fa13, 1.0 );
		TR << "Testing dslf_fa13 score term." << std::endl;
		CyclicGeometryTestHelper helper;
		helper.cyclic_pose_test(scorefxn, poses_2chain_, mirror_poses_2chain_);
		return;
	}

	/// @brief Tests cyclic permutation scoring with the fa_dun_dev scorefunction.
	/// @author Vikram K. Mulligan (vmullig@uw.edu)
	void test_cyclic_permutation_fa_dun_dev() {
		//Set up the scorefunction
		core::scoring::ScoreFunctionOP scorefxn( new core::scoring::ScoreFunction );
		scorefxn->set_weight( core::scoring::fa_dun_dev, 1.0 );
		TR << "Testing fa_dun_dev score term." << std::endl;
		CyclicGeometryTestHelper helper;
		helper.cyclic_pose_test(scorefxn, poses_2chain_, mirror_poses_2chain_);
		return;
	}

	/// @brief Tests cyclic permutation scoring with the fa_dun_semi scorefunction.
	/// @author Vikram K. Mulligan (vmullig@uw.edu)
	void test_cyclic_permutation_fa_dun_semi() {
		//Set up the scorefunction
		core::scoring::ScoreFunctionOP scorefxn( new core::scoring::ScoreFunction );
		scorefxn->set_weight( core::scoring::fa_dun_semi, 1.0 );
		TR << "Testing fa_dun_semi score term." << std::endl;
		CyclicGeometryTestHelper helper;
		helper.cyclic_pose_test(scorefxn, poses_2chain_, mirror_poses_2chain_);
		return;
	}

	/// @brief Tests cyclic permutation scoring with the rama_prepro scorefunction.
	/// @author Vikram K. Mulligan (vmullig@uw.edu)
	void test_cyclic_permutation_rama_prepro() {
		//Set up the scorefunction
		core::scoring::ScoreFunctionOP scorefxn( new core::scoring::ScoreFunction );
		scorefxn->set_weight( core::scoring::rama_prepro, 1.0 );
		TR << "Testing rama_prepro score term." << std::endl;
		CyclicGeometryTestHelper helper;
		helper.cyclic_pose_test(scorefxn, poses_2chain_, mirror_poses_2chain_);
		return;
	}

	/// @brief Tests cyclic permutation scoring with the p_aa_pp scorefunction.
	/// @author Vikram K. Mulligan (vmullig@uw.edu)
	void test_cyclic_permutation_p_aa_pp() {
		//Set up the scorefunction
		core::scoring::ScoreFunctionOP scorefxn( new core::scoring::ScoreFunction );
		scorefxn->set_weight( core::scoring::p_aa_pp, 1.0 );
		TR << "Testing p_aa_pp score term." << std::endl;
		CyclicGeometryTestHelper helper;
		helper.cyclic_pose_test(scorefxn, poses_2chain_, mirror_poses_2chain_);
		return;
	}


	/// @brief Tests cyclic permutation scoring with the full scorefunction that's the current default (whatever that is).
	/// @author Vikram K. Mulligan (vmullig@uw.edu)
	void test_cyclic_permutation_current_default_scorefxn() {
		//Set up the scorefunction
		core::scoring::ScoreFunctionOP scorefxn( core::scoring::get_score_function() );
		TR << "Testing full default score function." << std::endl;
		CyclicGeometryTestHelper helper;
		helper.cyclic_pose_test(scorefxn, poses_2chain_, mirror_poses_2chain_);
		return;
	}

private:
	utility::vector1 < core::pose::PoseOP > poses_2chain_;
	utility::vector1 < core::pose::PoseOP > mirror_poses_2chain_;


};
