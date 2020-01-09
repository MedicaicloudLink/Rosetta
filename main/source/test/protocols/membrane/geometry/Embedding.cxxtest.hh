// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file   protocols/membrane/geometry/Embedding.cxxtest.hh
/// @brief   Unit test for Embedding class
/// @author  JKLeman (julia.koehler1982@gmail.com)
/// @author  Modified by Rebecca Alford (rfalford12@gmail.com)

// Test Headers
#include <cxxtest/TestSuite.h>
#include <test/core/init_util.hh>

// Unit Headers
#include <core/conformation/Conformation.hh>
#include <core/conformation/membrane/SpanningTopology.hh>
#include <core/conformation/membrane/Span.hh>
#include <core/conformation/membrane/MembraneInfo.hh>
#include <protocols/membrane/geometry/EmbeddingDef.hh>
#include <protocols/membrane/geometry/Embedding.hh>
#include <protocols/membrane/util.hh>
#include <core/pose/Pose.hh>
#include <core/import_pose/import_pose.hh>

// Package Headers
#include <core/types.hh>

// Utility Headers
#include <basic/Tracer.hh>
#include <core/conformation/membrane/Exceptions.hh>
#include <core/pose/util.hh>
#include <utility/vector1.hh>
#include <utility/exit.hh>

// C++ Headers
#include <cstdlib>
#include <string>
#include <cmath>

static basic::Tracer TR("protocols.membrane.geometry.Embedding.cxxtest");

using namespace core;
using namespace core::pose;
using namespace core::conformation;
using namespace core::conformation::membrane;
using namespace protocols::membrane;
using namespace protocols::membrane::geometry;

class EmbeddingTest : public CxxTest::TestSuite {

public: // test functions

	/// Test Setup Functions ////////

	/// @brief Setup Test
	void setUp(){

		// Initialize
		core_init();

	}

	/// @brief Standard Tear Down
	void tearDown() {}

	///// Test Methods /////////////

	////////////////////////////////////////////////////////////////////////////////

	// constructor from topology and radius
	void test_constructor_from_topo_and_radius() {

		TR << "Test constructor from topology and radius" << std::endl;

		SpanningTopologyOP topology( new SpanningTopology() );

		topology->add_span( 5, 15);
		topology->add_span(25, 35);
		topology->add_span(45, 55);
		topology->add_span(65, 75);
		Real radius(10);

		// create object using constructor
		EmbeddingOP embed( new Embedding( *topology, radius ) );
		embed->show( TR );

		// create vectors for centers and normal
		Vector center1( 0, 10, 0); // for spans
		Vector center2(-10, 0, 0);
		Vector center3( 0,-10, 0);
		Vector center4( 10, 0, 0);

		Vector center(0, 0, 0);  // for overall embedding
		Vector normal(0, 0, 1);

		// check centers on y, -x, -y, x axis and normals at 0, 0, 1
		TS_ASSERT( position_equal_within_delta( embed->embedding(1)->center(), center1, 0.001 ) );
		TS_ASSERT( position_equal_within_delta( embed->embedding(1)->normal(), normal, 0.001 ) );

		TS_ASSERT( position_equal_within_delta( embed->embedding(2)->center(), center2, 0.001 ) );
		TS_ASSERT( position_equal_within_delta( embed->embedding(2)->normal(), -normal, 0.001 ) );

		TS_ASSERT( position_equal_within_delta( embed->embedding(3)->center(), center3, 0.001 ) );
		TS_ASSERT( position_equal_within_delta( embed->embedding(3)->normal(), normal, 0.001 ) );

		TS_ASSERT( position_equal_within_delta( embed->embedding(4)->center(), center4, 0.001 ) );
		TS_ASSERT( position_equal_within_delta( embed->embedding(4)->normal(), -normal, 0.001 ) );

		TS_ASSERT( position_equal_within_delta( embed->total_embed()->center(), center, 0.001 ) );
		TS_ASSERT( position_equal_within_delta( embed->total_embed()->normal(), normal, 0.001 ) );

	}

	// constructor from topology and pose
	void test_constructor_from_topo_and_pose() {

		TR << "Test constructor from topology and pose" << std::endl;

		// read in pose
		Pose pose;
		core::import_pose::pose_from_file( pose, "protocols/membrane/geometry/1AFO_AB.pdb" , core::import_pose::PDB_file);

		// create topology object
		std::map< std::string, core::Size > pdb2pose_map = core::pose::get_pdb2pose_numbering_as_stdmap( pose );
		SpanningTopologyOP topology( new SpanningTopology("protocols/membrane/geometry/1AFO_AB.span", pdb2pose_map ) );

		// create object using constructor
		EmbeddingOP embed( new Embedding( *topology, pose ) );
		embed->show( TR );

		// create vectors for centers and normals
		Vector center1(-1.9835, -3.184, -0.108);  // for spans
		Vector normal1(-0.08249, -0.10743, 0.99078);
		Vector center2(1.9495, 2.977, -0.6515);
		Vector normal2(-0.03305, 0.50658, 0.861558);

		Vector center(-0.017, -0.1035, -0.37975);  // for overall embedding
		Vector normal(-0.06086, 0.210257, 0.97575);

		// check positions of vectors
		// also tests getter embedding(span) and total_embed()
		TS_ASSERT( position_equal_within_delta( embed->embedding(1)->center(), center1, 0.001 ) );
		TS_ASSERT( position_equal_within_delta( embed->embedding(1)->normal(), normal1, 0.001 ) );

		TS_ASSERT( position_equal_within_delta( embed->embedding(2)->center(), center2, 0.001 ) );
		TS_ASSERT( position_equal_within_delta( embed->embedding(2)->normal(), normal2, 0.001 ) );

		TS_ASSERT( position_equal_within_delta( embed->total_embed()->center(), center, 0.001 ) );
		TS_ASSERT( position_equal_within_delta( embed->total_embed()->normal(), normal, 0.001 ) );

		// test embeddings() getter
		TS_ASSERT( position_equal_within_delta( embed->embeddings()[1]->center(), center1, 0.001 ) );
		TS_ASSERT( position_equal_within_delta( embed->embeddings()[2]->center(), center2, 0.001 ) );

	}

	////////////////////////////////////////////////////////////////////////////////


	/// @brief Position equal within delta (helper method)
	bool position_equal_within_delta( Vector a, Vector b, Real delta ) {

		TS_ASSERT_DELTA( a.x(), b.x(), delta );
		TS_ASSERT_DELTA( a.y(), b.y(), delta );
		TS_ASSERT_DELTA( a.z(), b.z(), delta );

		return true;
	}
};
