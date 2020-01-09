// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file   test/protocols/jd3/StandardJobQueen.cxxtest.hh
/// @brief  test suite for the StandardJobQueen
/// @author Andrew Leaver-Fay (aleaverfay@gmail.com)
/// @author Jared Adolf-Bryfogle (jadolfbr@gmail.com); Refactoring

// Test headers
#include <cxxtest/TestSuite.h>
#include <test/core/init_util.hh>

// Unit headers
#include <protocols/jd3/standard/StandardJobQueen.hh>

// Package headers
#include <protocols/jd3/JobQueen.hh>
#include <protocols/jd3/JobDigraph.hh>
#include <protocols/jd3/LarvalJob.hh>
#include <protocols/jd3/InnerLarvalJob.hh>
#include <protocols/jd3/pose_inputters/PoseInputSource.hh>
#include <protocols/jd3/deallocation/InputPoseDeallocationMessage.hh>
#include <protocols/jd3/pose_inputters/PDBPoseInputter.hh>
#include <protocols/jd3/pose_outputters/PDBPoseOutputter.hh>
#include <protocols/jd3/standard/PreliminaryLarvalJobTracker.hh>
#include <protocols/jd3/standard/PreliminaryLarvalJob.hh>
#include <protocols/jd3/JobTracker.hh>

// basic headers
#include <basic/options/option.hh>

// Utility headers
#include <utility/string_util.hh>
#include <utility/options/OptionCollection.hh>
#include <utility/options/keys/OptionKey.hh>
#include <utility/options/keys/BooleanOptionKey.hh>
#include <utility/options/keys/StringOptionKey.hh>
#include <utility/excn/Exceptions.hh>
#include <utility/tag/Tag.hh>
#include <utility/tag/XMLSchemaGeneration.hh>
#include <utility/string_util.hh>

// Boost headers
#include <boost/bind.hpp>
#include <boost/function.hpp>

// C++ headers
#include <sstream>

using namespace utility::tag;
using namespace protocols::jd3;
using namespace protocols::jd3::standard;
using namespace protocols::jd3::pose_inputters;

//local options
namespace basic { namespace options { namespace OptionKeys {
basic::options::BooleanOptionKey const bool_arg1("bool_arg1");
basic::options::BooleanOptionKey const bool_arg2("bool_arg2");
basic::options::BooleanOptionKey const bool_arg3("bool_arg3");
basic::options::BooleanOptionKey const bool_arg_sjq_does_not_track("bool_arg_sjq_does_not_track");
basic::options::StringOptionKey const string_arg1("string_arg1");
basic::options::StringOptionKey const string_arg2("string_arg2");
basic::options::StringOptionKey const string_arg_w_default("string_arg_w_default");
basic::options::StringOptionKey const string_arg_sjq_does_not_track("string_arg_sjq_does_not_track");
namespace dummy_jq {
basic::options::BooleanOptionKey const bool_arg4("dummy_jq:bool_arg4");
}
utility::options::IntegerVectorOptionKey const intvect_arg1("intvect_arg1");
}}}//basic::options::OptionKeys

class DummyJobQueen : public StandardJobQueen
{
public:
	typedef StandardJobQueen parent;

	using parent::note_job_completed;


public:
	DummyJobQueen()
	{
		utility::options::OptionKeyList opts;

		add_options( opts );
		add_option( basic::options::OptionKeys::bool_arg1 );
		add_option( basic::options::OptionKeys::bool_arg2 );
		add_option( basic::options::OptionKeys::bool_arg3 );
		add_option( basic::options::OptionKeys::string_arg1 );
		add_option( basic::options::OptionKeys::string_arg2 );
		add_option( basic::options::OptionKeys::string_arg_w_default );
		add_option( basic::options::OptionKeys::dummy_jq::bool_arg4 );
		add_option( basic::options::OptionKeys::intvect_arg1 );
	}

	~DummyJobQueen() {}

	void append_job_tag_subelements(
		utility::tag::XMLSchemaDefinition & job_definition_xsd,
		utility::tag::XMLSchemaComplexTypeGenerator & job_ct_gen
	) const override
	{
		if ( append_job_tag_subelements_ ) {
			append_job_tag_subelements_( job_definition_xsd, job_ct_gen );
		}
	}

	void
	append_common_tag_subelements(
		utility::tag::XMLSchemaDefinition & job_definition_xsd,
		utility::tag::XMLSchemaComplexTypeGenerator & ct_gen
	) const override
	{
		if ( append_common_tag_subelements_ ) {
			append_common_tag_subelements_( job_definition_xsd, ct_gen );
		}
	}

	protocols::jd3::JobOP
	complete_larval_job_maturation(
		protocols::jd3::LarvalJobCOP larval_job,
		utility::options::OptionCollectionCOP job_options,
		utility::vector1< JobResultCOP > const &
	) override
	{
		if ( complete_job_maturation_ ) {
			complete_job_maturation_( larval_job, job_options );
		}
		return protocols::jd3::JobOP();
	}

	//virtual bool has_job_completed( protocols::jd3::LarvalJobCOP job ) { return pose_outputter_for_job( *job->inner_job() )->job_has_already_completed( *job ); }

	void note_job_completed( protocols::jd3::LarvalJobCOP job, protocols::jd3::JobStatus status, core::Size nresults ) override {
		StandardJobQueen::note_job_completed( job, status, nresults );
	}

	//void completed_job_result( protocols::jd3::LarvalJobCOP /*job*/, core::Size, protocols::jd3::JobResultOP /*result*/ ) override {}s

	//numeric::DiscreteIntervalEncodingTree< core::Size > const & completed_jobs() const { return parent::completed_jobs();}
	//numeric::DiscreteIntervalEncodingTree< core::Size > const & successful_jobs() const { return parent::successful_jobs();}
	//numeric::DiscreteIntervalEncodingTree< core::Size > const & failed_jobs() const { return parent::failed_jobs();}
	//numeric::DiscreteIntervalEncodingTree< core::Size > const & output_jobs() const { return parent::processed_jobs();}


public:
	//Public wrappers for protected functions to aid in testing

	using parent::get_preliminary_larval_jobs_determined;
	using parent::get_prelim_larval_job_tracker;
	using parent::get_job_graph;
	using parent::get_preliminary_larval_jobs;
	using parent::get_job_tracker;

public:

