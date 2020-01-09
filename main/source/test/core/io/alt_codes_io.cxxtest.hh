// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file  test/core/io/pdb/alt_codes_io.cxxtest.hh
/// @brief   Test suite for alternative PDB 3-letter-code database loading
/// @author  Labonte <JWLabonte@jhu.edu>


// Test headers
#include <cxxtest/TestSuite.h>
#include <test/core/init_util.hh>

// Unit header
#include <core/io/alt_codes_io.hh>

// Basic header
#include <basic/database/open.hh>
#include <basic/Tracer.hh>

// C++ header
#include <utility>

static basic::Tracer TR( "core.io.alt_codes_io.cxxtest" );

class AltCodesIOTests : public CxxTest::TestSuite {
public: // Standard methods ///////////////////////////////////////////////////
	// Initialization
	void setUp()
	{
		core_init();
	}

	// Destruction
	void tearDown()
	{}


public: // Tests //////////////////////////////////////////////////////////////
	// Confirm that alternative PDB 3-letter-codes are loaded correctly from the database.
	void test_read_alternative_3_letter_codes_from_database_file()
	{
		using namespace std;
		using namespace core::io;

		TR <<  "Testing read_read_alternative_3_letter_codes_from_database_file() method."  << std::endl;

		AltCodeMap alt_codes( read_alternative_3_letter_codes_from_database_file(
			basic::database::full_name( "input_output/3-letter_codes/sentence_case.codes" ) ) );

		TS_ASSERT_EQUALS( alt_codes.size(), 25 );  // 20 canonical and 4 NCAAs and one goofball.
		TS_ASSERT_EQUALS( get< 0 >( alt_codes[ "Ala" ] ), "ALA" );
		TS_ASSERT_EQUALS( get< 1 >( alt_codes[ "Ala" ] ), "" );
		TS_ASSERT_EQUALS( get< 0 >( alt_codes[ "Hcy" ] ), "HCY" );
		TS_ASSERT_EQUALS( get< 1 >( alt_codes[ "Hcy" ] ), "HOMOCYSTEINE" );
		TS_ASSERT_EQUALS( get< 0 >( alt_codes[ "Val" ] ), "VAL" );

		TS_ASSERT_EQUALS( get< 0 >( alt_codes[ "Foo" ] ), "FOO" );
		TS_ASSERT_EQUALS( get< 1 >( alt_codes[ "Foo" ] ), "FOOBARINE" );
		TS_ASSERT_EQUALS( get< 2 >( alt_codes[ "Foo" ] ), '9' );
		TS_ASSERT_EQUALS( ( get< 3 >( alt_codes[ "Foo" ] ) ).size(), 2 );
		TS_ASSERT_EQUALS( ( get< 3 >( alt_codes[ "Foo" ] ) )[ 1 ], "Woo" );
		TS_ASSERT_EQUALS( ( get< 3 >( alt_codes[ "Foo" ] ) )[ 2 ], "Hoo" );
	}
};  // class AltCodesIOTests
