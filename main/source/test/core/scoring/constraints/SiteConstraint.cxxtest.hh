// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file   core/scoring/constaints/SiteConstraint.cxxtest.hh
/// @brief  test for SiteConstraint
/// @author Brian Weitzner

// Test headers
#include <cxxtest/TestSuite.h>

#include <test/core/init_util.hh>
#include <test/util/pose_funcs.hh>

#include <core/types.hh>
#include <core/scoring/EnergyMap.hh>
#include <core/scoring/constraints/SiteConstraint.hh>
#include <core/scoring/func/FlatHarmonicFunc.hh>
#include <core/scoring/func/XYZ_Func.hh>
#include <basic/Tracer.hh>

//Auto Headers
#include <utility/vector1.hh>

#ifdef SERIALIZATION
// Cereal headers
#include <cereal/archives/binary.hpp>
#include <cereal/types/polymorphic.hpp>
#endif


using basic::Error;
using basic::Warning;

static basic::Tracer TR("core.scoring.constraints.SiteConstraint.cxxtest");

using namespace core;
using namespace core::scoring;
using namespace core::scoring::constraints;
class SiteConstraintTests : public CxxTest::TestSuite
{
public:
	SiteConstraintTests() {}

	pose::PoseOP the_pose;
	core::scoring::func::FlatHarmonicFuncOP func;
	std::string name;

	// Shared initialization goes here.
	void setUp() {
		core_init();
		the_pose = utility::pointer::make_shared< pose::Pose >();
		core::import_pose::centroid_pose_from_pdb( *the_pose, "protocols/scoring/dock_in.pdb" );
		func = utility::pointer::make_shared< core::scoring::func::FlatHarmonicFunc >( 0.0, 1.0, 5.0 );
		name = "CA";
	}

	// Shared finalization goes here.
	void tearDown() {
		the_pose.reset();
		func.reset();
		name = "";
	}

	void test_site_constraint_near_interface() {

		Size res = 192;
		SiteConstraintOP site_cst( new SiteConstraint() );
		site_cst->setup_csts( res, name, "I", *the_pose, func );

		EnergyMap weights, emap;
		core::scoring::func::ConformationXYZ xyz_func( the_pose->conformation() );

		weights[ atom_pair_constraint ] = 1.0;
		site_cst->score( xyz_func, weights, emap );

		Size before_precision = std::cout.precision();
		std::cout.precision( 16 );

		TS_ASSERT_DELTA( emap[ atom_pair_constraint ],   0.000000, 1e-6 );

		std::cout.precision( before_precision );

	}
	void test_site_constraint_far_from_interface() {

		Size res = 3;
		Real distance = 26.71618914; // measured in pdb file
		SiteConstraintOP site_cst( new SiteConstraint() );
		site_cst->setup_csts( res, name, "I", *the_pose, func );

		EnergyMap weights, emap;
		weights[ atom_pair_constraint ] = 1.0;
		core::scoring::func::ConformationXYZ xyz_func( the_pose->conformation() );
		site_cst->score( xyz_func, weights, emap );

		Size before_precision = std::cout.precision();
		std::cout.precision( 16 );
		//std::cout << "Best score from site constraint: " << emap[ atom_pair_constraint ] << std::endl;
		//std::cout << "What I think the score should be:" << func->func( distance ) << std::endl;
		TS_ASSERT_DELTA( emap[ atom_pair_constraint ],   func->func( distance ), 1e-6 );

		std::cout.precision( before_precision );

	}

	void test_serialize_SiteConstraint() {
		TS_ASSERT( true ); // for non-serialization builds
#ifdef SERIALIZATION
		using namespace core::scoring::constraints;

		Size res = 192;
		SiteConstraintOP site_cst( new SiteConstraint() );
		site_cst->setup_csts( res, name, "I", *the_pose, func );

		ConstraintOP instance( site_cst ); // serialize this through a pointer to the base class

		std::ostringstream oss;
		{
			cereal::BinaryOutputArchive arc( oss );
			arc( instance );
		}

		ConstraintOP instance2; // deserialize also through a pointer to the base class
		std::istringstream iss( oss.str() );
		{
			cereal::BinaryInputArchive arc( iss );
			arc( instance2 );
		}

		// make sure the deserialized base class pointer points to a SiteConstraint
		TS_ASSERT( utility::pointer::dynamic_pointer_cast< SiteConstraint > ( instance2 ));
		TS_ASSERT( *instance == *instance2 );
#endif // SERIALIZATION
	}

};
