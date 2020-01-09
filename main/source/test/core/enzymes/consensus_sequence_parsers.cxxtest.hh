// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file    test/core/enzymes/consensus_sequence_parsers.cxxtest.hh
/// @brief   Test suite for consensus sequence parsersing helper functions.
/// @author  Labonte <JWLabonte@jhu.edu>


// Test headers
#include <cxxtest/TestSuite.h>
#include <test/core/init_util.hh>

// Unit header
#include <core/enzymes/consensus_sequence_parsers.hh>

// Utility header
#include <utility/vector1.hh>
#include <basic/Tracer.hh>

// C++ header
#include <string>


static basic::Tracer TR("ConcensusSequenceParserTests");


class ConsensusSequenceParserTests : public CxxTest::TestSuite {
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
	// Confirm that peptide consensus sequences are parsed properly.
	void test_get_3_letter_codes_from_peptide_consensus_sequence()
	{
		using namespace std;
		using namespace utility;

		TR << "Testing get_3_letter_codes_from_peptide_consensus_sequence()..." << std::endl;

		string const sequence( "HE(S/H/E)IS(A/X[ORN])BUTLOVESJAZZSAX" );
		vector1< vector1< string > > consensus_residues;

		consensus_residues = core::enzymes::get_3_letter_codes_from_peptide_consensus_sequence( sequence );

		TS_ASSERT_EQUALS( consensus_residues.size(), 21 );
		TS_ASSERT_EQUALS( consensus_residues[ 1 ].size(), 1 );
		TS_ASSERT_EQUALS( consensus_residues[ 1 ][ 1 ], "HIS" );
		TS_ASSERT_EQUALS( consensus_residues[ 3 ].size(), 3 );
		TS_ASSERT_EQUALS( consensus_residues[ 3 ][ 1 ], "SER" );
		TS_ASSERT_EQUALS( consensus_residues[ 3 ][ 2 ], "HIS" );
		TS_ASSERT_EQUALS( consensus_residues[ 3 ][ 3 ], "GLU" );
		TS_ASSERT_EQUALS( consensus_residues[ 4 ].size(), 1 );
		TS_ASSERT_EQUALS( consensus_residues[ 6 ].size(), 2 );
		TS_ASSERT_EQUALS( consensus_residues[ 6 ][ 2 ], "ORN" );
		TS_ASSERT_EQUALS( consensus_residues[ 7 ].size(), 2 );  // B is Asx.
		TS_ASSERT_EQUALS( consensus_residues[ 7 ][ 1 ], "ASP" );
		TS_ASSERT_EQUALS( consensus_residues[ 7 ][ 2 ], "ASN" );
		TS_ASSERT_EQUALS( consensus_residues[ 8 ].size(), 1 );  // U is Sec.
		TS_ASSERT_EQUALS( consensus_residues[ 8 ][ 1 ], "SEC" );
		TS_ASSERT_EQUALS( consensus_residues[ 11 ].size(), 1 );  // O is Pyl.
		TS_ASSERT_EQUALS( consensus_residues[ 11 ][ 1 ], "PYL" );
		TS_ASSERT_EQUALS( consensus_residues[ 15 ].size(), 2 );  // J is Xle.
		TS_ASSERT_EQUALS( consensus_residues[ 15 ][ 1 ], "LEU" );
		TS_ASSERT_EQUALS( consensus_residues[ 15 ][ 2 ], "ILE" );
		TS_ASSERT_EQUALS( consensus_residues[ 17 ].size(), 2 );  // Z is Glx.
		TS_ASSERT_EQUALS( consensus_residues[ 17 ][ 1 ], "GLU" );
		TS_ASSERT_EQUALS( consensus_residues[ 17 ][ 2 ], "GLN" );
		TS_ASSERT_EQUALS( consensus_residues[ 21 ].size(), 20 );  // X is any CAA.
	}
};  // class ConsensusSequenceParserTests
