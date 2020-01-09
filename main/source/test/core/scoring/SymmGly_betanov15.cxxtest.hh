// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.
/// @file test/core/scoring/SymmGly.cxxtest.hh
/// @brief Unit tests for glycine scoring with the --symmetric_gly_tables option.
/// @detials Left- and right-handed conformations of glycine should score identically with this option.
/// @author Vikram K. Mulligan (vmullig@uw.edu)

// Test headers
#include <cxxtest/TestSuite.h>
#include <test/core/init_util.hh>
#include <test/UTracer.hh>

// Unit headers
#include <core/scoring/ScoringManager.hh>
#include <core/scoring/ScoreFunction.fwd.hh>
#include <core/scoring/ScoreFunction.hh>
#include <core/scoring/ScoreFunctionFactory.hh>
#include <core/scoring/Energies.hh>

// Core headers
#include <core/types.hh>
#include <core/pose/Pose.hh>
#include <core/import_pose/import_pose.hh>
#include <core/pose/annotated_sequence.hh>
#include <core/chemical/AA.hh>

//Minimizer
#include <core/optimization/AtomTreeMinimizer.hh>
#include <core/optimization/MinimizerOptions.hh>
#include <core/optimization/MinimizerOptions.fwd.hh>
#include <core/kinematics/MoveMap.fwd.hh>
#include <core/kinematics/MoveMap.hh>

using namespace std;

using core::Size;
using core::Real;
using core::pose::Pose;
using core::chemical::AA;


static basic::Tracer TR("core.scoring.SymmGly_betanov15_Tests.cxxtest");

class SymmGly_betanov15_Tests : public CxxTest::TestSuite {

public:

	void setUp() {
		//core_init();
		core_init_with_additional_options( "-score:weights beta_nov15.wts -symmetric_gly_tables true" );
	}

	void tearDown() {
	}

	/// @brief Construct repeat sequences with poly-glycine, and confirm that mirror-image conformations
	/// score identically with a given scorefunction.
	void repeat_structure_test( core::scoring::ScoreFunctionOP sfxn ) {
		core::pose::PoseOP pose( new core::pose::Pose() );
		core::pose::make_pose_from_sequence(*pose, "GGGGGGGG", "fa_standard", false);
		core::pose::PoseOP pose2( pose->clone() );
		for ( int iphi=-180; iphi<180; iphi+=10 ) {
			for ( int ipsi=-180; ipsi<=0; ipsi+=10 ) {
				for ( core::Size ir=1; ir<=8; ++ir ) {
					pose->set_omega(ir, 180.0);
					pose->set_phi(ir, static_cast<core::Real>(iphi));
					pose->set_psi(ir, static_cast<core::Real>(ipsi));
					pose2->set_omega(ir, 180.0);
					pose2->set_phi(ir, -1.0*static_cast<core::Real>(iphi));
					pose2->set_psi(ir, -1.0*static_cast<core::Real>(ipsi));
					(*sfxn)(*pose);
					(*sfxn)(*pose2);
					//TR << "phi=" << iphi << " psi=" << ipsi << " E1=" << pose->energies().total_energy() << " E2=" << pose2->energies().total_energy() << std::endl;
					TS_ASSERT_DELTA(pose->energies().total_energy(), pose2->energies().total_energy(), std::max( std::abs( std::max(pose->energies().total_energy(), pose2->energies().total_energy())/1000.0 ), 1e-5 ) );
				}
			}
		}
	}

	/// @brief Tests symmetric scoring of glycine with the cart_bonded scorefunction.
	/// @author Vikram K. Mulligan (vmullig@uw.edu)
	void test_symm_gly_cart_bonded() {
		//Set up the scorefunction
		core::scoring::ScoreFunctionOP scorefxn( new core::scoring::ScoreFunction );
		scorefxn->set_weight( core::scoring::cart_bonded, 1.0 );
		TR << "Testing cart_bonded score term." << std::endl;
		repeat_structure_test(scorefxn);
		return;
	}

