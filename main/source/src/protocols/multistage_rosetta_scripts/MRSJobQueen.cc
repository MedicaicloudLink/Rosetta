// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file protocols/multistage_rosetta_scripts/MRSJobQueen.cc
/// @brief Queen for JD3 multistep protocol
/// @author Jack Maguire, jackmaguire1444@gmail.com

#include <protocols/multistage_rosetta_scripts/MRSJobQueen.hh>
#include <protocols/multistage_rosetta_scripts/MRSJob.hh>
#include <protocols/multistage_rosetta_scripts/MRSJobSummary.hh>

//Turns out that KMedoidsOnTheFly is just as fast as KMedoids for simple metrics
//#include <protocols/multistage_rosetta_scripts/cluster/KMedoids.hh>
#include <protocols/multistage_rosetta_scripts/cluster/KMedoidsOnTheFly.hh>
#include <protocols/multistage_rosetta_scripts/cluster/ClusterMetricFactory.hh>
#include <protocols/multistage_rosetta_scripts/cluster/util.hh>

#include <numeric>
#include <algorithm>

#include <basic/options/option.hh>
#include <basic/Tracer.hh>
#include <basic/options/keys/parser.OptionKeys.gen.hh>

#include <core/select/residue_selector/ResidueSelector.hh>
#include <core/pose/Pose.hh>

#include <protocols/jd3/InnerLarvalJob.hh>
#include <protocols/jd3/Job.hh>
#include <protocols/jd3/JobDigraph.hh>
#include <protocols/jd3/JobOutputIndex.hh>
#include <protocols/jd3/JobResult.hh>
#include <protocols/jd3/JobSummary.hh>
#include <protocols/jd3/JobTracker.hh>
#include <protocols/jd3/CompletedJobOutput.fwd.hh>
#include <protocols/jd3/standard/PreliminaryLarvalJob.hh>

#include <protocols/jd3/dag_node_managers/NodeManager.hh>
#include <protocols/jd3/dag_node_managers/EvenlyPartitionedNodeManager.hh>
#include <protocols/jd3/dag_node_managers/SimpleNodeManager.hh>

#include <protocols/jd3/pose_inputters/PoseInputSource.hh>
#include <protocols/jd3/pose_inputters/PoseInputter.hh>
#include <protocols/jd3/pose_outputters/PoseOutputSpecification.hh>
#include <protocols/jd3/job_summaries/StandardPoseJobSummary.hh>
#include <protocols/jd3/job_results/PoseJobResult.hh>

#include <protocols/filters/BasicFilters.hh>
#include <protocols/filters/filter_schemas.hh>
#include <protocols/filters/FilterFactory.hh>
#include <protocols/filters/FilterCreator.hh>

#include <protocols/jobdist/standard_mains.hh>
#include <protocols/jobdist/Jobs.hh>

#include <protocols/rosetta_scripts/RosettaScriptsParser.hh>
#include <protocols/parser/DataLoader.hh>
#include <protocols/parser/DataLoaderFactory.hh>
#include <protocols/parser/ResidueSelectorLoader.hh>

#include <protocols/moves/NullMover.hh>
#include <protocols/moves/mover_schemas.hh>
#include <protocols/moves/MoverFactory.hh>
#include <protocols/moves/MoverCreator.hh>

#include <utility/tag/Tag.hh>
#include <utility/tag/XMLSchemaGeneration.hh>
#include <utility/tag/XMLSchemaValidation.hh>
#include <utility/options/OptionCollection.fwd.hh>
#include <utility/pointer/memory.hh>

static basic::Tracer TR( "protocols.multistage_rosetta_scripts.MRSJobQueen" );

using namespace utility;
using namespace protocols::jd3;
using namespace protocols::jd3::dag_node_managers;
using namespace protocols::jd3::job_results;

#ifdef    SERIALIZATION
// Utility serialization headers
#include <utility/vector1.srlz.hh>
#include <utility/serialization/serialization.hh>

// Cereal headers
#include <cereal/types/map.hpp>
#include <cereal/types/polymorphic.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/unordered_map.hpp>
#include <cereal/types/utility.hpp>
#endif // SERIALIZATION

