// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file   core/scoring/constaints/ConstantConstraint.hh.cxxtest.hh
/// @brief  test suite for ConstantConstraint
/// @author Andrew Leaver-Fay (aleaverfay@gmail.com)

// Test headers
#include <cxxtest/TestSuite.h>

// basic headers
#include <basic/Tracer.hh>

// unit headers
#include <core/scoring/constraints/ConstantConstraint.hh>

#ifdef SERIALIZATION
#include <core/id/AtomID.hh>
#include <core/scoring/func/HarmonicFunc.hh>

// Cereal headers
#include <cereal/archives/binary.hpp>
#include <cereal/types/polymorphic.hpp>
#endif


class ConstantConstraintTests : public CxxTest::TestSuite
{
public:


	void test_serialize_ConstantConstraint() {
		TS_ASSERT( true ); // for non-serialization builds
#ifdef SERIALIZATION
		using namespace core::scoring::constraints;
		using namespace core::scoring::func;
		using namespace core::id;

		FuncOP some_func( new HarmonicFunc( 1, 2 ));
		ConstraintOP instance( new ConstantConstraint( some_func ) ); // serialize this through a pointer to the base class

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

		// make sure the deserialized base class pointer points to a ConstantConstraint
		TS_ASSERT( utility::pointer::dynamic_pointer_cast< ConstantConstraint > ( instance2 ));
		TS_ASSERT( *instance == *instance2 );
#endif // SERIALIZATION
	}

};

