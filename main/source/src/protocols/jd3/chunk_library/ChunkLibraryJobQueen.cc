// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file   protocols/jd3/chunk_library/ChunkLibraryJobQueen.cc
/// @brief  ChunkLibraryJobQueen class's method definitions
/// @author Andrew Leaver-Fay (aleaverfay@gmail.com)

//unit headers
#include <protocols/jd3/chunk_library/ChunkLibraryJobQueen.hh>

// package headers
#include <protocols/jd3/InnerLarvalJob.hh>
#include <protocols/jd3/chunk_library/ChunkLibraryInnerLarvalJob.hh>
#include <protocols/jd3/LarvalJob.hh>
#include <protocols/jd3/JobDigraph.hh>
#include <protocols/jd3/JobOutputIndex.hh>
#include <protocols/jd3/chunk_library/MoverAndChunkLibraryJob.hh>
#include <protocols/jd3/output/MultipleOutputSpecification.hh>
#include <protocols/jd3/output/MultipleOutputter.hh>
#include <protocols/jd3/chunk_library_inputters/ChunkLibraryInputSource.hh>
#include <protocols/jd3/chunk_library_inputters/PDBChunkLibraryInputter.hh>
#include <protocols/jd3/chunk_library_inputters/PDBChunkLibraryInputterCreator.hh>
#include <protocols/jd3/chunk_library_inputters/ChunkLibraryInputterFactory.hh>
//#include <protocols/jd3/pose_outputters/PDBPoseOutputter.hh>
#include <protocols/jd3/pose_outputters/DeNovoSilentFilePoseOutputter.hh>
#include <protocols/jd3/pose_outputters/DeNovoSilentFilePoseOutputterCreator.hh>
#include <protocols/jd3/pose_outputters/PoseOutputSpecification.hh>
#include <protocols/jd3/pose_outputters/PoseOutputter.hh>
#include <protocols/jd3/pose_outputters/PoseOutputterCreator.hh>
#include <protocols/jd3/pose_outputters/PoseOutputterFactory.hh>
#include <protocols/jd3/pose_outputters/SecondaryPoseOutputter.hh>
#include <protocols/jd3/deallocation/DeallocationMessage.hh>
#include <protocols/jd3/deallocation/InputPoseDeallocationMessage.hh>

//project headers
#include <core/pose/Pose.hh>
#include <core/import_pose/import_pose_options.hh>
// #include <basic/resource_manager/JobOptions.hh>

//utility headers
#include <utility/file/FileName.hh>
#include <utility/file/PathName.hh>
#include <utility/keys/VariantKey.hh>
#include <utility/options/OptionCollection.hh>
#include <utility/options/keys/OptionKeyList.hh>
#include <utility/pointer/ReferenceCount.hh>
#include <utility/tag/Tag.hh>
#include <utility/tag/XMLSchemaGeneration.hh>
#include <utility/tag/XMLSchemaValidation.hh>
#include <utility/tag/xml_schema_group_initialization.hh>
#include <utility/vector1.hh>

//basic headers
#include <basic/options/option.hh>
#include <basic/options/util.hh>
#include <basic/options/keys/out.OptionKeys.gen.hh>
#include <basic/options/keys/in.OptionKeys.gen.hh>
#include <basic/options/option.cc.gen.hh>
#include <basic/datacache/ConstDataMap.hh>
#include <basic/Tracer.hh>

static basic::Tracer TR( "protocols.jd3.chunk_library.ChunkLibraryJobQueen" );


