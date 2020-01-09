// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file   test/core/scoring/methods/ProClosureEnergy.cxxtest.hh
/// @brief  test suite for core::scoring::ProClosureEnergy.cc
/// @author Andrew Leaver-Fay
/// @author Vikram K. Mulligan (vmullig@uw.edu), Baker laboratory.

// Test headers
#include <cxxtest/TestSuite.h>

// Unit headers
#include <core/scoring/methods/ProClosureEnergy.hh>

#include <platform/types.hh>

// Package Headers
#include <test/util/pose_funcs.hh>
#include <test/util/deriv_funcs.hh>
#include <test/core/init_util.hh>

#include <core/kinematics/DomainMap.hh>
#include <core/pose/Pose.hh>
#include <core/scoring/ScoreFunction.hh>

#include <protocols/cyclic_peptide/PeptideStubMover.hh> //Convenience mover to build test peptide.

//Auto Headers
#include <utility/vector1.hh>


// --------------- Test Class --------------- //

// using declarations
using namespace core;
using namespace core::id;
using namespace core::kinematics;
using namespace core::pose;
using namespace core::scoring;
using namespace core::scoring::methods;

class ProClosureEnergyTests : public CxxTest::TestSuite {

public:

	// --------------- Fixtures --------------- //

	// Define a test fixture (some initial state that several tests share)
	// In CxxTest, setUp()/tearDown() are executed around each test case. If you need a fixture on the test
	// suite level, i.e. something that gets constructed once before all the tests in the test suite are run,
	// suites have to be dynamically created. See CxxTest sample directory for example.

	// Shared initialization goes here.
	void setUp() {
		using namespace core;
		core_init();
	}

	// Shared finalization goes here.
	void tearDown() {}

