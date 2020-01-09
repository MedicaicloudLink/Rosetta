// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file   test/core/scoring/methods/LinearChainbreakEnergy.cxxtest.hh
/// @brief  test suite for core::scoring::methods::LinearChainbreakEnergy
/// @author Andrew Leaver-Fay (aleaverfay@gmail.com)

// Test headers
#include <cxxtest/TestSuite.h>

// Unit headers

#include <platform/types.hh>

// Package Headers
#include <test/util/deriv_funcs.hh>
#include <test/util/pose_funcs.hh>
#include <test/util/pdb1ubq.hh>
#include <test/core/init_util.hh>

#include <core/chemical/VariantType.hh>

#include <core/kinematics/FoldTree.hh>


//Auto Headers
#include <core/pose/variant_util.hh>
#include <utility/vector1.hh>


// --------------- Test Class --------------- //

// using declarations
using namespace core;
using namespace core::pose;
using namespace core::scoring;
using namespace core::scoring::methods;

class LinearChainbreakEnergyTests : public CxxTest::TestSuite {

public:

	// --------------- Fixtures --------------- //

	// Shared initialization goes here.
	void setUp() {
		core_init();
	}

	// Shared finalization goes here.
	void tearDown() {
	}


	/// This can be used to ensure norm matches norm-numeric
	void test_linear_chainbreak_deriv_check()
	{
		using namespace core::chemical;
		using namespace core::kinematics;
		using namespace core::pose;
		using namespace core::scoring;

		Pose pose = pdb1ubq5to13_pose();
		FoldTree ft;
		ft.add_edge( 1, 3, -1 );
		ft.add_edge( 3, 7,  1 );
		ft.add_edge( 7, 9, -1 );
		ft.add_edge( 7, 6, -1 );
		ft.add_edge( 3, 5, -1 );

		pose.fold_tree( ft );
		pose.set_phi( 4, 180 );
		pose.set_psi( 4, 180 );
		core::pose::add_variant_type_to_pose_residue( pose, CUTPOINT_LOWER, 5 );
		core::pose::add_variant_type_to_pose_residue( pose, CUTPOINT_UPPER, 6 );


		//pose.dump_pdb( "test_ft1.pdb" );

		ScoreFunction sfxn;
		sfxn.set_weight( linear_chainbreak, 0.5 );
		TS_ASSERT_DELTA( sfxn( pose ), 4.5628, 1e-3 );

		//Size old_precision = std::cout.precision();
		//std::cout.precision( 16 );
		//std::cout << "linear_chainbreak score: " << sfxn( pose ) << std::endl;
		//std::cout.precision( old_precision );

		MoveMap movemap( create_movemap_to_allow_all_torsions() );
		AtomDerivValidator adv( pose, sfxn, movemap );
		adv.simple_deriv_check( true, 1e-6 );
	}

};


