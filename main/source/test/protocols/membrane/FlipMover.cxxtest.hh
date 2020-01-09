// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file   protocols/membrane/geometry/MembraneGeometryUtil.cxxtest.hh
/// @brief   Unit test for util functions
/// @author  JKLeman (julia.koehler1982@gmail.com)

// Test Headers
#include <cxxtest/TestSuite.h>
#include <test/core/init_util.hh>

// Unit Headers
#include <core/conformation/Residue.hh>
#include <core/conformation/membrane/SpanningTopology.hh>
#include <protocols/membrane/geometry/EmbeddingDef.hh>
#include <protocols/membrane/geometry/Embedding.hh>
#include <protocols/membrane/util.hh>
#include <protocols/membrane/AddMembraneMover.hh>
#include <protocols/membrane/FlipMover.hh>
#include <core/pose/Pose.hh>
#include <core/import_pose/import_pose.hh>

// Package Headers
#include <core/types.hh>

// Utility Headers
#include <basic/Tracer.hh>
#include <core/conformation/membrane/Exceptions.hh>
#include <numeric/xyzVector.hh>
#include <utility/vector1.hh>
#include <utility/exit.hh>
#include <utility/tag/Tag.hh>
#include <basic/datacache/DataMap.hh>

// C++ Headers
#include <cstdlib>
#include <string>
#include <cmath>

static basic::Tracer TR("protocols.membrane.FlipMover.cxxtest");

using namespace core;
using namespace core::pose;
using namespace core::conformation;
using namespace core::conformation::membrane;
using namespace protocols::membrane;
using namespace protocols::membrane::geometry;

class FlipMoverTest : public CxxTest::TestSuite {

public: // test functions

	/// Test Setup Functions ////////

	/// @brief Setup Test
	void setUp(){

		// Initialize
		core_init();

		// Load in pose from pdb
		pose_ = utility::pointer::make_shared< Pose >();
		core::import_pose::pose_from_file( *pose_, "protocols/membrane/1AFO_AB.pdb" , core::import_pose::PDB_file);

		// Initialize Spans from spanfile
		std::string spanfile = "protocols/membrane/geometry/1AFO_AB.span";

		// AddMembraneMover
		AddMembraneMoverOP add_mem( new AddMembraneMover( spanfile ) );
		add_mem->apply( *pose_ );

	}

	/// @brief Standard Tear Down
	void tearDown() {}

	///// Test Methods /////////////

	////////////////////////////////////////////////////////////////////////////////

	// test default constructor
	void test_default_constructor () {

		TR << "\n\n========== TESTING DEFAULT CONSTRUCTOR" << std::endl;

		// define vectors
		Vector res1(-17.655, -4.816, -4.520);
		Vector res40(-3.575, -9.497, 26.734);
		Vector res41(6.154, 6.641, 5.600);
		Vector res80(4.693, 10.910, -24.577);

		// create and run FlipMover
		FlipMoverOP flip( new FlipMover() );
		flip->apply( *pose_ );

		// compare
		TS_ASSERT( position_equal_within_delta( pose_->residue(1).xyz("CA"), res1, 0.001 ) );
		TS_ASSERT( position_equal_within_delta( pose_->residue(40).xyz("CA"), res40, 0.001 ) );
		TS_ASSERT( position_equal_within_delta( pose_->residue(41).xyz("CA"), res41, 0.001 ) );
		TS_ASSERT( position_equal_within_delta( pose_->residue(80).xyz("CA"), res80, 0.001 ) );

	}

	////////////////////////////////////////////////////////////////////////////////

	// test constructor from jumpnum
	void test_constructor_from_jumpnum () {

		TR << "\n\n========== TESTING CONSTRUCTOR FROM JUMP NUMBER" << std::endl;

		// define vectors
		Vector res1(-17.655, -4.816, -4.520);
		Vector res40(-3.575, -9.497, 26.734);
		Vector res41(6.154, 6.641, 5.600);
		Vector res80(4.693, 10.910, -24.577);

		// create and run FlipMover
		FlipMoverOP flip( new FlipMover( 1 ) );
		flip->apply( *pose_ );

		// compare
		TS_ASSERT( position_equal_within_delta( pose_->residue(1).xyz("CA"), res1, 0.001 ) );
		TS_ASSERT( position_equal_within_delta( pose_->residue(40).xyz("CA"), res40, 0.001 ) );
		TS_ASSERT( position_equal_within_delta( pose_->residue(41).xyz("CA"), res41, 0.001 ) );
		TS_ASSERT( position_equal_within_delta( pose_->residue(80).xyz("CA"), res80, 0.001 ) );

	}

	////////////////////////////////////////////////////////////////////////////////