	// --------------- Test Cases --------------- //
	void test_eval_energy()
	{
		Pose trpcage( create_trpcage_ideal_pose() );

		std::cout.precision( 16 );

		ProClosureEnergy proclose_energy;
		ScoreFunction sfxn;
		sfxn.set_weight( pro_close, 0.5 );

		{
			EnergyMap emap;
			proclose_energy.eval_intrares_energy( trpcage.residue( 1 ), trpcage, sfxn, emap );
			TS_ASSERT_DELTA( emap[ pro_close ], 0, 1e-12 );
		}
		{
			EnergyMap tbemap;
			proclose_energy.residue_pair_energy( trpcage.residue( 1 ), trpcage.residue( 2 ), trpcage, sfxn, tbemap );
			TS_ASSERT_DELTA( tbemap[ pro_close ], 0, 1e-12 );
		}
		{
			EnergyMap emap;
			proclose_energy.eval_intrares_energy( trpcage.residue( 2 ), trpcage, sfxn, emap );
			TS_ASSERT_DELTA( emap[ pro_close ], 0, 1e-12 );
		}
		{
			EnergyMap tbemap;
			proclose_energy.residue_pair_energy( trpcage.residue( 2 ), trpcage.residue( 1 ), trpcage, sfxn, tbemap );
			TS_ASSERT_DELTA( tbemap[ pro_close ], 0, 1e-12 );
		}
		{
			EnergyMap tbemap;
			proclose_energy.residue_pair_energy( trpcage.residue( 2 ), trpcage.residue( 3 ), trpcage, sfxn, tbemap );
			TS_ASSERT_DELTA( tbemap[ pro_close ], 0, 1e-12 );
		}
		{
			EnergyMap emap;
			proclose_energy.eval_intrares_energy( trpcage.residue( 3 ), trpcage, sfxn, emap );
			TS_ASSERT_DELTA( emap[ pro_close ], 0, 1e-12 );
		}
		{
			EnergyMap tbemap;
			proclose_energy.residue_pair_energy( trpcage.residue( 3 ), trpcage.residue( 2 ), trpcage, sfxn, tbemap );
			TS_ASSERT_DELTA( tbemap[ pro_close ], 0, 1e-12 );
		}
		{
			EnergyMap tbemap;
			proclose_energy.residue_pair_energy( trpcage.residue( 3 ), trpcage.residue( 4 ), trpcage, sfxn, tbemap );
			TS_ASSERT_DELTA( tbemap[ pro_close ], 0, 1e-12 );
		}
		{
			EnergyMap emap;
			proclose_energy.eval_intrares_energy( trpcage.residue( 4 ), trpcage, sfxn, emap );
			TS_ASSERT_DELTA( emap[ pro_close ], 0, 1e-12 );
		}
		{
			EnergyMap tbemap;
			proclose_energy.residue_pair_energy( trpcage.residue( 4 ), trpcage.residue( 3 ), trpcage, sfxn, tbemap );
			TS_ASSERT_DELTA( tbemap[ pro_close ], 0, 1e-12 );
		}
		{
			EnergyMap tbemap;
			proclose_energy.residue_pair_energy( trpcage.residue( 4 ), trpcage.residue( 5 ), trpcage, sfxn, tbemap );
			TS_ASSERT_DELTA( tbemap[ pro_close ], 0, 1e-12 );
		}
		{
			EnergyMap emap;
			proclose_energy.eval_intrares_energy( trpcage.residue( 5 ), trpcage, sfxn, emap );
			TS_ASSERT_DELTA( emap[ pro_close ], 0, 1e-12 );
		}
		{
			EnergyMap tbemap;
			proclose_energy.residue_pair_energy( trpcage.residue( 5 ), trpcage.residue( 4 ), trpcage, sfxn, tbemap );
			TS_ASSERT_DELTA( tbemap[ pro_close ], 0, 1e-12 );
		}
		{
			EnergyMap tbemap;
			proclose_energy.residue_pair_energy( trpcage.residue( 5 ), trpcage.residue( 6 ), trpcage, sfxn, tbemap );
			TS_ASSERT_DELTA( tbemap[ pro_close ], 0, 1e-12 );
		}
		{
			EnergyMap emap;
			proclose_energy.eval_intrares_energy( trpcage.residue( 6 ), trpcage, sfxn, emap );
			TS_ASSERT_DELTA( emap[ pro_close ], 0, 1e-12 );
		}
		{
			EnergyMap tbemap;
			proclose_energy.residue_pair_energy( trpcage.residue( 6 ), trpcage.residue( 5 ), trpcage, sfxn, tbemap );
			TS_ASSERT_DELTA( tbemap[ pro_close ], 0, 1e-12 );
		}
		{
			EnergyMap tbemap;
			proclose_energy.residue_pair_energy( trpcage.residue( 6 ), trpcage.residue( 7 ), trpcage, sfxn, tbemap );
			TS_ASSERT_DELTA( tbemap[ pro_close ], 0, 1e-12 );
		}
		{
			EnergyMap emap;
			proclose_energy.eval_intrares_energy( trpcage.residue( 7 ), trpcage, sfxn, emap );
			TS_ASSERT_DELTA( emap[ pro_close ], 0, 1e-12 );
		}
		{
			EnergyMap tbemap;
			proclose_energy.residue_pair_energy( trpcage.residue( 7 ), trpcage.residue( 6 ), trpcage, sfxn, tbemap );
			TS_ASSERT_DELTA( tbemap[ pro_close ], 0, 1e-12 );
		}
		{
			EnergyMap tbemap;
			proclose_energy.residue_pair_energy( trpcage.residue( 7 ), trpcage.residue( 8 ), trpcage, sfxn, tbemap );
			TS_ASSERT_DELTA( tbemap[ pro_close ], 0, 1e-12 );
		}
		{
			EnergyMap emap;
			proclose_energy.eval_intrares_energy( trpcage.residue( 8 ), trpcage, sfxn, emap );
			TS_ASSERT_DELTA( emap[ pro_close ], 0, 1e-12 );
		}
		{
			EnergyMap tbemap;
			proclose_energy.residue_pair_energy( trpcage.residue( 8 ), trpcage.residue( 7 ), trpcage, sfxn, tbemap );
			TS_ASSERT_DELTA( tbemap[ pro_close ], 0, 1e-12 );
		}
		{
			EnergyMap tbemap;
			proclose_energy.residue_pair_energy( trpcage.residue( 8 ), trpcage.residue( 9 ), trpcage, sfxn, tbemap );
			TS_ASSERT_DELTA( tbemap[ pro_close ], 0, 1e-12 );
		}
		{
			EnergyMap emap;
			proclose_energy.eval_intrares_energy( trpcage.residue( 9 ), trpcage, sfxn, emap );
			TS_ASSERT_DELTA( emap[ pro_close ], 0, 1e-12 );
		}
		{
			EnergyMap tbemap;
			proclose_energy.residue_pair_energy( trpcage.residue( 9 ), trpcage.residue( 8 ), trpcage, sfxn, tbemap );
			TS_ASSERT_DELTA( tbemap[ pro_close ], 0, 1e-12 );
		}
		{
			EnergyMap tbemap;
			proclose_energy.residue_pair_energy( trpcage.residue( 9 ), trpcage.residue( 10 ), trpcage, sfxn, tbemap );
			TS_ASSERT_DELTA( tbemap[ pro_close ], 0, 1e-12 );
		}
		{
			EnergyMap emap;
			proclose_energy.eval_intrares_energy( trpcage.residue( 10 ), trpcage, sfxn, emap );
			TS_ASSERT_DELTA( emap[ pro_close ], 0, 1e-12 );
		}
		{
			EnergyMap tbemap;
			proclose_energy.residue_pair_energy( trpcage.residue( 10 ), trpcage.residue( 9 ), trpcage, sfxn, tbemap );
			TS_ASSERT_DELTA( tbemap[ pro_close ], 0, 1e-12 );
		}
		{
			EnergyMap tbemap;
			proclose_energy.residue_pair_energy( trpcage.residue( 10 ), trpcage.residue( 11 ), trpcage, sfxn, tbemap );
			TS_ASSERT_DELTA( tbemap[ pro_close ], 0, 1e-12 );
		}
		{
			EnergyMap emap;
			proclose_energy.eval_intrares_energy( trpcage.residue( 11 ), trpcage, sfxn, emap );
			TS_ASSERT_DELTA( emap[ pro_close ], 0, 1e-12 );
		}
		{
			EnergyMap tbemap;
			proclose_energy.residue_pair_energy( trpcage.residue( 11 ), trpcage.residue( 10 ), trpcage, sfxn, tbemap );
			TS_ASSERT_DELTA( tbemap[ pro_close ], 0, 1e-12 );
		}
		{
			EnergyMap tbemap;
			proclose_energy.residue_pair_energy( trpcage.residue( 11 ), trpcage.residue( 12 ), trpcage, sfxn, tbemap );
			TS_ASSERT_DELTA( tbemap[ pro_close ], 0.004436792234172629, 1e-12 );
		}
		{
			EnergyMap emap;
			proclose_energy.eval_intrares_energy( trpcage.residue( 12 ), trpcage, sfxn, emap );
			TS_ASSERT_DELTA( emap[ pro_close ], 0.0124967778684113, 1e-12 );
		}
		{
			EnergyMap tbemap;
			proclose_energy.residue_pair_energy( trpcage.residue( 12 ), trpcage.residue( 11 ), trpcage, sfxn, tbemap );
			TS_ASSERT_DELTA( tbemap[ pro_close ], 0.004436792234172629, 1e-12 );
		}
		{
			EnergyMap tbemap;
			proclose_energy.residue_pair_energy( trpcage.residue( 12 ), trpcage.residue( 13 ), trpcage, sfxn, tbemap );
			TS_ASSERT_DELTA( tbemap[ pro_close ], 0, 1e-12 );
		}
		{
			EnergyMap emap;
			proclose_energy.eval_intrares_energy( trpcage.residue( 13 ), trpcage, sfxn, emap );
			TS_ASSERT_DELTA( emap[ pro_close ], 0, 1e-12 );
		}
		{
			EnergyMap tbemap;
			proclose_energy.residue_pair_energy( trpcage.residue( 13 ), trpcage.residue( 12 ), trpcage, sfxn, tbemap );
			TS_ASSERT_DELTA( tbemap[ pro_close ], 0, 1e-12 );
		}
		{
			EnergyMap tbemap;
			proclose_energy.residue_pair_energy( trpcage.residue( 13 ), trpcage.residue( 14 ), trpcage, sfxn, tbemap );
			TS_ASSERT_DELTA( tbemap[ pro_close ], 0, 1e-12 );
		}
		{
			EnergyMap emap;
			proclose_energy.eval_intrares_energy( trpcage.residue( 14 ), trpcage, sfxn, emap );
			TS_ASSERT_DELTA( emap[ pro_close ], 0, 1e-12 );
		}
		{
			EnergyMap tbemap;
			proclose_energy.residue_pair_energy( trpcage.residue( 14 ), trpcage.residue( 13 ), trpcage, sfxn, tbemap );
			TS_ASSERT_DELTA( tbemap[ pro_close ], 0, 1e-12 );
		}
		{
			EnergyMap tbemap;
			proclose_energy.residue_pair_energy( trpcage.residue( 14 ), trpcage.residue( 15 ), trpcage, sfxn, tbemap );
			TS_ASSERT_DELTA( tbemap[ pro_close ], 0, 1e-12 );
		}
		{
			EnergyMap emap;
			proclose_energy.eval_intrares_energy( trpcage.residue( 15 ), trpcage, sfxn, emap );
			TS_ASSERT_DELTA( emap[ pro_close ], 0, 1e-12 );
		}
		{
			EnergyMap tbemap;
			proclose_energy.residue_pair_energy( trpcage.residue( 15 ), trpcage.residue( 14 ), trpcage, sfxn, tbemap );
			TS_ASSERT_DELTA( tbemap[ pro_close ], 0, 1e-12 );
		}
		{
			EnergyMap tbemap;
			proclose_energy.residue_pair_energy( trpcage.residue( 15 ), trpcage.residue( 16 ), trpcage, sfxn, tbemap );
			TS_ASSERT_DELTA( tbemap[ pro_close ], 0, 1e-12 );
		}
		{
			EnergyMap emap;
			proclose_energy.eval_intrares_energy( trpcage.residue( 16 ), trpcage, sfxn, emap );
			TS_ASSERT_DELTA( emap[ pro_close ], 0, 1e-12 );
		}
		{
			EnergyMap tbemap;
			proclose_energy.residue_pair_energy( trpcage.residue( 16 ), trpcage.residue( 15 ), trpcage, sfxn, tbemap );
			TS_ASSERT_DELTA( tbemap[ pro_close ], 0, 1e-12 );
		}
		{
			EnergyMap tbemap;
			proclose_energy.residue_pair_energy( trpcage.residue( 16 ), trpcage.residue( 17 ), trpcage, sfxn, tbemap );
			TS_ASSERT_DELTA( tbemap[ pro_close ], 3.287050070076195e-05, 1e-12 );
		}
		{
			EnergyMap emap;
			proclose_energy.eval_intrares_energy( trpcage.residue( 17 ), trpcage, sfxn, emap );
			TS_ASSERT_DELTA( emap[ pro_close ], 0.2063039233793921, 1e-12 );
		}
		{
			EnergyMap tbemap;
			proclose_energy.residue_pair_energy( trpcage.residue( 17 ), trpcage.residue( 16 ), trpcage, sfxn, tbemap );
			TS_ASSERT_DELTA( tbemap[ pro_close ], 3.287050070076195e-05, 1e-12 );
		}
		{
			EnergyMap tbemap;
			proclose_energy.residue_pair_energy( trpcage.residue( 17 ), trpcage.residue( 18 ), trpcage, sfxn, tbemap );
			TS_ASSERT_DELTA( tbemap[ pro_close ], 0.008603170724419516, 1e-12 );
		}
		{
			EnergyMap emap;
			proclose_energy.eval_intrares_energy( trpcage.residue( 18 ), trpcage, sfxn, emap );
			TS_ASSERT_DELTA( emap[ pro_close ], 0.1033872216433958, 1e-12 );
		}
		{
			EnergyMap tbemap;
			proclose_energy.residue_pair_energy( trpcage.residue( 18 ), trpcage.residue( 17 ), trpcage, sfxn, tbemap );
			TS_ASSERT_DELTA( tbemap[ pro_close ], 0.008603170724419516, 1e-12 );
		}
		{
			EnergyMap tbemap;
			proclose_energy.residue_pair_energy( trpcage.residue( 18 ), trpcage.residue( 19 ), trpcage, sfxn, tbemap );
			TS_ASSERT_DELTA( tbemap[ pro_close ], 0.07438490402267119, 1e-12 );
		}
		{
			EnergyMap emap;
			proclose_energy.eval_intrares_energy( trpcage.residue( 19 ), trpcage, sfxn, emap );
			TS_ASSERT_DELTA( emap[ pro_close ], 0.1980550619309646, 1e-12 );
		}
		{
			EnergyMap tbemap;
			proclose_energy.residue_pair_energy( trpcage.residue( 19 ), trpcage.residue( 18 ), trpcage, sfxn, tbemap );
			TS_ASSERT_DELTA( tbemap[ pro_close ], 0.07438490402267119, 1e-12 );
		}
		{
			EnergyMap tbemap;
			proclose_energy.residue_pair_energy( trpcage.residue( 19 ), trpcage.residue( 20 ), trpcage, sfxn, tbemap );
			TS_ASSERT_DELTA( tbemap[ pro_close ], 0, 1e-12 );
		}
		{
			EnergyMap emap;
			proclose_energy.eval_intrares_energy( trpcage.residue( 20 ), trpcage, sfxn, emap );
			TS_ASSERT_DELTA( emap[ pro_close ], 0, 1e-12 );
		}
		{
			EnergyMap tbemap;
			proclose_energy.residue_pair_energy( trpcage.residue( 20 ), trpcage.residue( 19 ), trpcage, sfxn, tbemap );
			TS_ASSERT_DELTA( tbemap[ pro_close ], 0, 1e-12 );
		}


	}

