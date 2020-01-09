// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file   test/protocols/denovo_design/util.cxxtest.hh
/// @brief  test suite for protocols::denovo_design::util.cc
/// @author Tom Linsky (tlinsky@uw.edu)


// Test headers
#include <cxxtest/TestSuite.h>
#include <test/protocols/init_util.hh>

// Unit headers
#include <protocols/denovo_design/util.hh>

// Protocol headers
#include <protocols/denovo_design/components/Segment.hh>
#include <protocols/denovo_design/components/StructureData.hh>
//include <protocols/denovo_design/connection/Connection.hh>

// Core headers

// Utility headers
#include <basic/options/option.hh>
#include <basic/options/keys/run.OptionKeys.gen.hh>
#include <basic/Tracer.hh>

// Boost headers
#include <boost/assign.hpp>
#include <boost/lexical_cast.hpp>

// C++ headers

static basic::Tracer TR( "protocols.denovo_design.UtilTests.cxxtest" );

typedef utility::vector1< std::string > Motifs;

protocols::denovo_design::components::StructureDataOP
structuredata_from_motifs( Motifs const & motifs )
{
	using namespace protocols::denovo_design;
	using namespace protocols::denovo_design::components;

	StructureDataOP perm( new StructureData( "test" ) );
	core::Size segment_num = 1;
	Motifs::const_iterator prev = motifs.end();
	Motifs::const_iterator next = ++motifs.begin();
	for ( Motifs::const_iterator m=motifs.begin(); m!=motifs.end(); ++m, ++segment_num, ++next ) {
		std::string const segment_name = boost::lexical_cast< std::string >( segment_num );

		std::string lower = "";
		if ( prev != motifs.end() ) {
			lower = boost::lexical_cast< std::string >( segment_num - 1 );
		}
		std::string upper = "";
		if ( next != motifs.end() ) {
			upper = boost::lexical_cast< std::string >( segment_num + 1 );
		}

		Segment newsegment;
		newsegment.parse_motif( *m );
		newsegment.set_movable_group( segment_num );
		newsegment.delete_lower_padding();
		newsegment.delete_upper_padding();
		newsegment.set_lower_segment( lower );
		newsegment.set_upper_segment( upper );
		perm->add_segment( segment_name, newsegment );
		prev = m;
	}
	return perm;
}

// --------------- Test Class --------------- //
class DeNovoUtilTests : public CxxTest::TestSuite {
public:

	// Shared initialization goes here.
	void setUp() {
		// load params for ligand
		protocols_init();

		// set preserve header always for "tomponent"
		basic::options::option[basic::options::OptionKeys::run::preserve_header].value(true);
	}

	// Shared finalization goes here.
	void tearDown() {
	}


	void reset( protocols::denovo_design::components::StructureData & perm )
	{
		perm.set_data_str( "2", "paired_strands", ",4" );
		perm.set_data_str( "4", "paired_strands", "2," );
		perm.set_data_int( "2", "orientation", 1 );
		perm.set_data_int( "4", "orientation", 0 );
	}

	void test_bulge_strandpairing() {
		using namespace protocols::denovo_design;
		using namespace protocols::denovo_design::components;
		Motifs const motifs = boost::assign::list_of
			("1LX")("5EB")("2LG")("4EB")("1LX");
		TR << motifs << std::endl;
		StructureDataOP perm = structuredata_from_motifs( motifs );
		TS_ASSERT( perm );
		TS_ASSERT_THROWS_NOTHING( perm->check_consistency() );

		// add pairing info
		reset( *perm );

		// add bulge
		utility::vector1< std::string > e1_abego = abego_vector( perm->segment( "2" ).abego() );
		e1_abego[ 2 ] = "A";
		perm->set_abego( "2", e1_abego );
		TS_ASSERT_EQUALS( perm->segment( "2" ).abego()[ 1 ], 'A' );
		TR << *perm << std::endl;

		// count bulges
		TS_ASSERT_EQUALS( count_bulges( *perm, "2" ), 1 );
		TS_ASSERT_EQUALS( count_bulges( *perm, "4" ), 0 );

		// get strandpairings
		TS_ASSERT_EQUALS( get_strandpairings( *perm, true ), "1-2.A.0" );

		// switch to parallel
		perm->set_data_int( "4", "orientation", 1 );
		TS_ASSERT_EQUALS( get_strandpairings( *perm, true ), "1-2.P.0" );

		// flipped orientations
		perm->set_data_int( "2", "orientation", 0 );
		TS_ASSERT_EQUALS( get_strandpairings( *perm, true ), "1-2.A.0" );

		// reset and try a shift
		reset( *perm );
		perm->set_data_str( "4", "paired_strands", ",2" );
		perm->set_data_str( "2", "paired_strands", "4," );

		// get strandpairings
		TS_ASSERT_EQUALS( get_strandpairings( *perm, true ), "1-2.A.0" );

		// switch to parallel
		perm->set_data_int( "4", "orientation", 1 );
		TS_ASSERT_EQUALS( get_strandpairings( *perm, true ), "1-2.P.0" );

		// flipped orientations
		perm->set_data_int( "2", "orientation", 0 );
		TS_ASSERT_EQUALS( get_strandpairings( *perm, true ), "1-2.A.0" );
	}
};