	// callbacks
	typedef boost::function< void ( protocols::jd3::LarvalJobCOP, utility::options::OptionCollectionCOP ) > CompleteJobMaturationCallback;
	CompleteJobMaturationCallback complete_job_maturation_;

	typedef boost::function< void ( utility::tag::XMLSchemaDefinition & xsd, utility::tag::XMLSchemaComplexTypeGenerator & ctgen ) > SubtagAppenderCallback;
	SubtagAppenderCallback append_job_tag_subelements_;
	SubtagAppenderCallback append_common_tag_subelements_;

};


class StandardJobQueenTests : public CxxTest::TestSuite
{
public:

	StandardJobQueenTests() : local_options_added_( false ) {}

	void setUp() {
		if ( ! local_options_added_ ) {
			using namespace basic::options;
			option.add( OptionKeys::bool_arg1, "" ).def(false);
			option.add( OptionKeys::bool_arg2, "" ).def(false);
			option.add( OptionKeys::bool_arg3, "" ).def(true);
			option.add( OptionKeys::bool_arg_sjq_does_not_track, "" ).def(false);
			option.add( OptionKeys::string_arg1, "" );
			option.add( OptionKeys::string_arg2, "" );
			option.add( OptionKeys::string_arg_sjq_does_not_track, "" );
			option.add( OptionKeys::string_arg_w_default, "" ).def("fiddlesticks");
			option.add( OptionKeys::dummy_jq::bool_arg4, "" ).def(false);
			option.add( OptionKeys::intvect_arg1, "" );

			local_options_added_ = true;
		}
	}

	void test_job_options_initialization() {
		core_init_with_additional_options( "-bool_arg1 -bool_arg_sjq_does_not_track -string_arg1 wakka_wakka_wakka -string_arg_sjq_does_not_track yippie -s 1ubq.pdb -intvect_arg1 1 2 3 4 5" );
		DummyJobQueen djq;
		djq.create_and_set_initial_job_dag(); // no need to hold the DAG returned by this func, but it must be called

		//TS_ASSERT( );
		LarvalJobs jobs = djq.determine_job_list( 1, 1000 );
		TS_ASSERT( ! jobs.empty() );
		TS_ASSERT( djq.get_preliminary_larval_jobs_determined());



		if ( jobs.empty() ) return;

		djq.complete_job_maturation_ = boost::bind( StandardJobQueenTests::callback_complete_larval_job_maturation1, _1, _2 );
		utility::vector1< JobResultOP > empty_vector;
		djq.mature_larval_job( jobs.front(), empty_vector ); // invokes callback_complete_larval_job_maturation1
	}

	static
	void
	callback_complete_larval_job_maturation1(
		protocols::jd3::LarvalJobCOP larval_job,
		utility::options::OptionCollectionCOP job_options_ptr
	)
	{
		using namespace basic::options::OptionKeys;
		TS_ASSERT_EQUALS( larval_job->inner_job()->n_input_sources(), 1 );
		TS_ASSERT_EQUALS( larval_job->inner_job()->input_source().origin(), pose_inputters::PDBPoseInputter::keyname() );
		TS_ASSERT_EQUALS( larval_job->inner_job()->input_source().input_tag(), "1ubq" );
		TS_ASSERT_EQUALS( larval_job->inner_job()->job_tag(), "1ubq" );

		TS_ASSERT( job_options_ptr );
		utility::options::OptionCollection const & job_options( * job_options_ptr );

		TS_ASSERT(   job_options[ bool_arg1 ].user() );
		TS_ASSERT( ! job_options[ bool_arg2 ].user() );
		TS_ASSERT( ! job_options[ bool_arg3 ].user() );

		TS_ASSERT(   job_options[ bool_arg1 ].active() );
		TS_ASSERT(   job_options[ bool_arg2 ].active() );
		TS_ASSERT(   job_options[ bool_arg3 ].active() );

		TS_ASSERT(   job_options[ bool_arg1 ] );
		TS_ASSERT( ! job_options[ bool_arg2 ] );
		TS_ASSERT(   job_options[ bool_arg3 ] );

		TS_ASSERT(   job_options[ string_arg1          ].user() );
		TS_ASSERT( ! job_options[ string_arg2          ].user() );
		TS_ASSERT( ! job_options[ string_arg_w_default ].user() );

		TS_ASSERT(   job_options[ string_arg1          ].active() );
		TS_ASSERT( ! job_options[ string_arg2          ].active() );
		TS_ASSERT(   job_options[ string_arg_w_default ].active() );

		TS_ASSERT(   job_options[ string_arg1 ]() == "wakka_wakka_wakka" );

		TS_ASSERT( ! job_options.has( bool_arg_sjq_does_not_track ) );
		TS_ASSERT( ! job_options.has( string_arg_sjq_does_not_track ) );

		TS_ASSERT(   job_options[ intvect_arg1         ].user() );
		TS_ASSERT(   job_options[ intvect_arg1         ].active() );
		utility::vector1< int > intvect_arg1_expected( 5 );
		intvect_arg1_expected[ 1 ] = 1;
		intvect_arg1_expected[ 2 ] = 2;
		intvect_arg1_expected[ 3 ] = 3;
		intvect_arg1_expected[ 4 ] = 4;
		intvect_arg1_expected[ 5 ] = 5;
	}

	// Test that having two subelements of the SecondaryOutput tag in a Job definition
	// is ok.
	static
	void
	callback_complete_larval_job_maturation2(
		protocols::jd3::LarvalJobCOP larval_job,
		utility::options::OptionCollectionCOP
	)
	{
		using namespace basic::options::OptionKeys;
		TS_ASSERT_EQUALS( larval_job->inner_job()->n_input_sources(), 1 );
		TS_ASSERT_EQUALS( larval_job->inner_job()->input_source().origin(), pose_inputters::PDBPoseInputter::keyname() );
		TS_ASSERT_EQUALS( larval_job->inner_job()->input_source().input_tag(), "1ubq" );
		TS_ASSERT_EQUALS( larval_job->inner_job()->job_tag(), "1ubq" );

		TS_ASSERT( larval_job->inner_job()->jobdef_tag() );
		if ( ! larval_job->inner_job()->jobdef_tag() ) return;

		utility::tag::TagCOP job_tag = larval_job->inner_job()->jobdef_tag();
		TS_ASSERT( job_tag->hasTag( "SecondaryOutput" ) );
		if ( ! job_tag->hasTag( "SecondaryOutput" ) ) return;

		utility::tag::TagCOP secondary_output_tag = job_tag->getTag( "SecondaryOutput" );
		TS_ASSERT_EQUALS( secondary_output_tag->getTags().size(), 2 );
		if ( secondary_output_tag->getTags().size() != 2 ) return;
		for ( core::Size ii = 0; ii < 2; ++ii ) {
			TS_ASSERT_EQUALS( secondary_output_tag->getTags()[ii]->getName(), "ScoreFile" );
		}
	}