	/// @brief Tests whether the pro_close energy evaluation is working as expected with the ring closure part of the energy term turned off.
	/// @author Vikram K. Mulligan (vmullig@uw.edu), Baker laboratory.
	void test_eval_energy_noring()
	{
		Pose trpcage( create_trpcage_ideal_pose() );

		std::cout.precision( 16 );

		ProClosureEnergy proclose_energy;
		proclose_energy.set_skip_ring_closure(true);
		ScoreFunction sfxn;
		sfxn.set_weight( pro_close, 0.5 );

		{
			EnergyMap emap;
			proclose_energy.eval_intrares_energy( trpcage.residue( 1 ), trpcage, sfxn, emap );
			TS_ASSERT_DELTA( emap[ pro_close ], 0, 1e-12 );
		}
		{
			EnergyMap tbemap;
			proclose_energy.residue_pair_energy( trpcage.residue( 1 ), trpcage.residue( 2 ), trpcage, sfxn, tbemap );
			TS_ASSERT_DELTA( tbemap[ pro_close ], 0, 1e-12 );
		}
		{
			EnergyMap emap;
			proclose_energy.eval_intrares_energy( trpcage.residue( 2 ), trpcage, sfxn, emap );
			TS_ASSERT_DELTA( emap[ pro_close ], 0, 1e-12 );
		}
		{
			EnergyMap tbemap;
			proclose_energy.residue_pair_energy( trpcage.residue( 2 ), trpcage.residue( 1 ), trpcage, sfxn, tbemap );
			TS_ASSERT_DELTA( tbemap[ pro_close ], 0, 1e-12 );
		}
		{
			EnergyMap tbemap;
			proclose_energy.residue_pair_energy( trpcage.residue( 2 ), trpcage.residue( 3 ), trpcage, sfxn, tbemap );
			TS_ASSERT_DELTA( tbemap[ pro_close ], 0, 1e-12 );
		}
		{
			EnergyMap emap;
			proclose_energy.eval_intrares_energy( trpcage.residue( 3 ), trpcage, sfxn, emap );
			TS_ASSERT_DELTA( emap[ pro_close ], 0, 1e-12 );
		}
		{
			EnergyMap tbemap;
			proclose_energy.residue_pair_energy( trpcage.residue( 3 ), trpcage.residue( 2 ), trpcage, sfxn, tbemap );
			TS_ASSERT_DELTA( tbemap[ pro_close ], 0, 1e-12 );
		}
		{
			EnergyMap tbemap;
			proclose_energy.residue_pair_energy( trpcage.residue( 3 ), trpcage.residue( 4 ), trpcage, sfxn, tbemap );
			TS_ASSERT_DELTA( tbemap[ pro_close ], 0, 1e-12 );
		}
		{
			EnergyMap emap;
			proclose_energy.eval_intrares_energy( trpcage.residue( 4 ), trpcage, sfxn, emap );
			TS_ASSERT_DELTA( emap[ pro_close ], 0, 1e-12 );
		}
		{
			EnergyMap tbemap;
			proclose_energy.residue_pair_energy( trpcage.residue( 4 ), trpcage.residue( 3 ), trpcage, sfxn, tbemap );
			TS_ASSERT_DELTA( tbemap[ pro_close ], 0, 1e-12 );
		}
		{
			EnergyMap tbemap;
			proclose_energy.residue_pair_energy( trpcage.residue( 4 ), trpcage.residue( 5 ), trpcage, sfxn, tbemap );
			TS_ASSERT_DELTA( tbemap[ pro_close ], 0, 1e-12 );
		}
		{
			EnergyMap emap;
			proclose_energy.eval_intrares_energy( trpcage.residue( 5 ), trpcage, sfxn, emap );
			TS_ASSERT_DELTA( emap[ pro_close ], 0, 1e-12 );
		}
		{
			EnergyMap tbemap;
			proclose_energy.residue_pair_energy( trpcage.residue( 5 ), trpcage.residue( 4 ), trpcage, sfxn, tbemap );
			TS_ASSERT_DELTA( tbemap[ pro_close ], 0, 1e-12 );
		}
		{
			EnergyMap tbemap;
			proclose_energy.residue_pair_energy( trpcage.residue( 5 ), trpcage.residue( 6 ), trpcage, sfxn, tbemap );
			TS_ASSERT_DELTA( tbemap[ pro_close ], 0, 1e-12 );
		}
		{
			EnergyMap emap;
			proclose_energy.eval_intrares_energy( trpcage.residue( 6 ), trpcage, sfxn, emap );
			TS_ASSERT_DELTA( emap[ pro_close ], 0, 1e-12 );
		}
		{
			EnergyMap tbemap;
			proclose_energy.residue_pair_energy( trpcage.residue( 6 ), trpcage.residue( 5 ), trpcage, sfxn, tbemap );
			TS_ASSERT_DELTA( tbemap[ pro_close ], 0, 1e-12 );
		}
		{
			EnergyMap tbemap;
			proclose_energy.residue_pair_energy( trpcage.residue( 6 ), trpcage.residue( 7 ), trpcage, sfxn, tbemap );
			TS_ASSERT_DELTA( tbemap[ pro_close ], 0, 1e-12 );
		}
		{
			EnergyMap emap;
			proclose_energy.eval_intrares_energy( trpcage.residue( 7 ), trpcage, sfxn, emap );
			TS_ASSERT_DELTA( emap[ pro_close ], 0, 1e-12 );
		}
		{
			EnergyMap tbemap;
			proclose_energy.residue_pair_energy( trpcage.residue( 7 ), trpcage.residue( 6 ), trpcage, sfxn, tbemap );
			TS_ASSERT_DELTA( tbemap[ pro_close ], 0, 1e-12 );
		}
		{
			EnergyMap tbemap;
			proclose_energy.residue_pair_energy( trpcage.residue( 7 ), trpcage.residue( 8 ), trpcage, sfxn, tbemap );
			TS_ASSERT_DELTA( tbemap[ pro_close ], 0, 1e-12 );
		}
		{
			EnergyMap emap;
			proclose_energy.eval_intrares_energy( trpcage.residue( 8 ), trpcage, sfxn, emap );
			TS_ASSERT_DELTA( emap[ pro_close ], 0, 1e-12 );
		}
		{
			EnergyMap tbemap;
			proclose_energy.residue_pair_energy( trpcage.residue( 8 ), trpcage.residue( 7 ), trpcage, sfxn, tbemap );
			TS_ASSERT_DELTA( tbemap[ pro_close ], 0, 1e-12 );
		}
		{
			EnergyMap tbemap;
			proclose_energy.residue_pair_energy( trpcage.residue( 8 ), trpcage.residue( 9 ), trpcage, sfxn, tbemap );
			TS_ASSERT_DELTA( tbemap[ pro_close ], 0, 1e-12 );
		}
		{
			EnergyMap emap;
			proclose_energy.eval_intrares_energy( trpcage.residue( 9 ), trpcage, sfxn, emap );
			TS_ASSERT_DELTA( emap[ pro_close ], 0, 1e-12 );
		}
		{
			EnergyMap tbemap;
			proclose_energy.residue_pair_energy( trpcage.residue( 9 ), trpcage.residue( 8 ), trpcage, sfxn, tbemap );
			TS_ASSERT_DELTA( tbemap[ pro_close ], 0, 1e-12 );
		}
		{
			EnergyMap tbemap;
			proclose_energy.residue_pair_energy( trpcage.residue( 9 ), trpcage.residue( 10 ), trpcage, sfxn, tbemap );
			TS_ASSERT_DELTA( tbemap[ pro_close ], 0, 1e-12 );
		}
		{
			EnergyMap emap;
			proclose_energy.eval_intrares_energy( trpcage.residue( 10 ), trpcage, sfxn, emap );
			TS_ASSERT_DELTA( emap[ pro_close ], 0, 1e-12 );
		}
		{
			EnergyMap tbemap;
			proclose_energy.residue_pair_energy( trpcage.residue( 10 ), trpcage.residue( 9 ), trpcage, sfxn, tbemap );
			TS_ASSERT_DELTA( tbemap[ pro_close ], 0, 1e-12 );
		}
		{
			EnergyMap tbemap;
			proclose_energy.residue_pair_energy( trpcage.residue( 10 ), trpcage.residue( 11 ), trpcage, sfxn, tbemap );
			TS_ASSERT_DELTA( tbemap[ pro_close ], 0, 1e-12 );
		}
		{
			EnergyMap emap;
			proclose_energy.eval_intrares_energy( trpcage.residue( 11 ), trpcage, sfxn, emap );
			TS_ASSERT_DELTA( emap[ pro_close ], 0, 1e-12 );
		}
		{
			EnergyMap tbemap;
			proclose_energy.residue_pair_energy( trpcage.residue( 11 ), trpcage.residue( 10 ), trpcage, sfxn, tbemap );
			TS_ASSERT_DELTA( tbemap[ pro_close ], 0, 1e-12 );
		}
		{
			EnergyMap tbemap;
			proclose_energy.residue_pair_energy( trpcage.residue( 11 ), trpcage.residue( 12 ), trpcage, sfxn, tbemap );
			TS_ASSERT_DELTA( tbemap[ pro_close ], 0.004436792234172629, 1e-12 );
		}
		{
			EnergyMap emap;
			proclose_energy.eval_intrares_energy( trpcage.residue( 12 ), trpcage, sfxn, emap );
			TS_ASSERT_DELTA( emap[ pro_close ], 0, 1e-12 );
		}
		{
			EnergyMap tbemap;
			proclose_energy.residue_pair_energy( trpcage.residue( 12 ), trpcage.residue( 11 ), trpcage, sfxn, tbemap );
			TS_ASSERT_DELTA( tbemap[ pro_close ], 0.004436792234172629, 1e-12 );
		}
		{
			EnergyMap tbemap;
			proclose_energy.residue_pair_energy( trpcage.residue( 12 ), trpcage.residue( 13 ), trpcage, sfxn, tbemap );
			TS_ASSERT_DELTA( tbemap[ pro_close ], 0, 1e-12 );
		}
		{
			EnergyMap emap;
			proclose_energy.eval_intrares_energy( trpcage.residue( 13 ), trpcage, sfxn, emap );
			TS_ASSERT_DELTA( emap[ pro_close ], 0, 1e-12 );
		}
		{
			EnergyMap tbemap;
			proclose_energy.residue_pair_energy( trpcage.residue( 13 ), trpcage.residue( 12 ), trpcage, sfxn, tbemap );
			TS_ASSERT_DELTA( tbemap[ pro_close ], 0, 1e-12 );
		}
		{
			EnergyMap tbemap;
			proclose_energy.residue_pair_energy( trpcage.residue( 13 ), trpcage.residue( 14 ), trpcage, sfxn, tbemap );
			TS_ASSERT_DELTA( tbemap[ pro_close ], 0, 1e-12 );
		}
		{
			EnergyMap emap;
			proclose_energy.eval_intrares_energy( trpcage.residue( 14 ), trpcage, sfxn, emap );
			TS_ASSERT_DELTA( emap[ pro_close ], 0, 1e-12 );
		}
		{
			EnergyMap tbemap;
			proclose_energy.residue_pair_energy( trpcage.residue( 14 ), trpcage.residue( 13 ), trpcage, sfxn, tbemap );
			TS_ASSERT_DELTA( tbemap[ pro_close ], 0, 1e-12 );
		}
		{
			EnergyMap tbemap;
			proclose_energy.residue_pair_energy( trpcage.residue( 14 ), trpcage.residue( 15 ), trpcage, sfxn, tbemap );
			TS_ASSERT_DELTA( tbemap[ pro_close ], 0, 1e-12 );
		}
		{
			EnergyMap emap;
			proclose_energy.eval_intrares_energy( trpcage.residue( 15 ), trpcage, sfxn, emap );
			TS_ASSERT_DELTA( emap[ pro_close ], 0, 1e-12 );
		}
		{
			EnergyMap tbemap;
			proclose_energy.residue_pair_energy( trpcage.residue( 15 ), trpcage.residue( 14 ), trpcage, sfxn, tbemap );
			TS_ASSERT_DELTA( tbemap[ pro_close ], 0, 1e-12 );
		}
		{
			EnergyMap tbemap;
			proclose_energy.residue_pair_energy( trpcage.residue( 15 ), trpcage.residue( 16 ), trpcage, sfxn, tbemap );
			TS_ASSERT_DELTA( tbemap[ pro_close ], 0, 1e-12 );
		}
		{
			EnergyMap emap;
			proclose_energy.eval_intrares_energy( trpcage.residue( 16 ), trpcage, sfxn, emap );
			TS_ASSERT_DELTA( emap[ pro_close ], 0, 1e-12 );
		}
		{
			EnergyMap tbemap;
			proclose_energy.residue_pair_energy( trpcage.residue( 16 ), trpcage.residue( 15 ), trpcage, sfxn, tbemap );
			TS_ASSERT_DELTA( tbemap[ pro_close ], 0, 1e-12 );
		}
		{
			EnergyMap tbemap;
			proclose_energy.residue_pair_energy( trpcage.residue( 16 ), trpcage.residue( 17 ), trpcage, sfxn, tbemap );
			TS_ASSERT_DELTA( tbemap[ pro_close ], 3.287050070076195e-05, 1e-12 );
		}
		{
			EnergyMap emap;
			proclose_energy.eval_intrares_energy( trpcage.residue( 17 ), trpcage, sfxn, emap );
			TS_ASSERT_DELTA( emap[ pro_close ], 0, 1e-12 );
		}
		{
			EnergyMap tbemap;
			proclose_energy.residue_pair_energy( trpcage.residue( 17 ), trpcage.residue( 16 ), trpcage, sfxn, tbemap );
			TS_ASSERT_DELTA( tbemap[ pro_close ], 3.287050070076195e-05, 1e-12 );
		}
		{
			EnergyMap tbemap;
			proclose_energy.residue_pair_energy( trpcage.residue( 17 ), trpcage.residue( 18 ), trpcage, sfxn, tbemap );
			TS_ASSERT_DELTA( tbemap[ pro_close ], 0.008603170724419516, 1e-12 );
		}
		{
			EnergyMap emap;
			proclose_energy.eval_intrares_energy( trpcage.residue( 18 ), trpcage, sfxn, emap );
			TS_ASSERT_DELTA( emap[ pro_close ], 0, 1e-12 );
		}
		{
			EnergyMap tbemap;
			proclose_energy.residue_pair_energy( trpcage.residue( 18 ), trpcage.residue( 17 ), trpcage, sfxn, tbemap );
			TS_ASSERT_DELTA( tbemap[ pro_close ], 0.008603170724419516, 1e-12 );
		}
		{
			EnergyMap tbemap;
			proclose_energy.residue_pair_energy( trpcage.residue( 18 ), trpcage.residue( 19 ), trpcage, sfxn, tbemap );
			TS_ASSERT_DELTA( tbemap[ pro_close ], 0.07438490402267119, 1e-12 );
		}
		{
			EnergyMap emap;
			proclose_energy.eval_intrares_energy( trpcage.residue( 19 ), trpcage, sfxn, emap );
			TS_ASSERT_DELTA( emap[ pro_close ], 0, 1e-12 );
		}
		{
			EnergyMap tbemap;
			proclose_energy.residue_pair_energy( trpcage.residue( 19 ), trpcage.residue( 18 ), trpcage, sfxn, tbemap );
			TS_ASSERT_DELTA( tbemap[ pro_close ], 0.07438490402267119, 1e-12 );
		}
		{
			EnergyMap tbemap;
			proclose_energy.residue_pair_energy( trpcage.residue( 19 ), trpcage.residue( 20 ), trpcage, sfxn, tbemap );
			TS_ASSERT_DELTA( tbemap[ pro_close ], 0, 1e-12 );
		}
		{
			EnergyMap emap;
			proclose_energy.eval_intrares_energy( trpcage.residue( 20 ), trpcage, sfxn, emap );
			TS_ASSERT_DELTA( emap[ pro_close ], 0, 1e-12 );
		}
		{
			EnergyMap tbemap;
			proclose_energy.residue_pair_energy( trpcage.residue( 20 ), trpcage.residue( 19 ), trpcage, sfxn, tbemap );
			TS_ASSERT_DELTA( tbemap[ pro_close ], 0, 1e-12 );
		}


	}

