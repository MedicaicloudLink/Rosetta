// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file protocols/rosetta_scripts/RosettaScriptsParser.cxxtest.hh
/// @brief Test suite for the RosettaScripts parser.
/// @details We were without a unit test for this until 6 May 2016!  This tests the xi:include functionality, but still isn't a comprehensive test.
/// @author Vikram K. Mulligan (vmullig@uw.edu)

// Test headers
#include <cxxtest/TestSuite.h>
#include <test/protocols/init_util.hh>

// Package headers
#include <core/pose/Pose.hh>
#include <core/pose/annotated_sequence.hh>
#include <core/pose/extra_pose_info_util.hh>
#include <core/scoring/ScoreFunction.hh>
#include <core/scoring/ScoreType.hh>

#include <protocols/rosetta_scripts/ParsedProtocol.hh>
#include <protocols/rosetta_scripts/RosettaScriptsParser.hh>

// Basic headers
#include <basic/options/option.hh>

// Utility headers
#include <util/pose_funcs.hh>
#include <utility/excn/Exceptions.hh>
#include <utility/pointer/owning_ptr.hh>
#include <utility/tag/Tag.hh>
#include <utility/exit.hh>
#include <utility/io/izstream.hh>
#include <utility/string_util.hh>
#include <basic/Tracer.hh>
#include <utility/pointer/owning_ptr.hh>

// Numberic headers

// C++ headers
#include <string>
#include <iostream>

static basic::Tracer TR("protocols.rosetta_scripts.RosettaScriptsParser.cxxtest");


////////////////////////////////////////////////////////////////////////
// Tests

class RosettaScriptsParserTests : public CxxTest::TestSuite {

public:

	void setUp() {
		protocols_init();
	}

	void test_allowed_multiple_dependencies() {
		TR << "Starting RosettaScriptsParserTests::test_allowed_multiple_dependencies()" << std::endl;
		TR << "This test ensures that the <xi:include ... /> function can be used to include the same file multiple times, if one so wishes." << std::endl;
		TR << "Created by Vikram K. Mulligan (vmullig@uw.edu), Baker lab, 6 May 2016." << std::endl;
		try {
			protocols::rosetta_scripts::RosettaScriptsParser parser;
			utility::vector1< std::string > files_read_in;
			try {
				TR << "===THIS SHOULD NOT TRIGGER AN EXCEPTION===" << std::endl;
				std::string substituted_contents;
				parser.read_in_and_recursively_replace_includes( "protocols/rosetta_scripts/permitted1.xml", substituted_contents, files_read_in, 0 );
			} catch (utility::excn::Exception & e ) {
				TR << "CAUGHT EXCEPTION [" << e.msg() << "]" << std::endl;
				TS_ASSERT(false); //We shouldn't get here.
			}
		} catch (utility::excn::Exception & e ) {
			// The XSD itself is faulty;
			TR << "CAUGHT EXCEPTION [" << e.msg() << "]" << std::endl;
			TS_ASSERT( false );
		}
		TR << "===THE ABOVE SHOULD NOT HAVE TRIGGERED AN EXCEPTION===" << std::endl;
		TR.flush();
	}

	void test_prohibit_circular_dependencies() {
		TR << "Starting RosettaScriptsParserTests::test_prohibit_circular_dependencies()" << std::endl;
		TR << "This test ensures that the <xi:include ... /> function can't be used for circular inclusion patterns (e.g. A includes B includes C includes A)." << std::endl;
		TR << "Created by Vikram K. Mulligan (vmullig@uw.edu), Baker lab, 6 May 2016." << std::endl;
		try {
			protocols::rosetta_scripts::RosettaScriptsParser parser;
			utility::vector1< std::string > files_read_in;
			try {
				TR << "===THIS SHOULD TRIGGER AN EXCEPTION===" << std::endl;
				std::string substituted_contents;
				parser.read_in_and_recursively_replace_includes( "protocols/rosetta_scripts/prohibited1.xml", substituted_contents, files_read_in, 0 );
			} catch (utility::excn::Exception & e ) {
				TR << "CAUGHT EXCEPTION [" << e.msg() << "]" << std::endl;
				TR << "===THE ABOVE SHOULD HAVE TRIGGERED AN EXCEPTION===" << std::endl;

				std::string expected_err_msg = "Error in protocols::rosetta_scipts::RosettaScriptsParser::read_in_and_recursively_replace_includes(): Circular inclusion pattern detected when reading \"protocols/rosetta_scripts/prohibited1.xml\".";
				TS_ASSERT( e.msg().find(expected_err_msg) != std::string::npos );
				return;
			}
		} catch (utility::excn::Exception & e ) {
			// The XSD itself is faulty;
			TR << "CAUGHT EXCEPTION [" << e.msg() << "]" << std::endl;
			TS_ASSERT( false );
		}
		TR << "Error in RosettaScriptsParserTests::test_prohibit_circular_dependencies(): an exception should have been thrown by the circular dependencies, but wasn't." << std::endl;
		TS_ASSERT(false); //We shouldn't get here.
	}