	/// @brief Tests symmetric scoring of glycine with the fa_atr scorefunction.
	/// @author Vikram K. Mulligan (vmullig@uw.edu)
	void test_symm_gly_fa_atr() {
		//Set up the scorefunction
		core::scoring::ScoreFunctionOP scorefxn( new core::scoring::ScoreFunction );
		scorefxn->set_weight( core::scoring::fa_atr, 1.0 );
		TR << "Testing fa_atr score term." << std::endl;
		repeat_structure_test(scorefxn);
		return;
	}

	/// @brief Tests symmetric scoring of glycine with the fa_rep scorefunction.
	/// @author Vikram K. Mulligan (vmullig@uw.edu)
	void test_symm_gly_fa_rep() {
		//Set up the scorefunction
		core::scoring::ScoreFunctionOP scorefxn( new core::scoring::ScoreFunction );
		scorefxn->set_weight( core::scoring::fa_rep, 1.0 );
		TR << "Testing fa_rep score term." << std::endl;
		repeat_structure_test(scorefxn);
		return;
	}

	/// @brief Tests symmetric scoring of glycine with the fa_intra_rep scorefunction.
	/// @author Vikram K. Mulligan (vmullig@uw.edu)
	void test_symm_gly_fa_intra_rep() {
		//Set up the scorefunction
		core::scoring::ScoreFunctionOP scorefxn( new core::scoring::ScoreFunction );
		scorefxn->set_weight( core::scoring::fa_intra_rep, 1.0 );
		TR << "Testing fa_intra_rep score term." << std::endl;
		repeat_structure_test(scorefxn);
		return;
	}

	/// @brief Tests symmetric scoring of glycine with the lk_ball_wtd scorefunction.
	/// @author Vikram K. Mulligan (vmullig@uw.edu)
	void test_symm_gly_lk_ball_wtd() {
		//Set up the scorefunction
		core::scoring::ScoreFunctionOP scorefxn( new core::scoring::ScoreFunction );
		scorefxn->set_weight( core::scoring::lk_ball_wtd, 1.0 );
		scorefxn->set_weight( core::scoring::fa_sol, 1.0 );
		TR << "Testing lk_ball_wtd score term." << std::endl;
		repeat_structure_test(scorefxn);
		return;
	}

	/// @brief Tests symmetric scoring of glycine with the fa_intra_sol_xover4 scorefunction.
	/// @author Vikram K. Mulligan (vmullig@uw.edu)
	void test_symm_gly_fa_intra_sol_xover4() {
		//Set up the scorefunction
		core::scoring::ScoreFunctionOP scorefxn( new core::scoring::ScoreFunction );
		scorefxn->set_weight( core::scoring::fa_intra_sol_xover4, 1.0 );
		scorefxn->set_weight( core::scoring::fa_sol, 1.0 );
		TR << "Testing fa_intra_sol_xover4 score term." << std::endl;
		repeat_structure_test(scorefxn);
		return;
	}

	/// @brief Tests symmetric scoring of glycine with the fa_sol scorefunction.
	/// @author Vikram K. Mulligan (vmullig@uw.edu)
	void test_symm_gly_fa_sol() {
		//Set up the scorefunction
		core::scoring::ScoreFunctionOP scorefxn( new core::scoring::ScoreFunction );
		scorefxn->set_weight( core::scoring::fa_sol, 1.0 );
		TR << "Testing fa_sol score term." << std::endl;
		repeat_structure_test(scorefxn);
		return;
	}

	/// @brief Tests symmetric scoring of glycine with the fa_elec scorefunction.
	/// @author Vikram K. Mulligan (vmullig@uw.edu)
	void test_symm_gly_fa_elec() {
		//Set up the scorefunction
		core::scoring::ScoreFunctionOP scorefxn( new core::scoring::ScoreFunction );
		scorefxn->set_weight( core::scoring::fa_elec, 1.0 );
		TR << "Testing fa_elec score term." << std::endl;
		repeat_structure_test(scorefxn);
		return;
	}