	/// @brief Test that the pro_close energy works properly with L-oligourea-proline.
	/// @note The OU3_PRO rotamer set is not included with Rosetta, but must be downloaded from the Git LFS.
	/// As such, this test never invokes the packer or the fa_dun score term.
	/// @author Vikram K. Mulligan (vmullig@uw.edu).
	void test_proclose_L_oligourea() {
		protocols::cyclic_peptide::PeptideStubMover builder;
		builder.add_residue("Append", "GLY:NtermProteinFull", 1, true, "", 0, 1, "");
		builder.add_residue("Append", "OU3_PRO", 2, false, "N", 0, 1, "C");
		builder.add_residue("Append", "GLY:CtermProteinFull", 3, false, "N", 0, 2, "C");
		core::pose::Pose pose;
		builder.apply(pose); //Build the peptide.

		//These chi values should result in a nasty, open proline ring:
		pose.set_chi( 1, 2, 45.0 );
		pose.set_chi( 2, 2, 50.0 );
		pose.set_chi( 3, 2, 30.0 );

		core::scoring::ScoreFunctionOP sfxn( new core::scoring::ScoreFunction );
		sfxn->set_weight( core::scoring::pro_close, 1.0 );
		core::Real const proenergy( (*sfxn)(pose) );

		TS_ASSERT_LESS_THAN( 0.1, proenergy ); //Confirm that the open proline produces a positive energy value.
	}