	// test constructor from jumpnum for membrane jump
	void test_constructor_from_jumpnum_mem () {

		TR << "\n\n========== TESTING CONSTUCTOR FROM JUMP NUMBER FOR MEMBRANE JUMP" << std::endl;

		// define vectors
		Vector res1(-17.655, 4.609, 3.761);
		Vector res40(-3.575, 9.290, -27.493);
		Vector res41(2.974, -7.711, 6.866);
		Vector res80(9.846, -11.919, -22.564);

		// create and run FlipMover
		FlipMoverOP flip( new FlipMover( 2 ) );
		flip->apply( *pose_ );

		// compare
		TS_ASSERT( position_equal_within_delta( pose_->residue(1).xyz("CA"), res1, 0.001 ) );
		TS_ASSERT( position_equal_within_delta( pose_->residue(40).xyz("CA"), res40, 0.001 ) );
		TS_ASSERT( position_equal_within_delta( pose_->residue(41).xyz("CA"), res41, 0.001 ) );
		TS_ASSERT( position_equal_within_delta( pose_->residue(80).xyz("CA"), res80, 0.001 ) );

	}

	////////////////////////////////////////////////////////////////////////////////

	// test constructor from jumpnum and axis
	void test_constructor_from_jumpnum_and_axis () {

		TR << "\n\n========== TESTING CONSTRUCTOR FROM JUMP NUMBER AND AXIS" << std::endl;

		// define vectors
		Vector res1(-17.655, -4.816, -4.520);
		Vector res40(-3.575, -9.497, 26.734);
		Vector res41(0.925, 7.504, 6.323);
		Vector res80(-5.947, 11.712, -23.107);

		// create and run FlipMover
		Vector axis(0, 1, 0);
		FlipMoverOP flip( new FlipMover( 1, axis ) );
		flip->apply( *pose_ );

		// compare
		TS_ASSERT( position_equal_within_delta( pose_->residue(1).xyz("CA"), res1, 0.001 ) );
		TS_ASSERT( position_equal_within_delta( pose_->residue(40).xyz("CA"), res40, 0.001 ) );
		TS_ASSERT( position_equal_within_delta( pose_->residue(41).xyz("CA"), res41, 0.001 ) );
		TS_ASSERT( position_equal_within_delta( pose_->residue(80).xyz("CA"), res80, 0.001 ) );

	}

	////////////////////////////////////////////////////////////////////////////////

	// test constructor from jumpnum and axis, mem jump
	void test_constructor_from_jumpnum_and_axis_mem () {

		TR << "\n\n========== TESTING CONSTRUCTOR FROM JUMP NUMBER AND AXIS FOR MEMBRANE JUMP" << std::endl;

		// define vectors
		Vector res1(17.621, -4.816, 3.760);
		Vector res40(3.541, -9.497, -27.494);
		Vector res41(-3.008, 7.504, 6.867);
		Vector res80(-9.880, 11.712, -22.563);

		// create and run FlipMover
		Vector axis(0, 1, 0);
		FlipMoverOP flip( new FlipMover( 2, axis ) );
		flip->apply( *pose_ );

		// compare
		TS_ASSERT( position_equal_within_delta( pose_->residue(1).xyz("CA"), res1, 0.001 ) );
		TS_ASSERT( position_equal_within_delta( pose_->residue(40).xyz("CA"), res40, 0.001 ) );
		TS_ASSERT( position_equal_within_delta( pose_->residue(41).xyz("CA"), res41, 0.001 ) );
		TS_ASSERT( position_equal_within_delta( pose_->residue(80).xyz("CA"), res80, 0.001 ) );

	}

	////////////////////////////////////////////////////////////////////////////////

	// test constructor from jumpnum and angle
	void test_constructor_from_jumpnum_and_angle () {

		TR << "\n\n========== TESTING CONSTRUCTOR FROM JUMP NUMBER AND ANGLE" << std::endl;

		// define vectors
		Vector res1(-17.655, -4.816, -4.520);
		Vector res40(-3.575, -9.497, 26.734);
		Vector res41(7.348, 4.785, -6.798);
		Vector res80(-4.714, 20.529, 16.391);

		// create and run FlipMover
		FlipMoverOP flip( new FlipMover(1, 45) );
		flip->apply( *pose_ );

		// compare
		TS_ASSERT( position_equal_within_delta( pose_->residue(1).xyz("CA"), res1, 0.001 ) );
		TS_ASSERT( position_equal_within_delta( pose_->residue(40).xyz("CA"), res40, 0.001 ) );
		TS_ASSERT( position_equal_within_delta( pose_->residue(41).xyz("CA"), res41, 0.001 ) );
		TS_ASSERT( position_equal_within_delta( pose_->residue(80).xyz("CA"), res80, 0.001 ) );

	}

	////////////////////////////////////////////////////////////////////////////////