	/// @brief Test read-in of an XML that includes several other XMLs.
	void test_multiple_includes() {
		TR << "Starting RosettaScriptsParserTests::test_multiple_includes()" << std::endl;
		TR << "This test looks for proper function of the <xi:include ... /> functionality." << std::endl;
		TR << "Created by Vikram K. Mulligan (vmullig@uw.edu), Baker lab, 6 May 2016." << std::endl;
		try {
			protocols::rosetta_scripts::RosettaScriptsParser parser;

			//Read in the reference XML (no includes):
			std::string test1xml;
			utility::io::izstream inputstream;
			inputstream.open("protocols/rosetta_scripts/test1.xml");
			utility::slurp( inputstream, test1xml );
			inputstream.close();

			//Read in the test XML (with includes):
			utility::vector1< std::string > files_read_in;
			std::string test2xml;
			parser.read_in_and_recursively_replace_includes("protocols/rosetta_scripts/test2.xml", test2xml, files_read_in, 0);

			if ( TR.visible() ) {
				TR << "REFERENCE FILE (no includes):" << std::endl;
				TR << test1xml << std::endl;
				TR << "TEST FILE (with includes):" << std::endl;
				TR << test2xml << std::endl;
			}

			//Are the interpreted files (with includes replaced with the contents of the other file) the same?
			TS_ASSERT( test1xml == test2xml );
		} catch (utility::excn::Exception & e ) {
			// The XSD itself is faulty;
			TR << "CAUGHT EXCEPTION [" << e.msg() << "]" << std::endl;
			TS_ASSERT( false );
		}

		TR.flush();
	}

	void test_all_attributes_in_xsd_have_descriptions()
	{

	}