	/// @brief Test that the pro_close energy works properly with D-oligourea-proline.
	/// @note The OU3_PRO rotamer set is not included with Rosetta, but must be downloaded from the Git LFS.
	/// As such, this test never invokes the packer or the fa_dun score term.
	/// @author Vikram K. Mulligan (vmullig@uw.edu).
	void test_proclose_D_oligourea() {
		protocols::cyclic_peptide::PeptideStubMover builder;
		builder.add_residue("Append", "GLY:NtermProteinFull", 1, true, "", 0, 1, "");
		builder.add_residue("Append", "DOU3_PRO", 2, false, "N", 0, 1, "C");
		builder.add_residue("Append", "GLY:CtermProteinFull", 3, false, "N", 0, 2, "C");
		core::pose::Pose pose;
		builder.apply(pose); //Build the peptide.

		//These chi values should result in a nasty, open proline ring:
		pose.set_chi( 1, 2, 45.0 );
		pose.set_chi( 2, 2, 50.0 );
		pose.set_chi( 3, 2, 30.0 );

		core::scoring::ScoreFunctionOP sfxn( new core::scoring::ScoreFunction );
		sfxn->set_weight( core::scoring::pro_close, 1.0 );
		core::Real const proenergy( (*sfxn)(pose) );

		TS_ASSERT_LESS_THAN( 0.1, proenergy ); //Confirm that the open proline produces a positive energy value.
	}

	void test_proclose_start_score_start_func_match_w_total_flexibility()
	{
		core::pose::Pose pose = create_trpcage_ideal_pose();
		core::scoring::ScoreFunction sfxn;
		sfxn.set_weight( pro_close, 0.5 );
		kinematics::MoveMap movemap( create_movemap_to_allow_all_torsions() );
		AtomDerivValidator adv;
		adv.set_pose( pose );
		adv.set_score_function( sfxn );
		adv.set_movemap( movemap );
		adv.validate_start_func_matches_start_score( 0.303850361152064 );

	}

	void test_proclose_deriv_check_w_total_flexibility()
	{
		core::pose::Pose pose = create_trpcage_ideal_pose();
		core::scoring::ScoreFunction sfxn;
		sfxn.set_weight( pro_close, 0.5 );
		kinematics::MoveMap movemap( create_movemap_to_allow_all_torsions() );
		AtomDerivValidator adv;
		adv.set_pose( pose );
		adv.set_score_function( sfxn );
		adv.set_movemap( movemap );
		adv.simple_deriv_check( false, 1e-6 );

	}

};


