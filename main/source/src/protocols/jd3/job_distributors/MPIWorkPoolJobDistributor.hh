// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file   protocols/jd3/job_distributors/MPIWorkPoolJobDistributor.hh
/// @brief  jd3 version of header for MPIWorkPoolJobDistributor - intended for MPI jobs on
///         large numbers of nodes where the head node is dedicated to handing out new
///         jobs to workers.  This code cannot be compiled without C++11 and Serialization,
///         but it can be compiled without MPI.
/// @author Andy Watkins (amw579@nyu.edu)
/// @author Andrew Leaver-Fay (aleaverfay@gmail.com)


#ifndef INCLUDED_protocols_jd3_MPIWorkPoolJobDistributor_hh
#define INCLUDED_protocols_jd3_MPIWorkPoolJobDistributor_hh

#ifdef SERIALIZATION

// Unit headers
#include <protocols/jd3/job_distributors/MPIWorkPoolJobDistributor.fwd.hh>

// Package headers
#include <protocols/jd3/CompletedJobOutput.hh>
#include <protocols/jd3/JobDistributor.hh>
#include <protocols/jd3/Job.fwd.hh>
#include <protocols/jd3/JobDigraph.fwd.hh>
#include <protocols/jd3/JobSummary.fwd.hh>
#include <protocols/jd3/JobQueen.fwd.hh>
#include <protocols/jd3/deallocation/DeallocationMessage.fwd.hh>
#include <protocols/jd3/job_distributors/JobExtractor.fwd.hh>
#include <protocols/jd3/output/OutputSpecification.fwd.hh>

// Project headers
#include <core/types.hh>

// Numeric headers
#include <numeric/DiscreteIntervalEncodingTree.hh>

// Utility headers
#include <utility/heap.hh>
#include <utility/string_util.hh>

// C++ headers
#include <list>
#include <map>
#include <set>
#include <string>
#include <fstream>
#include <chrono>

namespace protocols {
namespace jd3 {
namespace job_distributors {

/// @brief Tags used to tag messeges sent by MPI functions used to decide whether a slave is requesting a new job id or
/// flagging as job as being a bad input
enum mpi_tags {
	mpi_work_pool_jd_new_job_request,
	mpi_work_pool_jd_deallocation_message,
	mpi_work_pool_jd_new_job_available,
	mpi_work_pool_jd_job_success,
	mpi_work_pool_jd_job_failed_w_message,
	mpi_work_pool_jd_job_failed_do_not_retry,
	mpi_work_pool_jd_job_failed_bad_input,
	mpi_work_pool_jd_job_failed_retry_limit_exceeded,
	mpi_work_pool_jd_job_success_and_archival_complete,
	mpi_work_pool_jd_archive_job_result,
	mpi_work_pool_jd_output_job_result_already_available, // when the result is already being held on the output node
	mpi_work_pool_jd_accept_and_output_job_result, // when the result is on node0 and is sent along with the output spec
	mpi_work_pool_jd_output_job_result_on_archive, // when the result is on an archive node and must be fetched
	mpi_work_pool_jd_archival_completed,
	mpi_work_pool_jd_output_completed,
	mpi_work_pool_jd_retrieve_job_result,
	mpi_work_pool_jd_job_result_retrieved,         // found it.
	mpi_work_pool_jd_failed_to_retrieve_job_result, // didn't find it
	mpi_work_pool_jd_retrieve_and_discard_job_result,
	mpi_work_pool_jd_discard_job_result,
	mpi_work_pool_jd_begin_checkpointing,
	mpi_work_pool_jd_checkpointing_complete,
	mpi_work_pool_jd_restore_from_checkpoint,
	mpi_work_pool_jd_spin_down,
	mpi_work_pool_jd_error
};


/// @details This job distributor is meant for running jobs where the machine you are using has a large number of
/// processors, the number of jobs is much greater than the number of processors, or the runtimes of the individual jobs
/// could vary greatly. It dedicates the head node (whichever processor gets processor rank #0) to handling job requests
/// from the slave nodes (all nonzero ranks). Unlike the MPIWorkPartitionJobDistributor, this JD will not work at all
/// without MPI and the implementations of all but the interface functions have been put inside of ifdef directives.
/// Generally each function has a master and slave version, and the interface functions call one or the other depending
/// on processor rank.
class MPIWorkPoolJobDistributor : public JobDistributor
{
public:
	typedef std::list< core::Size > SizeList;
	typedef std::pair< LarvalJobOP, JobResultOP > LarvalJobAndResult;
	typedef std::map< JobResultID, utility::pointer::shared_ptr< std::string > > SerializedJobResultMap;
	typedef std::map< JobResultID, Size > JobResultLocationMap;
	typedef std::map< Size, LarvalJobOP > JobMap;
	typedef utility::pointer::shared_ptr< JobMap > JobMapOP;
	typedef std::set< core::Size > JobSet;
	typedef utility::pointer::shared_ptr< JobSet > JobSetOP;
	typedef std::map< core::Size, JobSetOP > OutstandingJobsForDigraphNodeMap;
	typedef std::map< core::Size, core::Size > DigraphNodeForJobMap;
	typedef std::map< core::Size, core::Size > WorkerNodeForJobMap;
	typedef std::chrono::time_point< std::chrono::system_clock > TimePoint;

public:

	///@brief ctor is protected; singleton pattern
	MPIWorkPoolJobDistributor();

	virtual ~MPIWorkPoolJobDistributor();

	/// @brief dummy for master/slave version
	virtual
	void
	go( JobQueenOP queen );

private:
	void
	go_master();

	void
	go_archive();

	void
	go_worker();

	void
	master_setup();

	core::Size
	decide_restore_from_checkpoint();

	bool
	decide_checkpoint_now();

	void
	checkpoint_master();

	void
	checkpoint_archive();

	void
	restore_from_checkpoint_master(core::Size checkpoint_index);

	void
	restore_from_checkpoint_archive();

	bool
	node_is_archive( int node_rank ) const;

	bool
	node_is_outputter( int node_rank ) const;

	void
	process_job_request_from_node( int worker_node );

	void
	process_job_failed_w_message_from_node( int worker_node );

	void
	process_job_failed_do_not_retry( int worker_node );

	void
	process_job_failed_bad_input( int worker_node );

	void
	process_job_failed_retry_limit_exceeded( int worker_node );

	void
	process_job_succeeded( int worker_node );

	int
	pick_archival_node();

	void
	process_archival_complete_message( int worker_node );

	void
	process_output_complete_message( int archival_node );

	void
	process_retrieve_job_result_request( int worker_node );

	void
	process_output_job_result_already_available_request( int remote_node );

	void
	process_accept_and_output_job_result_request( int remote_node );

	void
	process_retrieve_result_from_archive_and_output_request();

	void
	process_failed_to_retrieve_job_result_request( int archival_node );

	void
	process_retrieve_and_discard_job_result_request( int worker_node );

	void
	process_discard_job_result_request( int remote_node );

	void
	process_archive_job_result_request( int remote_node );

	void
	send_error_message_to_node0( std::string const & error_message );

	/// @brief Pull the next output specification off the queue (out of the heap) and
	/// send it to the indicated node (either an archive or an output/worker)
	bool
	send_output_instruction_to_node( int output_node );

	/// @brief Sends the next job in the queue to the remote node that has requested a job.
	/// Returns true if the job was sent, and false if there were no jobs that could be sent
	bool
	send_next_job_to_node( int worker_node );

	void
	send_deallocation_messages_to_node( int worker_node );

	void
	note_job_no_longer_running( Size job_id );

	void
	potentially_output_some_job_results();

	void
	potentially_discard_some_job_results();

	/// @brief For node0 instructing an archive node or a worker/outputter
	/// node to output a job result that node0 has been storing
	void
	send_output_spec_and_job_result_to_remote(
		Size node_id,
		output::OutputSpecificationOP spec
	);

	/// @brief For node0 instructing an archive node to output a
	/// job whose result the archive node is already storing
	void
	send_output_spec_to_remote(
		Size node_id,
		output::OutputSpecificationOP spec
	);