	bool tag_has_subtag_w_name( TagCOP tag, std::string const & tag_name, std::string const & name_attribute ) {
		for ( auto const & subtag : tag->getTags() ) {
			if ( subtag->getName() != tag_name ) continue;
			if ( subtag->hasOption( "name" ) ) {
				if ( subtag->getOption< std::string >( "name" ) == name_attribute ) {
					return true;
				}
			}
		}
		return false;
	}

	TagCOP
	subtag_w_name( TagCOP tag, std::string const & tag_name, std::string const & name_attribute ) {
		for ( auto const & subtag : tag->getTags() ) {
			if ( subtag->getName() != tag_name ) continue;
			if ( subtag->hasOption( "name" ) ) {
				if ( subtag->getOption< std::string >( "name" ) == name_attribute ) {
					return subtag;
				}
			}
		}
		return TagCOP();
	}

	void test_job_definition_file_xsd() {
		core_init();
		DummyJobQueen djq;
		djq.append_job_tag_subelements_ = boost::bind( StandardJobQueenTests::job_tag_xsd1, _1, _2 );
		std::string job_def_xsd = djq.job_definition_xsd();
		// now lets turn this into a Tag object and then make sure the Job tag has the definition it ought to

		//std::cout << "job def xsd:\n" << job_def_xsd << std::endl;

		TagCOP job_def_xsd_tag = Tag::create( job_def_xsd );
		TS_ASSERT_EQUALS( job_def_xsd_tag->getName(), std::string( "xs:schema" ) );

		TS_ASSERT( tag_has_subtag_w_name( job_def_xsd_tag, "xs:complexType", "job_def_Job_type" ));
		TS_ASSERT( tag_has_subtag_w_name( job_def_xsd_tag, "xs:complexType", "job_def_Options_type" ));
		TagCOP options_type_tag = subtag_w_name( job_def_xsd_tag, "xs:complexType", "job_def_Options_type" );
		if ( ! options_type_tag ) return;

		TS_ASSERT( options_type_tag->hasTag( "xs:all" ));
		TagCOP options_type_xs_all = options_type_tag->getTag( "xs:all" );
		if ( ! options_type_xs_all ) return;

		TS_ASSERT( tag_has_subtag_w_name( options_type_xs_all, "xs:element", "bool_arg1" ));
		TS_ASSERT( tag_has_subtag_w_name( options_type_xs_all, "xs:element", "bool_arg2" ));
		TS_ASSERT( tag_has_subtag_w_name( options_type_xs_all, "xs:element", "bool_arg3" ));
		TS_ASSERT( tag_has_subtag_w_name( options_type_xs_all, "xs:element", "string_arg1" ));
		TS_ASSERT( tag_has_subtag_w_name( options_type_xs_all, "xs:element", "string_arg2" ));
		TS_ASSERT( tag_has_subtag_w_name( options_type_xs_all, "xs:element", "string_arg_w_default" ));
		TS_ASSERT( tag_has_subtag_w_name( options_type_xs_all, "xs:element", "dummy_jq__bool_arg4" ));
		TS_ASSERT( tag_has_subtag_w_name( options_type_xs_all, "xs:element", "intvect_arg1" ));
	}

	static
	void job_tag_xsd1( utility::tag::XMLSchemaDefinition &, utility::tag::XMLSchemaComplexTypeGenerator & ctgen )
	{

		using namespace utility::tag;
		AttributeList attributes;
		attributes + XMLSchemaAttribute( "foo", xs_string , "" ) + XMLSchemaAttribute( "bar", xs_integer , "" );

		XMLSchemaSimpleSubelementList subelements;
		subelements.add_simple_subelement( "Hoo", attributes, "There once was a" ).add_simple_subelement( "Ville", attributes, "grinch" );
		ctgen.add_ordered_subelement_set_as_pick_one( subelements );
	}

	void test_read_jobs_from_xml_file()
	{

		std::string jobdef_file =
			"<JobDefinitionFile>\n"
			" <Job>\n"
			"  <Input>\n"
			"   <PDB filename=\"1ubq.pdb\"/>\n"
			"  </Input>\n"
			"  <Options>\n"
			"   <bool_arg1/>\n"
			"   <string_arg1 value=\"wakka_wakka_wakka\"/>\n"
			"   <intvect_arg1 value=\"1 2 3 4 5\"/>\n"
			"  </Options>\n"
			" </Job>\n"
			"</JobDefinitionFile>\n";

		core_init(); // all options passed through job-definition file

		DummyJobQueen djq;
		try {
			djq.determine_preliminary_job_list_from_xml_file( jobdef_file );
		} catch (utility::excn::Exception & e ) {
			std::cout << e.msg() << std::endl;
			TS_ASSERT( false );
		}
		djq.create_and_set_initial_job_dag(); // no need to hold the DAG returned by this func, but it must be called

		LarvalJobs jobs = djq.determine_job_list( 1, 1000 );

		utility::vector1< JobResultOP > empty_vector;
		djq.complete_job_maturation_ = boost::bind( StandardJobQueenTests::callback_complete_larval_job_maturation1, _1, _2 );
		djq.mature_larval_job( jobs.front(), empty_vector ); // invokes callback_complete_larval_job_maturation1

	}