namespace protocols {
namespace multistage_rosetta_scripts {

//Constructor
MRSJobQueen::MRSJobQueen() :
	StandardJobQueen(),
	has_been_initialized_( false ),
	num_input_structs_( 0 ),
	num_stages_( 0 ),
	most_recent_pose_id_( 0, 0 )
{
	set_common_block_precedes_job_blocks( false );
	rosetta_scripts::RosettaScriptsParser::register_factory_prototypes();
}

//Destructor
MRSJobQueen::~MRSJobQueen()
{}

JobDigraphOP
MRSJobQueen::create_initial_job_dag() {
	has_been_initialized_ = true;
	determine_preliminary_job_list();

	JobDigraphOP dag = pointer::make_shared< JobDigraph >( num_stages_ );
	for ( core::Size ii = 1; ii < num_stages_; ++ii ) {
		dag->add_edge( ii, ii+1 );
	}
	return dag;
}

void
MRSJobQueen::cluster (
	core::Size const stage_about_to_start
) {
	core::Size const stage_being_clustered = stage_about_to_start - 1;

	utility::vector1< ResultElements > results_from_previous_stage =
		node_managers_[ stage_being_clustered ]->results_to_keep();

	//remove empty elements from vector
	auto is_empty_dummy = []( ResultElements const & res_elem ) -> bool {
		return res_elem.global_job_id == 0;
		};
	std::remove_if( results_from_previous_stage.begin(), results_from_previous_stage.end(), is_empty_dummy );

	//populate_vector
	utility::vector1< cluster::ClusterMetricCOP > points ( results_from_previous_stage.size(), 0 );
	for ( core::Size ii=1; ii<= results_from_previous_stage.size(); ++ii ) {
		ResultElements const & elem = results_from_previous_stage[ ii ];
		debug_assert( cluster_data_for_results_of_stage_[ stage_being_clustered ][ elem.job_result_id() ] );
		points[ ii ] = cluster_data_for_results_of_stage_[ stage_being_clustered ][ elem.job_result_id() ];
	}

	//cluster
	utility::vector1< bool > medoids;
	core::Size const num_clusters = num_results_to_keep_after_clustering_for_stage_[ stage_being_clustered ];
	cluster::k_medoids_on_the_fly( points, num_clusters, medoids );
	//We can implement the precalculated version of k_medoids if the distance metric is slow to compute AND the number of elements is less than 50,000 (ideally less than 10,000)

	cluster_data_for_results_of_stage_[ stage_being_clustered ].clear();
	most_recent_cluster_results_.clear();
	for ( core::Size ii = 1; ii <= medoids.size(); ++ii ) {
		if ( medoids[ ii ] ) {
			most_recent_cluster_results_.push_back( results_from_previous_stage[ ii ].job_result_id() );
		} else {
			//These get taken care of by the JobGenealogist, no need to discard them here
			//results_to_be_discarded_.push_back( results_from_previous_stage[ ii ].job_result_id() );
		}
	}

	for ( core::Size ii = 1; ii <= stage_being_clustered; ++ii ) {
		node_managers_[ ii ]->clear();
	}
}

std::list< LarvalJobOP >
MRSJobQueen::determine_job_list(
	core::Size const job_dag_node_index,
	core::Size const max_njobs
) {
	TR << "determine_job_list " << max_njobs << " " << num_total_jobs_for_stage_[ job_dag_node_index ] << std::endl;
	if ( job_dag_node_index > 1 ) {
		runtime_assert( node_managers_[ job_dag_node_index - 1 ]->all_results_are_in() );//todo remove this if you ever try to fight the sawtooth pattern
	}

	if ( node_managers_[ job_dag_node_index ]->num_jobs_submitted() == 0 ) {
		debug_assert( current_inner_larval_job_for_stage_.size() == job_dag_node_index - 1 );
		current_inner_larval_job_for_stage_.push_back(
			std::pair< InnerLarvalJobOP, core::Size >( 0, num_jobs_per_input_for_stage_[ job_dag_node_index ] ) );

		if ( job_dag_node_index > 1 ) {
			auto const num_results_for_previous_stage = node_managers_[ job_dag_node_index - 1 ]->num_results_total();
			auto const max_num_jobs_for_current_stage = num_jobs_per_input_for_stage_[ job_dag_node_index ] * num_results_for_previous_stage;
			if ( max_num_jobs_for_current_stage < node_managers_[ job_dag_node_index ]->num_jobs() ) {
				//It is possible that the node manager originally expected to be running more jobs but the previous stage did not fill its quota of results.
				//In this case, we want to decrease the number of results expected by the node manager.
				node_managers_[ job_dag_node_index ]->set_num_jobs( max_num_jobs_for_current_stage );
			}

			TR << "Total number of results for stage " << (job_dag_node_index - 1)
				<< ": " << node_managers_[ job_dag_node_index - 1 ]->num_results_total() << std::endl;

			//Cluster if needed
			if ( cluster_metric_tag_for_stage_[ job_dag_node_index - 1 ] ) {
				cluster( job_dag_node_index );
				cluster_data_for_results_of_stage_[ job_dag_node_index - 1 ].clear();
			}
		}
	}

	core::Size const job_offset = node_managers_[ job_dag_node_index ]->job_offset();
	std::list< LarvalJobOP > jobs;
	for ( core::Size ii = 1; ii <= max_njobs; ++ii ) {
		if ( node_managers_[ job_dag_node_index ]->done_submitting() ) {
			break;
		}
		core::Size const local_job_id = node_managers_[ job_dag_node_index ]->get_next_local_jobid();

		LarvalJobOP larval_job = 0;
		if ( job_dag_node_index == 1 ) {
			larval_job = get_nth_job_for_initial_stage( local_job_id );
		} else {
			larval_job = get_nth_job_for_noninitial_stage( job_dag_node_index, local_job_id);
		}
		if ( larval_job ) {
			jobs.push_back( larval_job );
			get_job_tracker().increment_current_job_index();
		} else {
			node_managers_[ job_dag_node_index ]->note_job_completed( job_offset + local_job_id, 0 );
		}

	}

	return jobs;
}

JobOP
MRSJobQueen::complete_larval_job_maturation(
	LarvalJobCOP job,
	utility::options::OptionCollectionCOP job_options,
	utility::vector1< JobResultCOP > const & input_job_results
)
{
	if ( ! has_been_initialized_ ) create_and_set_initial_job_dag();

	if ( !job_options ) {
		job_options = options_for_job( * job->inner_job() );
	}

	core::Size const input_pose_id = job->inner_job()->job_node();
	unsigned int const global_job_id = job->job_index();
	unsigned int const stage = stage_for_global_job_id( global_job_id );

	core::pose::PoseOP pose = 0;
	if ( stage == 1 ) {
		core::Size const input_pose_id = job->job_node();
		utility::vector1< standard::PreliminaryLarvalJob > const & all_preliminary_larval_jobs = get_preliminary_larval_jobs();
		InnerLarvalJobCOP inner_job = all_preliminary_larval_jobs[ input_pose_id ].inner_job;
		pose = pose_for_inner_job_derived( inner_job, * job_options );
		runtime_assert( pose );
	} else {
		debug_assert( input_job_results.size() == 1 );
		PoseJobResult const & result1 = static_cast< PoseJobResult const & >( * input_job_results[ 1 ] );
		pose = result1.pose()->clone();
		runtime_assert( pose );
	}

	MRSJobOP msp_job =
		pointer::make_shared< MRSJob >( max_num_results_to_keep_per_instance_for_stage_[ stage ] );

	msp_job->set_pose( pose );
	msp_job->set_cluster_metric_tag( cluster_metric_tag_for_stage_[ stage ] );

	ParsedTagCacheOP const data_for_job = tag_manager_.generate_data_for_input_pose_id( input_pose_id, * pose );
	msp_job->parse_my_tag(
		tag_for_stage_[ stage ],
		* pose,
		data_for_job
	);

	return msp_job;
}

void
MRSJobQueen::note_job_completed(
	core::Size job_id,
	JobStatus status,
	core::Size nresults,
	bool are_you_a_unit_test
){
	runtime_assert( are_you_a_unit_test );
	core::Size const stage = stage_for_global_job_id( job_id );

	if ( status != jd3_job_status_success ) {
		node_managers_[ stage ]->note_job_completed( job_id, 0 );
		job_genealogist_->note_job_completed( stage, job_id, 0 );
	} else {
		node_managers_[ stage ]->note_job_completed( job_id, nresults );
		job_genealogist_->note_job_completed( stage, job_id, nresults );
	}
}

void
MRSJobQueen::note_job_completed( LarvalJobCOP job, JobStatus status, core::Size nresults ){

	core::Size const job_id = job->job_index();
	core::Size const stage = stage_for_global_job_id( job_id );

	if ( status != jd3_job_status_success ) {
		node_managers_[ stage ]->note_job_completed( job_id, 0 );
		job_genealogist_->note_job_completed( stage, job_id, 0 );
	} else {
		node_managers_[ stage ]->note_job_completed( job_id, nresults );
		job_genealogist_->note_job_completed( stage, job_id, nresults );
	}

	if ( stage == num_stages_ ) {
		InnerLarvalJobCOP inner_job = job->inner_job();
		utility::options::OptionCollectionOP job_options = options_for_job( *inner_job );
		for ( core::Size ii = 1; ii <= nresults; ++ii ) {
			output::OutputSpecificationOP out_spec = create_output_specification_for_job_result( job, *job_options, ii, nresults );
			pose_output_specification_for_job_result_id_[ jd3::JobResultID( job_id, ii ) ] = out_spec;
		}
	}

}


void
MRSJobQueen::completed_job_summary( core::Size job_id, core::Size result_index, JobSummaryOP summary ){
	core::Size const stage = stage_for_global_job_id( job_id );

	//TR << "completed_job_summary() for stage=" << stage << " and job_id=" << job_id << " and result_index=" << result_index << std::endl;

	job_summaries::EnergyJobSummary const & energy_summary =
		static_cast< job_summaries::EnergyJobSummary const & >( * summary );

	if ( cluster_metric_tag_for_stage_[ stage ] ) {
		//TR << "\tculster_metric_tag found" << std::endl;
		MRSJobSummary const & mrs_summary =
			static_cast< MRSJobSummary const & >( energy_summary );

		cluster_data_for_results_of_stage_[ stage ][ jd3::JobResultID( job_id, result_index ) ] = mrs_summary.cluster_metric();
	}

	if ( max_num_results_to_keep_per_input_struct_for_stage_[ stage ] ) {
		//TR << "\tmax_num_results_to_keep_per_input_struct_for_stage_[ stage ] found and equal to " << max_num_results_to_keep_per_input_struct_for_stage_[ stage ] << std::endl;

		if ( stage == 1 ) {
			node_managers_[ stage ]->register_result(
				job_id, result_index, energy_summary.energy(), input_pose_id_for_jobid( job_id ), input_pose_id_for_jobid( job_id ) );
		} else {
			jd3::JGJobNodeCOP const job_node = job_genealogist_->get_const_job_node( stage, job_id );
			jd3::JGResultNodeCOP const parent = job_node->parents()[ 1 ].lock();
			jd3::JGJobNodeCOP const grandparent = parent->parent().lock();

			//first 32 bits of token are result id, last 32 bits are job id
			uint64_t token = parent->result_id();
			token = token << 32;
			token |= grandparent->global_job_id();
			node_managers_[ stage ]->register_result(
				job_id, result_index, energy_summary.energy(), input_pose_id_for_jobid( job_id ), token );

		}
	} else {
		node_managers_[ stage ]->register_result(
			job_id, result_index, energy_summary.energy(), input_pose_id_for_jobid( job_id ) );
	}

	if ( stage == num_stages_ ) {
		if ( node_managers_[ stage ]->all_results_are_in() ) {
			TR << "Job Genealogy:" << std::endl;
			TR << job_genealogist_->newick_tree() << std::endl;
		}
	}
}

std::list< jd3::JobResultID >
MRSJobQueen::job_results_that_should_be_discarded(){
	std::list< jd3::JobResultID > list_of_all_job_results_to_be_discarded;

	for ( core::Size stage = num_stages_; stage > 0; --stage ) {
		//Add job results that are being discarded because they did not score well enough
		std::list< jd3::JobResultID > job_results_to_be_discarded_for_stage;
		node_managers_[ stage ]->append_job_results_that_should_be_discarded( job_results_to_be_discarded_for_stage );

		for ( jd3::JobResultID const & discarded_result : job_results_to_be_discarded_for_stage ) {

			if ( ! cluster_data_for_results_of_stage_[ stage ].empty() ) {
				cluster_data_for_results_of_stage_[ stage ].erase( discarded_result );
			}

			job_genealogist_->discard_job_result( stage, discarded_result );

			if ( stage == num_stages_ ) {
				bool const member_is_present_in_map = pose_output_specification_for_job_result_id_.erase( discarded_result ) > 0;
				debug_assert( member_is_present_in_map );
			}

		}

		list_of_all_job_results_to_be_discarded.splice( list_of_all_job_results_to_be_discarded.end(),
			job_results_to_be_discarded_for_stage );

		//Add job results that are being discarded because they have no remaining children
		if ( stage < num_stages_ ) {
			if ( node_managers_[ stage + 1 ]->all_results_are_in() ) {
				if ( true ) { //TODO make this an option
					job_genealogist_->garbage_collection( stage, true, list_of_all_job_results_to_be_discarded );
				} else {
					job_genealogist_->all_job_results_for_node( stage, list_of_all_job_results_to_be_discarded );
					job_results_have_been_discarded_for_stage_[ stage ] = true;
				}
			}
		}

	}

	return list_of_all_job_results_to_be_discarded;
}

std::list< output::OutputSpecificationOP >
MRSJobQueen::jobs_that_should_be_output() {
	if ( ! node_managers_.back()->all_results_are_in() ) {
		return std::list< output::OutputSpecificationOP >( 0 );
	}

	//reset just in case:
	num_structs_output_for_input_job_tag_.assign( num_input_structs_, 0 );

	utility::vector1< ResultElements > const & all_results = node_managers_.back()->results_to_keep();
	std::list< output::OutputSpecificationOP > list_to_return;
	for ( ResultElements const & res_elem : all_results ) {

		debug_assert(
			pose_output_specification_for_job_result_id_.find( jd3::JobResultID ( res_elem.global_job_id, res_elem.local_result_id ) )
			!= pose_output_specification_for_job_result_id_.end()
		);

		output::OutputSpecificationOP res_elem_os = pose_output_specification_for_job_result_id_.at(
			jd3::JobResultID ( res_elem.global_job_id, res_elem.local_result_id )
		);

		debug_assert( res_elem_os );

		JobOutputIndex index;
		assign_output_index( res_elem.global_job_id, res_elem.local_result_id, index );
		res_elem_os->output_index( index );

		list_to_return.push_back( res_elem_os );
	}

	TR << "Preparing to output " << list_to_return.size() << " results!" << std::endl;

	return list_to_return;
}

core::pose::PoseOP
MRSJobQueen::pose_for_job_derived (
	LarvalJobCOP job,
	utility::options::OptionCollection const & options
)
{
	runtime_assert( job );
	InnerLarvalJobCOP inner_job =  job->inner_job();
	runtime_assert( inner_job );

	return pose_for_inner_job_derived( inner_job, options );
}


core::pose::PoseOP
MRSJobQueen::pose_for_inner_job_derived(
	InnerLarvalJobCOP inner_job,
	utility::options::OptionCollection const & options
)
{
	pose_inputters::PoseInputSource const & input_source =
		dynamic_cast< pose_inputters::PoseInputSource const & >( inner_job->input_source() );
	TR << "Looking for input source " << input_source.input_tag() << " with pose_id " << input_source.source_id() << std::endl;

	if ( most_recent_pose_id_.pose_id == input_source.source_id() ) {
		core::pose::PoseOP pose( pointer::make_shared< core::pose::Pose >() );
		pose->detached_copy( * most_recent_pose_id_.pose );
		return pose;
	}

	utility::tag::TagCOP inputter_tag;
	if ( inner_job->jobdef_tag() && inner_job->jobdef_tag()->hasTag( "Input" ) ) {
		utility::tag::TagCOP input_tag = inner_job->jobdef_tag()->getTag( "Input" );
		if ( input_tag->getTags().size() != 0 ) {
			inputter_tag = input_tag->getTags()[ 0 ];
		}
	}

	core::pose::PoseOP input_pose =
		pose_inputter_for_job( *inner_job )->pose_from_input_source( input_source, options, inputter_tag );
	TR << "Storing Pose for input source " << input_source.input_tag() << " with pose_id " << input_source.source_id() << std::endl;

	most_recent_pose_id_.pose = input_pose;
	most_recent_pose_id_.pose_id = input_source.source_id();

	core::pose::PoseOP pose( pointer::make_shared< core::pose::Pose >() );
	pose->detached_copy( *input_pose );
	return pose;
}


LarvalJobOP
MRSJobQueen::get_nth_job_for_initial_stage( core::Size local_job_id ){

	core::Size const global_job_id = local_job_id;
	core::Size const pose_input_source_id = 1 + ( ( global_job_id - 1) / num_jobs_per_input_for_stage_[ 1 ] );

	debug_assert( pose_input_source_id );
	runtime_assert( pose_input_source_id <= num_input_structs_ );

	if ( ++current_inner_larval_job_for_stage_[ 1 ].second > num_jobs_per_input_for_stage_[ 1 ] ) {
		current_inner_larval_job_for_stage_[ 1 ].first =
			standard::StandardJobQueen::create_and_init_inner_larval_job_from_preliminary( num_jobs_per_input_for_stage_[ 1 ], pose_input_source_id );
		current_inner_larval_job_for_stage_[ 1 ].second = 1;
	}

	LarvalJobOP ljob =
		pointer::make_shared< LarvalJob >(
		current_inner_larval_job_for_stage_[ 1 ].first,
		current_inner_larval_job_for_stage_[ 1 ].second,
		global_job_id );

	job_genealogist_->register_new_job( 1, global_job_id, pose_input_source_id );

	return ljob;
}

LarvalJobOP
MRSJobQueen::get_nth_job_for_noninitial_stage( core::Size stage, core::Size local_job_id ){

	core::Size const prev_job_result_id = 1 + ( local_job_id - 1 ) / num_jobs_per_input_for_stage_[ stage ];

	//0-indexing equivalent: num_jobs_per_input_for_stage_[ stage-1 ] / local_job_id
	jd3::JobResultID input_job_result;
	if ( cluster_metric_tag_for_stage_[ stage - 1 ] ) {
		if ( local_job_id > most_recent_cluster_results_.size() ) return 0;
		input_job_result = most_recent_cluster_results_[ prev_job_result_id ];
	} else {
		input_job_result = node_managers_[ stage-1 ]->get_nth_job_result_id( prev_job_result_id );
	}

	if ( !input_job_result.first && !input_job_result.second ) return 0;

	core::Size const pose_input_source_id = input_pose_id_for_jobid( input_job_result.first );
	debug_assert( pose_input_source_id );

	core::Size const global_job_id = node_managers_[ stage ]->job_offset() + local_job_id;
	debug_assert( global_job_id );

	job_genealogist_->register_new_job( stage, global_job_id, stage-1, input_job_result );

	if ( ++current_inner_larval_job_for_stage_[ stage ].second > num_jobs_per_input_for_stage_[ stage ] ) {
		current_inner_larval_job_for_stage_[ stage ].first =
			standard::StandardJobQueen::create_and_init_inner_larval_job_from_preliminary( num_jobs_per_input_for_stage_[ stage ], pose_input_source_id );
		current_inner_larval_job_for_stage_[ stage ].second = 1;

		current_inner_larval_job_for_stage_[ stage ].first->add_input_job_result_index( input_job_result );
	}

	LarvalJobOP ljob = pointer::make_shared< LarvalJob > (
		current_inner_larval_job_for_stage_[ stage ].first,
		current_inner_larval_job_for_stage_[ stage ].second,
		global_job_id );

	return ljob;
}


void
MRSJobQueen::append_common_tag_subelements(
	utility::tag::XMLSchemaDefinition & xsd,
	utility::tag::XMLSchemaComplexTypeGenerator & ct_gen
) const {

	using namespace utility::tag;

	moves::MoverFactory::get_instance()->define_mover_xml_schema( xsd );
	filters::FilterFactory::get_instance()->define_filter_xml_schema( xsd );

	for ( auto data_loader_pair : parser::DataLoaderFactory::get_instance()->loader_map() ) {
		data_loader_pair.second->provide_xml_schema( xsd );
	}

	XMLSchemaSimpleSubelementList data_elements;
	for ( auto data_loader_pair : parser::DataLoaderFactory::get_instance()->loader_map() ) {
		data_elements.add_already_defined_subelement(
			data_loader_pair.first, data_loader_pair.second->schema_ct_naming_function() );
	}
	data_elements.add_already_defined_subelement(
		"MOVERS", & parser::DataLoaderFactory::data_loader_ct_namer );
	data_elements.add_already_defined_subelement(
		"FILTERS", & parser::DataLoaderFactory::data_loader_ct_namer );

	ct_gen.add_ordered_subelement_set_as_repeatable( data_elements );

	//PROTOCOLS
	XMLSchemaComplexTypeGenerator protocols_block_ct_gen;
	XMLSchemaSimpleSubelementList optional_protocols_list;

	XMLSchemaAttribute num_runs_per_input_struct(
		"num_runs_per_input_struct",
		XMLSchemaType(xsct_non_negative_integer),
		"Defines the number of jobs to spawn from a single job result from the previous mover" );
	num_runs_per_input_struct.default_value( "1" );
	//num_runs_per_input_struct.is_required( true );

	XMLSchemaAttribute max_num_results_to_keep_per_instance (
		"max_num_results_to_keep_per_instance",
		XMLSchemaType(xsct_non_negative_integer),
		"If the final mover of a stage can produce multiple output poses, this defines the maximum number of output poses that will be asked for. 0 means no limit. This only applies to the final mover of a step." );
	num_runs_per_input_struct.default_value( "0" );
	num_runs_per_input_struct.is_required( false );

	XMLSchemaAttribute max_num_results_to_keep_per_input_struct (
		"max_num_results_to_keep_per_input_struct",
		XMLSchemaType(xsct_non_negative_integer),
		"If num_runs_per_input_struct is greater than 1, then you may want to use this to make sure you do not end up with too many near-duplicate results." );
	num_runs_per_input_struct.default_value( "0" );
	num_runs_per_input_struct.is_required( false );

	XMLSchemaAttribute result_cutoff(
		"result_cutoff",
		XMLSchemaType(xsct_non_negative_integer),
		"Setting this to non-zero allows a stage to finish early if it reaches a certain number of results. For example, if you set this to 100 then this stage will stop running once it generates 100 results that pass filters." );

	XMLSchemaAttribute total_num_results_to_keep(
		"total_num_results_to_keep",
		XMLSchemaType(xsct_non_negative_integer),
		"Defines the number of results accross all jobs for this mover to keep" );
	total_num_results_to_keep.is_required( true );

	XMLSchemaAttribute merge_results_after_this_stage(
		"merge_results_after_this_stage",
		XMLSchemaType(xs_boolean),
		"Say you have k input poses for stage 1 (k job blocks in job-definition file) and N results for this stage. If merge_results_after_this_stage=0, we will save the N/k best results for each of the k 'groups' of results. If merge_results_after_this_stage=1 for this stage or any stage prior to this stage, we will save the N best results regardless of origin. If merge_results_after_this_stage is never used, the result-merging will happen for after docking." );
	merge_results_after_this_stage.default_value( "false" );

	//SINGLE STAGE
	XMLSchemaComplexTypeGenerator stage_block_ct_gen;
	XMLSchemaSimpleSubelementList stage_elements;

	std::map< std::string, moves::MoverCreatorOP > const & all_movers =
		moves::MoverFactory::get_instance()->mover_creator_map();
	for ( auto const & string_mover_pair : all_movers ) {
		string_mover_pair.second->provide_xml_schema( xsd );
		stage_elements.add_already_defined_subelement( string_mover_pair.first, & moves::complex_type_name_for_mover );
	}
	//TODO moves::MoverFactory::get_instance()->define_mover_xml_schema()

	std::map< std::string, filters::FilterCreatorOP > const & all_filters =
		filters::FilterFactory::get_instance()->filter_creator_map();
	for ( auto const & string_filter_pair : all_filters ) {
		string_filter_pair.second->provide_xml_schema( xsd );
		stage_elements.add_already_defined_subelement( string_filter_pair.first, & filters::complex_type_name_for_filter );
	}

	//ADD
	//please keep this up to date with whatever src/protocol/rosetta_scripts/ParsedProtocol is using:
	XMLSchemaSimpleSubelementList ssl;
	AttributeList add_subattlist;
	add_subattlist + XMLSchemaAttribute( "mover_name", xs_string, "The mover whose execution is desired" )
		+ XMLSchemaAttribute( "mover", xs_string, "The mover whose execution is desired" );
	add_subattlist + XMLSchemaAttribute( "filter_name", xs_string, "The filter whose execution is desired" )
		+ XMLSchemaAttribute( "filter", xs_string, "The filter whose execution is desired" );
	ssl.add_simple_subelement( "Add", add_subattlist, "Elements that add a particular mover-filter pair to a ParsedProtocol" )
		.complex_type_naming_func( & protocols_subelement_mangler );

	AttributeList sort_subattlist;
	sort_subattlist + XMLSchemaAttribute( "filter_name", xs_string, "The filter whose execution is desired" )
		+ XMLSchemaAttribute( "filter", xs_string, "The filter whose execution is desired" );
	sort_subattlist + XMLSchemaAttribute( "negative_score_is_good", xsct_rosetta_bool, "If this is true, results with more negative scores are favored. Otherwise, results with more positive scores are favored." );

	XMLSchemaSimpleSubelementList ssl2;
	ssl2.add_simple_subelement( "Sort", sort_subattlist, "Defines Filter to be used to create a sorting metric. Filter thresholds will still be respected, so poses may fail at this step." )
		.complex_type_naming_func( & protocols_subelement_mangler );

	stage_block_ct_gen
		.element_name( "Stage" )
		.description( "Defines Movers and Filters used in a single round" )
		.add_ordered_subelement_set_as_repeatable( ssl )
		.add_ordered_subelement_set_as_required( ssl2 )
		.add_attribute( num_runs_per_input_struct )
		.add_attribute( total_num_results_to_keep )
		.add_attribute( max_num_results_to_keep_per_instance )
		.add_attribute( max_num_results_to_keep_per_input_struct )
		.add_attribute( result_cutoff )
		.add_attribute( merge_results_after_this_stage )
		.complex_type_naming_func( & protocols_subelement_mangler )
		.write_complex_type_to_schema( xsd );

	//Checkpoint
	XMLSchemaAttribute cp_filename( "filename", XMLSchemaType( xs_string ),
		"The name of the file created to store the information for this checkpoint");
	num_runs_per_input_struct.is_required( true );

	XMLSchemaAttribute cp_old_filename( "old_filename_to_delete", XMLSchemaType( xs_string ),
		"(Optional) Perhaps you want to delete an earlier checkpoint once this one is completed. If so, provide the filename of the old one here.");
	num_runs_per_input_struct.default_value( "" );
	num_runs_per_input_struct.is_required( false );

	//Checkpoint
	XMLSchemaComplexTypeGenerator checkpoint_block_ct_gen;
	checkpoint_block_ct_gen
		.element_name( "Checkpoint" )
		.description( "Allows the user to save progress." )
		.add_attribute( cp_filename )
		.add_attribute( cp_old_filename )
		.complex_type_naming_func( & protocols_subelement_mangler )
		.write_complex_type_to_schema( xsd );


	//Clustering
	XMLSchemaSimpleSubelementList cluster_subelement;
	std::map< std::string, cluster::ClusterMetricCreatorOP > const & all_metrics
		= cluster::ClusterMetricFactory::get_instance()->creator_map();
	for ( auto const & string_metric_pair : all_metrics ) {
		string_metric_pair.second->provide_xml_schema( xsd );
		cluster_subelement.add_already_defined_subelement( string_metric_pair.first, & cluster::complex_type_name_for_cluster_metric );
	}

	//cluster_subelement.add_group_subelement( & cluster::ClusterMetricFactory::cluster_metric_xml_schema_group_name );

	XMLSchemaComplexTypeGenerator cluster_block_ct_gen;
	checkpoint_block_ct_gen
		.element_name( "Cluster" )
		.description( "Cluster surviving results based on some metric" )
		.add_attribute( total_num_results_to_keep )
		.complex_type_naming_func( & protocols_subelement_mangler )
		.set_subelements_pick_one( cluster_subelement )
		.write_complex_type_to_schema( xsd );

	//PROTOCOLS
	XMLSchemaSimpleSubelementList mandatory_protocols_list;
	mandatory_protocols_list.add_already_defined_subelement( "Stage", & protocols_subelement_mangler );
	mandatory_protocols_list.add_already_defined_subelement( "Checkpoint", & protocols_subelement_mangler );
	mandatory_protocols_list.add_already_defined_subelement( "Cluster", & protocols_subelement_mangler );
	protocols_block_ct_gen.add_ordered_subelement_set_as_repeatable( mandatory_protocols_list );

	protocols_block_ct_gen
		.element_name( "PROTOCOLS" )
		.description( "Describes Movers And Filters Used For Each Step Of The Process" )
		.complex_type_naming_func( & protocols_subelement_mangler )
		.write_complex_type_to_schema( xsd );

	XMLSchemaSimpleSubelementList protocols_element;
	protocols_element.add_already_defined_subelement( "PROTOCOLS", & protocols_subelement_mangler );
	ct_gen.add_ordered_subelement_set_as_required( protocols_element );

}

void
MRSJobQueen::parse_job_definition_tags(
	utility::tag::TagCOP common_block_tags,
	utility::vector1< standard::PreliminaryLarvalJob > const & prelim_larval_jobs
){
	num_input_structs_ = prelim_larval_jobs.size();
	num_structs_output_for_input_job_tag_.assign( num_input_structs_, 0 );
	tag_manager_.set_num_input_pose_ids( num_input_structs_ );

	parse_common_tag( common_block_tags );

	outputters_.reserve( prelim_larval_jobs.size() );
	input_job_tags_.reserve( prelim_larval_jobs.size() );
	job_results_have_been_discarded_for_stage_.assign( num_stages_, false );

	for ( core::Size ii = 1; ii <= prelim_larval_jobs.size(); ++ii ) {
		parse_single_job_tag( prelim_larval_jobs[ ii ], ii );

		InnerLarvalJobOP inner_larval_job_ii = prelim_larval_jobs[ ii ].inner_job;
		outputters_.push_back( inner_larval_job_ii->outputter() );
		input_job_tags_.push_back( inner_larval_job_ii->job_tag() );
	}
}


void MRSJobQueen::parse_common_tag( utility::tag::TagCOP common_tag ){
	core::pose::Pose dummy_pose;

	std::list< utility::tag::TagCOP > data_tags;

	utility::vector0< utility::tag::TagCOP > const & subtags = common_tag->getTags();
	for ( auto subtag : subtags ) {
		if ( subtag->getName() == "PROTOCOLS" ) {
			utility::vector0< utility::tag::TagCOP > const & protocol_subtags = subtag->getTags();

			core::Size stage_to_merge_after = 0;
			core::Size current_stage = 0;
			for ( utility::tag::TagCOP stage_subtag : protocol_subtags ) {
				if ( stage_subtag->getName() == "Stage" ) {
					++current_stage;
					cluster_metric_tag_for_stage_.push_back( 0 );
					num_results_to_keep_after_clustering_for_stage_.push_back( 0 );
					if ( stage_subtag->getOption< bool > ("merge_results_after_this_stage", false ) ) {
						stage_to_merge_after = current_stage;
					}
					parse_single_stage_tag( stage_subtag );

				} else if ( stage_subtag->getName() == "Checkpoint" ) {
					checkpoints_.push_back(
						std::pair< core::Size, utility::tag::TagCOP >( current_stage+1, std::move( stage_subtag ) ) );

				} else if ( stage_subtag->getName() == "Cluster" ) {
					auto const & cluter_subtags = stage_subtag->getTags();
					runtime_assert( cluter_subtags.size() == 1 );
					cluster_metric_tag_for_stage_.back() = cluter_subtags.front();
					num_results_to_keep_after_clustering_for_stage_.back() =
						stage_subtag->getOption< core::Size >( "total_num_results_to_keep" );
				} else {
					utility_exit_with_message( stage_subtag->getName() + " is not expected in PROTOCOLS" );
				}
			}

			TR << "Will merge results after stage " << stage_to_merge_after << std::endl;

			job_genealogist_ = pointer::make_shared< JobGenealogist >( num_stages_, num_input_structs_ );
			node_managers_.reserve( num_stages_ );
			core::Size job_offset = 0;
			for ( core::Size ii=1; ii <= num_stages_; ++ii ) {
				if ( ii < stage_to_merge_after ) {
					node_managers_.push_back(
						pointer::make_shared< EvenlyPartitionedNodeManager > (
						job_offset,
						num_total_jobs_for_stage_[ ii ],
						num_results_to_keep_for_stage_[ ii ],
						num_input_structs_,
						result_cutoffs_for_stage_[ ii ],
						true )
					);
				} else {
					node_managers_.push_back(
						pointer::make_shared< SimpleNodeManager > (
						job_offset,
						num_total_jobs_for_stage_[ ii ],
						num_results_to_keep_for_stage_[ ii ],
						result_cutoffs_for_stage_[ ii ],
						true )
					);
				}
				node_managers_[ ii ]->set_max_num_results_with_same_token_per_partition(
					max_num_results_to_keep_per_input_struct_for_stage_[ ii ] );
				job_offset += num_total_jobs_for_stage_[ ii ];
			}

		} else { //Data
			data_tags.push_back( subtag );
		}
	}

	cluster_data_for_results_of_stage_.resize( num_stages_ );
	for ( auto & map : cluster_data_for_results_of_stage_ ) {
		map.max_load_factor( 0.6 );
	}
	tag_manager_.set_common_data_tags( std::move( data_tags ) );
}

void MRSJobQueen::parse_single_stage_tag( utility::tag::TagCOP stage_subtag ){
	core::Size const stage = ++num_stages_;

	runtime_assert( num_results_to_keep_for_stage_.size() == stage-1 );

	core::Size num_runs_per_input_struct =
		stage_subtag->getOption< core::Size >( "num_runs_per_input_struct", 1 );

	runtime_assert( num_runs_per_input_struct );
	num_jobs_per_input_for_stage_.push_back( num_runs_per_input_struct );

	if ( stage == 1 ) {
		core::Size const num_total_jobs = num_runs_per_input_struct * num_input_structs_;
		num_total_jobs_for_stage_.push_back ( num_total_jobs );
	} else {
		core::Size const num_total_jobs = num_runs_per_input_struct * num_results_to_keep_for_stage_[ stage-1 ];
		num_total_jobs_for_stage_.push_back ( num_total_jobs );
	}

	runtime_assert( num_results_to_keep_for_stage_.size() == stage-1 );
	num_results_to_keep_for_stage_.push_back (
		stage_subtag->getOption< core::Size >( "total_num_results_to_keep", 1 )
	);

	max_num_results_to_keep_per_instance_for_stage_.push_back (
		stage_subtag->getOption< core::Size >( "max_num_results_to_keep_per_instance", 0 )
	);

	max_num_results_to_keep_per_input_struct_for_stage_.push_back (
		stage_subtag->getOption< core::Size >( "max_num_results_to_keep_per_input_struct", 0 )
	);

	result_cutoffs_for_stage_.push_back (
		stage_subtag->getOption< core::Size >( "result_cutoff", 0 )
	);


	runtime_assert( tag_for_stage_.size() == stage-1 );
	tag_for_stage_.push_back( stage_subtag );

}

void MRSJobQueen::append_job_tag_subelements(
	utility::tag::XMLSchemaDefinition & job_definition_xsd,
	utility::tag::XMLSchemaComplexTypeGenerator & job_ct_gen
) const
{
	using namespace utility::tag;

	moves::MoverFactory::get_instance()->define_mover_xml_schema( job_definition_xsd );
	filters::FilterFactory::get_instance()->define_filter_xml_schema( job_definition_xsd );

	for ( auto data_loader_pair : parser::DataLoaderFactory::get_instance()->loader_map() ) {
		data_loader_pair.second->provide_xml_schema( job_definition_xsd );
	}

	XMLSchemaSimpleSubelementList data_elements;
	for ( auto data_loader_pair : parser::DataLoaderFactory::get_instance()->loader_map() ) {
		data_elements.add_already_defined_subelement(
			data_loader_pair.first, data_loader_pair.second->schema_ct_naming_function() );
	}
	data_elements.add_already_defined_subelement(
		"MOVERS", & parser::DataLoaderFactory::data_loader_ct_namer );
	data_elements.add_already_defined_subelement(
		"FILTERS", & parser::DataLoaderFactory::data_loader_ct_namer );

	job_ct_gen.add_ordered_subelement_set_as_repeatable( data_elements );

	XMLSchemaSimpleSubelementList residue_selectors;
	residue_selectors.add_already_defined_subelement(
		parser::ResidueSelectorLoader::loader_name(), & parser::ResidueSelectorLoader::res_selector_loader_ct_namer );

}

void MRSJobQueen::parse_single_job_tag( standard::PreliminaryLarvalJob const & prelim_larval_job, core::Size input_pose_id ){
	utility::tag::TagCOP job_tag = prelim_larval_job.job_tag;
	runtime_assert( num_stages_ );

	std::set< std::string > non_data_loader_tags;
	non_data_loader_tags.insert( "Input" );
	non_data_loader_tags.insert( "Output" );

	std::list < utility::tag::TagCOP > data_tags;

	utility::vector0 < utility::tag::TagCOP > const & all_tags = job_tag->getTags();

	//load data
	for ( Size ii = 0; ii < all_tags.size(); ++ii ) {
		utility::tag::TagCOP iitag = all_tags[ ii ];
		std::string const iitag_name = iitag->getName();
		if ( non_data_loader_tags.find( iitag_name ) == non_data_loader_tags.end() ) {
			data_tags.push_back( iitag );
		}
	}

	if ( data_tags.size() ) {
		TR << "input job #" << input_pose_id << " has custom data." << std::endl;
	}
	tag_manager_.register_data_tags_for_input_pose_id( input_pose_id, std::move( data_tags ) );

}

void MRSJobQueen::print_job_lineage() const {

	//Newick tree format
	//http://iubio.bio.indiana.edu/treeapp/treeprint-form.html
	//http://etetoolkit.org/treeview/

	TR << "Job Genealogy:" << std::endl;
	TR << job_genealogist_->newick_tree() << std::endl;
}

void MRSJobQueen::determine_validity_of_stage_tags(){//TODO only do this for node 0

	utility::vector1< standard::PreliminaryLarvalJob > const & prelim_larval_jobs = get_preliminary_larval_jobs();
	runtime_assert( num_input_structs_ > 0 );

	moves::MoverFactory const * mover_factory = moves::MoverFactory::get_instance();
	filters::FilterFactory const * filter_factory = filters::FilterFactory::get_instance();

	for ( core::Size ii = 1; ii <= num_input_structs_; ++ii ) {
		TR << "Confirming validity of tags for input job " << ii << std::endl;
		core::pose::PoseOP pose =
			pose_for_inner_job_derived( prelim_larval_jobs[ ii ].inner_job, * prelim_larval_jobs[ ii ].job_options );

		ParsedTagCacheOP const data_for_job = tag_manager_.generate_data_for_input_pose_id( ii, * pose );

		short unsigned int stage_count = 0;
		for ( utility::tag::TagCOP tag : tag_for_stage_ ) {
			TR << "\tConfirming validity of tags for stage " << ++stage_count << std::endl;
			short unsigned int add_or_sort_count = 0;
			for ( utility::tag::TagCOP subtag : tag->getTags() ) {
				++add_or_sort_count;
				debug_assert( subtag->getName() == "Add" || subtag->getName() == "Sort");

				if ( subtag->getName() == "Add"  ) {
					std::string const mover_name = subtag->getOption< std::string >( "mover",
						subtag->getOption< std::string >( "mover_name", "" )
					);//either one works
					if ( mover_name.size() ) { //length() instead?
						utility::tag::TagCOP mover_tag = data_for_job->mover_tags->at( mover_name );
						mover_factory->newMover( mover_tag, * data_for_job->data_map,
							* data_for_job->filters_map, * data_for_job->movers_map, * pose );
					}
				}

				std::string const filter_name = subtag->getOption< std::string >( "filter",
					subtag->getOption< std::string >( "filter_name", "" )
				);
				if ( filter_name.size() ) { //length() instead?
					utility::tag::TagCOP filter_tag = data_for_job->filter_tags->at( filter_name );
					filter_factory->newFilter( filter_tag, * data_for_job->data_map,
						* data_for_job->filters_map, * data_for_job->movers_map, * pose );
				} else if ( add_or_sort_count == tag->getTags().size() ) {
					utility_exit_with_message( "All stages need to finish with a filter. No filter at the end of stage "
						+ std::to_string( ii ) + " for input pose " + std::to_string( ii ) + "." );
				}

			}//subtag
		}//tag
	}//ii

}

void MRSJobQueen::assign_output_index(
	LarvalJobCOP larval_job,
	Size result_index_for_job,
	Size,
	JobOutputIndex & output_index
) {
	assign_output_index( larval_job->job_index(), result_index_for_job, output_index );
}

void MRSJobQueen::assign_output_index(
	core::Size global_job_id,
	core::Size local_result_id,
	JobOutputIndex & output_index
) {
	output_index.primary_output_index = global_job_id;
	output_index.n_primary_outputs =
		std::accumulate( num_total_jobs_for_stage_.begin(), num_total_jobs_for_stage_.end(), 0 );

	output_index.secondary_output_index = local_result_id;
	output_index.n_secondary_outputs = max_num_results_to_keep_per_instance_for_stage_.back();
}

} //multistage_rosetta_scripts
} //protocols