	void test_report_at_end()
	{
		std::string report_at_end_protocol =
			" <ROSETTASCRIPTS>"
			"     <TASKOPERATIONS>"
			"         <RestrictAbsentCanonicalAAS keep_aas=\"A\" name=\"to_ala\" resnum=\"0\"/>"
			"     </TASKOPERATIONS>"
			"     <FILTERS>"
			"         <ResidueCount name=\"ala_pre\" residue_types=\"ALA\" />"
			"         <ResidueCount name=\"ala_post_reported\" residue_types=\"ALA\" />"
			"         <ResidueCount name=\"ala_post\" residue_types=\"ALA\" />"
			"     </FILTERS>"
			"     <MOVERS>"
			"         <PackRotamersMover name=\"to_ala\" task_operations=\"to_ala\"/>"
			"     </MOVERS>"
			"     <PROTOCOLS>"
			"         <Add filter_name=\"ala_pre\" report_at_end=\"false\"/>"
			"         <Add filter_name=\"ala_post_reported\" />"
			"         <Add mover_name=\"to_ala\" />"
			"         <Add filter_name=\"ala_post\" report_at_end=\"false\"/>"
			"     </PROTOCOLS>"
			" </ROSETTASCRIPTS>";

		core::pose::Pose test_pose;
		core::pose::make_pose_from_sequence(test_pose, "TESTTESTTEST", "fa_standard", false);

		try {
			protocols::rosetta_scripts::RosettaScriptsParser parser;
			auto protocoltag = parser.create_tag_from_xml_string(report_at_end_protocol, utility::vector1<std::string>());

			auto parsed_mover = parser.parse_protocol_tag(
				test_pose,
				parser.create_tag_from_xml_string(report_at_end_protocol, utility::vector1<std::string>()),
				basic::options::option);

			parsed_mover->apply(test_pose);

		} catch (utility::excn::Exception & e ) {
			// The XSD itself is faulty;
			TR << "CAUGHT EXCEPTION [" << e.msg() << "]" << std::endl;
			TS_ASSERT( false );
		}

		core::Real ala_pre, ala_post, ala_post_reported;
		core::pose::getPoseExtraScore(test_pose, "ala_pre", ala_pre);
		core::pose::getPoseExtraScore(test_pose, "ala_post", ala_post);
		core::pose::getPoseExtraScore(test_pose, "ala_post_reported", ala_post_reported);

		TS_ASSERT(ala_pre == 0);
		TS_ASSERT(ala_post == 12);
		TS_ASSERT(ala_post_reported == 12);

	}
	void test_output_scoring()
	{
		core::pose::Pose test_pose;
		core::pose::make_pose_from_sequence(test_pose, "TESTTESTTEST", "fa_standard", false);

		std::string output_true_xml =
			" <ROSETTASCRIPTS>"
			"     <SCOREFXNS>"
			"         <ScoreFunction name=\"test\" >"
			"             <Reweight scoretype=\"fa_rep\" weight=\".22\" />"
			"             <Reweight scoretype=\"fa_elec\" weight=\".20\" />"
			"             <Reweight scoretype=\"fa_dun\"  weight=\"10\" />"
			"         </ScoreFunction>"
			"     </SCOREFXNS>"
			"     <TASKOPERATIONS>"
			"         <RestrictAbsentCanonicalAAS keep_aas=\"A\" name=\"to_ala\" resnum=\"0\"/>"
			"     </TASKOPERATIONS>"
			"     <FILTERS>"
			"         <ResidueCount name=\"ala_pre\" residue_types=\"ALA\" />"
			"         <ResidueCount name=\"ala_post_reported\" residue_types=\"ALA\" />"
			"         <ResidueCount name=\"ala_post\" residue_types=\"ALA\" />"
			"     </FILTERS>"
			"     <MOVERS>"
			"         <PackRotamersMover name=\"to_ala\" task_operations=\"to_ala\"/>"
			"     </MOVERS>"
			"     <PROTOCOLS>"
			"         <Add filter_name=\"ala_pre\" report_at_end=\"false\"/>"
			"         <Add filter_name=\"ala_post_reported\" />"
			"         <Add mover_name=\"to_ala\" />"
			"         <Add filter_name=\"ala_post\" report_at_end=\"false\"/>"
			"     </PROTOCOLS>"
			"     <OUTPUT scorefxn=\"test\"/>"
			" </ROSETTASCRIPTS>";

		std::string output_false_xml =
			" <ROSETTASCRIPTS>"
			"     <TASKOPERATIONS>"
			"         <RestrictAbsentCanonicalAAS keep_aas=\"A\" name=\"to_ala\" resnum=\"0\"/>"
			"     </TASKOPERATIONS>"
			"     <FILTERS>"
			"         <ResidueCount name=\"ala_pre\" residue_types=\"ALA\" />"
			"         <ResidueCount name=\"ala_post_reported\" residue_types=\"ALA\" />"
			"         <ResidueCount name=\"ala_post\" residue_types=\"ALA\" />"
			"     </FILTERS>"
			"     <MOVERS>"
			"         <PackRotamersMover name=\"to_ala\" task_operations=\"to_ala\"/>"
			"     </MOVERS>"
			"     <PROTOCOLS>"
			"         <Add filter_name=\"ala_pre\" report_at_end=\"false\"/>"
			"         <Add filter_name=\"ala_post_reported\" />"
			"         <Add mover_name=\"to_ala\" />"
			"         <Add filter_name=\"ala_post\" report_at_end=\"false\"/>"
			"     </PROTOCOLS>"
			" </ROSETTASCRIPTS>";


		try {

			protocols::rosetta_scripts::RosettaScriptsParser parser;

			//Test NO output.  Scorefunction in the ParsedProtocol mover should be null.
			bool modified_pose;
			std::string input_name = "test_pose_in";
			std::string output_name = "test_pose_out";

			auto parsed_mover = parser.generate_mover_and_apply_to_pose_xml_string(
				test_pose,
				basic::options::option,
				modified_pose,
				output_false_xml,
				input_name,
				output_name,
				nullptr);

			TS_ASSERT( parsed_mover->final_scorefxn() == nullptr);

			//Test WITH scoring output

			parsed_mover = parser.generate_mover_and_apply_to_pose_xml_string(
				test_pose,
				basic::options::option,
				modified_pose,
				output_true_xml,
				input_name,
				output_name,
				nullptr);
			TS_ASSERT( parsed_mover->final_scorefxn() != nullptr);


			core::scoring::ScoreFunctionOP test_scorefxn = utility::pointer::make_shared< core::scoring::ScoreFunction >();
			test_scorefxn->set_weight(core::scoring::fa_rep,  .22);
			test_scorefxn->set_weight(core::scoring::fa_elec, .20);
			test_scorefxn->set_weight(core::scoring::fa_dun,  10.0);


			//Make sure the scorefunction set in the OUTPUT is the scorefunction set in the ParsedProtocol mover.
			core::scoring::ScoreFunctionCOP scorefxn = parsed_mover->final_scorefxn();

			for ( core::Size i=1; i <= core::Size( core::scoring::n_score_types); ++i ) {
				core::scoring::ScoreType score_type = static_cast<core::scoring::ScoreType>(i);
				TS_ASSERT(scorefxn->get_weight(score_type) == test_scorefxn->get_weight( score_type ));
			}

		} catch (utility::excn::Exception & e ) {
			// The XSD itself is faulty;
			TR << "CAUGHT EXCEPTION [" << e.msg() << "]" << std::endl;
			TS_ASSERT( false );
		}

	}

};