	// Ensure that a job may use multiple SecondaryOutputters
	void test_read_jobs_from_xml_file2()
	{

		std::string jobdef_file =
			"<JobDefinitionFile>\n"
			" <Job>\n"
			"  <Input>\n"
			"   <PDB filename=\"1ubq.pdb\"/>\n"
			"  </Input>\n"
			"  <SecondaryOutput>\n"
			"   <ScoreFile filename=\"score1.sc\"/>\n"
			"   <ScoreFile filename=\"score2.sc\"/>\n"
			"  </SecondaryOutput>\n"
			" </Job>\n"
			"</JobDefinitionFile>\n";

		core_init(); // all options passed through job-definition file

		DummyJobQueen djq;
		try {
			djq.determine_preliminary_job_list_from_xml_file( jobdef_file );
		} catch (utility::excn::Exception & e ) {
			std::cout << e.msg() << std::endl;
			TS_ASSERT( false );
		}
		djq.create_and_set_initial_job_dag(); // no need to hold the DAG returned by this func, but it must be called

		JobDigraph const & job_graph = djq.get_job_graph();
		TS_ASSERT_EQUALS( job_graph.num_nodes(), 1);
		LarvalJobs jobs = djq.determine_job_list( 1, 1000 );

		utility::vector1< JobResultOP > empty_vector;
		djq.complete_job_maturation_ = boost::bind( StandardJobQueenTests::callback_complete_larval_job_maturation2, _1, _2 );
		djq.mature_larval_job( jobs.front(), empty_vector ); // invokes callback_complete_larval_job_maturation2

	}

