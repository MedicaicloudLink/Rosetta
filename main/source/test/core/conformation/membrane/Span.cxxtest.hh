// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file   core/membrane/Span.cxxtest.hh
/// @brief   Unit test for Span class
/// @author  JKLeman (julia.koehler1982@gmail.com)

// Test Headers
#include <cxxtest/TestSuite.h>
#include <test/core/init_util.hh>

// Unit Headers
#include <core/conformation/membrane/Span.hh>

// Package Headers
#include <core/types.hh>

// Utility Headers
#include <basic/Tracer.hh>
#include <core/conformation/membrane/Exceptions.hh>
#include <utility/vector1.hh>
#include <basic/Tracer.hh>

// C++ Headers
#include <cstdlib>
#include <string>
#include <cmath>

static basic::Tracer TR("core.conformation.membrane.Span.cxxtest");

using namespace core;
using namespace core::conformation;
using namespace core::conformation::membrane;

class SpanTest : public CxxTest::TestSuite {

public: // test functions

	/// Test Setup Functions ////////

	/// @brief Setup Test
	void setUp(){

		// Initialize
		core_init();
	}

	/// @brief Standard Tear Down
	void tearDown(){}

	///// Test Methods /////////////

	////////////////////////////////////////////////////////////////////////////////

	// create object from PDB
	void test_is_valid(){

		TR << "Test whether Span is valid." << std::endl;
		using namespace core::conformation::membrane;

		// normal span
		SpanOP span1( new Span( 1, 20 ) );
		TR << "...span1..." << std::endl;
		TS_ASSERT_EQUALS( span1->is_valid(), true );

		// start > end
		SpanOP span2( new Span( 10, 5 ) );
		TR << "...span2..." << std::endl;
		TS_ASSERT_EQUALS( span2->is_valid(), false );

		// zero end
		SpanOP span3( new Span( 4, 0 ) );
		TR << "...span3..." << std::endl;
		TS_ASSERT_EQUALS( span3->is_valid(), false );

		// zero start
		SpanOP span4( new Span( 0, 5 ) );
		TR << "...span4..." << std::endl;
		TS_ASSERT_EQUALS( span4->is_valid(), false );

		// too short, only warning
		SpanOP span5( new Span( 1, 3 ) );
		TR << "...span5..." << std::endl;
		TS_ASSERT_EQUALS( span5->is_valid(), true );

		// too long, only warning
		SpanOP span6( new Span( 5, 35 ) );
		TR << "...span6..." << std::endl;
		TS_ASSERT_EQUALS( span6->is_valid(), true );

		TR << "Finished span validity tests." << std::endl;
	}
};