namespace protocols {
namespace jd3 {
namespace chunk_library {

ChunkLibraryPreliminaryLarvalJob::ChunkLibraryPreliminaryLarvalJob() = default;
ChunkLibraryPreliminaryLarvalJob::~ChunkLibraryPreliminaryLarvalJob() = default;
ChunkLibraryPreliminaryLarvalJob::ChunkLibraryPreliminaryLarvalJob( ChunkLibraryPreliminaryLarvalJob const & /*src*/ ) = default;

ChunkLibraryPreliminaryLarvalJob &
ChunkLibraryPreliminaryLarvalJob::operator = ( ChunkLibraryPreliminaryLarvalJob const & rhs )
{
	if ( this != &rhs ) {
		inner_job = rhs.inner_job;
		job_tag   = rhs.job_tag;
		job_options = rhs.job_options;
		chunk_library_inputter = rhs.chunk_library_inputter;
	}
	return *this;
}



utility::excn::Exception
bad_inner_job_exception()
{
	return CREATE_EXCEPTION(utility::excn::Exception, "The InnerLarvalJob provided to the ChunkLibraryJobQueen by the DerivedJobQueen must derive from ChunkLibraryInnerLarvalJob. Cannot proceed.");
}


ChunkLibraryJobQueen::ChunkLibraryJobQueen() :
	use_factory_provided_chunk_library_inputters_( true ),
	use_factory_provided_pose_outputters_( true ),
	override_default_outputter_( false ),
	required_initialization_performed_( false ),
	larval_job_counter_( 0 ),
	preliminary_larval_jobs_determined_( false ),
	curr_inner_larval_job_index_( 0 ),
	njobs_made_for_curr_inner_larval_job_( 0 ),
	input_pose_counter_( 0 )
{
	do_not_accept_all_chunk_library_inputters_from_factory();
	chunk_library_inputters::ChunkLibraryInputterCreatorOP chunk_library_inputter_creator( new chunk_library_inputters::PDBChunkLibraryInputterCreator );
	//chunk_library_inputters::ChunkLibraryInputterCreatorOP chunk_library_inputter_creator2( new chunk_library_inputters::SilentChunkLibraryInputterCreator );
	allow_chunk_library_inputter( chunk_library_inputter_creator );
	//allow_chunk_library_inputter( chunk_library_inputter_creator2 );

	do_not_accept_all_pose_outputters_from_factory();
	//chunk_library_inputters::PoseOutputerCreatorOP pose_outputter_creator( new pose_outputters::PDBPoseOutputterCreator );
	pose_outputters::PoseOutputterCreatorOP pose_outputter_creator2( new pose_outputters::DeNovoSilentFilePoseOutputterCreator );
	//allow_pose_outputter( pose_outputter_creator );
	allow_pose_outputter( pose_outputter_creator2 );

	// begin to populate the per-job options object
	chunk_library_inputters::ChunkLibraryInputterFactory::get_instance()->list_options_read( inputter_options_ );
	pose_outputters::PoseOutputterFactory::get_instance()->list_outputter_options_read( outputter_options_ );
	pose_outputters::PoseOutputterFactory::get_instance()->list_secondary_outputter_options_read( secondary_outputter_options_ );

	// add options?
	// In theory we'd use some function provided by the stepwise options classes here or something...
	//add_option( basic::options::OptionKeys::in::file::fasta );
	/*utility::options::OptionKeyList opts;
	core::scoring::list_read_options_in_get_score_function( opts );
	core::pack::task::PackerTask::list_options_read( opts );
	core::pack::task::operation::ReadResfile::list_options_read( opts );
	protocols::simple_moves::PackRotamersMover::list_options_read( opts );
	add_options( opts );
	add_option( basic::options::OptionKeys::minimize_sidechains );
	add_option( basic::options::OptionKeys::min_pack );
	add_option( basic::options::OptionKeys::off_rotamer_pack );*/
}

ChunkLibraryJobQueen::~ChunkLibraryJobQueen() = default;

utility::options::OptionTypes
option_type_from_key(
	utility::options::OptionKey const & key
)
{
	using namespace utility::options;
	if ( dynamic_cast< StringOptionKey const * > ( &key ) ) {
		return STRING_OPTION;
	} else if ( dynamic_cast< StringVectorOptionKey const * > ( &key ) ) {
		return STRING_VECTOR_OPTION;
	} else if ( dynamic_cast< PathOptionKey const * > ( &key ) ) {
		return PATH_OPTION;
	} else if ( dynamic_cast< PathVectorOptionKey const * > ( &key ) ) {
		return PATH_VECTOR_OPTION;
	} else if ( dynamic_cast< FileOptionKey const * > ( &key ) ) {
		return FILE_OPTION;
	} else if ( dynamic_cast< FileVectorOptionKey const * > ( &key ) ) {
		return FILE_VECTOR_OPTION;
	} else if ( dynamic_cast< RealOptionKey const * > ( &key ) ) {
		return REAL_OPTION;
	} else if ( dynamic_cast< RealVectorOptionKey const * > ( &key ) ) {
		return REAL_VECTOR_OPTION;
	} else if ( dynamic_cast< IntegerOptionKey const * > ( &key ) ) {
		return INTEGER_OPTION;
	} else if ( dynamic_cast< IntegerVectorOptionKey const * > ( &key ) ) {
		return INTEGER_VECTOR_OPTION;
	} else if ( dynamic_cast< BooleanOptionKey const * > ( &key ) ) {
		return BOOLEAN_OPTION;
	} else if ( dynamic_cast< BooleanVectorOptionKey const * > ( &key ) ) {
		return BOOLEAN_VECTOR_OPTION;
	} else if ( dynamic_cast< ResidueChainVectorOptionKey const * > ( &key ) ) {
		return RESIDUE_CHAIN_VECTOR_OPTION;
	}
	return UNKNOWN_OPTION;
}

utility::tag::XMLSchemaType
value_attribute_type_for_option(
	utility::options::OptionTypes const & key
)
{
	using namespace utility::options;
	using namespace utility::tag;
	switch ( key ) {
	case STRING_OPTION :
	case STRING_VECTOR_OPTION :
	case PATH_OPTION :
	case PATH_VECTOR_OPTION :
	case FILE_OPTION :
	case FILE_VECTOR_OPTION :
	case RESIDUE_CHAIN_VECTOR_OPTION :
		return xs_string;
	case REAL_OPTION :
		return xsct_real;
	case REAL_VECTOR_OPTION :
		return xsct_real_wsslist;
	case INTEGER_OPTION :
		return xs_integer;
	case INTEGER_VECTOR_OPTION :
		return xsct_int_wsslist;
	case BOOLEAN_OPTION :
		return xsct_rosetta_bool;
	case BOOLEAN_VECTOR_OPTION :
		return xsct_bool_wsslist; // note: double check that options system uses utility/string_funcs.hh to cast from strings to bools.
	default :
		throw CREATE_EXCEPTION(utility::excn::Exception,  "Unsupported option type hit in ChunkLibraryJobQueen.cc's value_attribute" );
	}
	return "ERROR";
}

std::string
ChunkLibraryJobQueen::job_definition_xsd() const
{
	using namespace utility::tag;
	using namespace utility::options;
	using namespace basic::options;

	utility::options::OptionKeyList all_options = concatenate_all_options();

	XMLSchemaDefinition xsd;

	if ( use_factory_provided_chunk_library_inputters_ ) {
		chunk_library_inputters::ChunkLibraryInputterFactory::get_instance()->define_chunk_library_inputter_xml_schema( xsd );
	} else {
		utility::tag::define_xml_schema_group(
			inputter_creators_,
			chunk_library_inputters::ChunkLibraryInputterFactory::chunk_library_inputter_xml_schema_group_name(),
			& chunk_library_inputters::ChunkLibraryInputterFactory::complex_type_name_for_chunk_library_inputter,
			xsd );
	}

	XMLSchemaSimpleSubelementList input_subelements;
	input_subelements.add_group_subelement( & chunk_library_inputters::ChunkLibraryInputterFactory::chunk_library_inputter_xml_schema_group_name );
	XMLSchemaComplexTypeGenerator input_ct;
	input_ct
		.element_name( "Input" )
		.description("XRW TO DO")
		.complex_type_naming_func( & job_def_complex_type_name )
		.set_subelements_pick_one( input_subelements )
		.write_complex_type_to_schema( xsd );

	if ( use_factory_provided_pose_outputters_ ) {
		pose_outputters::PoseOutputterFactory::get_instance()->define_pose_outputter_xml_schema( xsd );
	} else {
		pose_outputters::PoseOutputterFactory::get_instance()->define_secondary_pose_outputter_xml_schema( xsd );
		utility::tag::define_xml_schema_group(
			outputter_creators_,
			pose_outputters::PoseOutputterFactory::pose_outputter_xml_schema_group_name(),
			& pose_outputters::PoseOutputterFactory::complex_type_name_for_pose_outputter,
			xsd );
	}

	XMLSchemaSimpleSubelementList output_subelements;
	output_subelements.add_group_subelement( & pose_outputters::PoseOutputterFactory::pose_outputter_xml_schema_group_name );
	XMLSchemaComplexTypeGenerator output_ct;
	output_ct
		.element_name( "Output" )
		.description( "XRW TO DO" )
		.complex_type_naming_func( & job_def_complex_type_name )
		.set_subelements_pick_one( output_subelements )
		.write_complex_type_to_schema( xsd );

	XMLSchemaSimpleSubelementList secondary_output_subelements;
	secondary_output_subelements.add_group_subelement( & pose_outputters::PoseOutputterFactory::secondary_pose_outputter_xml_schema_group_name );
	XMLSchemaComplexTypeGenerator secondary_output_ct;
	secondary_output_ct
		.element_name( "SecondaryOutput" )
		.description( "XRW TO DO" )
		.complex_type_naming_func( & job_def_complex_type_name )
		.set_subelements_single_appearance_optional( secondary_output_subelements )
		.write_complex_type_to_schema( xsd );

	// write out option set -- this should be method-extracted into a place accessible to other job queens
	if  ( ! all_options.empty() ) {
		XMLSchemaComplexTypeGenerator option_generator;
		XMLSchemaSimpleSubelementList option_subelements;

		std::set< utility::keys::VariantKey< utility::options::OptionKey > > already_output_options;

		for ( auto const & iter : all_options ) {
			AttributeList attributes;
			utility::options::OptionKey const & opt_key( iter() );

			// only output each option once, even if it is read in more than
			// one context
			if ( already_output_options.count( opt_key ) ) continue;
			already_output_options.insert( opt_key );

			OptionTypes opt_type = option_type_from_key( opt_key );
			XMLSchemaType value_attribute_type = value_attribute_type_for_option( opt_type );
			if ( option[ opt_key ].has_default() ) {
				if ( opt_type == BOOLEAN_OPTION ) {
					attributes + XMLSchemaAttribute::attribute_w_default(  "value", value_attribute_type, "XRW TO DO",  option[ opt_key ].raw_default_string( ) );
				} else {
					attributes + XMLSchemaAttribute::attribute_w_default(  "value", value_attribute_type, "XRW TO DO",  option[ opt_key ].raw_default_string( ) );
				}
			} else { // no default; value is required, unless it's a boolean option
				if ( opt_type == BOOLEAN_OPTION ) {
					attributes + XMLSchemaAttribute::attribute_w_default(  "value", value_attribute_type, "XRW TO DO",  "false"  );
				} else {
					attributes + XMLSchemaAttribute::required_attribute( "value", value_attribute_type , "XRW TO DO" );
				}
			}

			std::string decolonized_name = basic::options::replace_option_namespace_colons_with_underscores( iter );
			option_subelements.add_simple_subelement( decolonized_name, attributes , "");
		}
		option_generator.element_name( "Options" )
			.description( "XRW TO DO" )
			.complex_type_naming_func( & job_def_complex_type_name )
			.set_subelements_single_appearance_optional( option_subelements )
			.write_complex_type_to_schema( xsd );
	}

	XMLSchemaComplexTypeGenerator job_ct;

	// Job <Input> element is required
	XMLSchemaSimpleSubelementList job_input_subelement;
	job_input_subelement.add_already_defined_subelement( "Input", & job_def_complex_type_name );
	job_ct.add_ordered_subelement_set_as_required( job_input_subelement );

	// Job <Output> element is optional
	XMLSchemaSimpleSubelementList job_output_subelement;
	job_output_subelement.add_already_defined_subelement( "Output", & job_def_complex_type_name );
	job_ct.add_ordered_subelement_set_as_optional( job_output_subelement );

	// Job <Options> element is optional
	XMLSchemaSimpleSubelementList job_options_subelement;
	job_options_subelement.add_already_defined_subelement( "Options", & job_def_complex_type_name );
	job_ct.add_ordered_subelement_set_as_optional( job_options_subelement );

	// Ask the derived class for what else belongs in the Job element.
	// NOTE: The derived class should only call the add_ordered_subelement_set_* functions
	// of the XMLSchemaComplexTypeGenerator or the <Input>, <Output>, and <Option>
	// subelements will be overwritten.
	append_job_tag_subelements( xsd, job_ct );

	// verify that the derived class did not call anything besides the add_ordered_subelement_set_*
	// functions.
	if ( job_ct.subelement_behavior() != se_ordered_sets ) {
		throw CREATE_EXCEPTION(utility::excn::Exception,  "Subclass of ChunkLibraryJobQueen's append_job_tag_subelements"
			" method invokes a method of the XMLSchemaComplexTypeGenerator that overwrote the <Input>, <Output>, and"
			" <Options> elements.  It should only call methods named \"add_ordered_subelement_set_*\"" );
	}

	job_ct
		.element_name( "Job" )
		.description( "XRW TO DO" )
		.complex_type_naming_func( & job_def_complex_type_name )
		.add_attribute( XMLSchemaAttribute::attribute_w_default(  "nstruct", xsct_non_negative_integer, "XRW TO DO",  "1"  ))
		.write_complex_type_to_schema( xsd );

	XMLSchemaComplexTypeGenerator common_block_ct_gen;

	// Common block <Options> subelement
	XMLSchemaSimpleSubelementList common_block_option_subelement;
	common_block_option_subelement.add_already_defined_subelement( "Options", & job_def_complex_type_name );
	common_block_ct_gen.add_ordered_subelement_set_as_optional( common_block_option_subelement );

	// Ask the derived class for what else belongs in the Common element.
	// NOTE: The derived class should only call the add_ordered_subelement_set_* functions
	// of the XMLSchemaComplexTypeGenerator or the <Options> subelement will be
	// overwritten.
	append_common_tag_subelements( xsd, common_block_ct_gen );

	// verify that the derived class did not call anything besides the add_ordered_subelement_set_*
	// functions.
	if ( common_block_ct_gen.subelement_behavior() != se_ordered_sets ) {
		throw CREATE_EXCEPTION(utility::excn::Exception,  "Subclass of ChunkLibraryJobQueen's append_job_tag_subelements"
			" method invokes a method of the XMLSchemaComplexTypeGenerator that overwrote the <Input>, <Output>, and"
			" <Options> elements.  It should only call methods named \"add_ordered_subelement_set_*\"" );
	}

	common_block_ct_gen
		.element_name( "Common" )
		.description( "XRW TO DO" )
		.complex_type_naming_func( & job_def_complex_type_name )
		.write_complex_type_to_schema( xsd );

	XMLSchemaComplexTypeGenerator job_def_file_ct;
	XMLSchemaSimpleSubelementList job_def_subelements;
	job_def_subelements.add_already_defined_subelement( "Common", & job_def_complex_type_name, 0, 1 );
	job_def_subelements.add_already_defined_subelement( "Job", & job_def_complex_type_name, 1, xsminmax_unbounded );
	job_def_file_ct.element_name( "JobDefinitionFile" )
		.complex_type_naming_func( & job_def_complex_type_name )
		.description( "XRW TO DO" )
		.set_subelements_single_appearance_required_and_ordered( job_def_subelements )
		.write_complex_type_to_schema( xsd );

	XMLSchemaElement root_element;
	root_element.name( "JobDefinitionFile" ).type_name( job_def_complex_type_name( "JobDefinitionFile" ));
	xsd.add_top_level_element( root_element );

	std::string xsd_string = xsd.full_definition();

	try {
		utility::tag::test_if_schema_is_valid( xsd_string );
	} catch ( utility::excn::Exception const & e ) {
		std::ostringstream oss;
		oss << "The XML Schema for the job definition file is invalid.  The error message is:\n" << e.msg()
			<< "\nAnd the whole schema is:\n" << xsd_string << "\nThis executable cannot be used in its"
			<< " current state.\n";
		throw CREATE_EXCEPTION(utility::excn::Exception,  oss.str() );
	}

	return xsd_string;

}

std::string
ChunkLibraryJobQueen::resource_definition_xsd() const
{
	// TO DO!
	return "";
}

/// @details The base class provides a digraph with a single node -- that is, all the jobs
/// are independent of each other.  This is equivalent to the kind of jobs that could be
/// run in JD2.
JobDigraphOP
ChunkLibraryJobQueen::create_initial_job_dag()
{
	determine_preliminary_job_list();

	core::Size n_pjns = preliminary_larval_jobs_.size();
	preliminary_job_node_inds_.resize( n_pjns );
	preliminary_job_nodes_complete_.resize( n_pjns );
	pjn_job_ind_begin_.resize( n_pjns );
	pjn_job_ind_end_.resize( n_pjns );

	for ( core::Size ii = 1; ii <= n_pjns; ++ii ) {
		preliminary_job_node_inds_[ ii ] = ii;
		pjn_job_ind_begin_[ ii ] = pjn_job_ind_end_[ ii ] = 0;
		preliminary_job_nodes_complete_[ ii ] = 0;
	}
	// create a DAG with as many nodes in it as there are preliminary larval jobs
	job_graph_ = JobDigraphOP( new JobDigraph( preliminary_larval_jobs_.size() ) );
	return job_graph_;
}

void ChunkLibraryJobQueen::update_job_dag( JobDigraphUpdater & ) {}

/// @details The process begins by first constructing the job definition and resource definition
/// XSDs.  With these schemas, the %ChunkLibraryJobQueen validates the input XML files (if present).
/// The %ChunkLibraryJobQueen then populates preliminary versions of LarvalJob objects./ If the XSD
/// includes "command line options" (which may be specified either from the command line or in
/// the <options> sections of the Job XML file), the %ChunkLibraryJobQueen loads the preliminary
/// LarvalJob objects with the options. These preliminary LarvalJob objects will not have been
/// nstruct expanded (i.e. if there are 100 nstruct for each of 5 different jobs, then there will
/// only be 5 preliminary larval jobs created). It then passes the preliminary LarvalJob list and
/// the TagOP objects for each preliminary LarvalJob to the derived class through the
/// refine_job_list method.
LarvalJobs
ChunkLibraryJobQueen::determine_job_list( Size job_dag_node_index, Size max_njobs )
{
	// ok -- we're going to look for a job definition file, and failing that, fall back on
	// the ChunkLibraryInputterFactory to determine where the input sources are coming from.

	if ( ! preliminary_larval_jobs_determined_ ) {
		determine_preliminary_job_list();
	}

	LarvalJobs larval_jobs;
	if ( job_dag_node_index <= preliminary_larval_jobs_.size() ) {

		// now that the ChunkLibraryPreliminaryLarvalJobs have been constructed, go ahead
		// and start delivering LarvalJobs to the JobDistributor.
		larval_jobs = next_batch_of_larval_jobs_from_prelim( job_dag_node_index, max_njobs );
	} else {
		utility_exit_with_message("Please override determine_job_list from the SJQ for nodes other than the prelininary nodes.");
	}

	for ( auto job : larval_jobs ) {
		core::Size source_id = job->inner_job()->input_source().source_id();
		if ( !last_job_for_input_pose_.count( source_id ) ) {
			last_job_for_input_pose_[ source_id ] = job->job_index();
		} else if ( last_job_for_input_pose_[ source_id ] < job->job_index() ) {
			last_job_for_input_pose_[ source_id ] = job->job_index();
		}
	}

	return larval_jobs;
}

bool
ChunkLibraryJobQueen::has_job_previously_been_output( protocols::jd3::LarvalJobCOP job )
{
	ChunkLibraryInnerLarvalJobCOP inner_job = utility::pointer::dynamic_pointer_cast< ChunkLibraryInnerLarvalJob const > ( job->inner_job() );
	if ( ! inner_job ) { throw bad_inner_job_exception(); }
	utility::options::OptionCollectionCOP job_options = options_for_job( *inner_job );
	return pose_outputter_for_job( *inner_job )->job_has_already_completed( *job, *job_options );
}

JobOP
ChunkLibraryJobQueen::mature_larval_job(
	protocols::jd3::LarvalJobCOP larval_job,
	utility::vector1< JobResultCOP > const & input_results
)
{
	using namespace utility::options;
	using namespace utility::tag;
	using namespace basic::datacache;

	// There are a few pieces of initialization that need to occur on "remote nodes" upon the first
	// time they are asked to mature larval jobs.
	// E.g., the common_block_tags_ must be loaded prior to the call of complete_larval_job_maturation
	// and the ChunkLibraryInputters require the Input TagCOP that corresponds to the preliminary job node
	// that this job came from.
	if ( ! required_initialization_performed_ ) {
		determine_preliminary_job_list();
	}

	// initialize the options collection for this job.
	ChunkLibraryInnerLarvalJobCOP inner_job = utility::pointer::dynamic_pointer_cast< ChunkLibraryInnerLarvalJob const > ( larval_job->inner_job() );
	if ( ! inner_job ) { throw bad_inner_job_exception(); }
	utility::options::OptionCollectionCOP job_options = options_for_job( *inner_job );

	return complete_larval_job_maturation( larval_job, job_options, input_results );
}

/// @details Prepare this job for output by building an OutputSpecification for it and
/// storing this specification in the list of recent successes.
void ChunkLibraryJobQueen::note_job_completed( LarvalJobCOP job, JobStatus status, Size nresults )
{
	//Size const job_id( job->job_index() );
	if ( status == jd3_job_status_success ) {
		ChunkLibraryInnerLarvalJobCOP inner_job = utility::pointer::dynamic_pointer_cast< ChunkLibraryInnerLarvalJob const > ( job->inner_job() );
		if ( ! inner_job ) { throw bad_inner_job_exception(); }

		utility::options::OptionCollectionOP job_options = options_for_job( *inner_job );
		for ( Size ii = 1; ii <= nresults; ++ii ) {
			create_and_store_output_specification_for_job_result( job, *job_options, ii, nresults );
		}
	}

}

void ChunkLibraryJobQueen::completed_job_summary( LarvalJobCOP, core::Size, JobSummaryOP ) {}


std::list< output::OutputSpecificationOP >
ChunkLibraryJobQueen::jobs_that_should_be_output()
{
	std::list< output::OutputSpecificationOP > return_list;
	recent_successes_.swap( return_list );
	return return_list;
}

/// @details Default implementation does not discard any job results.
std::list< JobResultID >
ChunkLibraryJobQueen::job_results_that_should_be_discarded() {
	return std::list< JobResultID >();
}

output::ResultOutputterOP
ChunkLibraryJobQueen::result_outputter(
	output::OutputSpecification const & spec
)
{
	using MOS = output::MultipleOutputSpecification;
	using POS = pose_outputters::PoseOutputSpecification;
	using MO = output::MultipleOutputter;
	using MOOP = output::MultipleOutputterOP;
	debug_assert( dynamic_cast< MOS const * > ( &spec ) );
	auto const & mo_spec( static_cast< MOS const & > (spec) );
	MOOP outputters( new MO );
	for ( Size ii = 1; ii <= mo_spec.output_specifications().size(); ++ii ) {
		output::OutputSpecification const & ii_spec( *mo_spec.output_specifications()[ ii ] );
		debug_assert( dynamic_cast< POS const * > (&ii_spec) );
		auto const & ii_pos( static_cast< POS const & > (ii_spec) );
		// Note assumption that there is always a primary pose outputter -- return here
		// when trying to implement the -no_output option for JD3
		pose_outputters::PoseOutputterOP ii_outputter;
		if ( ii == 1 ) {
			ii_outputter = pose_outputter_for_job( ii_pos );
		} else {
			ii_outputter = secondary_outputter_for_job( ii_pos );
		}
		outputters->append_outputter( ii_outputter );
	}
	return outputters;
}


//void ChunkLibraryJobQueen::completed_job_result(
// LarvalJobCOP job,
// core::Size result_index,
// JobResultOP job_result
//)
//{
// ChunkLibraryInnerLarvalJobCOP inner_job = utility::pointer::dynamic_pointer_cast< ChunkLibraryInnerLarvalJob const > ( job->inner_job() );
// if ( ! inner_job ) { throw bad_inner_job_exception(); }
// pose_outputters::PoseOutputterOP outputter = pose_outputter_for_job( *inner_job );
// PoseJobResultOP pose_result = utility::pointer::dynamic_pointer_cast< PoseJobResult >( job_result );
// if ( ! pose_result ) {
//  utility::excn::EXCN_Msg_Exception( "JobResult handed to ChunkLibraryJobQueen::completed_job_result is not a PoseJobResult or derived from PoseJobResult" );
// }
// utility::options::OptionCollectionOP job_options = options_for_job( *inner_job );
// utility::tag::TagCOP outputter_tag;
// if ( job->inner_job()->jobdef_tag() ) {
//  utility::tag::TagCOP tag = job->inner_job()->jobdef_tag();
//  if ( tag->hasTag( "Output" ) ) {
//   outputter_tag = tag->getTag( "Output" )->getTags()[ 0 ];
//  }
// }
//
// Size const n_results_for_job = results_processed_for_job_[ job->job_index() ].n_results;
// JobOutputIndex output_index;
// output_index.primary_output_index   = job->nstruct_index();
// output_index.n_primary_outputs      = job->nstruct_max();
// output_index.secondary_output_index = result_index;
// output_index.n_secondary_outputs    = n_results_for_job;
//
// assign_output_index( job, result_index, n_results_for_job, output_index );
//
// outputter->write_output_pose( *job, output_index, *job_options, outputter_tag, *pose_result->pose() );
//
// std::list< pose_outputters::SecondaryPoseOutputterOP > secondary_outputters = secondary_outputters_for_job( *inner_job, *job_options );
// for ( std::list< pose_outputters::SecondaryPoseOutputterOP >::const_iterator
//   iter = secondary_outputters.begin(), iter_end = secondary_outputters.end();
//   iter != iter_end; ++iter ) {
//  (*iter)->write_output_pose( *job, output_index, *job_options, *pose_result->pose() );
// }
//
// note_job_result_output_or_discarded( job, result_index );
//}

/// @details Construct the XSD and then invoke the (private) determine_preliminary_job_list_from_xml_file method,
/// which is also invoked by determine_preliminary_job_list.
void
ChunkLibraryJobQueen::determine_preliminary_job_list_from_xml_file( std::string const & job_def_string )
{
	std::string job_def_schema = job_definition_xsd();
	determine_preliminary_job_list_from_xml_file( job_def_string, job_def_schema );
}

void
ChunkLibraryJobQueen::flush()
{
	for ( PoseOutputterMap::const_iterator iter = pose_outputter_map_.begin(); iter != pose_outputter_map_.end(); ++iter ) {
		iter->second->flush();
	}
	for ( SecondaryOutputterMap::const_iterator iter = secondary_outputter_map_.begin(); iter != secondary_outputter_map_.end(); ++iter ) {
		iter->second->flush();
	}
}

std::list< deallocation::DeallocationMessageOP >
ChunkLibraryJobQueen::deallocation_messages()
{
	// We will operate under the assumption that all of the times a Pose will
	// be used will be for sequential jobs, and that it is safe when this function
	// is called to deallocate all of the Poses whose last job index is less than
	// the number of jobs we have handed out so far - but that it would not be
	// safe to deallocate the Pose whose last job index is equal to the number of
	// jobs we've handed out so far because we will perhaps hand out more jobs
	// that use that Pose in just a moment.

	std::list< deallocation::DeallocationMessageOP > messages;
	for ( auto it = last_job_for_input_pose_.begin(); it != last_job_for_input_pose_.end(); /*no inc*/ ) {

		if ( it->second < larval_job_counter_ ) {
			deallocation::DeallocationMessageOP msg( new deallocation::InputPoseDeallocationMessage( it->first ));
			messages.push_back( msg );

			auto pose_iter = input_pose_index_map_.find( it->first );
			if ( pose_iter != input_pose_index_map_.end() ) {
				input_pose_index_map_.erase( pose_iter );
			}
			it = last_job_for_input_pose_.erase( it );
		} else {
			++it;
		}
	}
	return messages;
}

void
ChunkLibraryJobQueen::process_deallocation_message(
	deallocation::DeallocationMessageOP message
)
{
	using namespace deallocation;
	if ( message->deallocation_type() == input_pose_deallocation_msg ) {
		InputPoseDeallocationMessageOP pose_dealloc_msg =
			utility::pointer::dynamic_pointer_cast< InputPoseDeallocationMessage > ( message );
		if ( input_pose_index_map_.count( pose_dealloc_msg->pose_id() ) ) {
			TR << "Erasing previously-stored Pose with index " << pose_dealloc_msg->pose_id() << std::endl;
			input_pose_index_map_.erase( pose_dealloc_msg->pose_id() );
		}
	} else {
		// any other type of deallocation message, for now, will have to be
		// handled by the derived class.
		derived_process_deallocation_message( message );
	}
}


std::string
ChunkLibraryJobQueen::job_def_complex_type_name( std::string const & type )
{ return "job_def_" + type + "_type"; }


void
ChunkLibraryJobQueen::do_not_accept_all_chunk_library_inputters_from_factory()
{
	if ( use_factory_provided_chunk_library_inputters_ ) {
		use_factory_provided_chunk_library_inputters_ = false;
		inputter_options_.clear();
	}
}

void
ChunkLibraryJobQueen::allow_chunk_library_inputter( chunk_library_inputters::ChunkLibraryInputterCreatorOP creator )
{
	if ( use_factory_provided_chunk_library_inputters_ ) {
		use_factory_provided_chunk_library_inputters_ = false;
		// the user has not told us they want to drop the original set of creators
		// so interpret this as them wanting to allow the user to provide another
		// inputter.
		inputter_creator_list_ = chunk_library_inputters::ChunkLibraryInputterFactory::get_instance()->chunk_library_inputter_creators();
		for ( auto const & creator_ii : inputter_creator_list_ ) {
			inputter_creators_[ creator_ii->keyname() ] = creator_ii;
		}
	}
	if ( ! inputter_creators_.count( creator->keyname() ) ) {
		inputter_creator_list_.push_back( creator );
		inputter_creators_[ creator->keyname() ] = creator;
		creator->list_options_read( inputter_options_ );
	}
}

void
ChunkLibraryJobQueen::do_not_accept_all_pose_outputters_from_factory()
{
	if ( use_factory_provided_pose_outputters_ ) {
		use_factory_provided_pose_outputters_ = false;
		override_default_outputter_ = true;
		outputter_options_.clear();
	}
}

void
ChunkLibraryJobQueen::allow_pose_outputter( pose_outputters::PoseOutputterCreatorOP creator )
{
	if ( use_factory_provided_pose_outputters_ ) {
		use_factory_provided_pose_outputters_ = false;
		// the user has not told us they want to drop the original set of creators
		// so interpret this as them wanting to allow the user to provide another
		// outputter in addition to the original set.
		outputter_creator_list_ = pose_outputters::PoseOutputterFactory::get_instance()->pose_outputter_creators();
		for ( auto const & creator_ii : outputter_creator_list_ ) {
			outputter_creators_[ creator_ii->keyname() ] = creator_ii;
		}
	}
	if ( ! outputter_creators_.count( creator->keyname() ) ) {
		outputter_creator_list_.push_back( creator );
		outputter_creators_[ creator->keyname() ] = creator;
		creator->list_options_read( outputter_options_ );
		if ( override_default_outputter_ && ! default_outputter_creator_ ) {
			default_outputter_creator_ = creator;
		}
	}
}

void
ChunkLibraryJobQueen::set_default_outputter( pose_outputters::PoseOutputterCreatorOP creator )
{
	override_default_outputter_ = true;
	default_outputter_creator_ = creator;
	if ( use_factory_provided_pose_outputters_ ) {
		// the user has not told us they want to drop the original set of creators
		// but to provide the desired functionality, the SJQ has to handle outputter
		// instantiation herself
		use_factory_provided_pose_outputters_ = false;
		outputter_creator_list_ = pose_outputters::PoseOutputterFactory::get_instance()->pose_outputter_creators();
		for ( auto const & creator_ii : outputter_creator_list_ ) {
			outputter_creators_[ creator_ii->keyname() ] = creator_ii;
		}
	}
}


/// @details Derived classes may choose to not override this method as a way to indicate that
/// they have no additional subtags of the <Job> tag they wish to add. This is a no-op
/// implementation.
void ChunkLibraryJobQueen::append_job_tag_subelements(
	utility::tag::XMLSchemaDefinition &,
	utility::tag::XMLSchemaComplexTypeGenerator &
) const
{}

/// @details Derived classes may choose to not override this method as a way to indicate that
/// they have no common-block data that needs to be defined.
void
ChunkLibraryJobQueen::append_common_tag_subelements(
	utility::tag::XMLSchemaDefinition &,
	utility::tag::XMLSchemaComplexTypeGenerator &
) const
{}

void
ChunkLibraryJobQueen::determine_preliminary_job_list()
{
	using namespace basic::options;
	using namespace basic::options::OptionKeys;
	using namespace utility::tag;

	// only should be called once
	if ( preliminary_larval_jobs_determined_ ) return;
	preliminary_larval_jobs_determined_ = true;
	required_initialization_performed_ = true;

	// Always generate a job definition schema and in the process, validate the schema.
	// If derived JobQueens have defined an invalid schema, execution must stop.
	std::string job_def_schema = job_definition_xsd();
	// TO DO: validate the XSD

	if ( option[ in::file::job_definition_file ].user() ) {
		std::string job_def_string = utility::file_contents( option[ in::file::job_definition_file ] );
		determine_preliminary_job_list_from_xml_file( job_def_string, job_def_schema );
	} else {
		determine_preliminary_job_list_from_command_line();
	}

}

/// @brief Read access for the subset of nodes in the job DAG which the %ChunkLibraryJobQueen
/// is responsible for producing the larval_jobs. They are called "preliminary" jobs because
/// they do not depend on outputs from any previous node in the graph. (The set of job nodes
/// that contain no incoming edges, though, could perhaps be different from the set of
/// preliminary job nodes, so the %ChunkLibraryJobQueen requires that the derived job queen
/// inform her of which nodes are the preliminary job nodes.)
utility::vector1< core::Size > const &
ChunkLibraryJobQueen::preliminary_job_nodes() const
{
	return preliminary_job_node_inds_;
}

bool
ChunkLibraryJobQueen::all_jobs_assigned_for_preliminary_job_node( core::Size node_id ) const
{
	return preliminary_job_nodes_complete_[ node_id ];
}

/// @brief Read access to derived JobQueens to the preliminary job list.
/// This will return an empty list if  determine_preliminary_jobs has not yet
/// been called.
utility::vector1< ChunkLibraryPreliminaryLarvalJob > const &
ChunkLibraryJobQueen::get_preliminary_larval_jobs() const
{
	return preliminary_larval_jobs_;
}

/// @details This base class implementation merely returns a one-element list containing the
/// input inner_job.  Derived classes have the flexibility to create several preliminary
/// jobs from this input job
ChunkLibraryInnerLarvalJobs
ChunkLibraryJobQueen::refine_preliminary_job( ChunkLibraryPreliminaryLarvalJob const & prelim_job )
{
	ChunkLibraryInnerLarvalJobs one_job( 1, prelim_job.inner_job );
	return one_job;
}

LarvalJobs
ChunkLibraryJobQueen::expand_job_list( ChunkLibraryInnerLarvalJobOP inner_job, core::Size max_larval_jobs_to_create )
{
	core::Size nstruct = inner_job->nstruct_max();
	LarvalJobs jobs;
	core::Size n_to_make = std::min( nstruct, max_larval_jobs_to_create );
	for ( core::Size jj = 1; jj <= n_to_make; ++jj ) {
		LarvalJobOP job = LarvalJobOP( new LarvalJob( inner_job, njobs_made_for_curr_inner_larval_job_ + jj, ++larval_job_counter_ ));
		//TR << "Expand larval job " << larval_job_counter_ << std::endl;
		jobs.push_back( job );
	}
	return jobs;
}

void
ChunkLibraryJobQueen::create_and_store_output_specification_for_job_result(
	LarvalJobCOP job,
	core::Size result_index,
	core::Size nresults
)
{
	ChunkLibraryInnerLarvalJobCOP inner_job = utility::pointer::dynamic_pointer_cast< ChunkLibraryInnerLarvalJob const > ( job->inner_job() );
	if ( ! inner_job ) { throw bad_inner_job_exception(); }

	utility::options::OptionCollectionOP job_options = options_for_job( *inner_job );
	create_and_store_output_specification_for_job_result(
		job, *job_options, result_index, nresults );
}

void
ChunkLibraryJobQueen::create_and_store_output_specification_for_job_result(
	LarvalJobCOP job,
	utility::options::OptionCollection const & job_options,
	core::Size result_index,
	core::Size nresults
)
{
	recent_successes_.push_back( create_output_specification_for_job_result( job, job_options, result_index, nresults ) );
}



/// @details Creates a MultipleOutputSpecification and packs it with one
/// OutputSpecification per PoseOutputter / SecondaryPoseOutputter. The order
/// of the OutputSpecifications that are given here needs to be recapitulated
/// for the PoseMultipleOutputter that will be created by the call to
/// result_outputter, as the OutputSpecifiations are iterated across in the
/// same order as the PoseOutputters in the PoseMultipleOutputter.
output::OutputSpecificationOP
ChunkLibraryJobQueen::create_output_specification_for_job_result(
	LarvalJobCOP job,
	utility::options::OptionCollection const & job_options,
	core::Size result_index,
	core::Size nresults
)
{
	ChunkLibraryInnerLarvalJobCOP inner_job = utility::pointer::dynamic_pointer_cast< ChunkLibraryInnerLarvalJob const > ( job->inner_job() );
	if ( ! inner_job ) { throw bad_inner_job_exception(); }

	utility::tag::TagCOP outputter_tag;
	utility::tag::TagCOP secondary_output_tag;
	if ( inner_job->jobdef_tag() ) {
		utility::tag::TagCOP job_tag = inner_job->jobdef_tag();
		if ( job_tag->hasTag( "Output" ) ) {
			utility::tag::TagCOP output_tag = job_tag->getTag( "Output" );
			if ( output_tag->getTags().size() != 0 ) {
				outputter_tag = output_tag->getTags()[ 0 ];
			}
		}

		if ( job_tag->hasTag( "SecondaryOutput" ) ) {
			secondary_output_tag = job_tag->getTag( "SecondaryOutput" );
		}
	}

	JobOutputIndex output_index = build_output_index( job, result_index, nresults );

	pose_outputters::PoseOutputterOP outputter = pose_outputter_for_job( *inner_job );

	// Note assumption that there *is* a primary outputter for this job.
	// What does -no_output look like in JD3?
	pose_outputters::PoseOutputSpecificationOP primary_spec = outputter->create_output_specification(
		*job, output_index, job_options, outputter_tag );

	output::MultipleOutputSpecificationOP specs( new output::MultipleOutputSpecification );
	specs->append_specification( primary_spec );

	// SecondaryOutputters are ordered:
	// 1st the set of (possibly-repeated) SecondaryOutputters that are given in the Job tag,
	// in the same order as they appear beneath the <SecondaryOutput> subtag.
	// 2nd the set of SecondaryOutputters that are specified on the command line and that are
	// not present in the Job tag.
	utility::vector1< pose_outputters::SecondaryPoseOutputterOP > secondary_outputters =
		secondary_outputters_for_job( *inner_job, job_options );
	for ( core::Size ii = 1; ii <= secondary_outputters.size(); ++ii ) {
		utility::tag::TagCOP secondary_outputter_tag;
		if ( secondary_output_tag ) {
			if ( secondary_output_tag->getTags().size() <= ii ) {
				secondary_outputter_tag = secondary_output_tag->getTags()[ ii ];
			}
		}
		pose_outputters::PoseOutputSpecificationOP spec =
			secondary_outputters[ ii ]->create_output_specification(
			*job, output_index, job_options, secondary_output_tag );
		specs->append_specification( spec );
	}
	specs->output_index( output_index );
	specs->result_id( JobResultID( job->job_index(), result_index ) );
	return specs;
}

JobOutputIndex
ChunkLibraryJobQueen::build_output_index(
	protocols::jd3::LarvalJobCOP job,
	Size result_index,
	Size n_results_for_job
)
{
	JobOutputIndex output_index;
	output_index.primary_output_index   = job->nstruct_index();
	output_index.n_primary_outputs      = std::max( job->nstruct_max(), (Size) 1000 );
	output_index.secondary_output_index = result_index;
	output_index.n_secondary_outputs    = n_results_for_job;

	assign_output_index( job, result_index, n_results_for_job, output_index );
	return output_index;
}

/// @brief No-op implementation -- leave the indices the way they were initialized
void
ChunkLibraryJobQueen::assign_output_index(
	protocols::jd3::LarvalJobCOP,
	Size,
	Size,
	JobOutputIndex &
)
{}

void
ChunkLibraryJobQueen::derived_process_deallocation_message(
	deallocation::DeallocationMessageOP
)
{}



void ChunkLibraryJobQueen::add_options( utility::options::OptionKeyList const & opts )
{
	using namespace utility::options;
	for ( auto const & opt : opts ) {
		options_.push_back( opt );
	}
}

void ChunkLibraryJobQueen::add_option( utility::options::OptionKey const & key )
{
	options_.emplace_back(key );
}

/// @details TO DO
void ChunkLibraryJobQueen::remove_default_input_element() {}

utility::tag::TagCOP
ChunkLibraryJobQueen::common_block_tags() const
{
	return common_block_tags_;
}

core::pose::PoseOP
ChunkLibraryJobQueen::pose_for_job(
	LarvalJobCOP job,
	utility::options::OptionCollection const & options
)
{
	using namespace chunk_library_inputters;
	// either read the Pose in using the pose_inputter (and then keep a copy
	// in the resource manager), or retrieve the Pose from the resource manager
	// initial version: just read the pose in for each job.

	ChunkLibraryInnerLarvalJobCOP inner_job = utility::pointer::dynamic_pointer_cast< ChunkLibraryInnerLarvalJob const > ( job->inner_job() );
	if ( ! inner_job ) { throw bad_inner_job_exception(); }

	auto const & input_source = dynamic_cast< ChunkLibraryInputSource const & >( inner_job->input_source() );
	//TR << "Looking for input source " << job->inner_job()->input_source().input_tag() << " with pose_id " << job->inner_job()->input_source().pose_id() << std::endl;
	if ( input_pose_index_map_.count( input_source.source_id() ) ) {

		core::pose::PoseOP pose( new core::pose::Pose );
		// Why does the chunk_library job queen use detached_copy? Because it is important in multithreaded
		// contexts that no two Poses pointing to that same data end up in two separate threads,
		// and then try to modify that data at the same time.  It turns out there are places
		// in Pose where it covertly modifies data in some other Pose and this could lead to
		// race conditions.
		pose->detached_copy( *input_pose_index_map_[ input_source.source_id() ] );
		return pose;
	}

	utility::tag::TagCOP inputter_tag;
	if ( inner_job->jobdef_tag() && inner_job->jobdef_tag()->hasTag( "Input" ) ) {
		utility::tag::TagCOP input_tag = inner_job->jobdef_tag()->getTag( "Input" );
		if ( input_tag->getTags().size() != 0 ) {
			inputter_tag = input_tag->getTags()[ 0 ];
		}
	}

	core::pose::PoseOP input_pose = chunk_library_inputter_for_job( *inner_job )->chunk_library_from_input_source( input_source, options, inputter_tag );
	//TR << "Storing Pose for input source " << job->inner_job()->input_source().input_tag() << " with pose_id " << job->inner_job()->input_source().pose_id() << std::endl;
	input_pose_index_map_[ input_source.source_id() ] = input_pose;

	core::pose::PoseOP pose( new core::pose::Pose );
	pose->detached_copy( *input_pose );
	return pose;
}

//ResourceManagerOP ChunkLibraryJobQueen::resource_manager()
//{}

/// @brief Access the pose inputter
chunk_library_inputters::ChunkLibraryInputterOP
ChunkLibraryJobQueen::chunk_library_inputter_for_job( ChunkLibraryInnerLarvalJob const & inner_job ) const
{
	// find the preliminary job node for this job, if available
	// and return the already-created pose inputter
	if ( inner_job.job_node() == 0 ) {
		if ( use_factory_provided_chunk_library_inputters_ ) {
			return chunk_library_inputters::ChunkLibraryInputterFactory::get_instance()->new_chunk_library_inputter( inner_job.input_source().origin() );
		} else {
			runtime_assert( inputter_creators_.count( inner_job.input_source().origin() ) );
			auto iter = inputter_creators_.find( inner_job.input_source().origin() );
			return iter->second->create_inputter();
		}
	} else {
		debug_assert( preliminary_larval_jobs_[ inner_job.job_node() ].chunk_library_inputter );
		return preliminary_larval_jobs_[ inner_job.job_node() ].chunk_library_inputter;
	}
}

/// @brief Access the pose outputter for a particular job; perhaps this outputter has already been created, or
/// perhaps it needs to be created and stored
pose_outputters::PoseOutputterOP
ChunkLibraryJobQueen::pose_outputter_for_job( ChunkLibraryInnerLarvalJob const & inner_job )
{
	utility::options::OptionCollectionOP job_options = options_for_job( inner_job );
	return pose_outputter_for_job( inner_job, *job_options );
}

pose_outputters::PoseOutputterOP
ChunkLibraryJobQueen::pose_outputter_for_job(
	ChunkLibraryInnerLarvalJob const & inner_job,
	utility::options::OptionCollection const & job_options
)
{
	pose_outputters::PoseOutputterOP representative_outputter;
	representative_outputter = representative_pose_outputter_map_[ inner_job.outputter() ];

	utility::tag::TagCOP output_tag;
	if ( inner_job.jobdef_tag() ) {
		utility::tag::TagCOP job_tag = inner_job.jobdef_tag();
		if ( job_tag->hasTag( "Output" ) ) {
			output_tag = job_tag->getTag( "Output" );
		}
	}
	std::string which_outputter = representative_outputter->outputter_for_job( output_tag, job_options, inner_job );

	if ( which_outputter == "" ) {
		return representative_outputter;
	}
	PoseOutputterMap::const_iterator iter = pose_outputter_map_.find( std::make_pair( inner_job.outputter(), which_outputter ) );
	if ( iter != pose_outputter_map_.end() ) {
		return iter->second;
	}

	pose_outputters::PoseOutputterOP outputter;
	if ( use_factory_provided_pose_outputters_ ) {
		outputter = pose_outputters::PoseOutputterFactory::get_instance()->new_pose_outputter( inner_job.outputter() );
	} else {
		outputter = outputter_creators_[ inner_job.outputter() ]->create_outputter();
	}
	pose_outputter_map_[ std::make_pair( inner_job.outputter(), which_outputter ) ] = outputter;
	return outputter;
}

pose_outputters::PoseOutputterOP
ChunkLibraryJobQueen::pose_outputter_for_job( pose_outputters::PoseOutputSpecification const & /*spec*/ )
{
	// This doesn't have to be complicated. The only correct pose outputter is the DeNovoSilentFilePoseOutputter.
	// this may kill multi-rd protocol support but that's a bridge for another day.
	/*
	std::string const & outputter_type = spec.outputter_type();
	pose_outputters::PoseOutputterOP representative;
	auto rep_iter = representative_pose_outputter_map_.find( outputter_type );
	if ( rep_iter == representative_pose_outputter_map_.end() ) {
	if ( use_factory_provided_pose_outputters_ ) {
	representative = pose_outputters::PoseOutputterFactory::get_instance()->new_pose_outputter( outputter_type );
	} else {
	runtime_assert( outputter_creators_.count( outputter_type ) != 0 );
	auto iter = outputter_creators_.find( outputter_type );
	representative = iter->second->create_outputter();
	}
	representative_pose_outputter_map_[ outputter_type ] = representative;
	} else {
	representative = rep_iter->second;
	}

	// This outputter name may have a job-distributor-assigned suffix, and so it may not yet
	// have been created, even if this JobQueen has been doing some amount of output.
	std::string actual_outputter_name = representative->outputter_for_job( spec );
	if ( actual_outputter_name == "" ) return representative;

	std::pair< std::string, std::string > outputter_key =
	std::make_pair( outputter_type, actual_outputter_name );
	auto iter = pose_outputter_map_.find( outputter_key );
	if ( iter != pose_outputter_map_.end() ) {
	return iter->second;
	}
	*/

	std::pair< std::string, std::string > key( "DeNovoSilentFile", "" );
	pose_outputters::PoseOutputterOP outputter;
	if ( use_factory_provided_pose_outputters_ ) {
		outputter = pose_outputters::PoseOutputterFactory::get_instance()->new_pose_outputter( key.first );
	} else {
		debug_assert( outputter_creators_.count( key.first ) );
		outputter = outputter_creators_[ key.first ]->create_outputter();
	}
	pose_outputter_map_[ key ] = outputter;
	return outputter;
}


utility::vector1< pose_outputters::SecondaryPoseOutputterOP >
ChunkLibraryJobQueen::secondary_outputters_for_job(
	ChunkLibraryInnerLarvalJob const & inner_job,
	utility::options::OptionCollection const & job_options
)
{
	std::set< std::string > secondary_outputters_added;
	utility::vector1< pose_outputters::SecondaryPoseOutputterOP > secondary_outputters;
	if ( inner_job.jobdef_tag() ) {
		utility::tag::TagCOP job_tag = inner_job.jobdef_tag();
		if ( job_tag->hasTag( "SecondaryOutput" ) ) {
			utility::tag::TagCOP secondary_output_tags = job_tag->getTag( "SecondaryOutput" );
			utility::vector0< utility::tag::TagCOP > const & subtags = secondary_output_tags->getTags();
			for ( core::Size ii = 0; ii < subtags.size(); ++ii ) {
				utility::tag::TagCOP iitag = subtags[ ii ];
				// allow repeats! Why not?
				// if ( secondary_outputters_added.count( iitag->getName() ) ) continue;
				secondary_outputters_added.insert( iitag->getName() );

				// returns 0 if the secondary outputter is repressed for a particular job
				pose_outputters::SecondaryPoseOutputterOP outputter = secondary_outputter_for_job( inner_job, job_options, iitag->getName() );
				if ( outputter ) {
					secondary_outputters.push_back( outputter );
				}
			}
		}
	}

	if ( cl_outputters_.empty() ) {
		cl_outputters_ = pose_outputters::PoseOutputterFactory::get_instance()->secondary_pose_outputters_from_command_line();
	}
	for ( auto const & elem : cl_outputters_ ) {

		if ( representative_secondary_outputter_map_.count( elem->class_key() ) == 0 ) {
			representative_secondary_outputter_map_[ elem->class_key() ] = elem;
		}
		// Do not add a command-line specified secondary outputter if that one
		// has already been specified within the <SecondaryOutput> tag.
		if ( secondary_outputters_added.count( elem->class_key() ) ) continue;
		if ( elem->class_key() == "ScoreFile" ) continue;

		// returns 0 if the secondary outputter is repressed for a particular job
		pose_outputters::SecondaryPoseOutputterOP outputter = secondary_outputter_for_job(
			inner_job, job_options, elem->class_key() );
		if ( outputter ) {
			secondary_outputters.push_back( outputter );
		}
	}

	return secondary_outputters;
}



/// @details the nstruct count is taken from either the Job tag, or the
/// command line. "nstruct" is not a valid option to be provided
/// in the <Options> element.
core::Size
ChunkLibraryJobQueen::nstruct_for_job( utility::tag::TagCOP job_tag ) const
{
	if ( job_tag && job_tag->hasOption( "nstruct" ) ) {
		return job_tag->getOption< core::Size >( "nstruct" );
	}
	return basic::options::option[ basic::options::OptionKeys::out::nstruct ];
}


utility::options::OptionCollectionOP
ChunkLibraryJobQueen::options_for_job( ChunkLibraryInnerLarvalJob const & inner_job ) const
{
	using namespace utility::tag;

	TagCOP job_options_tag;
	if ( inner_job.jobdef_tag() ) {
		TagCOP job_tags = inner_job.jobdef_tag();
		if ( job_tags && job_tags->hasTag( "Options" ) ) {
			job_options_tag = job_tags->getTag( "Options" );
		}
	}

	// now let's walk through all of the option keys and read their values
	// out of the global options system into the per-job options object
	return options_from_tag( job_options_tag );
}


/// @details Note that jobs specified on the command line but which the ChunkLibraryJobQueen does
/// not know about do not get set or added to the per-job options.  Functions trying to read
/// per-job options that the ChunkLibraryJobQueen does not know about will die. This is intentional.
utility::options::OptionCollectionOP
ChunkLibraryJobQueen::options_from_tag( utility::tag::TagCOP job_options_tag ) const
{
	using namespace utility::tag;
	using namespace utility::options;

	utility::options::OptionKeyList all_options = concatenate_all_options();
	OptionCollectionOP opts = basic::options::read_subset_of_global_option_collection( all_options );

	TagCOP common_options_tag;
	if ( common_block_tags_ && common_block_tags_->hasTag( "Options" ) ) {
		common_options_tag = common_block_tags_->getTag( "Options" );
	}

	for ( auto const & option : all_options ) {
		utility::options::OptionKey const & opt( option() );
		OptionTypes opt_type = option_type_from_key( opt );

		std::string opt_tag_name = basic::options::replace_option_namespace_colons_with_underscores( opt );
		if ( job_options_tag && job_options_tag->hasTag( opt_tag_name ) ) {
			TagCOP opt_tag = job_options_tag->getTag( opt_tag_name );
			if ( opt_type == BOOLEAN_OPTION ) {
				(*opts)[ opt ].set_value( opt_tag->getOption< std::string >( "value", "true" ) );
			} else {
				debug_assert( opt_tag->hasOption( "value" ) );
				(*opts)[ opt ].set_value( opt_tag->getOption< std::string >( "value" ) );
			}
		} else if ( common_options_tag && common_options_tag->hasTag( opt_tag_name ) ) {
			TagCOP opt_tag = common_options_tag->getTag( opt_tag_name );
			if ( opt_type == BOOLEAN_OPTION ) {
				(*opts)[ opt ].set_value( opt_tag->getOption< std::string >( "value", "true" ) );
			} else {
				debug_assert( opt_tag->hasOption( "value" ) );
				(*opts)[ opt ].set_value( opt_tag->getOption< std::string >( "value" ) );
			}
		}
	}

	return opts;
}

void
ChunkLibraryJobQueen::determine_preliminary_job_list_from_xml_file(
	std::string const & job_def_string,
	std::string const & job_def_schema
)
{
	using namespace basic::options;
	using namespace basic::options::OptionKeys;
	using namespace utility::tag;
	using namespace chunk_library_inputters;

	preliminary_larval_jobs_determined_ = true;

	load_job_definition_file( job_def_string, job_def_schema );

	// now iterate across all tags, and for each Job subtag, create a ChunkLibraryPreliminaryLarvalJob and load it
	// with all of the options that are within the <Option> subtag, if present -- and reading any options
	// not present in the tag from the (global) options system.
	Tag::tags_t const & subtags = job_definition_file_tags_->getTags();
	preliminary_larval_jobs_.reserve( subtags.size() );
	core::Size count_prelim_nodes( 0 );
	for ( auto subtag : subtags ) {
		if ( subtag->getName() != "Job" ) {
			debug_assert( subtag->getName() == "Common" );
			continue;
		}

		TagCOP job_options_tag;
		if ( subtag->hasTag( "Options" ) ) {
			job_options_tag = subtag->getTag( "Options" );
		}
		utility::options::OptionCollectionCOP job_options = options_from_tag( job_options_tag );

		// ok -- look at input tag
		TagCOP input_tag = subtag->getTag( "Input" );
		debug_assert( input_tag ); // XML schema validation should ensure that there is an "Input" subelement
		debug_assert( input_tag->getTags().size() == 1 ); // schema validation should ensure there is exactly one subelement
		TagCOP input_tag_child = input_tag->getTags()[ 0 ];

		// Get the right inputter for this Job.
		chunk_library_inputters::ChunkLibraryInputterOP inputter;
		if ( use_factory_provided_chunk_library_inputters_ ) {
			inputter = chunk_library_inputters::ChunkLibraryInputterFactory::get_instance()->new_chunk_library_inputter( input_tag_child->getName() );
		} else {
			runtime_assert( inputter_creators_.count( input_tag_child->getName() ) != 0 );
			inputter = inputter_creators_[ input_tag_child->getName() ]->create_inputter();
		}

		ChunkLibraryInputSources input_poses = inputter->chunk_library_input_sources_from_tag( *job_options, input_tag_child );
		for ( auto input_source : input_poses ) {
			input_source->source_id( ++input_pose_counter_ );
			//TR << "Assigning input_source " << input_source->input_tag() << " pose_id " << input_pose_counter_ << " and stored as " << input_source->pose_id() << std::endl;
		}

		// Get the right outputter for this job.
		pose_outputters::PoseOutputterOP outputter = get_outputter_from_job_tag( subtag );

		if ( representative_pose_outputter_map_.count( outputter->class_key() ) == 0 ) {
			representative_pose_outputter_map_[ outputter->class_key() ] = outputter;
		}

		// now iterate across the input sources for this job and create
		// a preliminary job for each
		core::Size nstruct = nstruct_for_job( subtag );
		for ( ChunkLibraryInputSources::const_iterator iter = input_poses.begin(); iter != input_poses.end(); ++iter ) {
			ChunkLibraryPreliminaryLarvalJob prelim_job;
			ChunkLibraryInnerLarvalJobOP inner_job( new ChunkLibraryInnerLarvalJob( nstruct, ++count_prelim_nodes ) );
			inner_job->input_source( *iter );
			inner_job->jobdef_tag( subtag );
			inner_job->outputter( outputter->class_key() );

			prelim_job.inner_job = inner_job;
			prelim_job.job_tag = subtag;
			prelim_job.job_options = job_options;
			prelim_job.chunk_library_inputter = inputter;

			preliminary_larval_jobs_.push_back( prelim_job );
		}
	}
}


void
ChunkLibraryJobQueen::load_job_definition_file(
	std::string const & job_def_string,
	std::string const & job_def_schema
)
{
	using namespace basic::options;
	using namespace basic::options::OptionKeys;
	using namespace utility::tag;

	TR << "Loading job definition file" << std::endl;

	XMLValidationOutput validator_output;
	try {
		validator_output = validate_xml_against_xsd( job_def_string, job_def_schema );
	} catch ( utility::excn::Exception const & e ) {
		std::ostringstream oss;
		if ( option[ in::file::job_definition_file ].user() ) {
			oss << "Job definition file \"" << option[ in::file::job_definition_file ] << "\" failed to validate against"
				" the schema for this application\nUse the option -jd3::job_definition_schema <output filename> to output"
				" the schema to a file.\n" << e.msg() << "\n";
		} else {
			oss << "Job definition file  failed to validate against"
				" the schema for this application\nUse the option -jd3::job_definition_schema <output filename> to output"
				" the schema to a file.\n" << e.msg() << "\n";
		}
		throw CREATE_EXCEPTION(utility::excn::Exception,  oss.str() );
	}

	if ( ! validator_output.valid() ) {
		std::ostringstream oss;
		if ( option[ in::file::job_definition_file ].user() ) {
			oss << "Job definition file \"" << option[ in::file::job_definition_file ] << "\" failed to validate against"
				" the schema for this application\nUse the option -jd3::job_definition_schema <output filename> to output"
				" the schema to a file.\n";
		} else {
			oss << "Job definition file failed to validate against"
				" the schema for this application\nUse the option -jd3::job_definition_schema <output filename> to output"
				" the schema to a file.\n";
		}
		oss << "Error messages were: " << validator_output.error_messages() << "\n";
		oss << "Warning messages were: " << validator_output.warning_messages() << "\n";
		throw CREATE_EXCEPTION(utility::excn::Exception,  oss.str() );
	}


	job_definition_file_tags_ = Tag::create( job_def_string );

	// look for the Common block, if there is one
	Tag::tags_t const & subtags = job_definition_file_tags_->getTags();
	for ( auto subtag : subtags ) {
		if ( subtag->getName() == "Common" ) {
			common_block_tags_ = subtag;
			return;
		}
	}
}


void
ChunkLibraryJobQueen::determine_preliminary_job_list_from_command_line()
{
	using namespace utility::tag;
	using namespace chunk_library_inputters;

	// read from the command line a list of all of the input jobs
	ChunkLibraryInputterFactory::ChunkLibraryInputSourcesAndInputters input_poses;
	if ( use_factory_provided_chunk_library_inputters_ ) {
		input_poses = ChunkLibraryInputterFactory::get_instance()->chunk_library_inputs_from_command_line();
	} else {
		for ( auto inputter_creator : inputter_creator_list_ ) {
			ChunkLibraryInputterOP inputter = inputter_creator->create_inputter();
			if ( inputter->job_available_on_command_line() ) {
				ChunkLibraryInputSources iter_sources = inputter->chunk_library_input_sources_from_command_line();
				input_poses.reserve( input_poses.size() + iter_sources.size() );
				for ( core::Size ii = 1; ii <= iter_sources.size(); ++ii ) {
					input_poses.push_back( std::make_pair( iter_sources[ ii ], inputter ) );
				}
			}
		}
	}
	for ( auto input_source : input_poses ) {
		input_source.first->source_id( ++input_pose_counter_ );
		//TR << "Assigning input_source " << input_source.first->input_tag() << " pose_id " << input_pose_counter_ << std::endl;
	}

	pose_outputters::PoseOutputterOP outputter = get_outputter_from_job_tag( utility::tag::TagCOP() );

	if ( representative_pose_outputter_map_.count( outputter->class_key() ) == 0 ) {
		representative_pose_outputter_map_[ outputter->class_key() ] = outputter;
	}

	// pass in a null-pointing TagCOP and construct the job options object from the command line.
	utility::options::OptionCollectionCOP job_options = options_from_tag( utility::tag::TagCOP() );

	// now iterate across the input sources for this job and create
	// a preliminary job for each
	preliminary_larval_jobs_.reserve( input_poses.size() );
	core::Size count_prelim_nodes( 0 );
	for ( auto const & input_pose : input_poses ) {
		ChunkLibraryPreliminaryLarvalJob prelim_job;
		Size nstruct = nstruct_for_job( nullptr );
		ChunkLibraryInnerLarvalJobOP inner_job( new ChunkLibraryInnerLarvalJob( nstruct, ++count_prelim_nodes ) );
		inner_job->input_source( input_pose.first );
		inner_job->outputter( outputter->class_key() );

		prelim_job.inner_job = inner_job;
		prelim_job.job_tag = TagCOP(); // null ptr
		prelim_job.job_options = job_options;
		prelim_job.chunk_library_inputter = input_pose.second;
		preliminary_larval_jobs_.push_back( prelim_job );
	}
}

LarvalJobs
ChunkLibraryJobQueen::next_batch_of_larval_jobs_from_prelim( core::Size job_node_index, core::Size max_njobs )
{
	LarvalJobs jobs;
	if ( preliminary_job_nodes_complete_[ job_node_index ] ) return jobs;

	while ( true ) {
		// Each iteration through this loop advances either  curr_inner_larval_job_, or
		// njobs_made_for_curr_inner_larval_job_ to ensure
		// that this loop does not continue indefinitely.

		if ( curr_inner_larval_job_index_ == 0 ) {

			ChunkLibraryPreliminaryLarvalJob curr_prelim_job = preliminary_larval_jobs_[ job_node_index ];
			inner_larval_jobs_for_curr_prelim_job_ = refine_preliminary_job( curr_prelim_job );
			curr_inner_larval_job_index_ = 1;
			njobs_made_for_curr_inner_larval_job_ = 0;

			// now initialize the inner larval jobs in the inner_larval_jobs_for_curr_prelim_job_ vector
			// setting the job tag.
			utility::tag::TagCOP output_tag;
			if ( curr_prelim_job.job_tag && curr_prelim_job.job_tag->hasTag( "Output" ) ) {
				output_tag = curr_prelim_job.job_tag->getTag( "Output" );
			}
			for ( ChunkLibraryInnerLarvalJobOP iijob : inner_larval_jobs_for_curr_prelim_job_ ) {
				pose_outputters::PoseOutputterOP ii_outputter = pose_outputter_for_job( *iijob );
				ii_outputter->determine_job_tag( output_tag, *curr_prelim_job.job_options, *iijob );
			}

		}

		if ( curr_inner_larval_job_index_ > inner_larval_jobs_for_curr_prelim_job_.size() ) {
			// prepare for the next time we call this function for a different job node
			curr_inner_larval_job_index_ = 0;
			pjn_job_ind_end_[ job_node_index ] = larval_job_counter_;
			preliminary_job_nodes_complete_[ job_node_index ] = 1;
			return jobs;
		}

		if ( curr_inner_larval_job_index_ <= inner_larval_jobs_for_curr_prelim_job_.size() ) {
			// create LarvalJobs
			ChunkLibraryInnerLarvalJobOP curr_inner_larval_job = inner_larval_jobs_for_curr_prelim_job_[ curr_inner_larval_job_index_ ];
			core::Size max_to_make = max_njobs;

			if ( max_to_make > curr_inner_larval_job->nstruct_max() - njobs_made_for_curr_inner_larval_job_ ) {
				max_to_make = curr_inner_larval_job->nstruct_max() - njobs_made_for_curr_inner_larval_job_;
			}

			if ( pjn_job_ind_begin_[ job_node_index ] == 0 ) {
				pjn_job_ind_begin_[ job_node_index ] = 1 + larval_job_counter_;
			}
			LarvalJobs curr_jobs = expand_job_list( curr_inner_larval_job, max_to_make );

			core::Size n_made = curr_jobs.size();
			if ( max_njobs >= n_made ) {
				max_njobs -= n_made;
			} else {
				max_njobs = 0;
				// this should never happen!
				throw CREATE_EXCEPTION(utility::excn::Exception,  "expand_job_list returned " + utility::to_string( n_made ) + " jobs when it was given a maximum number of " + utility::to_string( max_to_make ) + " to make (with max_njobs of " + utility::to_string( max_njobs ) + ")\n" );
			}

			jobs.splice( jobs.end(), curr_jobs );
			if ( n_made + njobs_made_for_curr_inner_larval_job_ < curr_inner_larval_job->nstruct_max() ) {
				njobs_made_for_curr_inner_larval_job_ += n_made;
				return jobs;
			} else {
				++curr_inner_larval_job_index_;
				njobs_made_for_curr_inner_larval_job_ = 0;
			}
		}
	}
}

/// @details helper function that should only be called by the above secondary_outputter_for_job function
/// because of its assumption that the representative_secondary_outputter_map_ map contains an entry for
/// the requested secondary_outputter_type
pose_outputters::SecondaryPoseOutputterOP
ChunkLibraryJobQueen::secondary_outputter_for_job(
	ChunkLibraryInnerLarvalJob const & inner_job,
	utility::options::OptionCollection const & job_options,
	std::string const & secondary_outputter_type
)
{
	pose_outputters::SecondaryPoseOutputterOP representative_outputter;
	representative_outputter = representative_secondary_outputter_map_[ secondary_outputter_type ];
	debug_assert( representative_outputter );

	utility::tag::TagCOP output_tag;
	if ( inner_job.jobdef_tag() ) {
		utility::tag::TagCOP job_tag = inner_job.jobdef_tag();
		if ( job_tag->hasTag( "SecondaryOutput" ) ) {
			output_tag = job_tag->getTag( "SecondaryOutput" );
		}
	}
	std::string which_outputter = representative_outputter->outputter_for_job( output_tag, job_options, inner_job );


	if ( which_outputter == "" ) {
		return representative_outputter;
	}
	SecondaryOutputterMap::const_iterator iter =
		secondary_outputter_map_.find( std::make_pair( secondary_outputter_type, which_outputter ) );
	if ( iter != secondary_outputter_map_.end() ) {
		return iter->second;
	}

	pose_outputters::SecondaryPoseOutputterOP outputter =
		pose_outputters::PoseOutputterFactory::get_instance()->new_secondary_outputter( secondary_outputter_type );
	secondary_outputter_map_[ std::make_pair( secondary_outputter_type, which_outputter ) ] = outputter;
	return outputter;

}

pose_outputters::SecondaryPoseOutputterOP
ChunkLibraryJobQueen::secondary_outputter_for_job( pose_outputters::PoseOutputSpecification const & spec )
{
	std::string const & outputter_type = spec.outputter_type();
	pose_outputters::SecondaryPoseOutputterOP representative;
	auto rep_iter = representative_secondary_outputter_map_.find( outputter_type );
	if ( rep_iter == representative_secondary_outputter_map_.end() ) {
		representative = pose_outputters::PoseOutputterFactory::get_instance()->new_secondary_outputter( outputter_type );
		representative_secondary_outputter_map_[ outputter_type ] = representative;
	} else {
		representative = rep_iter->second;
	}

	// This outputter name may have a job-distributor-assigned suffix, and so it may not yet
	// have been created, even if this JobQueen has been doing some amount of output.
	std::string actual_outputter_name = representative->outputter_for_job( spec );
	if ( actual_outputter_name == "" ) return representative;

	std::pair< std::string, std::string > outputter_key =
		std::make_pair( outputter_type, actual_outputter_name );
	auto iter = secondary_outputter_map_.find( outputter_key );
	if ( iter != secondary_outputter_map_.end() ) {
		return iter->second;
	}

	pose_outputters::SecondaryPoseOutputterOP outputter;
	outputter = pose_outputters::PoseOutputterFactory::get_instance()->new_secondary_outputter( outputter_type );
	secondary_outputter_map_[ outputter_key ] = outputter;
	return outputter;
}

utility::options::OptionKeyList
ChunkLibraryJobQueen::concatenate_all_options() const
{
	utility::options::OptionKeyList all_options( options_ );
	all_options.insert( all_options.end(), inputter_options_.begin(), inputter_options_.end() );
	all_options.insert( all_options.end(), outputter_options_.begin(), outputter_options_.end() );
	all_options.insert( all_options.end(), secondary_outputter_options_.begin(), secondary_outputter_options_.end() );
	return all_options;
}

pose_outputters::PoseOutputterOP
ChunkLibraryJobQueen::get_outputter_from_job_tag( utility::tag::TagCOP tag ) const
{
	using utility::tag::TagCOP;
	pose_outputters::PoseOutputterOP outputter;
	if ( tag && tag->hasTag( "Output" ) ) {
		TagCOP output_tag = tag->getTag( "Output" );
		debug_assert( output_tag );
		debug_assert( output_tag->getTags().size() == 1 );
		TagCOP output_subtag = output_tag->getTags()[ 0 ];
		if ( use_factory_provided_pose_outputters_ ) {
			outputter = pose_outputters::PoseOutputterFactory::get_instance()->new_pose_outputter( output_subtag->getName() );
		} else {
			runtime_assert( outputter_creators_.count( output_subtag->getName() ) != 0 );
			auto iter = outputter_creators_.find( output_subtag->getName() );
			outputter = iter->second->create_outputter();
		}
	} else {
		if ( use_factory_provided_pose_outputters_ ) {
			outputter = pose_outputters::PoseOutputterFactory::get_instance()->pose_outputter_from_command_line();
		} else {
			for ( auto outputter_creator : outputter_creator_list_ ) {
				if ( outputter_creator->outputter_specified_by_command_line() ) {
					outputter = outputter_creator->create_outputter();
					break;
				}
			}
			if ( ! outputter ) {
				if ( override_default_outputter_ ) {
					outputter = default_outputter_creator_->create_outputter();
				} else {
					runtime_assert( outputter_creators_.count( pose_outputters::DeNovoSilentFilePoseOutputter::keyname() ) );
					outputter = pose_outputters::PoseOutputterOP( new pose_outputters::DeNovoSilentFilePoseOutputter );
				}
			}
		}
	}
	return outputter;
}

} // namespace chunk_library
} // namespace jd3
} // namespace protocols