	void test_preliminary_job_node_job_index_ranges()
	{

		std::string jobdef_file =
			"<JobDefinitionFile>\n"
			" <Job nstruct=\"5\">\n"
			"  <Input>\n"
			"   <PDB filename=\"1ubq.pdb\"/>\n"
			"  </Input>\n"
			" </Job>\n"
			" <Job nstruct=\"11\">\n"
			"  <Input>\n"
			"   <PDB filename=\"1ubq.pdb\"/>\n"
			"  </Input>\n"
			" </Job>\n"
			" <Job nstruct=\"3\">\n"
			"  <Input>\n"
			"   <PDB filename=\"1ubq.pdb\"/>\n"
			"  </Input>\n"
			" </Job>\n"
			"</JobDefinitionFile>\n";

		core_init(); // all options passed through job-definition file

		DummyJobQueen djq;
		try {
			djq.determine_preliminary_job_list_from_xml_file( jobdef_file );
		} catch (utility::excn::Exception & e ) {
			std::cout << e.msg() << std::endl;
			TS_ASSERT( false );
		}
		JobDigraphOP dag = djq.create_and_set_initial_job_dag();
		TS_ASSERT_EQUALS( dag->num_nodes(), 3 );
		TS_ASSERT_EQUALS( dag->num_edges(), 0 );

		std::string node_label ="T1";
		for ( core::Size i = 1; i <= 3; ++i ) {
			dag->get_job_node(i)->set_node_label(node_label);
		}

		TS_ASSERT_EQUALS( dag->get_job_node(1)->get_node_label(), "T1");
		TS_ASSERT_EQUALS( dag->get_job_node(2)->get_node_label(), "T1");
		TS_ASSERT_EQUALS( dag->get_job_node(3)->get_node_label(), "T1");

		JobDigraph const & job_graph = djq.get_job_graph();
		TS_ASSERT_EQUALS( job_graph.num_nodes(), 3);
		TS_ASSERT_EQUALS( job_graph.num_edges(), 0);

		PreliminaryLarvalJobTracker &  prelim_tracker = djq.get_prelim_larval_job_tracker();
		utility::vector1< core::Size > prelim_nodes( 3 );
		for ( core::Size ii = 1; ii <= 3; ++ii ) prelim_nodes[ ii ] = ii;
		TS_ASSERT_EQUALS( prelim_tracker.get_preliminary_job_node_indices(), prelim_nodes );

		LarvalJobs jobs = djq.determine_job_list_and_track( 1, 4 );
		prelim_tracker = djq.get_prelim_larval_job_tracker();
		TS_ASSERT_EQUALS( jobs.size(), 4 );
		TS_ASSERT_EQUALS( prelim_tracker.get_job_node_assigned( 1 ), false );
		TS_ASSERT_EQUALS( prelim_tracker.get_job_node_assigned( 2 ), false );
		TS_ASSERT_EQUALS( prelim_tracker.get_job_node_assigned( 3 ), false );
		TS_ASSERT_EQUALS( prelim_tracker.get_job_index_starting_job_node( 1 ), 1 );
		TS_ASSERT_EQUALS( prelim_tracker.get_job_index_ending_job_node( 1 ),   4 );
		TS_ASSERT_EQUALS( prelim_tracker.get_job_index_starting_job_node( 2 ), 0 );
		TS_ASSERT_EQUALS( prelim_tracker.get_job_index_ending_job_node( 2 ),   0 );
		TS_ASSERT_EQUALS( prelim_tracker.get_job_index_starting_job_node( 3 ), 0 );
		TS_ASSERT_EQUALS( prelim_tracker.get_job_index_ending_job_node( 3 ),   0 );

		JobTracker & tracker = djq.get_job_tracker();
		TS_ASSERT_EQUALS( tracker.last_job_for_input_source_id(1), 4);
		TS_ASSERT( tracker.started_jobs_for_node(1).member(1));
		TS_ASSERT( tracker.started_jobs_for_node(1).member(2));
		TS_ASSERT( tracker.started_jobs_for_node(1).member(3));
		TS_ASSERT( tracker.started_jobs_for_node(1).member(4));
		TS_ASSERT( tracker.started_jobs().member(1));
		TS_ASSERT( tracker.started_jobs().member(2));
		TS_ASSERT( tracker.started_jobs().member(3));
		TS_ASSERT( tracker.started_jobs().member(4));

		utility::vector1< LarvalJobOP > jobs_vector( jobs.size() );
		std::copy( jobs.begin(), jobs.end(), jobs_vector.begin() );
		djq.note_job_completed_and_track( jobs_vector[ 1 ], jd3_job_status_success, 1 );
		djq.note_job_completed_and_track( jobs_vector[ 2 ], jd3_job_status_success, 1 );
		djq.note_job_completed_and_track( jobs_vector[ 3 ], jd3_job_status_success, 1 );
		djq.note_job_completed_and_track( jobs_vector[ 4 ], jd3_job_status_failed_max_retries, 0 );

		tracker = djq.get_job_tracker();

		TS_ASSERT( tracker.completed_jobs().member( 1 ) );
		TS_ASSERT( tracker.completed_jobs().member( 2 ) );
		TS_ASSERT( tracker.completed_jobs().member( 3 ) );
		TS_ASSERT( tracker.completed_jobs().member( 4 ) );

		TS_ASSERT(   tracker.successful_jobs().member( 1 ) );
		TS_ASSERT(   tracker.successful_jobs().member( 2 ) );
		TS_ASSERT(   tracker.successful_jobs().member( 3 ) );
		TS_ASSERT( ! tracker.successful_jobs().member( 4 ) );

		TS_ASSERT( ! tracker.failed_jobs().member( 1 ) );
		TS_ASSERT( ! tracker.failed_jobs().member( 2 ) );
		TS_ASSERT( ! tracker.failed_jobs().member( 3 ) );
		TS_ASSERT(   tracker.failed_jobs().member( 4 ) );

		TS_ASSERT( tracker.completed_jobs_for_node(1).member(1));
		TS_ASSERT( tracker.completed_jobs_for_node(1).member(2));
		TS_ASSERT( tracker.completed_jobs_for_node(1).member(3));
		TS_ASSERT( tracker.completed_jobs_for_node(1).member(4));

		jobs = djq.determine_job_list_and_track( 1, 4 );
		prelim_tracker = djq.get_prelim_larval_job_tracker();
		TS_ASSERT_EQUALS( jobs.size(), 1 );
		TS_ASSERT_EQUALS( prelim_tracker.get_job_node_assigned( 1 ), true );
		TS_ASSERT_EQUALS( prelim_tracker.get_job_node_assigned( 2 ), false );
		TS_ASSERT_EQUALS( prelim_tracker.get_job_node_assigned( 3 ), false );
		TS_ASSERT_EQUALS( prelim_tracker.get_job_index_starting_job_node( 1 ), 1 );
		TS_ASSERT_EQUALS( prelim_tracker.get_job_index_ending_job_node(1 ),   5 );
		TS_ASSERT_EQUALS( prelim_tracker.get_job_index_starting_job_node( 2 ), 0 );
		TS_ASSERT_EQUALS( prelim_tracker.get_job_index_ending_job_node( 2 ),   0 );
		TS_ASSERT_EQUALS( prelim_tracker.get_job_index_starting_job_node( 3 ), 0 );
		TS_ASSERT_EQUALS( prelim_tracker.get_job_index_ending_job_node( 3 ),   0 );

		TS_ASSERT_EQUALS( tracker.last_job_for_input_source_id(1), 5);
		TS_ASSERT( tracker.started_jobs_for_node(1).member(1));
		TS_ASSERT( tracker.started_jobs_for_node(1).member(2));
		TS_ASSERT( tracker.started_jobs_for_node(1).member(3));
		TS_ASSERT( tracker.started_jobs_for_node(1).member(4));
		TS_ASSERT( tracker.started_jobs_for_node(1).member(5));
		TS_ASSERT( tracker.started_jobs().member(1));
		TS_ASSERT( tracker.started_jobs().member(2));
		TS_ASSERT( tracker.started_jobs().member(3));
		TS_ASSERT( tracker.started_jobs().member(4));
		TS_ASSERT( tracker.started_jobs().member(5));

		TS_ASSERT( ! tracker.completed_jobs_for_node(1).member(5));
		TS_ASSERT( ! tracker.completed_jobs_for_node(1).member(6));
		TS_ASSERT( ! tracker.completed_jobs_for_node(1).member(7));
		TS_ASSERT( ! tracker.completed_jobs_for_node(1).member(8));

		jobs = djq.determine_job_list_and_track( 2, 6 );
		prelim_tracker = djq.get_prelim_larval_job_tracker();
		TS_ASSERT_EQUALS( jobs.size(), 6 );
		TS_ASSERT_EQUALS( prelim_tracker.get_job_node_assigned( 1 ), true );
		TS_ASSERT_EQUALS( prelim_tracker.get_job_node_assigned( 2 ), false );
		TS_ASSERT_EQUALS( prelim_tracker.get_job_node_assigned( 3 ), false );
		TS_ASSERT_EQUALS( prelim_tracker.get_job_index_starting_job_node( 1 ), 1 );
		TS_ASSERT_EQUALS( prelim_tracker.get_job_index_ending_job_node( 1 ),   5 );
		TS_ASSERT_EQUALS( prelim_tracker.get_job_index_starting_job_node( 2 ), 6 );
		TS_ASSERT_EQUALS( prelim_tracker.get_job_index_ending_job_node( 2 ),   11 );
		TS_ASSERT_EQUALS( prelim_tracker.get_job_index_starting_job_node( 3 ), 0 );
		TS_ASSERT_EQUALS( prelim_tracker.get_job_index_ending_job_node( 3 ),   0 );

		//Although they are the same input pose, since they are defined in Job, they are counted as separate poses.
		TS_ASSERT_EQUALS( tracker.last_job_for_input_source_id(2), 11);

		TS_ASSERT( tracker.started_jobs_for_node(2).member(6));
		TS_ASSERT( tracker.started_jobs_for_node(2).member(7));
		TS_ASSERT( tracker.started_jobs_for_node(2).member(8));
		TS_ASSERT( tracker.started_jobs_for_node(2).member(9));
		TS_ASSERT( tracker.started_jobs_for_node(2).member(10));
		TS_ASSERT( tracker.started_jobs_for_node(2).member(11));
		TS_ASSERT( tracker.started_jobs().member(6));
		TS_ASSERT( tracker.started_jobs().member(7));
		TS_ASSERT( tracker.started_jobs().member(8));
		TS_ASSERT( tracker.started_jobs().member(9));
		TS_ASSERT( tracker.started_jobs().member(10));
		TS_ASSERT( tracker.started_jobs().member(11));
		TS_ASSERT( tracker.started_jobs().member(6));
		TS_ASSERT( tracker.started_jobs().member(7));
		TS_ASSERT( tracker.started_jobs().member(8));
		TS_ASSERT( tracker.started_jobs().member(9));
		TS_ASSERT( tracker.started_jobs().member(10));
		TS_ASSERT( tracker.started_jobs().member(11));

		jobs = djq.determine_job_list_and_track( 2, 6 );
		prelim_tracker = djq.get_prelim_larval_job_tracker();

		TS_ASSERT_EQUALS( jobs.size(), 5 );
		TS_ASSERT_EQUALS( prelim_tracker.get_job_node_assigned( 1 ), true );
		TS_ASSERT_EQUALS( prelim_tracker.get_job_node_assigned( 2 ), true );
		TS_ASSERT_EQUALS( prelim_tracker.get_job_node_assigned( 3 ), false );
		TS_ASSERT_EQUALS( prelim_tracker.get_job_index_starting_job_node( 1 ), 1 );
		TS_ASSERT_EQUALS( prelim_tracker.get_job_index_ending_job_node( 1 ),   5 );
		TS_ASSERT_EQUALS( prelim_tracker.get_job_index_starting_job_node( 2 ), 6 );
		TS_ASSERT_EQUALS( prelim_tracker.get_job_index_ending_job_node( 2 ),  16 );
		TS_ASSERT_EQUALS( prelim_tracker.get_job_index_starting_job_node( 3 ), 0 );
		TS_ASSERT_EQUALS( prelim_tracker.get_job_index_ending_job_node( 3 ),   0 );
		TS_ASSERT_EQUALS( tracker.last_job_for_input_source_id(2), 16);

		jobs = djq.determine_job_list_and_track( 3, 6 );
		prelim_tracker = djq.get_prelim_larval_job_tracker();
		TS_ASSERT_EQUALS( jobs.size(), 3 );
		TS_ASSERT_EQUALS( prelim_tracker.get_job_node_assigned( 1 ), true );
		TS_ASSERT_EQUALS( prelim_tracker.get_job_node_assigned( 2 ), true );
		TS_ASSERT_EQUALS( prelim_tracker.get_job_node_assigned( 3 ), true );
		TS_ASSERT_EQUALS( prelim_tracker.get_job_index_starting_job_node( 1 ),  1 );
		TS_ASSERT_EQUALS( prelim_tracker.get_job_index_ending_job_node( 1 ),    5 );
		TS_ASSERT_EQUALS( prelim_tracker.get_job_index_starting_job_node( 2 ),  6 );
		TS_ASSERT_EQUALS( prelim_tracker.get_job_index_ending_job_node( 2 ),   16 );
		TS_ASSERT_EQUALS( prelim_tracker.get_job_index_starting_job_node( 3 ), 17 );
		TS_ASSERT_EQUALS( prelim_tracker.get_job_index_ending_job_node( 3 ),   19 );
		TS_ASSERT_EQUALS( tracker.last_job_for_input_source_id(3), 19);

	}