	/// @brief For node0 instructing a worker/outputter node that
	/// a job result lives on a particular archive node
	void
	send_output_spec_and_retrieval_instruction_to_remote(
		Size node_id, // node that will perform the output
		Size archive_node, // node where the job lives
		output::OutputSpecificationOP spec
	);

	void
	update_output_list_empty_bool();

	void
	queue_jobs_for_next_node_to_run();

	//void
	//queue_initial_digraph_nodes_and_jobs();

	bool
	not_done();

	bool
	any_worker_nodes_not_yet_spun_down();

	bool
	any_archive_node_still_outputting();

	bool
	jobs_ready_to_go();

	bool
	jobs_remain();

	void
	note_node_wants_a_job( int worker_node );

	void
	send_spin_down_signal_to_node( int worker_node );

	void
	assign_jobs_to_idling_nodes();

	void
	assign_output_tasks_to_idling_nodes();

	void
	store_deallocation_messages( std::list< deallocation::DeallocationMessageOP > const & messages );

	void
	throw_after_failed_to_retrieve_job_result(
		int worker_node,
		Size job_id,
		Size result_index,
		int archival_node
	);

	int
	request_job_from_master();

	void
	retrieve_job_and_run();

	void
	retrieve_deallocation_messages_and_new_job_and_run();

	std::pair< LarvalJobOP, utility::vector1< JobResultCOP > >
	retrieve_job_maturation_data();

	void
	worker_send_fail_do_not_retry_message_to_master(
		LarvalJobOP larval_job
	);

	void
	worker_send_fail_bad_inputs_message_to_master(
		LarvalJobOP larval_job
	);

	void
	worker_send_fail_on_bad_inputs_exception_message_to_master(
		LarvalJobOP larval_job,
		std::string const & error_message
	);

	void
	worker_send_fail_w_message_to_master(
		LarvalJobOP larval_job,
		std::string const & error_message
	);

	void
	worker_send_fail_retry_limit_exceeded(
		LarvalJobOP larval_job
	);

	void
	worker_send_job_result_to_master_and_archive(
		LarvalJobOP larval_job,
		CompletedJobOutput job_output
	);

	void
	write_output(
		output::OutputSpecificationOP spec,
		LarvalJobAndResult const & job_and_result
	);

	LarvalJobOP
	deserialize_larval_job(
		std::string const & larval_job_string
	) const;

	std::string
	serialize_larval_job(
		LarvalJobOP larval_job
	) const;

protected:
	LarvalJobAndResult
	deserialize_larval_job_and_result(
		std::string const & job_and_result_string
	) const;

private:
	std::string
	serialize_larval_job_and_result(
		LarvalJobAndResult job_and_result
	) const;

	utility::vector1< JobSummaryOP >
	deserialize_job_summaries(
		std::string const & job_summaries_string
	) const;

	std::string
	serialize_job_summaries(
		utility::vector1< JobSummaryOP > const & job_summaries
	) const;

	output::OutputSpecificationOP
	deserialize_output_specification( std::string const & spec_string ) const;

	std::string
	serialize_output_specification( output::OutputSpecificationOP output_spec ) const;

	std::string get_string_from_disk( Size job_id, Size result_index ) const;

	std::string filename_for_archive( Size job_id, Size result_index ) const;

	std::string checkpoint_file_prefix( Size checkpoint_counter ) const;

	std::string checkpoint_file_name_for_node( std::string const & prefix, int node_index ) const;

	/// @brief Create a file in the checkpoint directory that states the number of
	/// archive nodes in this execution.
	void create_checkpoint_sanity_check_file() const;

	/// @brief Returns true if the contents of the checkpoint directory describe the
	/// right number of archive nodes.
	bool checkpoint_sanity_check() const;

	void delete_checkpoint( Size checkpoint_index ) const;

	/// @throws std::string when the requested job result cannot be found.
	void retrieve_job_result_and_act( JobResultID const & id, std::function< void (std::string const & ) > const & action);

	/// @throws std::string when the requested job result cannot be found.
	void retrieve_job_result_and_act_and_delete( JobResultID const & id, std::function< void (std::string const & ) > const & action );

	void delete_job_result( JobResultID const & id );

