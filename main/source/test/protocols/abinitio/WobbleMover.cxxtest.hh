// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file   core/pack/RotamerTrials.cxxtest.hh
/// @brief  test suite for rotamer_trials
/// @author Oliver Lange

// Test headers
#include <cxxtest/TestSuite.h>

#include <test/core/init_util.hh>

#include <protocols/simple_moves/WobbleMover.hh>

#include <protocols/simple_moves/GunnCost.hh>

#include <core/scoring/ScoreFunction.fwd.hh>

#include <core/kinematics/MoveMap.hh>

#include <core/fragment/ConstantLengthFragSet.hh>


#include <core/pose/Pose.hh>
#include <core/types.hh>

#include <basic/Tracer.hh>

//Auto Headers
#include <core/id/AtomID_Mask.hh>
#include <core/import_pose/import_pose.hh>
#include <utility/fix_boinc_read.hh>
#include <utility/vector1.hh>


using basic::Error;
using basic::Warning;

//static basic::Tracer TR("core.fragment.ConstantLengthFragments.cxxtest");

using namespace core;
using namespace fragment;


static basic::Tracer Debug("protocols.simple_moves.WobbleMover.cxxtest", basic::t_debug );

class WobbleMoverTest : public CxxTest::TestSuite
{
	pose::Pose pose_;
public:
	WobbleMoverTest() {};

	// Shared initialization goes here.
	void setUp() {
		core_init();
		core::import_pose::pose_from_file( pose_, "protocols/abinitio/2GB3.pdb" , core::import_pose::PDB_file);

		fragset3mer_ = utility::pointer::make_shared< ConstantLengthFragSet >( 3 );
		fragset3mer_->read_fragment_file( "protocols/abinitio/mfr_aa2GB3_03_05.200_v1_3" );

	}

	void test_wobble();

	// Shared finalization goes here.
	void tearDown() {
	}

private:
	ConstantLengthFragSetOP fragset3mer_;
};


void WobbleMoverTest::test_wobble() {
	using namespace pose;
	using namespace fragment;
	using namespace protocols;
	using namespace simple_moves;
	using namespace scoring;
	kinematics::MoveMapOP movemap( new kinematics::MoveMap ); //dummy ( functionality not used yet )
	//Size len (3);
	movemap->set_bb( true );
	// for (Size ii=1; ii<=6; ii++ ) {
	// movemap->set_bb( ii, false );
	// movemap->set_bb( pose_.size()-ii, false );
	// TS_ASSERT( !movemap->get_bb( ii ) );
	//}
	Pose pose = pose_;
	protocols::simple_moves::WobbleMover wobbles( fragset3mer_, movemap, utility::pointer::make_shared< GunnCost >( 7.0 ) );
	// moves::TrialMover wobble_min_trial

#if 0
	kinematics::MoveMapOP hardly_moves( new kinematics::MoveMap );
	hardly_moves->set_bb( pose_.size(), true );
	protocols::minimization_packing::MinMover minimize(
		hardly_moves,
		get_score_function(),
		"dfpmin",
		0.01, /* tolerance */
		true /* use nb_list */
	);
#endif

	// pose.dump_pdb( "wobble_0" );
	// pose.dump_pdb( "mini_0" );
	for ( Size i=1; i<= 100; i++ ) {
		Debug << " ----------------------------- " << i << " -------------------------------- " << std::endl;
		wobbles.apply( pose );
		//  pose.dump_pdb( "wobble_"+string_of( i ) );
		// minimize.apply( pose );
		// pose.dump_pdb( "mini_"+string_of( i ) );
		pose=pose_;
	}

	/*
	T(TR,100) << "25" << " " << i << " " << data.q1 << " " << data.q2 << " " << data.q3<< " " << data.q4<< " " << data.q5<< " " << data.q6 << "\n";
	*/
	/*
	TS_ASSERT_DELTA( data.q1, -0.7429, 0.001 );
	TS_ASSERT_DELTA( data.q2, -0.6831, 0.001 );
	TS_ASSERT_DELTA( data.q3,  0.9926, 0.001 );
	TS_ASSERT_DELTA( data.q4,  0.7593, 0.001 );
	TS_ASSERT_DELTA( data.q5,  0.5410, 0.001 );
	TS_ASSERT_DELTA( data.q6,  5.9096, 0.001 );
	*/
}