	void test_standard_job_queen_pose_deallocation_messages()
	{

		std::string jobdef_file =
			"<JobDefinitionFile>\n"
			" <Job nstruct=\"5\">\n"
			"  <Input>\n"
			"   <PDB filename=\"1ubq.pdb\"/>\n"
			"  </Input>\n"
			" </Job>\n"
			" <Job nstruct=\"11\">\n"
			"  <Input>\n"
			"   <PDB filename=\"1ubq.pdb\"/>\n"
			"  </Input>\n"
			" </Job>\n"
			" <Job nstruct=\"3\">\n"
			"  <Input>\n"
			"   <PDB filename=\"1ubq.pdb\"/>\n"
			"  </Input>\n"
			" </Job>\n"
			"</JobDefinitionFile>\n";

		core_init(); // all options passed through job-definition file

		DummyJobQueen djq;
		try {
			djq.determine_preliminary_job_list_from_xml_file( jobdef_file );
		} catch (utility::excn::Exception & e ) {
			std::cout << e.msg() << std::endl;
			TS_ASSERT( false );
		}
		JobDigraphOP dag = djq.create_and_set_initial_job_dag();
		TS_ASSERT_EQUALS( dag->num_nodes(), 3 );
		TS_ASSERT_EQUALS( dag->num_edges(), 0 );

		PreliminaryLarvalJobTracker  const & prelim_tracker = djq.get_prelim_larval_job_tracker();
		utility::vector1< core::Size > prelim_nodes( 3 );
		for ( core::Size ii = 1; ii <= 3; ++ii ) prelim_nodes[ ii ] = ii;
		TS_ASSERT_EQUALS( prelim_tracker.get_preliminary_job_node_indices(), prelim_nodes );

		LarvalJobs jobs = djq.determine_job_list( 1, 4 );
		for ( LarvalJobOP node1_job : jobs ) {
			TS_ASSERT_EQUALS( node1_job->inner_job()->input_source().source_id(), 1 );
		}
		std::list< deallocation::DeallocationMessageOP > msgs1 = djq.deallocation_messages();
		TS_ASSERT( msgs1.empty() );

		for ( LarvalJobOP node1_job : jobs ) {
			djq.note_job_completed_and_track( node1_job, jd3_job_status_success, 1 );
		}

		LarvalJobs jobs2 = djq.determine_job_list( 1, 2 );
		for ( LarvalJobOP node1_job : jobs2 ) {
			TS_ASSERT_EQUALS( node1_job->inner_job()->input_source().source_id(), 1 );
		}
		std::list< deallocation::DeallocationMessageOP > msgs2 = djq.deallocation_messages();
		TS_ASSERT( msgs2.empty() );

		for ( LarvalJobOP node1_job : jobs2 ) {
			djq.note_job_completed_and_track( node1_job, jd3_job_status_success, 1 );
		}

		LarvalJobs jobs3 = djq.determine_job_list( 2, 10 );
		TS_ASSERT_EQUALS( jobs3.size(), 10 );

		// After we tell the DJQ that all the jobs for node two have finished and
		// she has started assigning jobs for node2, then she will tell us that
		// it's ok to deallocate the input pose for node 1
		std::list< deallocation::DeallocationMessageOP > msgs3 = djq.deallocation_messages();
		TS_ASSERT_EQUALS( msgs3.size(), 1 );
		typedef deallocation::InputPoseDeallocationMessage   PoseDealloc;
		typedef deallocation::InputPoseDeallocationMessageOP PoseDeallocOP;
		PoseDeallocOP msg3 = utility::pointer::dynamic_pointer_cast< PoseDealloc > ( msgs3.front() );
		TS_ASSERT( msg3 );
		TS_ASSERT_EQUALS( msg3->pose_id(), 1 );
		std::list< deallocation::DeallocationMessageOP > msgs4 = djq.deallocation_messages();
		TS_ASSERT( msgs4.empty() );

		for ( LarvalJobOP node2_job : jobs3 ) {
			djq.note_job_completed_and_track( node2_job, jd3_job_status_success, 1 );
		}

		LarvalJobs jobs4 = djq.determine_job_list( 2, 10 );
		TS_ASSERT_EQUALS( jobs4.size(), 1 );
		std::list< deallocation::DeallocationMessageOP > msgs5 = djq.deallocation_messages();
		TS_ASSERT( msgs5.empty() );

		LarvalJobs jobs5 = djq.determine_job_list( 2, 10 );
		TS_ASSERT( jobs5.empty() );
		std::list< deallocation::DeallocationMessageOP > msgs6 = djq.deallocation_messages();
		TS_ASSERT( msgs6.empty() );

		// ok, let's mark all of the jobs from node 2 complete
		for ( LarvalJobOP node2_job : jobs4 ) {
			djq.note_job_completed_and_track( node2_job, jd3_job_status_success, 1 );
		}

		LarvalJobs jobs6 = djq.determine_job_list( 3, 10 );
		TS_ASSERT_EQUALS( jobs6.size(), 3 );
		std::list< deallocation::DeallocationMessageOP > msgs7 = djq.deallocation_messages();
		TS_ASSERT_EQUALS( msgs7.size(), 1 );
		PoseDeallocOP msg7 = utility::pointer::dynamic_pointer_cast< PoseDealloc > ( msgs7.front() );
		TS_ASSERT( msg7 );
		TS_ASSERT_EQUALS( msg7->pose_id(), 2 );

	}