	/// @throws std::string if the contents could not be archived
	void save_job_result( JobResultID const & id, utility::pointer::shared_ptr< std::string > const & job_result );

public:
	/// @brief Set the interval between checkpoint events, in seconds.
	/// Useful for testing only. The JD will otherwise initialize this value
	/// from the options system in its constructor.
	void set_checkpoint_period( Size period_seconds );

	/// @brief Retrieve the JobQueen from the JobDistributor; used only in testing.
	JobQueenOP job_queen() { return job_queen_; }

private:

	int mpi_rank_;
	std::string mpi_rank_string_;
	int mpi_nprocs_;
	int n_archives_;
	// nodes 1..max_n_outputters_ write output
	// The nodes in the range from n_archives_ to max_n_outputters_ are both worker nodes
	// and output nodes. They request work from node0 and sometimes are given jobs and sometimes
	// are given output specifications.
	int max_n_outputters_;
	bool store_on_node0_;
	//bool output_on_node0_; -- no output is performed from node 0
	bool compress_job_results_;

	bool archive_on_disk_;
	std::string archive_dir_name_;

	JobQueenOP job_queen_;
	JobDigraphCOP job_dag_;
	JobExtractorOP job_extractor_;

	SizeList worker_nodes_waiting_for_jobs_;

	// The big old map that stores all the JobResults that are generated
	// over the course of execution.  Held on Node-0 and on the archival
	// nodes (if any).
	SerializedJobResultMap job_results_;

	// The map that records the location of the archival nodes where
	// each not-yet-discarded job lives; not used by node 0 if
	// there are no archival nodes.
	JobResultLocationMap job_result_location_map_;
	// The heap for keeping track of which archives are holding the least
	// number of JobResults
	utility::heap n_results_per_archive_;

	// keep track of which archives are busy outputting already; don't keep track of the
	// non-archive output nodes, since they will tell us whenever they complete their tasks
	utility::vector1< bool > archive_currently_outputting_;
	// where was the JobResult stored that this given outputter is outputting?
	// E.g. if node_outputter_is_outputting_for_[ 3 ] is 2, that means node 3
	// is writing the output that was stored previously on node 2.
	utility::vector1< int > node_outputter_is_outputting_for_;
	// The list of output specifications that should be sent to the archive nodes
	// when they are next ready to output again. No output is performed on node 0.
	utility::vector1< std::list< output::OutputSpecificationOP > > jobs_to_output_for_archives_;
	utility::heap n_outstanding_output_tasks_for_archives_;
	std::list< output::OutputSpecificationOP > jobs_to_output_with_results_stored_on_node0_;
	bool output_list_empty_;

	// the worker node that is currently running each job
	WorkerNodeForJobMap  worker_node_for_job_;
	// the jobs that are currently running on remote nodes
	JobMap in_flight_larval_jobs_;

	// indexes 1..mpi_nproc_-1 because node0 does not track itself.
	utility::vector1< bool > nodes_spun_down_;

	// Node 0 keeps track of which nodes have receieved which deallocation messages
	// and when all of the remote nodes have received a particular deallocation message,
	// then that deallocation message is deleted
	bool first_call_to_determine_job_list_;
	utility::vector1< std::list< core::Size > > deallocation_messages_for_node_;
	utility::vector1< std::string > deallocation_messages_;
	utility::vector1< core::Size > n_remaining_nodes_for_deallocation_message_;

	//numeric::DiscreteIntervalEncodingTree< core::Size > output_jobs_;
	//numeric::DiscreteIntervalEncodingTree< core::Size > discarded_jobs_;

	Size default_retry_limit_;

	bool checkpoint_;
	std::string checkpoint_directory_;
	Size checkpoint_counter_;
	Size checkpoint_period_;
	TimePoint last_checkpoint_wall_clock_;
};

}//job_distributors
}//jd3
}//protocols

#else

// declare the namespace, but don't put anything in it
namespace protocols { namespace jd3 { namespace job_distributors {} } }

#endif // SERIALIZATION


#endif // INCLUDED_protocols_jd3_job_distributors_MPIWorkPoolJobDistributor_HH