	// test constructor from jumpnum and angle
	void test_constructor_from_jumpnum_and_angle_mem () {

		TR << "\n\n========== TESTING CONSTRUCTOR FROM JUMP NUMBER AND ANGLE FOR MEMBRANE JUMP" << std::endl;

		// define vectors
		Vector res1(-17.655, -0.508, -6.640);
		Vector res40(-3.575, -25.918, 12.150);
		Vector res41(2.974, 10.400, -0.124);
		Vector res80(9.846, -7.435, 23.661);

		// create and run FlipMover
		FlipMoverOP flip( new FlipMover(2, 45) );
		flip->apply( *pose_ );

		// compare
		TS_ASSERT( position_equal_within_delta( pose_->residue(1).xyz("CA"), res1, 0.001 ) );
		TS_ASSERT( position_equal_within_delta( pose_->residue(40).xyz("CA"), res40, 0.001 ) );
		TS_ASSERT( position_equal_within_delta( pose_->residue(41).xyz("CA"), res41, 0.001 ) );
		TS_ASSERT( position_equal_within_delta( pose_->residue(80).xyz("CA"), res80, 0.001 ) );

	}

	////////////////////////////////////////////////////////////////////////////////

	// test constructor from jumpnum, axis and angle
	void test_constructor_from_jumpnum_axis_angle () {

		TR << "\n\n========== TESTING CONSTRUCTOR FROM JUMP NUMBER, AXIS AND ANGLE" << std::endl;

		// define vectors
		Vector res1(-17.655, -4.816, -4.520);
		Vector res40(-3.575, -9.497, 26.734);
		Vector res41(-2.258, 7.504, -6.308);
		Vector res80(23.412, 11.712, 9.643);

		// create and run FlipMover
		Vector axis(0, 1, 0);
		FlipMoverOP flip( new FlipMover(1, axis, 45) );
		flip->apply( *pose_ );

		// compare
		TS_ASSERT( position_equal_within_delta( pose_->residue(1).xyz("CA"), res1, 0.001 ) );
		TS_ASSERT( position_equal_within_delta( pose_->residue(40).xyz("CA"), res40, 0.001 ) );
		TS_ASSERT( position_equal_within_delta( pose_->residue(41).xyz("CA"), res41, 0.001 ) );
		TS_ASSERT( position_equal_within_delta( pose_->residue(80).xyz("CA"), res80, 0.001 ) );

	}
	////////////////////////////////////////////////////////////////////////////////

	// test constructor from jumpnum, axis and angle
	void test_constructor_from_jumpnum_axis_angle_mem () {

		TR << "\n\n========== TESTING CONSTRUCTOR FROM JUMP NUMBER, AXIS AND ANGLE FOR MEMBRANE JUMP" << std::endl;

		// define vectors
		Vector res1(-15.417, -4.816, 9.165);
		Vector res40(16.639, -9.497, 21.308);
		Vector res41(-3.026, 7.504, -7.619);
		Vector res80(22.643, 11.712, 8.332);

		// create and run FlipMover
		Vector axis(0, 1, 0);
		FlipMoverOP flip( new FlipMover(2, axis, 45) );
		flip->apply( *pose_ );

		// compare
		TS_ASSERT( position_equal_within_delta( pose_->residue(1).xyz("CA"), res1, 0.001 ) );
		TS_ASSERT( position_equal_within_delta( pose_->residue(40).xyz("CA"), res40, 0.001 ) );
		TS_ASSERT( position_equal_within_delta( pose_->residue(41).xyz("CA"), res41, 0.001 ) );
		TS_ASSERT( position_equal_within_delta( pose_->residue(80).xyz("CA"), res80, 0.001 ) );

	}

	////////////////////////////////////////////////////////////////////////////////

	/// @brief Position equal within delta (helper method)
	bool position_equal_within_delta( Vector a, Vector b, Real delta ) {

		TS_ASSERT_DELTA( a.x(), b.x(), delta );
		TS_ASSERT_DELTA( a.y(), b.y(), delta );
		TS_ASSERT_DELTA( a.z(), b.z(), delta );

		return true;
	}

	////////////////////////////////////////////////////////////////////////////////

	/// @brief test parse_my_tag() read-in
	void test_flipmover_parse_my_tag_input() {
		std::stringstream tag_ss("<FlipMover jump_num=3 axisx=1 axisy=0 axisz=1 angle=45.5 random_angle=true max_angle_dev=12.0/>");
		utility::tag::TagCOP tag = utility::tag::Tag::create( tag_ss );

		basic::datacache::DataMap dm;
		protocols::filters::Filters_map fm;
		protocols::moves::Movers_map mm;
		core::pose::Pose pose;

		FlipMoverOP xflip( new FlipMover() );
		xflip->parse_my_tag( tag, dm, fm, mm, pose );

		TS_ASSERT(xflip->get_jump_num() == 3);

		numeric::xyzVector< int > v1 = xflip->get_axis();
		TS_ASSERT_EQUALS(v1.x(), 1);
		TS_ASSERT_EQUALS(v1.y(), 0);
		TS_ASSERT_EQUALS(v1.z(), 1);

		TS_ASSERT_EQUALS(xflip->get_angle(), 45.5);
		TS_ASSERT_EQUALS(xflip->get_random_angle(), true);
		TS_ASSERT_EQUALS(xflip->get_max_angle_dev(), 12.0);
	}



private: // data

	core::pose::PoseOP pose_;

};
