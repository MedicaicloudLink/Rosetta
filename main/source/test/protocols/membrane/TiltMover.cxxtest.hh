// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @brief   Unit test for TiltMover
/// @author  JKLeman (julia.koehler1982@gmail.com)

// Test Headers
#include <cxxtest/TestSuite.h>
#include <test/core/init_util.hh>

// Unit Headers
#include <core/conformation/Conformation.hh>
#include <core/conformation/membrane/SpanningTopology.hh>
#include <core/conformation/membrane/MembraneInfo.hh>
#include <protocols/membrane/geometry/EmbeddingDef.hh>
#include <protocols/membrane/util.hh>
#include <protocols/membrane/AddMembraneMover.hh>
#include <protocols/membrane/TiltMover.hh>
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

using namespace core;
using namespace core::pose;
using namespace core::conformation;
using namespace core::conformation::membrane;
using namespace protocols::membrane;
using namespace protocols::membrane::geometry;

static basic::Tracer TR("protocols.membrane.TiltMover.cxxtest");

class TiltMoverTest : public CxxTest::TestSuite {

public: // test functions

	/// Test Setup Functions ////////

	/// @brief Setup Test
	void setUp(){

		// Initialize
		core_init();

		// Load in pose from pdb
		pose_ = Pose();
		core::import_pose::pose_from_file( pose_, "protocols/membrane/1C17_tr.pdb" , core::import_pose::PDB_file);

		// Initialize Spans from spanfile
		std::string spanfile = "protocols/membrane/1C17_tr.span";

		// AddMembraneMover
		AddMembraneMoverOP add_mem( new AddMembraneMover( spanfile ) );
		add_mem->apply( pose_ );

	}

	/// @brief Standard Tear Down
	void tearDown() {}

	///// Test Methods /////////////

	////////////////////////////////////////////////////////////////////////////////

	// test constructor from jumpnum and angle
	void test_constructor_from_jumpnum_and_angle () {

		TR << "\n\n========== TESTING CONSTUCTOR FROM JUMP NUMBER AND ANGLE" << std::endl;

		// jump = 1
		// angle = -43
		// axis = perpendicular to axis connecting protein embedding centers

		// compute downstream empedding
		SpanningTopologyOP topo = pose_.conformation().membrane_info()->spanning_topology();
		SpanningTopologyOP topo_up_( new SpanningTopology() );
		SpanningTopologyOP topo_down_( new SpanningTopology() );

		// show foldtree and membrane info
		pose_.fold_tree().show( TR );
		pose_.conformation().membrane_info()->show( TR );

		// split_topology_by_jump_noshift
		split_topology_by_jump_noshift( pose_, 1, topo, topo_up_, topo_down_ );

		// compute embedding for downstream partner
		EmbeddingDefOP emb_down( compute_structure_based_embedding( pose_, *topo_down_ ) );

		// define vectors
		Vector cntr_before(18.6088, 11.9908, -0.248);
		Vector norm_before(0.0076786, 0.0858815, 0.996276);
		Vector cntr_after(18.6087, 11.9908, -0.248);
		Vector norm_after(0.566798, 0.44631, 0.692494);

		// compare before
		TS_ASSERT( position_equal_within_delta( emb_down->center(), cntr_before, 0.001 ) );
		TS_ASSERT( position_equal_within_delta( emb_down->normal(), norm_before, 0.001 ) );

		// create and run TiltMover
		TiltMoverOP tilt( new TiltMover( 1, -43) );
		tilt->apply( pose_ );
		pose_.dump_pdb("protocols/membrane/test_out.pdb");

		// compute embedding for downstream partner
		EmbeddingDefOP emb_down1( compute_structure_based_embedding( pose_, *topo_down_ ) );

		// compare after
		TS_ASSERT( position_equal_within_delta( emb_down1->center(), cntr_after, 0.001 ) );
		TS_ASSERT( position_equal_within_delta( emb_down1->normal(), norm_after, 0.001 ) );

	}

	void test_tiltmover_parse_my_tag_input() {
		std::string tag_string = "<TiltMover jump_num=3 random_angle=true angle=-5/>";
		std::stringstream ss( tag_string );
		utility::tag::TagOP tag( new utility::tag::Tag() );

		basic::datacache::DataMap dm;
		protocols::filters::Filters_map fm;
		protocols::moves::Movers_map mm;
		core::pose::Pose pose;

		tag->read( ss );

		TiltMoverOP xtilt( new TiltMover() );
		xtilt->parse_my_tag( tag, dm, fm, mm, pose );

		TS_ASSERT(xtilt->get_jump_num() == 3);
		TS_ASSERT(xtilt->get_random_angle() == true);
		TS_ASSERT(xtilt->get_angle() == -5);

	}

	////////////////////////////////////////////////////////////////////////////////

	/// @brief Position equal within delta (helper method)
	bool position_equal_within_delta( Vector a, Vector b, Real delta ) {

		TS_ASSERT_DELTA( a.x(), b.x(), delta );
		TS_ASSERT_DELTA( a.y(), b.y(), delta );
		TS_ASSERT_DELTA( a.z(), b.z(), delta );

		return true;
	}

private: // data

	core::pose::Pose pose_;

};