	void test_standard_job_queen_pose_deallocation_messages_no_preserve_poses()
	{
		core_init_with_additional_options( "-in:file:s 1ubq.pdb" );
		std::string jobdef_file =
			"<JobDefinitionFile>\n"
			" <Job nstruct=\"5\">\n"
			" </Job>\n"
			" <Job nstruct=\"11\">\n"
			" </Job>\n"
			" <Job nstruct=\"3\">\n"
			" </Job>\n"
			"</JobDefinitionFile>\n";

		DummyJobQueen djq;
		try {
			djq.determine_preliminary_job_list_from_xml_file( jobdef_file );
		} catch (utility::excn::Exception & e ) {
			std::cout << e.msg() << std::endl;
			TS_ASSERT( false );
		}
		JobDigraphOP dag = djq.create_and_set_initial_job_dag();
		TS_ASSERT_EQUALS( dag->num_nodes(), 3 );
		TS_ASSERT_EQUALS( dag->num_edges(), 0 );

		PreliminaryLarvalJobTracker const & prelim_tracker = djq.get_prelim_larval_job_tracker();
		utility::vector1< core::Size > prelim_nodes( 3 );
		for ( core::Size ii = 1; ii <= 3; ++ii ) prelim_nodes[ ii ] = ii;
		TS_ASSERT_EQUALS( prelim_tracker.get_preliminary_job_node_indices(), prelim_nodes );

		LarvalJobs jobs = djq.determine_job_list( 1, 4 );
		for ( LarvalJobOP node1_job : jobs ) {
			TS_ASSERT_EQUALS( node1_job->inner_job()->input_source().source_id(), 1 );
		}
		std::list< deallocation::DeallocationMessageOP > msgs1 = djq.deallocation_messages();
		TS_ASSERT( msgs1.empty() );

		for ( LarvalJobOP node1_job : jobs ) {
			djq.note_job_completed_and_track( node1_job, jd3_job_status_success, 1 );
		}

		LarvalJobs jobs2 = djq.determine_job_list( 1, 2 );
		for ( LarvalJobOP node1_job : jobs2 ) {
			TS_ASSERT_EQUALS( node1_job->inner_job()->input_source().source_id(), 1 );
		}
		std::list< deallocation::DeallocationMessageOP > msgs2 = djq.deallocation_messages();
		TS_ASSERT( msgs2.empty() );

		for ( LarvalJobOP node1_job : jobs2 ) {
			djq.note_job_completed_and_track( node1_job, jd3_job_status_success, 1 );
		}

		LarvalJobs jobs3 = djq.determine_job_list( 2, 10 );
		TS_ASSERT_EQUALS( jobs3.size(), 10 );

		// After we tell the DJQ that all the jobs for node two have finished and
		// she has started assigning jobs for node2, then she will tell us that
		// it's ok to deallocate the input pose for node 1
		std::list< deallocation::DeallocationMessageOP > msgs3 = djq.deallocation_messages();
		TS_ASSERT_EQUALS( msgs3.size(), 1 );
		typedef deallocation::InputPoseDeallocationMessage   PoseDealloc;
		typedef deallocation::InputPoseDeallocationMessageOP PoseDeallocOP;
		PoseDeallocOP msg3 = utility::pointer::dynamic_pointer_cast< PoseDealloc > ( msgs3.front() );
		TS_ASSERT( msg3 );
		TS_ASSERT_EQUALS( msg3->pose_id(), 1 );
		std::list< deallocation::DeallocationMessageOP > msgs4 = djq.deallocation_messages();
		TS_ASSERT( msgs4.empty() );

		for ( LarvalJobOP node2_job : jobs3 ) {
			djq.note_job_completed_and_track( node2_job, jd3_job_status_success, 1 );
		}

		LarvalJobs jobs4 = djq.determine_job_list( 2, 10 );
		TS_ASSERT_EQUALS( jobs4.size(), 1 );
		std::list< deallocation::DeallocationMessageOP > msgs5 = djq.deallocation_messages();
		TS_ASSERT( msgs5.empty() );

		LarvalJobs jobs5 = djq.determine_job_list( 2, 10 );
		TS_ASSERT( jobs5.empty() );
		std::list< deallocation::DeallocationMessageOP > msgs6 = djq.deallocation_messages();
		TS_ASSERT( msgs6.empty() );

		// ok, let's mark all of the jobs from node 2 complete
		for ( LarvalJobOP node2_job : jobs4 ) {
			djq.note_job_completed_and_track( node2_job, jd3_job_status_success, 1 );
		}

		LarvalJobs jobs6 = djq.determine_job_list( 3, 10 );
		TS_ASSERT_EQUALS( jobs6.size(), 3 );
		std::list< deallocation::DeallocationMessageOP > msgs7 = djq.deallocation_messages();
		TS_ASSERT_EQUALS( msgs7.size(), 1 );
		PoseDeallocOP msg7 = utility::pointer::dynamic_pointer_cast< PoseDealloc > ( msgs7.front() );
		TS_ASSERT( msg7 );
		TS_ASSERT_EQUALS( msg7->pose_id(), 2 );

	}

