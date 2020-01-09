// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file   core/scoring/func/GaussianFunc.cxxtest.hh
/// @brief  test suite for GaussianFunc function
/// @author James Thompson
/// @author modified Apr 23 2008 by Sergey Lyskov: rewriting to use UTracer

// Test headers
#include <cxxtest/TestSuite.h>

#include <test/core/init_util.hh>


#include <core/scoring/func/GaussianFunc.hh>
#include <core/scoring/func/GaussianFunc.fwd.hh>

#include <core/types.hh>

#include <test/UTracer.hh>

//Auto Headers
#include <utility/vector1.hh>

#ifdef SERIALIZATION
// Cereal headers
#include <cereal/archives/binary.hpp>
#include <cereal/types/polymorphic.hpp>
#endif


using basic::Error;
using basic::Warning;

static basic::Tracer TR("core.scoring.constraints.GaussianFunc.cxxtest");

using namespace core;

class GaussianFuncTests : public CxxTest::TestSuite
{

public:
	GaussianFuncTests() {};

	// Shared initialization goes here.
	void setUp() {
		core_init();
	}

	// Shared finalization goes here.
	void tearDown() {

	}

	///////////////////////////////////////////////////////////////////////////////
	// ------------------------------------------ //
	/// @brief simple test minimization
	void test_gaussian_func()
	{
		using namespace core::scoring::func;

		// UTracer log file
		test::UTracer UT("core/scoring/func/GaussianFunc.u");

		GaussianFuncOP func( new GaussianFunc( 5.16, 1.5 ) );

		float const TOLERATED_ERROR = 0.001;
		const core::Real start = 2;
		const core::Real end   = 20;
		const core::Real res   = 0.5;

		UT.abs_tolerance( TOLERATED_ERROR ) << "\n";

		core::Size nsteps = core::Size( ( end - start ) / res );
		for ( core::Size i = 0; i < nsteps; ++i ) {
			core::Real r = start + (i * res);
			UT << "r=" << r << " func=" << func->func(r) << " dfunc=" << func->dfunc(r) << std::endl;
		}
	} // test_gaussian_func

	void test_serialize_GaussianFunc() {
		TS_ASSERT( true ); // for non-serialization builds
#ifdef SERIALIZATION
		using namespace core::scoring::func;

		FuncOP instance( new GaussianFunc( 5.16, 7.8 ) ); // serialize this through a pointer to the base class

		std::ostringstream oss;
		{
			cereal::BinaryOutputArchive arc( oss );
			arc( instance );
		}

		FuncOP instance2; // deserialize also through a pointer to the base class
		std::istringstream iss( oss.str() );
		{
			cereal::BinaryInputArchive arc( iss );
			arc( instance2 );
		}

		// make sure the deserialized base class pointer points to a GaussianFunc
		TS_ASSERT( utility::pointer::dynamic_pointer_cast< GaussianFunc > ( instance2 ));
		TS_ASSERT( *instance == *instance2 );
#endif // SERIALIZATION
	}

};
