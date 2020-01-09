// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file  protocols/sewing/data_storage/SmartAssemblyTests.cxxtest.hh
/// @brief  Test sewing's SmartAssembly class
/// @author Minnie Langlois (minnie@email.unc.edu)


// Test headers
#include <test/UMoverTest.hh>
#include <test/UTracer.hh>
#include <cxxtest/TestSuite.h>

// Project Headers
#include <test/protocols/sewing/extra_functions.hh>
#include <protocols/sewing/data_storage/SmartAssembly.hh>
// Protocol Headers
#include <basic/Tracer.hh>
using namespace protocols::sewing;

static basic::Tracer TR("LigandScorerTests");


class LigandScorerTests : public CxxTest::TestSuite {
	//Define Variables
private:
	data_storage::SmartAssemblyOP assembly_;
	//hashing::SegmentVectorOP segment_vector_;
	core::pose::Pose pose_;
public:
	void setUp(){
		core_init();
		segment_vector_ = hashing::ModelFileReader::read_model_file( "protocols/sewing/inputs/test.segments" );
		assembly_ = utility::pointer::make_shared< data_storage::SmartAssembly >( segment_vector_ );
		core::pose::Pose pose_;
		core::import_pose::pose_from_file( pose_, "protocols/sewing/inputs/helix_with_zinc.pdb" );
	}
	void tearDown(){
		assembly_ = utility::pointer::make_shared< data_storage::SmartAssembly >( segment_vector_ );
	}

	void test_ligand_assembly_scorer(){
		//Call add_pose_segments_to_segment_vector
		assembly_ = sewing_testing::create_assembly_from_starting_segment( assembly_, assembly->pdb_segments().at( 1 ) );
		TS_ASSERT( true );
	}

};