	/// @brief Tests symmetric scoring of glycine with the hbonds scorefunction.
	/// @author Vikram K. Mulligan (vmullig@uw.edu)
	void test_symm_gly_hbonds() {
		//Set up the scorefunction
		core::scoring::ScoreFunctionOP scorefxn( new core::scoring::ScoreFunction );
		scorefxn->set_weight( core::scoring::hbond_sr_bb, 1.0 );
		scorefxn->set_weight( core::scoring::hbond_lr_bb, 1.0 );
		scorefxn->set_weight( core::scoring::hbond_sc, 1.0 );
		scorefxn->set_weight( core::scoring::hbond_bb_sc, 1.0 );
		TR << "Testing hbonds score terms." << std::endl;
		repeat_structure_test(scorefxn);
		return;
	}

	/// @brief Tests symmetric scoring of glycine with the fa_dun scorefunction.
	/// @author Vikram K. Mulligan (vmullig@uw.edu)
	void test_symm_gly_fa_dun() {
		//Set up the scorefunction
		core::scoring::ScoreFunctionOP scorefxn( new core::scoring::ScoreFunction );
		scorefxn->set_weight( core::scoring::fa_dun, 1.0 );
		TR << "Testing fa_dun score term." << std::endl;
		repeat_structure_test(scorefxn);
		return;
	}

	/// @brief Tests symmetric scoring of glycine with the omega scorefunction.
	/// @author Vikram K. Mulligan (vmullig@uw.edu)
	void test_symm_gly_omega() {
		//Set up the scorefunction
		core::scoring::ScoreFunctionOP scorefxn( new core::scoring::ScoreFunction );
		scorefxn->set_weight( core::scoring::omega, 1.0 );
		TR << "Testing omega score term." << std::endl;
		repeat_structure_test(scorefxn);
		return;
	}

	/// @brief Tests symmetric scoring of glycine with the rama scorefunction.
	/// @author Vikram K. Mulligan (vmullig@uw.edu)
	void test_symm_gly_rama() {
		//Set up the scorefunction
		core::scoring::ScoreFunctionOP scorefxn( new core::scoring::ScoreFunction );
		scorefxn->set_weight( core::scoring::rama, 1.0 );
		TR << "Testing rama score term." << std::endl;
		repeat_structure_test(scorefxn);
		return;
	}

	/// @brief Tests symmetric scoring of glycine with the rama_prepro scorefunction.
	/// @author Vikram K. Mulligan (vmullig@uw.edu)
	void test_symm_gly_rama_prepro() {
		//Set up the scorefunction
		core::scoring::ScoreFunctionOP scorefxn( new core::scoring::ScoreFunction );
		scorefxn->set_weight( core::scoring::rama_prepro, 1.0 );
		TR << "Testing rama_prepro score term." << std::endl;
		repeat_structure_test(scorefxn);
		return;
	}

	/// @brief Tests symmetric scoring of glycine with the p_aa_pp scorefunction.
	/// @author Vikram K. Mulligan (vmullig@uw.edu)
	void test_symm_gly_p_aa_pp() {
		//Set up the scorefunction
		core::scoring::ScoreFunctionOP scorefxn( new core::scoring::ScoreFunction );
		scorefxn->set_weight( core::scoring::p_aa_pp, 1.0 );
		TR << "Testing p_aa_pp score term." << std::endl;
		repeat_structure_test(scorefxn);
		return;
	}

	/// @brief Tests symmetric scoring of glycine with the full beta_nov15 scorefunction.
	/// @author Vikram K. Mulligan (vmullig@uw.edu)
	void test_symm_gly_beta_nov15() {
		//Set up the scorefunction
		core::scoring::ScoreFunctionOP scorefxn( new core::scoring::ScoreFunction );
		scorefxn->add_weights_from_file("beta_nov15.wts");
		TR << "Testing full beta_nov15 score function." << std::endl;
		repeat_structure_test(scorefxn);
		return;
	}

	/// @brief Tests symmetric scoring of glycine with the full default scorefunction, whatever it currently is.
	/// @author Vikram K. Mulligan (vmullig@uw.edu)
	void test_symm_gly_default_scorefxn() {
		//Set up the scorefunction
		core::scoring::ScoreFunctionOP scorefxn( core::scoring::get_score_function() );
		TR << "Testing full default score function." << std::endl;
		repeat_structure_test(scorefxn);
		return;
	}

};
