// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file   core/scoring/constaints/BackboneStubConstraint.hh.cxxtest.hh
/// @brief  test suite for BackboneStubConstraint
/// @author Andrew Leaver-Fay (aleaverfay@gmail.com)

// Test headers
#include <cxxtest/TestSuite.h>
#include <test/core/init_util.hh>

// basic headers
#include <basic/Tracer.hh>

// unit headers
#include <core/scoring/constraints/BackboneStubConstraint.hh>

#ifdef SERIALIZATION
#include <core/id/AtomID.hh>
#include <core/scoring/func/HarmonicFunc.hh>
#include <core/pose/Pose.hh>
#include <test/util/pose_funcs.hh>

// Cereal headers
#include <cereal/archives/binary.hpp>
#include <cereal/types/polymorphic.hpp>
#endif


class BackboneStubConstraintTests : public CxxTest::TestSuite
{
public:

	void setUp() {
		core_init();
	}

	void test_serialize_BackboneStubConstraint() {
		TS_ASSERT( true ); // for non-serialization builds
#ifdef SERIALIZATION
		using namespace core::scoring::constraints;
		using namespace core::scoring::func;
		using namespace core::pose;
		using namespace core::id;

		Pose trpcage = create_trpcage_ideal_pose();
		// serialize this through a pointer to the base class
		ConstraintOP instance( new BackboneStubConstraint( trpcage, 5, AtomID( 1, 1 ), trpcage, 10, 1.3, 2.6 ) );

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

		// make sure the deserialized base class pointer points to a BackboneStubConstraint
		TS_ASSERT( utility::pointer::dynamic_pointer_cast< BackboneStubConstraint > ( instance2 ));
		TS_ASSERT( *instance == *instance2 );
#endif // SERIALIZATION
	}

};