#ifdef    SERIALIZATION

/// @brief Automatically generated serialization method
template< class Archive >
void
protocols::multistage_rosetta_scripts::MRSJobQueen::save( Archive & arc ) const {
	arc( cereal::base_class< protocols::jd3::standard::StandardJobQueen >( this ) );
	arc( CEREAL_NVP( has_been_initialized_ ) ); // _Bool
	arc( CEREAL_NVP( sorter_ ) ); // struct protocols::multistage_rosetta_scripts::SortByLowEnergy
	arc( CEREAL_NVP( num_input_structs_ ) ); // core::Size
	arc( CEREAL_NVP( num_stages_ ) ); // core::Size
	arc( CEREAL_NVP( num_total_jobs_for_stage_ ) ); // utility::vector1<core::Size>
	arc( CEREAL_NVP( num_results_to_keep_for_stage_ ) ); // utility::vector1<core::Size>
	arc( CEREAL_NVP( max_num_results_to_keep_per_instance_for_stage_ ) ); // utility::vector1<core::Size>
	arc( CEREAL_NVP( max_num_results_to_keep_per_input_struct_for_stage_ ) ); // utility::vector1<core::Size>
	arc( CEREAL_NVP( num_jobs_per_input_for_stage_ ) ); // utility::vector1<core::Size>
	arc( CEREAL_NVP( result_cutoffs_for_stage_ ) ); // utility::vector1<core::Size>
	arc( CEREAL_NVP( job_results_have_been_discarded_for_stage_ ) ); // utility::vector1<core::Size>
	arc( CEREAL_NVP( node_managers_ ) ); // utility::vector1<jd3::dag_node_managers::NodeManagerOP>
	arc( CEREAL_NVP( tag_for_stage_ ) ); // utility::vector1<utility::tag::TagCOP>
	arc( CEREAL_NVP( tag_manager_ ) ); // class protocols::multistage_rosetta_scripts::TagManager
	//arc( CEREAL_NVP( validator_ ) ); // utility::tag::XMLSchemaValidatorOP
	arc( CEREAL_NVP( outputters_ ) ); // utility::vector1<std::string>
	arc( CEREAL_NVP( input_job_tags_ ) ); // utility::vector1<std::string>
	arc( CEREAL_NVP( num_structs_output_for_input_job_tag_ ) ); // utility::vector1<core::Size>
	arc( CEREAL_NVP( current_inner_larval_job_for_stage_ ) ); // utility::vector1<std::pair<jd3::InnerLarvalJobOP, core::Size> >
	arc( CEREAL_NVP( job_genealogist_ ) ); // jd3::JobGenealogistOP
	arc( CEREAL_NVP( pose_output_specification_for_job_result_id_ ) ); // std::map<jd3::JobResultID, jd3::output::OutputSpecificationOP>
	arc( CEREAL_NVP( most_recent_pose_id_ ) ); // struct protocols::multistage_rosetta_scripts::PoseForPoseID
	arc( CEREAL_NVP( checkpoints_ ) ); // utility::vector1<std::pair<core::Size, utility::tag::TagCOP> >
	arc( CEREAL_NVP( cluster_metric_tag_for_stage_ ) ); // utility::vector1<utility::tag::TagCOP>
	arc( CEREAL_NVP( num_results_to_keep_after_clustering_for_stage_ ) ); // utility::vector1<core::Size>
	arc( CEREAL_NVP( cluster_data_for_results_of_stage_ ) ); // utility::vector1<std::unordered_map<jd3::JobResultID, cluster::ClusterMetricCOP, JobResultID_hash> >
	arc( CEREAL_NVP( most_recent_cluster_results_ ) ); // utility::vector1<jd3::JobResultID>
}