	void test_standard_job_queen_pose_deallocation_messages_and_preserve_input_poses()
	{
		core_init_with_additional_options( "-in:file:s 1ubq.pdb -jd3:load_input_poses_only_once" );

		// The first two Jobs will read from the command line, and the last one will read from
		// the Input tag; so the first two preliminary job nodes will both use the same
		// pose_id. After the jobs finish for node 2, then the DJQ should report that it's now time
		// to deallocate pose #1.
		std::string jobdef_file =
			"<JobDefinitionFile>\n"
			" <Job nstruct=\"5\">\n"
			" </Job>\n"
			" <Job nstruct=\"11\">\n"
			" </Job>\n"
			" <Job nstruct=\"3\">\n"
			"  <Input>\n"
			"   <PDB filename=\"1ubq.pdb\"/>\n"
			"  </Input>\n"
			" </Job>\n"
			"</JobDefinitionFile>\n";

		DummyJobQueen djq;
		try {
			djq.determine_preliminary_job_list_from_xml_file( jobdef_file );
		} catch (utility::excn::Exception & e ) {
			std::cout << e.msg() << std::endl;
			TS_ASSERT( false );
		}
		JobDigraphOP dag = djq.create_and_set_initial_job_dag();
		TS_ASSERT_EQUALS( dag->num_nodes(), 3 );
		TS_ASSERT_EQUALS( dag->num_edges(), 0 );

		PreliminaryLarvalJobTracker const & prelim_tracker = djq.get_prelim_larval_job_tracker();
		utility::vector1< core::Size > prelim_nodes( 3 );
		for ( core::Size ii = 1; ii <= 3; ++ii ) prelim_nodes[ ii ] = ii;
		TS_ASSERT_EQUALS( prelim_tracker.get_preliminary_job_node_indices(), prelim_nodes );

		LarvalJobs jobs = djq.determine_job_list( 1, 4 );
		for ( LarvalJobOP node1_job : jobs ) {
			TS_ASSERT_EQUALS( node1_job->inner_job()->input_source().source_id(), 1 );
		}
		std::list< deallocation::DeallocationMessageOP > msgs1 = djq.deallocation_messages();
		TS_ASSERT( msgs1.empty() );

		for ( LarvalJobOP node1_job : jobs ) {
			djq.note_job_completed_and_track( node1_job, jd3_job_status_success, 1 );
		}

		LarvalJobs jobs2 = djq.determine_job_list( 1, 2 );
		for ( LarvalJobOP node1_job : jobs2 ) {
			TS_ASSERT_EQUALS( node1_job->inner_job()->input_source().source_id(), 1 );
		}
		std::list< deallocation::DeallocationMessageOP > msgs2 = djq.deallocation_messages();
		TS_ASSERT( msgs2.empty() );

		for ( LarvalJobOP node1_job : jobs2 ) {
			djq.note_job_completed_and_track( node1_job, jd3_job_status_success, 1 );
		}

		LarvalJobs jobs3 = djq.determine_job_list( 2, 10 );
		TS_ASSERT_EQUALS( jobs3.size(), 10 );

		// After we tell the DJQ that all the jobs for node two have finished and
		// she has started assigning jobs for node2, then she would tell us that
		// it's ok to deallocate the input pose for node 1 IF the load_input_poses_only_once flag
		// weren't on the command line, but it is, so she'll not tell us to deallocate anything
		std::list< deallocation::DeallocationMessageOP > msgs3 = djq.deallocation_messages();
		TS_ASSERT( msgs3.empty() );

		for ( LarvalJobOP node2_job : jobs3 ) {
			djq.note_job_completed_and_track( node2_job, jd3_job_status_success, 1 );
		}

		LarvalJobs jobs4 = djq.determine_job_list( 2, 10 );
		TS_ASSERT_EQUALS( jobs4.size(), 1 );
		std::list< deallocation::DeallocationMessageOP > msgs5 = djq.deallocation_messages();
		TS_ASSERT( msgs5.empty() );

		LarvalJobs jobs5 = djq.determine_job_list( 2, 10 );
		TS_ASSERT( jobs5.empty() );
		std::list< deallocation::DeallocationMessageOP > msgs6 = djq.deallocation_messages();
		TS_ASSERT( msgs6.empty() );

		// ok, let's mark all of the jobs from node 2 complete
		for ( LarvalJobOP node2_job : jobs4 ) {
			djq.note_job_completed_and_track( node2_job, jd3_job_status_success, 1 );
		}

		LarvalJobs jobs6 = djq.determine_job_list( 3, 10 );
		TS_ASSERT_EQUALS( jobs6.size(), 3 );

		// OK! Now the DJQ should say to deallocate the first input pose.
		std::list< deallocation::DeallocationMessageOP > msgs7 = djq.deallocation_messages();
		TS_ASSERT_EQUALS( msgs7.size(), 1 );
		typedef deallocation::InputPoseDeallocationMessage   PoseDealloc;
		typedef deallocation::InputPoseDeallocationMessageOP PoseDeallocOP;
		PoseDeallocOP msg7 = utility::pointer::dynamic_pointer_cast< PoseDealloc > ( msgs7.front() );
		TS_ASSERT( msg7 );
		TS_ASSERT_EQUALS( msg7->pose_id(), 1 );


	}

	void test_sjq_remote_node_mature_larval_job() {
		TS_ASSERT( true );
		// heh - looks like I never wrote this unit test?
	}

private:
	bool local_options_added_;

};