/// @brief Automatically generated deserialization method
template< class Archive >
void
protocols::multistage_rosetta_scripts::MRSJobQueen::load( Archive & arc ) {
	arc( cereal::base_class< protocols::jd3::standard::StandardJobQueen >( this ) );
	arc( has_been_initialized_ ); // _Bool
	arc( sorter_ ); // struct protocols::multistage_rosetta_scripts::SortByLowEnergy
	arc( num_input_structs_ ); // core::Size
	arc( num_stages_ ); // core::Size
	arc( num_total_jobs_for_stage_ ); // utility::vector1<core::Size>
	arc( num_results_to_keep_for_stage_ ); // utility::vector1<core::Size>
	arc( max_num_results_to_keep_per_instance_for_stage_ ); // utility::vector1<core::Size>
	arc( max_num_results_to_keep_per_input_struct_for_stage_ ); // utility::vector1<core::Size>
	arc( num_jobs_per_input_for_stage_ ); // utility::vector1<core::Size>
	arc( result_cutoffs_for_stage_ ); // utility::vector1<core::Size>
	arc( job_results_have_been_discarded_for_stage_ ); // utility::vector1<core::Size>
	arc( node_managers_ ); // utility::vector1<jd3::dag_node_managers::NodeManagerOP>
	utility::vector1< utility::tag::TagOP > local_tag_for_stage;
	arc( local_tag_for_stage ); // utility::vector1<utility::tag::TagCOP>
	tag_for_stage_ = local_tag_for_stage; // copy the non-const pointer(s) into the const pointer(s)
	arc( tag_manager_ ); // class protocols::multistage_rosetta_scripts::TagManager
	//arc( validator_ ); // utility::tag::XMLSchemaValidatorOP
	arc( outputters_ ); // utility::vector1<std::string>
	arc( input_job_tags_ ); // utility::vector1<std::string>
	arc( num_structs_output_for_input_job_tag_ ); // utility::vector1<core::Size>
	arc( current_inner_larval_job_for_stage_ ); // utility::vector1<std::pair<jd3::InnerLarvalJobOP, core::Size> >
	arc( job_genealogist_ ); // jd3::JobGenealogistOP
	arc( pose_output_specification_for_job_result_id_ ); // std::map<jd3::JobResultID, jd3::output::OutputSpecificationOP>
	arc( most_recent_pose_id_ ); // struct protocols::multistage_rosetta_scripts::PoseForPoseID
	utility::vector1< std::pair< core::Size, utility::tag::TagOP > > local_checkpoints;
	arc( local_checkpoints ); // utility::vector1<std::pair<core::Size, utility::tag::TagCOP> >
	checkpoints_ = local_checkpoints;
	utility::vector1< std::shared_ptr< utility::tag::Tag > > local_cluster_metric_tag_for_stage;
	arc( local_cluster_metric_tag_for_stage ); // utility::vector1<utility::tag::TagCOP>
	cluster_metric_tag_for_stage_ = local_cluster_metric_tag_for_stage; // copy the non-const pointer(s) into the const pointer(s)
	arc( num_results_to_keep_after_clustering_for_stage_ ); // utility::vector1<core::Size>
	arc( cluster_data_for_results_of_stage_ ); // utility::vector1<std::unordered_map<jd3::JobResultID, cluster::ClusterMetricCOP, JobResultID_hash> >
	arc( most_recent_cluster_results_ ); // utility::vector1<jd3::JobResultID>
}

SAVE_AND_LOAD_SERIALIZABLE( protocols::multistage_rosetta_scripts::MRSJobQueen );
CEREAL_REGISTER_TYPE( protocols::multistage_rosetta_scripts::MRSJobQueen )


/// @brief Automatically generated serialization method
template< class Archive >
void
protocols::multistage_rosetta_scripts::PoseForPoseID::save( Archive & arc ) const {
	arc( CEREAL_NVP( pose_id ) ); // core::Size
	arc( CEREAL_NVP( pose ) ); // core::pose::PoseOP
}

/// @brief Automatically generated deserialization method
template< class Archive >
void
protocols::multistage_rosetta_scripts::PoseForPoseID::load( Archive & arc ) {
	arc( pose_id ); // core::Size
	arc( pose ); // core::pose::PoseOP
}

SAVE_AND_LOAD_SERIALIZABLE( protocols::multistage_rosetta_scripts::PoseForPoseID );
CEREAL_REGISTER_DYNAMIC_INIT( protocols_multistage_rosetta_scripts_MRSJobQueen )
#endif // SERIALIZATION
