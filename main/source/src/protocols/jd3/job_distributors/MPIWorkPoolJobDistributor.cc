// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file   protocols/jd3/job_distributors/MPIWorkPoolJobDistributor.cc
/// @brief  implementation of MPIWorkPoolJobDistributor
/// @author Andy Watkins (amw579@nyu.edu)
/// @author Andrew Leaver-Fay (aleaverfay@gmail.com)


#ifdef SERIALIZATION

// MPI headers
#ifdef USEMPI
#include <mpi.h> //keep this first
#endif

// Unit headers
#include <protocols/jd3/job_distributors/MPIWorkPoolJobDistributor.hh>

// Package headers
#include <protocols/jd3/LarvalJob.hh>
#include <protocols/jd3/JobQueen.hh>
#include <protocols/jd3/Job.hh>
#include <protocols/jd3/JobDigraph.hh>
#include <protocols/jd3/JobSummary.hh>
#include <protocols/jd3/JobResult.hh>
#include <protocols/jd3/deallocation/DeallocationMessage.hh>
#include <protocols/jd3/job_distributors/JobExtractor.hh>
#include <protocols/jd3/output/OutputSpecification.hh>
#include <protocols/jd3/output/ResultOutputter.hh>

// Basic headers
#include <basic/Tracer.hh>
#include <basic/options/option.hh>
#include <basic/options/keys/jd3.OptionKeys.gen.hh>
#include <basic/options/keys/out.OptionKeys.gen.hh>

// Utility headers
#include <utility/exit.hh>
#include <utility/assert.hh>
#include <utility/mpi_util.hh>
#include <utility/vector1.srlz.hh>
#include <utility/io/zipstream.hpp>
#include <utility/pointer/memory.hh>
#include <utility/file/file_sys_util.hh>

// Cereal headers
#include <cereal/archives/binary.hpp>
#include <cereal/types/polymorphic.hpp>
#include <cereal/types/list.hpp>
#include <cereal/types/map.hpp>
#include <cereal/types/utility.hpp>
#include <cereal/details/helpers.hpp>

// C++
#include <utility>
#include <cstdio>

static basic::Tracer TR( "protocols.jd3.job_distributors.MPIWorkPoolJobDistributor" );

namespace protocols {
namespace jd3 {
namespace job_distributors {

using core::Size;

MPIWorkPoolJobDistributor::MPIWorkPoolJobDistributor() :
	n_archives_( basic::options::option[ basic::options::OptionKeys::jd3::n_archive_nodes ] ),
	max_n_outputters_( 0 ),
	store_on_node0_( n_archives_ == 0 ||
	! basic::options::option[ basic::options::OptionKeys::jd3::do_not_archive_on_node0 ]() ),
	compress_job_results_( basic::options::option[ basic::options::OptionKeys::jd3::compress_job_results ]() ),
	archive_on_disk_( basic::options::option[ basic::options::OptionKeys::jd3::archive_on_disk ].user() ),
	archive_dir_name_( basic::options::option[ basic::options::OptionKeys::jd3::archive_on_disk ]() ),
	n_results_per_archive_( n_archives_ + 1 ),
	archive_currently_outputting_( n_archives_, false ),
	jobs_to_output_for_archives_( n_archives_ ),
	n_outstanding_output_tasks_for_archives_( n_archives_ ),
	output_list_empty_( true ),
	default_retry_limit_( 1 ),
	checkpoint_( false ),
	checkpoint_counter_( 1 ),
	checkpoint_period_( 1800 ),
	last_checkpoint_wall_clock_( std::chrono::system_clock::now() )
{
	mpi_rank_ = utility::mpi_rank();
	mpi_rank_string_ = utility::to_string( mpi_rank_ );
	mpi_nprocs_ = utility::mpi_nprocs();
	max_n_outputters_ = std::max( int( std::ceil( mpi_nprocs_ *
		basic::options::option[ basic::options::OptionKeys::jd3::mpi_fraction_outputters ])), 0 );

	if ( max_n_outputters_ < n_archives_ ) {
		max_n_outputters_ = n_archives_;
	}
	if ( max_n_outputters_ <= 0 ) {
		max_n_outputters_ = 1;
	}

	job_extractor_.reset( new JobExtractor );

	if ( mpi_rank_ == 0 ) {
		nodes_spun_down_.resize( mpi_nprocs_-1, false );
		deallocation_messages_for_node_.resize( mpi_nprocs_ );
	}

	if ( basic::options::option[ basic::options::OptionKeys::jd3::checkpoint ] ||
			basic::options::option[ basic::options::OptionKeys::jd3::checkpoint_period ].user() ||
			basic::options::option[ basic::options::OptionKeys::jd3::checkpoint_directory ].user() ) {
		checkpoint_ = true;
		if ( basic::options::option[ basic::options::OptionKeys::jd3::checkpoint_period ].user() ) {
			int checkpoint_period = basic::options::option[ basic::options::OptionKeys::jd3::checkpoint_period ];
			if ( checkpoint_period <= 0 ) {
				utility_exit_with_message("Positive values for checkpoint period must be given");
			}
			checkpoint_period_ = 60 * checkpoint_period;
		}
		checkpoint_directory_ = basic::options::option[ basic::options::OptionKeys::jd3::checkpoint_directory ];
	}
}

MPIWorkPoolJobDistributor::~MPIWorkPoolJobDistributor() {}

void
MPIWorkPoolJobDistributor::go( JobQueenOP queen )
{
	job_queen_ = queen;
	job_extractor_->set_job_queen( queen );
	if ( mpi_rank_ == 0 ) {
		go_master();
	} else if ( node_is_archive(mpi_rank_) ) {
		go_archive();
	} else {
		go_worker();
	}

#ifdef USEMPI
	MPI_Barrier( MPI_COMM_WORLD );
 	MPI_Finalize();
#endif

}


void
MPIWorkPoolJobDistributor::go_master()
{
	master_setup();
	core::Size checkpoint_index = decide_restore_from_checkpoint();
	if ( checkpoint_index > 0 ) {
		restore_from_checkpoint_master(checkpoint_index);
	}
	while ( not_done() ) {
		if ( decide_checkpoint_now() ) {
			checkpoint_master();
		}

		int worker_node = utility::receive_integer_from_anyone();
		int message = utility::receive_integer_from_node( worker_node );
		switch ( message ) {
		case mpi_work_pool_jd_new_job_request :
			process_job_request_from_node( worker_node );
			break;
		case mpi_work_pool_jd_job_success :
			// two part process; first the worker says "I'm done"
			// and node 0 says "go archive your result to node X"
			process_job_succeeded( worker_node );
			break;
		case mpi_work_pool_jd_archive_job_result :
			process_archive_job_result_request( worker_node );
			break;
		case mpi_work_pool_jd_job_success_and_archival_complete :
			// second part of the two part process: after archival
			// has completed, the node sends the JobStatus to node 0
			// so that the JobQueen can examine it
			process_archival_complete_message( worker_node );
			break;
		case mpi_work_pool_jd_output_completed :
			process_output_complete_message( worker_node );
			break;
		case mpi_work_pool_jd_job_failed_w_message :
			process_job_failed_w_message_from_node( worker_node );
			break;
		case mpi_work_pool_jd_job_failed_do_not_retry :
			process_job_failed_do_not_retry( worker_node );
			break;
		case mpi_work_pool_jd_job_failed_bad_input :
			process_job_failed_bad_input( worker_node );
			break;
		case mpi_work_pool_jd_job_failed_retry_limit_exceeded :
			process_job_failed_retry_limit_exceeded( worker_node );
			break;
		case mpi_work_pool_jd_retrieve_job_result :
			process_retrieve_job_result_request( worker_node );
			break;
		case mpi_work_pool_jd_failed_to_retrieve_job_result :
			// Fatal error: throw an exception and shut down execution
			job_queen_->flush();
			process_failed_to_retrieve_job_result_request( worker_node );
			break;
		case mpi_work_pool_jd_error :
			job_queen_->flush();
			throw CREATE_EXCEPTION(utility::excn::Exception,  "Received error from node " +
				utility::to_string( worker_node ) + ": " + utility::receive_string_from_node( worker_node ) );
			break;
		default :
			// error -- we should not have gotten here
			throw CREATE_EXCEPTION(utility::excn::Exception,  "Internal Error in the MPIWorkPoolJobDistributor: received inappropriate signal "
				"on node 0 from node " + utility::to_string( worker_node )
				+ ": " + utility::to_string( message ) );
		}
	}

	// spin-down-signal sending
	for ( SizeList::const_iterator iter = worker_nodes_waiting_for_jobs_.begin();
			iter != worker_nodes_waiting_for_jobs_.end(); ++iter ) {
		send_spin_down_signal_to_node( *iter );
	}

	// Other nodes that have not yet sent their final job-request message to node 0.
	while ( any_worker_nodes_not_yet_spun_down() ) {
		int worker_node = utility::receive_integer_from_anyone();
		int message = utility::receive_integer_from_node( worker_node );
		switch ( message ) {
		case mpi_work_pool_jd_new_job_request :
			process_job_request_from_node( worker_node );
			break;
		case mpi_work_pool_jd_output_completed :
			process_output_complete_message( worker_node );
			// there are no more jobs for the archive to output, so don't ask the
			// archive to output anything else, BUT, there are possibly some
			// output nodes that have not yet requested the job results from
			// the archives. Therefore, we will wait until all of the workers have
			// spun down before spinning down the archives.
			break;
		default :
			// error -- we should not have gotten here
			throw CREATE_EXCEPTION(utility::excn::Exception, "received inappropriate signal "
				"on node 0 from node " + utility::to_string( worker_node )
				+ ": " + utility::to_string( message ) + " in worker/outputter spin-down loop" );
		}
	}

	// for ( int ii = 1; ii <= n_archives_; ++ii ) {
	//  if ( ! archive_currently_outputting_[ii] ) {
	//   utility::send_integer_to_node( ii, 0 ); // archive is expecting a node ID
	//   send_spin_down_signal_to_node( ii );
	//  }
	// }

	// Finally, wait until all archives have completed their final output operations
	// and then tell each of them to spin down.
	while ( any_archive_node_still_outputting() ) {
		int archive_node = utility::receive_integer_from_anyone();
		int message = utility::receive_integer_from_node( archive_node );
		switch ( message ) {
		case mpi_work_pool_jd_output_completed :
			process_output_complete_message( archive_node );
			break;
		default :
			// error -- we should not have gotten here
			throw CREATE_EXCEPTION(utility::excn::Exception, "received inappropriate signal "
				"on node 0 from node " + utility::to_string( archive_node )
				+ ": " + utility::to_string( message ) + " in archive spin-down loop" );
		}
	}

	for ( int ii = 1; ii <= n_archives_; ++ii ) {
		utility::send_integer_to_node( ii, 0 ); // archive is expecting a node ID
		send_spin_down_signal_to_node( ii );
	}


	job_queen_->flush();

	if ( checkpoint_ ) {
		// Clean up any checkpoint files now that we are done
		delete_checkpoint( checkpoint_counter_ - 1 );
	}

	TR << "Exiting go_master" << std::endl;
}

void
MPIWorkPoolJobDistributor::go_archive()
{
	bool keep_going = true;
	while ( keep_going ) {
		int remote_node = utility::receive_integer_from_anyone();
		int message = utility::receive_integer_from_node( remote_node );
		switch ( message ) {
		case mpi_work_pool_jd_retrieve_job_result :
			process_retrieve_job_result_request( remote_node );
			break;
		case mpi_work_pool_jd_output_job_result_already_available :
			process_output_job_result_already_available_request( remote_node );
			break;
		case mpi_work_pool_jd_accept_and_output_job_result :
			process_accept_and_output_job_result_request( remote_node );
			break;
		case mpi_work_pool_jd_output_job_result_on_archive :
			process_retrieve_result_from_archive_and_output_request();
			break;
		case mpi_work_pool_jd_retrieve_and_discard_job_result :
			process_retrieve_and_discard_job_result_request( remote_node );
			break;
		case mpi_work_pool_jd_discard_job_result :
			process_discard_job_result_request( remote_node );
			break;
		case mpi_work_pool_jd_archive_job_result :
			process_archive_job_result_request( remote_node );
			break;
		case mpi_work_pool_jd_spin_down :
			keep_going = false; // exit the lister loop
			break;
		case mpi_work_pool_jd_begin_checkpointing :
			checkpoint_archive();
			break;
		case mpi_work_pool_jd_restore_from_checkpoint :
			restore_from_checkpoint_archive();
			break;
		default :
			send_error_message_to_node0(
				"Archival node " + utility::to_string( mpi_rank_ ) + " received an "
				"illegal message " + utility::to_string( message ) + " from node "
				+ utility::to_string( remote_node ) );
			break;
		}
	}
}

void
MPIWorkPoolJobDistributor::go_worker()
{
	bool done = false;
	while ( ! done ) {
		int message = request_job_from_master();
		switch ( message ) {
		case mpi_work_pool_jd_new_job_available :
			retrieve_job_and_run();
			break;
		case mpi_work_pool_jd_deallocation_message :
			retrieve_deallocation_messages_and_new_job_and_run();
			break;
		case mpi_work_pool_jd_accept_and_output_job_result :
			process_accept_and_output_job_result_request( 0 );
			break;
		case mpi_work_pool_jd_output_job_result_on_archive :
			process_retrieve_result_from_archive_and_output_request();
			break;
		case mpi_work_pool_jd_spin_down :
			done = true;
			break;
		}
	}
}

void
MPIWorkPoolJobDistributor::master_setup()
{
	job_dag_ = job_extractor_->get_initial_job_dag_and_queue();

	// initialize the n_results_per_archive_ heap
	bool err( false );
	if ( store_on_node0_ ) {
		n_results_per_archive_.heap_insert( 0, 0, err );
	}
	for ( int ii = 1; ii <= n_archives_; ++ii ) {
		n_results_per_archive_.heap_insert( ii, 0, err );
		n_outstanding_output_tasks_for_archives_.heap_insert( ii, 0, err );
	}
	node_outputter_is_outputting_for_.resize( max_n_outputters_, -1 );

	if ( archive_on_disk_ && ! utility::file::is_directory( archive_dir_name_ ) ) {
		bool const successfully_made_directory = utility::file::create_directory( archive_dir_name_ );
		if ( ! successfully_made_directory ) {
			utility_exit_with_message( "Unable to find or create directory: " + archive_dir_name_ );
		}
	}
	last_checkpoint_wall_clock_ = std::chrono::system_clock::now();

}

/// @details look for a complete set of checkpoint files in the
/// checkpoint directory with the highest checkpoint index.
Size
MPIWorkPoolJobDistributor::decide_restore_from_checkpoint()
{
	if ( !checkpoint_ ) return 0;
	if ( !basic::options::option[ basic::options::OptionKeys::jd3::restore_from_checkpoint ] ) return 0;

	bool const quit_on_failure = ! basic::options::option[ basic::options::OptionKeys::jd3::continue_past_failed_checkpoint_restoration ];

	TR << "Attempting to restore from checkpoints in directory " << checkpoint_directory_ << std::endl;

	if ( !utility::file::is_directory(checkpoint_directory_) ) {
		if ( quit_on_failure ) {
			throw CREATE_EXCEPTION(utility::excn::Exception, "Could not recover from checkpoint;"
				" checkpoint directory '" + checkpoint_directory_ + "' does not exist");
		}
		return 0;
	}

	// Sanity check: does the file in the existing checkpoints directory
	// list the right number of archive nodes?
	if ( !checkpoint_sanity_check() ) {
		TR << "Sanity check failed. Could not recover from checkpoint" << std::endl;
		if ( quit_on_failure ) {
			throw CREATE_EXCEPTION(utility::excn::Exception, "Could not recover from checkpoint."
				" Sanity check failed.");
		}
		return false;
	}

	utility::vector1< std::string > fnames;
	int err_code = utility::file::list_dir( checkpoint_directory_, fnames );
	if ( err_code != 0 ) {
		if ( quit_on_failure ) {
			throw CREATE_EXCEPTION(utility::excn::Exception, "Could not recover from checkpoint."
				" Unable to retreive contents of checkpoint directory '" + checkpoint_directory_ + "'");
		}
		return false;
	}
	utility::vector1< Size > candidates;
	for ( auto const & fname : fnames ) {
		// Let's break fname by _s
		utility::vector1< std::string > cols = utility::string_split_simple(fname, '_');
		if ( cols.size() != 3 ) continue;
		if ( cols[1] != "chkpt" ) continue;
		if ( cols[3] != "0.bin" ) continue;
		candidates.push_back(utility::string2Size(cols[2]));
	}
	std::sort(candidates.begin(), candidates.end());
	Size chosen(0);
	for ( Size ii = candidates.size(); ii > 0; --ii ) {
		Size cand = candidates[ii];
		bool all_found = true;
		for ( int jj = 1; jj <= n_archives_; ++jj ) {
			std::string fname = checkpoint_file_name_for_node(
				checkpoint_file_prefix( cand ), jj);
			TR << "For candidate " << cand << " looking for file " << fname << std::endl;
			if ( ! utility::file::file_exists( fname ) ) {
				TR << "  " << fname << " not found!" << std::endl;
				all_found = false;
				break;
			}
		}
		if ( all_found ) {
			chosen = cand;
			break;
		}
	}
	if ( chosen != 0 ) {
		TR << "Complete set of checkpoints found for checkpoint #" << chosen << std::endl;
	} else {
		TR << "No complete set of checkpoint files could be found" << std::endl;
		if ( quit_on_failure ) {
			throw CREATE_EXCEPTION(utility::excn::Exception, "Could not recover from checkpoint."
				" No complete set of checkpoint files could be found" );
		}
	}
	return chosen;
}

bool
MPIWorkPoolJobDistributor::decide_checkpoint_now()
{
	if ( ! checkpoint_ ) return false;
	std::chrono::duration<double> time_diff = std::chrono::system_clock::now() - last_checkpoint_wall_clock_;
	//TR << "DECIDE CHECKPOINT NOW: " << std::chrono::duration_cast< std::chrono::milliseconds >(time_diff).count() << std::endl;
	auto diff_seconds = std::chrono::duration_cast< std::chrono::seconds >(time_diff);
	return diff_seconds > std::chrono::seconds(checkpoint_period_);
}



/// @details
/// This is the function node 0 will use as it checkpoints.
///
/// Steps:
/// 1. Create the checkpoint directory if it doesn't exist;
///    this has to happen before contacting the archives.
/// 2. Rally all archive nodes. If any archive node is
///    in the middle of outputting a result to disk, wait
///    until the archive has completed its current output task,
///    but do not give it its next output task until archival
///    has completed (step 8 below).
/// 3. Create binary archive and archive all data;
///    if we are in archive_on_disk_ mode, then
///    a) load in the binary job-result data that's stored on disk, and
///    b) write that job-result data to the archive file; it would
///    not be safe to expect that the data that's on disk now will
///    still be on disk when we try to restore from a checkpoint later.
/// 4. Write binary archive to checkpoint directory
/// 5. Ensure all archives have completed their checkpointing
/// 6. Delete old checkpoint files
/// 7. Increment the checkpoint counter
/// 8. Send the next output result to any archive that was
///    in the middle of outputting its results when
///    checkpointing began.
///
/// Node 0 will store the data it needs in order to restore from a checkpoint,
/// but it will not be storing all of its data. Explainations for what will
/// be left behind are given below.
///
/// Data not serialized:
///   - worker_nodes_waiting_for_jobs_:
///     When restoreing from a checkpoint, the workers will not have stored
///     anything about what jobs they were running. Instead, they will ask
///     the master node (node 0) for new jobs. The master node will
///     redistribute the "in-flight" jobs at the time of the last archival,
///     Information about what nodes are waiting to be assigned work and even
///     how many workers there are should not be preserved.
///   - worker_node_for_job_:
///     As mentioned above, information about where jobs that are currently being
///     executed at the time of the checkpoint will not be preserved and instead
///     the set of in-flight jobs will be re-distributed.
///   - nodes_spun_down_:
///     If we are very close to the end of a run, then the job distributor will
///     need to re-spin down nodes. Worker nodes will not know that they had
///     been spun down in the previous execution.
///   - deallocation_messages_for_node_:
///     This is tricky and may need rethinking. The idea is something like this:
///     If the only time that a deallocation message needs to be delivered to a
///     node is after all of the jobs that use the thing (the Pose or the Resource)
///     have completed, then when the process is relaunched from a checkpoint file,
///     none of the in-flight jobs that the job distributor will assign will
///     require that thing. Therefore that thing will not be loaded into memory,
///     and therefore that thing will not need a deallocation message.
///     Therefore none of the deallocation-message-passing machinery ought to
///     be checkpointed.
///   - deallocation_messages_:
///     See deallocation_messages_for_node_ above.
///   - n_remaining_nodes_for_deallocation_message_:
///     See deallocation_messages_for_node_ above.
///
///
void
MPIWorkPoolJobDistributor::checkpoint_master()
{
	TR << "Creating checkpoint #" << checkpoint_counter_ << std::endl;
	// 1.
	if ( !utility::file::is_directory(checkpoint_directory_) ) {
		bool success = utility::file::create_directory(checkpoint_directory_);
		if ( !success ) {
			utility_exit_with_message( "Unable to find or create checkpoint directory: " +
				checkpoint_directory_);
		}
	}

	if ( checkpoint_counter_ == 1 ) {
		create_checkpoint_sanity_check_file();
	} else {
		if ( !checkpoint_sanity_check() ) {
			TR << "Failed sanity check while trying to create checkpoint #" << checkpoint_counter_ << "\nSkipping checkpoint attempt" << std::endl;
			return;
		}
	}

	// 2.
	for ( int ii = 1; ii <= n_archives_; ++ii ) {
		// If node i is currently writing an output, wait until it has finished.
		// This is essential if we are to avoid a deadlock.
		// Make sure that we don't send yet another output message to the node.
		// Do that after we have finished the archival process.
		if ( node_outputter_is_outputting_for_[ ii ] >= 0 ) {
			utility::receive_integer_from_node(ii);
			int message = utility::receive_integer_from_node(ii);
			runtime_assert( message == mpi_work_pool_jd_output_completed );
		}

		// Now tell the archive that it's time to run a checkpoint.
		utility::send_integer_to_node(ii, 0 );
		utility::send_integer_to_node(ii, mpi_work_pool_jd_begin_checkpointing);
		utility::send_string_to_node(ii, checkpoint_file_prefix( checkpoint_counter_ ) );
	}

	// 3.
	std::string fname = checkpoint_file_name_for_node(
		checkpoint_file_prefix(checkpoint_counter_), mpi_rank_);
	std::string fname_tmp = fname + ".tmp";
	try {
		std::ofstream out(fname_tmp,std::ios::binary);
		cereal::BinaryOutputArchive arc(out);
		arc(job_queen_);
		arc(job_dag_);
		arc(job_extractor_);
		// Skip worker_nodes_waiting_for_jobs_

		arc(job_results_);
		arc( archive_on_disk_ );
		if ( archive_on_disk_ ) {
			for ( auto jr_pair: job_results_ ) {
				TR << "Saving " << jr_pair.first << std::endl;
				arc( jr_pair.first );
				std::string contents = get_string_from_disk( jr_pair.first.first, jr_pair.first.second );
				arc( contents );
			}
		}

		arc(job_result_location_map_);
		arc(n_results_per_archive_);
		arc(jobs_to_output_for_archives_);
		arc(n_outstanding_output_tasks_for_archives_);
		arc(jobs_to_output_with_results_stored_on_node0_);
		arc(output_list_empty_);
		// Skip worker_node_for_job_
		arc(in_flight_larval_jobs_);
		// Skip nodes_spun_down_
		arc(first_call_to_determine_job_list_);
		// Skip deallocation_messages_for_node_
		// Skip deallocation_messages_
		// Skip n_remaining_nodes_for_deallocation_message_
		arc(checkpoint_counter_);
	} catch ( cereal::Exception & e ) {
		throw CREATE_EXCEPTION(utility::excn::Exception,  std::string("Failed to serialize the data"
			" in the MPIWorkPoolJobDistributor master node while creating a checkpoint."
			"\nError message from the cereal library:\n") + e.what() + "\n" );
	} catch ( std::ios_base::failure& ex ) {
		//std::cerr << "std::IO error detected... exiting..." << std::endl; // We can not longer use Tracer's at this point
		//// Using pure exit instead of utility_exit_with_status to avoid infinite recursion
		//std::exit( basic::options::option[ basic::options::OptionKeys::out::std_IO_exit_error_code]() );
		throw CREATE_EXCEPTION(utility::excn::Exception,  std::string("Failed to serialize the data"
			" in the MPIWorkPoolJobDistributor master node while creating a checkpoint."
			"\nIO error message from STL:\n") + ex.what() + "\n" );
	}
	// 4.
	// OK write the file to its permenent location on disk
	std::rename(fname_tmp.c_str(), fname.c_str());

	// 5.
	for ( int ii = 1; ii <= n_archives_; ++ii ) {
		int msg = utility::receive_integer_from_node(ii);
		if ( msg == mpi_work_pool_jd_error ) {
			throw CREATE_EXCEPTION(utility::excn::Exception,  "Received error from node " +
				utility::to_string(ii) + ": " + utility::receive_string_from_node(ii) );
		} else if ( msg == mpi_work_pool_jd_checkpointing_complete ) {
			// good!
		} else {
			throw CREATE_EXCEPTION(utility::excn::Exception,  "Unexpected message from archive node " +
				utility::to_string(ii) + " encountered while creating checkpoint file: " +
				utility::to_string( msg ));
		}
	}

	// 6.
	delete_checkpoint( checkpoint_counter_ - 1 );

	// 7.
	++checkpoint_counter_;

	// 8.
	for ( int ii = 1; ii <= n_archives_; ++ii ) {
		if ( node_outputter_is_outputting_for_[ ii ] >= 0 ) {
			process_output_complete_message(ii);
		}
	}

	last_checkpoint_wall_clock_ = std::chrono::system_clock::now();
	TR << "Wrote checkpoint #" << (checkpoint_counter_-1) << " to " << checkpoint_directory_ << std::endl;
}

/// @details After JD0 has instructed us to create a checkpoint,
/// we will
/// 1. Get the checkpoint-file prefix that we need in order to create the
///    appropriate checkpoint file.
/// 2. Create an archive with a temporary file on disk,
/// 3. Serialize the job_results_ data that we are storing; if we
///    are in archive_to_disk_ mode, then we will
///    a) retrieve the contents of the job result data stored on disk, and
///    b) write them out to the checkpoint archive
/// 4. Move the temporary file to its final location, and
/// 5. Tell JD0 that we have finished creating our checkpoint
void
MPIWorkPoolJobDistributor::checkpoint_archive()
{
	std::string checkpoint_prefix = utility::receive_string_from_node(0);
	auto checkpoint_fname = checkpoint_file_name_for_node( checkpoint_prefix, mpi_rank_ );
	auto checkpoint_fname_tmp = checkpoint_fname + ".tmp";
	std::ofstream out( checkpoint_fname_tmp.c_str(), std::ios::binary );
	if ( !out.is_open() ) {
		utility::send_integer_to_node(0, mpi_work_pool_jd_error );
		utility::send_string_to_node(0, "Failed to open checkpoint file " + checkpoint_fname_tmp );
	}
	try {
		cereal::BinaryOutputArchive arc(out);
		arc(job_results_);
		arc( archive_on_disk_ );
		if ( archive_on_disk_ ) {
			for ( auto jr_pair: job_results_ ) {
				auto archive_contents = [&] (std::string const & contents ) {
					TR.Debug << "Saving " << jr_pair.first << std::endl;
					arc( jr_pair.first );
					arc( contents );
				};
				try {
					retrieve_job_result_and_act( jr_pair.first, archive_contents );
				} catch ( std::string errstring ) {
					TR.Error << "Failed to retrieve archived job result " << jr_pair << std::endl;
					TR.Error << "Quitting" << std::endl;
					utility::send_integer_to_node(0, mpi_work_pool_jd_error );
					utility::send_string_to_node(0, "Archive node " + utility::to_string(mpi_rank_) +
						" failed to create checkpoint to file " + checkpoint_fname + " with error" +
						"\n" + errstring + "\n");
					return;
				}
			}
		}

	} catch ( cereal::Exception & e ) {
		utility::send_integer_to_node(0, mpi_work_pool_jd_error );
		utility::send_string_to_node(0, "Archive node " + utility::to_string(mpi_rank_) +
			" failed to create checkpoint to file " + checkpoint_fname + " with a serialization" +
			" error:\n" + e.what() + "\n");
		return;
	}
	out.close();

	std::rename(checkpoint_fname_tmp.c_str(), checkpoint_fname.c_str());

	utility::send_integer_to_node(0, mpi_work_pool_jd_checkpointing_complete);
}

/// @brief Restore the state of JD0 from the archived data in the checkpoint file.
///
/// @details At this point, we know which checkpoint file we are going to recover from.
///
/// 1. Restore the data stored in the checkpoint file. This overwrites many of
///    The data members in the JD, including the job queen.
///    If the archive was created in archive_to_disk_ mode, then we need
///    to unpack the archived job results one at a time from the archive;
///    if we are currently in archive_to_disk_ mode, then we need to put
///    the job results back out to disk. NOTE: you can switch between archival
///    modes. If the checkpoint was created with archive_to_disk_ mode, you
///    can turn off archive_to_disk_ mode when you are restoring. If the
///    checkpoint was created when not in archive_to_disk_ mode, you can
///    switch to archiving to disk.
/// 2. Re-queue the jobs that were in-flight when the checkpoint was created.
///    The jobs that were in-flight at the time of checkpoint creation will
///    not be running when we restore from the checkpoint and they did not
///    complete (or we can't assume they did). The JobQueen has no idea she
///    was checkpointed and has woken up from the checkpoint. She will not
///    ask a second time for these jobs to be executed. It is the JD's
///    responsibility to make sure that the jobs the JQ has given it will
///    be executed. Therefore, they must be re-queued.
/// 3. Purge the job_result_location_map_ of the JobResults that belong
///    to jobs that finished the 1st of the 2 step result-archival protocol.
///    When a worker has finished a job, JD0 tells them where they should
///    archive their results and the worker goes and processes the archival.
///    JD0 writes down which archive the worker will be storing their results
///    with. This is step 1. At this step, JD0 knows where the result will end
///    up, but will not know whether that archival has happened. Once the worker
///    has completed their archival, then they tell JD0 that the archival is
///    complete, and at this point, JD0 can guarantee that the archive actually
///    holds the result. This is step 2. If a job result is between steps 1
///    and 2, then the job_result_location_map_ is saying that a result lives
///    on an archive but it may not actually live there yet. To be safe,
///    we must purge the location information for any job result whose job
///    is listed as "in flight." Such job results are between stages 1 and 2.
/// 4. Clear the in_flight_jobs_ list.
/// 5. Now tell each archive which of its job results that it should preserve.
///    The archives may be holding some of the job results that are in between
///    stage 1 and stage 2; these results should not be recovered. New job
///    results for these in-flight jobs will be generated when those jobs are
///    re-run.
/// 6. Resume any interrupted job-outputting steps that were in-progress at
///    the time of checkpoint creation.
/// 7. Increment the checkpoint counter.
void
MPIWorkPoolJobDistributor::restore_from_checkpoint_master(core::Size checkpoint_index )
{
	std::string checkpoint_prefix = checkpoint_file_prefix(checkpoint_index);
	std::string fname = checkpoint_file_name_for_node(checkpoint_prefix, mpi_rank_);

	TR << "Attempting to restore from checkpoint #" << checkpoint_index <<
		" in directory " << checkpoint_directory_ << std::endl;

	// 1.
	std::ifstream ifs( fname.c_str(), std::ios::in|std::ios::binary );
	{
		cereal::BinaryInputArchive arc(ifs);
		arc(job_queen_);
		JobDigraphOP temp_job_dag;
		arc(temp_job_dag);
		job_dag_ = temp_job_dag;
		arc(job_extractor_);
		// Skip worker_nodes_waiting_for_jobs_

		arc(job_results_);
		bool prev_archived_on_disk;
		arc( prev_archived_on_disk );
		if ( prev_archived_on_disk ) {
			for ( core::Size ii = 1; ii <= job_results_.size(); ++ii ) {
				JobResultID id;
				arc(id);
				TR << "Restoring " << id << std::endl;
				std::string contents;
				arc(contents);
				if ( archive_on_disk_ ) {
					TR << "Writing out result " << id << std::endl;
					job_results_[id] = nullptr;
					std::string const filename = filename_for_archive( id.first, id.second );
					std::ofstream out( filename );
					debug_assert( out.is_open() );
					out << contents;
					out.close();
				} else {
					job_results_[id] = std::make_shared< std::string >(contents);
				}
			}
		}

		arc(job_result_location_map_);
		arc(n_results_per_archive_);
		arc(jobs_to_output_for_archives_);
		arc(n_outstanding_output_tasks_for_archives_);
		arc(jobs_to_output_with_results_stored_on_node0_);
		arc(output_list_empty_);
		// Skip worker_node_for_job_
		arc(in_flight_larval_jobs_);
		// Skip nodes_spun_down_
		arc(first_call_to_determine_job_list_);
		// Skip deallocation_messages_for_node_
		// Skip deallocation_messages_
		// Skip n_remaining_nodes_for_deallocation_message_
		arc(checkpoint_counter_);
	}

	// 2.
	for ( auto const & job_pair: in_flight_larval_jobs_ ) {
		job_extractor_->push_job_to_front_of_queue(job_pair.second);
	}

	// 3.
	// Purge records of job-results whose jobs were still considered
	// "in flight" at the time of checkpointing.
	for ( auto location_iter = job_result_location_map_.begin(); location_iter != job_result_location_map_.end(); /*no increment */ ) {
		auto const & id = location_iter->first;
		auto location_iter_next = location_iter;
		++location_iter_next;

		// Look to see if we still classify the job for this result-id
		// as "in flight"
		auto in_flight_iter = in_flight_larval_jobs_.find(id.first);
		if ( in_flight_iter != in_flight_larval_jobs_.end() ) {
			// Before checkpointing, we wrote down that this result would live
			// on a particular archive, but we cannot guarantee that the
			// result had been archived there. So we must remove this result
			// from the job_result_location_map_.
			job_result_location_map_.erase(location_iter);
		}

		location_iter = location_iter_next; // increment
	}
	// 4.
	in_flight_larval_jobs_.clear();

	// 5.
	// Now we inform the archives what job results they ought to be holding;
	// the archives are responsible for discarding the results that they do
	// not need
	utility::vector1< utility::vector1<Size> > results_first_for_archive(n_archives_);
	utility::vector1< utility::vector1<Size> > results_second_for_archive(n_archives_);
	for ( auto const & result_iter: job_result_location_map_ ) {
		if ( result_iter.second > 0 ) {
			results_first_for_archive[result_iter.second].push_back(result_iter.first.first);
			results_second_for_archive[result_iter.second].push_back(result_iter.first.second);
		}
	}
	for ( int ii = 1; ii <= n_archives_; ++ii ) {
		utility::send_integer_to_node(ii, 0);
		utility::send_integer_to_node(ii, mpi_work_pool_jd_restore_from_checkpoint);
		utility::send_string_to_node(ii, checkpoint_prefix);
		utility::send_sizes_to_node(ii, results_first_for_archive[ii]);
		utility::send_sizes_to_node(ii, results_second_for_archive[ii]);
	}

	// 6.
	// OK! well, the first thing we did after completing the archival was resume
	// outputting of any results that had not yet been output, so we should do that
	// now to get that process resumed.
	for ( int ii = 1; ii <= n_archives_; ++ii ) {
		if ( node_outputter_is_outputting_for_[ ii ] >= 0 ) {
			process_output_complete_message(ii);
		}
	}

	// 7.
	++checkpoint_counter_;

	TR << "Successfully restored from checkpoint" << std::endl;

}

/// @details Load the checkpointed job_results map, and store all of the
/// job results that the master node thinks we ought to have.
/// Note that it is possible that there are job results we are holding that
/// the master node does not yet know about, and therefore would never
/// ask this node to deallocate. Thus we ought to discard those results.
void
MPIWorkPoolJobDistributor::restore_from_checkpoint_archive()
{
	// OK, node 0 has just asked us to restore from a checkpoint file
	std::string checkpoint_prefix = utility::receive_string_from_node(0);

	utility::vector1<Size> results_to_keep_first = utility::receive_sizes_from_node(0);
	utility::vector1<Size> results_to_keep_second = utility::receive_sizes_from_node(0);

	std::string fname = checkpoint_file_name_for_node(checkpoint_prefix, mpi_rank_);
	TR << "Attempting to restore from checkpoint file " << fname << std::endl;
	std::ifstream ifs(fname.c_str(), std::ios::in|std::ios::binary);
	SerializedJobResultMap temp_job_results;
	try {
		cereal::BinaryInputArchive arc(ifs);
		arc(temp_job_results);
		bool prev_archived_on_disk;
		arc( prev_archived_on_disk );
		if ( prev_archived_on_disk ) {
			for ( core::Size ii = 1; ii <= temp_job_results.size(); ++ii ) {
				JobResultID id;
				arc(id);
				TR << "Restoring " << id << std::endl;
				std::string contents;
				arc(contents);
				temp_job_results[ id ] = std::make_shared< std::string >(contents);
			}
		}
	} catch ( cereal::Exception & e ) {
		utility::send_integer_to_node(0, mpi_work_pool_jd_error );
		utility::send_string_to_node(0, "Archive node " + utility::to_string(mpi_rank_) +
			" failed to restore from checkpoint file " + fname + " with a serialization" +
			" error:\n" + e.what() + "\n");
		return;
	}

	for ( Size ii = 1; ii <= results_to_keep_first.size(); ++ii ) {
		JobResultID id{results_to_keep_first[ii], results_to_keep_second[ii]};
		auto iter = temp_job_results.find(id);
		if ( iter == temp_job_results.end() ) {
			// Handle error!
			utility::send_integer_to_node(0, mpi_work_pool_jd_error );
			utility::send_string_to_node(0, "Archive node " + utility::to_string(mpi_rank_) +
				" did not unpack result with id " + utility::to_string(id) + " from checkpoint"
				" file with name " + fname + "; JD0 thinks we should have this file");
			return;
		}
		save_job_result( id, iter->second );
	}
	TR << "Successfully restored from checkpoint file " << fname << std::endl;
}


bool
MPIWorkPoolJobDistributor::node_is_archive( int node_rank ) const
{
	return node_rank != 0 && node_rank <= n_archives_;
}

bool
MPIWorkPoolJobDistributor::node_is_outputter( int node_rank ) const
{
	return node_rank != 0 && node_rank <= max_n_outputters_;
}

void
MPIWorkPoolJobDistributor::process_job_request_from_node( int worker_node )
{
	if ( node_is_outputter(worker_node) && ! output_list_empty_ ) {
		bool work_was_sent = send_output_instruction_to_node( worker_node );
		debug_assert( work_was_sent );
		return;
	}

	if ( jobs_ready_to_go() ) {
		bool job_sent = send_next_job_to_node( worker_node );
		if ( job_sent ) {
			return;
		}
	}

	if ( jobs_remain() ) {
		note_node_wants_a_job( worker_node );
	} else {
		send_spin_down_signal_to_node( worker_node );
	}
}

/// @details Retrieve the error message from the worker node, tell the JobQueen that
/// the job failed, and then note that the failed job is no longer running.
void
MPIWorkPoolJobDistributor::process_job_failed_w_message_from_node( int worker_node )
{
	Size job_id = utility::receive_size_from_node( worker_node );
	std::string error_message = utility::receive_string_from_node( worker_node );
	LarvalJobOP failed_job = job_extractor_->running_job( job_id );
	TR.Error << "Job " << job_id << " named " << failed_job <<
		" has exited with the message:\n" << error_message << std::endl;

	job_queen_->note_job_completed_and_track( failed_job, jd3_job_status_failed_w_exception, 1 /*dummy*/ );
	note_job_no_longer_running( job_id );
}

void
MPIWorkPoolJobDistributor::process_job_failed_do_not_retry( int worker_node )
{
	Size job_id = utility::receive_size_from_node( worker_node );
	LarvalJobOP failed_job = job_extractor_->running_job( job_id );
	job_queen_->note_job_completed_and_track( failed_job, jd3_job_status_failed_do_not_retry, 1 /*dummy*/ );
	note_job_no_longer_running( job_id );
}

void
MPIWorkPoolJobDistributor::process_job_failed_bad_input( int worker_node )
{
	Size job_id = utility::receive_size_from_node( worker_node );
	LarvalJobOP failed_job = job_extractor_->running_job( job_id );
	job_queen_->note_job_completed_and_track( failed_job, jd3_job_status_inputs_were_bad, 1 /*dummy*/ );

	// TO DO: add code that purges all other jobs with the same inner job as this one
	note_job_no_longer_running( job_id );
}

void
MPIWorkPoolJobDistributor::process_job_failed_retry_limit_exceeded( int worker_node )
{
	Size job_id = utility::receive_size_from_node( worker_node );
	LarvalJobOP failed_job = job_extractor_->running_job( job_id );
	job_queen_->note_job_completed_and_track( failed_job, jd3_job_status_failed_max_retries, 1 /*dummy*/ );

	// TO DO: add code that purges all other jobs with the same inner job as this one
	note_job_no_longer_running( job_id );
}

void
MPIWorkPoolJobDistributor::process_job_succeeded( int worker_node )
{

	Size job_id = utility::receive_size_from_node( worker_node );
	Size nresults = utility::receive_size_from_node( worker_node );
	TR << "Nresults from job " << job_id << ": " << nresults << std::endl;

	// Now pick a node to archive the job result on, perhaps storing the
	// job result on this node.
	utility::vector1< int > archival_nodes_for_results( nresults );
	for ( Size ii = 1; ii <= nresults; ++ii ) {
		archival_nodes_for_results[ ii ] = pick_archival_node();
	}
	// tell the worker node where to send the job result
	utility::send_integers_to_node( worker_node, archival_nodes_for_results );

	for ( Size ii = 1; ii <= nresults; ++ii ) {
		JobResultID result_id = std::make_pair( job_id, ii );
		job_result_location_map_[ result_id ] = archival_nodes_for_results[ ii ];
	}

	// changing this if ( archival_node == 0 ) {
	// changing this  // NOTE: when worker nodes are sending the larval job / job result pair
	// changing this  // they do not need to first send an archival-request message or the job id.
	// changing this  // Store serialized larval job & job result pair
	// changing this  std::string serialized_job_result_string =
	// changing this   utility::receive_string_from_node( worker_node );
	// changing this  job_results_[ job_id ] = serialized_job_result_string;
	// changing this  utility::send_integer_to_node( worker_node, mpi_work_pool_jd_archival_completed );
	// changing this }
}

int
MPIWorkPoolJobDistributor::pick_archival_node()
{
	int archive_node = n_results_per_archive_.head_item();
	float nresults = n_results_per_archive_.heap_head();
	nresults += 1;
	n_results_per_archive_.reset_coval( archive_node, nresults );

	//TR << "Picked archival node " << archive_node << std::endl;
	//for ( int ii = 1; ii <= n_archives_; ++ii ) {
	// TR << "  " << ii << " " << n_results_per_archive_.coval_for_val( ii ) << std::endl;
	//}
	return archive_node;
}


void
MPIWorkPoolJobDistributor::process_archival_complete_message( int worker_node )
{
	// The second half of a job-completion step.
	//
	// At this point, the job results for the completed job
	// have been archived on the archival nodes (perhaps on this node)
	// so it is now safe to inform the JobQueen that the job has completed.
	// It is also safe for the JobQueen to request to discard
	// or output the results. If we were to structure the archival
	// process in a single pass manner (i.e. in the process_job_succeeded method),
	// the remote node where the archival is taking place might not have
	// completed -- or even gotten started -- by the time we ask it for the
	// result. This would produce a race condition.

	// We keep the larval jobs that are in flight, and will stop keeping them
	// at the end of this function
	Size job_id = utility::receive_size_from_node( worker_node );

	auto larval_job_iter = in_flight_larval_jobs_.find( job_id );
	debug_assert( larval_job_iter != in_flight_larval_jobs_.end() );
	LarvalJobOP larval_job = larval_job_iter->second;

	// Older implementation: do not store the in-flight jobs on the head node.
	// Is this a useful optimization? It's hard to imagine it is!
	// // The remote node will also query the JobQueen to know which of the two
	// // messages it should send.
	// if ( job_queen_->larval_job_needed_for_note_job_completed() ||
	//   job_queen_->larval_job_needed_for_completed_job_summary() ) {
	//  std::string serialized_larval_job_string =
	//   utility::receive_string_from_node( worker_node );
	//  // Does this call to deserialize_larval_job need a try/catch block?
	//  // No.
	//  // This larval job has been deserialized previously, with no opportunity
	//  // for new data to have been added to it.
	//  larval_job = deserialize_larval_job( serialized_larval_job_string );
	//  job_id = larval_job->job_index();
	// } else {
	//  job_id = utility::receive_size_from_node( worker_node );
	// }

	JobStatus status = JobStatus( utility::receive_integer_from_node( worker_node ) );
	std::string serialized_job_summaries_string = utility::receive_string_from_node( worker_node );
	utility::vector1< JobSummaryOP > job_summaries;
	try {
		job_summaries = deserialize_job_summaries( serialized_job_summaries_string );
	} catch ( cereal::Exception & e ) {
		throw CREATE_EXCEPTION(utility::excn::Exception,  "Failed to deserialize the JobSummary array for job #" +
			utility::to_string( job_id ) + "\nError message from the cereal library:\n" + e.what() + "\n" );
	}

	// Inform the JobQueen of the completed job.
	// This step had to be delayed until after the archival process completed,
	// as it assumes that the JobResult is present on the node where we
	// wrote down it would be stored. Otherwise, jobs could be launched that
	// would produce a race condition by asking for JobResults as inputs that
	// either had or had not yet completed the archival process.
	Size const nsummaries = job_summaries.size();
	job_queen_->note_job_completed_and_track( larval_job, status, nsummaries );
	for ( Size ii = 1; ii <= nsummaries; ++ii ) {
		job_queen_->completed_job_summary( larval_job, ii, job_summaries[ ii ] );
	}

	note_job_no_longer_running( job_id );

	potentially_output_some_job_results();
	potentially_discard_some_job_results();
}

void
MPIWorkPoolJobDistributor::process_output_complete_message( int remote_node )
{
	debug_assert( node_outputter_is_outputting_for_[ remote_node ] >= 0 &&
		node_is_archive(node_outputter_is_outputting_for_[ remote_node ]));

	int const archive_node = node_outputter_is_outputting_for_[ remote_node ];
	node_outputter_is_outputting_for_[ remote_node ] = -1;

	float new_result_count = n_results_per_archive_.coval_for_val( archive_node ) - 1;
	debug_assert( new_result_count >= 0 );
	n_results_per_archive_.reset_coval( archive_node, new_result_count );

	if ( node_is_archive(remote_node) ) {
		TR << "Remote " << remote_node << " has completed an output" << std::endl;
		if ( jobs_to_output_for_archives_[ remote_node ].empty() ) {
			TR << "Sending remote " << remote_node << " an output archived on another node, if available." << std::endl;
			bool sent = send_output_instruction_to_node( remote_node );
			archive_currently_outputting_[ remote_node ] = sent;
			if ( sent ) {
				TR << "... output work was available" << std::endl;
			} else {
				TR << "... output work was NOT available" << std::endl;
			}
		} else {
			TR << "Sending remote " << remote_node << " an output that they already have archived" << std::endl;
			output::OutputSpecificationOP spec = jobs_to_output_for_archives_[ remote_node ].front();
			jobs_to_output_for_archives_[ remote_node ].pop_front();
			send_output_spec_to_remote( remote_node, spec );

			float remaining = n_outstanding_output_tasks_for_archives_.coval_for_val( remote_node ) + 1;
			debug_assert( -1 * remaining == jobs_to_output_for_archives_[ remote_node ].size() );
			n_outstanding_output_tasks_for_archives_.reset_coval( remote_node, remaining );
		}
		update_output_list_empty_bool();
	}
	// else, it was a worker/output node that reported it was done; no need to do anything else.
}


void
MPIWorkPoolJobDistributor::process_retrieve_job_result_request( int worker_node )
{
	using namespace utility;
	Size job_id = receive_size_from_node( worker_node );
	Size result_index = receive_size_from_node( worker_node );
	JobResultID result_id = { job_id, result_index };

	auto send_to_worker = [&](std::string const & str ) {
		TR.Debug << "Sending result " << job_id << " " << result_index << " to worker " << worker_node << std::endl;
		send_integer_to_node( worker_node, mpi_work_pool_jd_job_result_retrieved );
		send_string_to_node( worker_node, str );
	};

	try {
		retrieve_job_result_and_act( result_id, send_to_worker );
		return;
	} catch ( std::string errstring ) {
		TR << "Failed to retrieve result " << job_id << " " << result_index << " for worker " << worker_node << std::endl;
		TR << "Error message:" << errstring << std::endl;
		// fatal error: the requested job result is not present at this node.
		// inform node 0 (if this isn't node 0 )
		if ( mpi_rank_ == 0 ) {
			throw_after_failed_to_retrieve_job_result( worker_node, job_id, result_index, mpi_rank_ );
		} else {
			send_integer_to_node( worker_node, mpi_work_pool_jd_failed_to_retrieve_job_result );
			// tell node 0 about failed retrieval
			send_integer_to_node( 0, mpi_rank_ );
			send_integer_to_node( 0, mpi_work_pool_jd_failed_to_retrieve_job_result );
			send_integer_to_node( 0, worker_node );
			send_size_to_node( 0, job_id );
			send_size_to_node( 0, result_index );
			return;
		}
	}
}

/// @details Called by the archive node. The archive node should already have the indicated job
/// result stored on this node.
void
MPIWorkPoolJobDistributor::process_output_job_result_already_available_request( int remote_node )
{
	// remote node should be node 0
	std::string output_spec_string = utility::receive_string_from_node( remote_node );
	output::OutputSpecificationOP spec;
	try {
		spec = deserialize_output_specification( output_spec_string );
	} catch ( cereal::Exception & e ) {
		utility::send_integer_to_node( 0, mpi_rank_ );
		utility::send_integer_to_node( 0, mpi_work_pool_jd_error );
		std::ostringstream oss;
		oss << "Error on archive node " << mpi_rank_ << " trying to deserialize output specification\n";
		oss << "Error message from cereal library:\n";
		oss << e.what() << "\n";
		utility::send_string_to_node( 0, oss.str() );
		// do not exit immediately as this will cause the MPI process to exit before
		// the head node has a chance to flush its output
		return;
	}
	JobResultID result_id = spec->result_id();

	auto deserialize_and_write_output = [&] (std::string const & serialized_job_and_result ) {
		LarvalJobAndResult job_and_result;
		try {
			job_and_result = deserialize_larval_job_and_result( serialized_job_and_result );
		} catch ( cereal::Exception & e ) {
			utility::send_integer_to_node( 0, mpi_rank_ );
			utility::send_integer_to_node( 0, mpi_work_pool_jd_error );
			std::ostringstream oss;
			oss << "Error on archive node " << mpi_rank_ << " trying to deserialize larval job and job result";
			oss << " for result id ( " << result_id.first << ", " << result_id.second << " )\n";
			oss << "Error message from cereal library:\n";
			oss << e.what() << "\n";
			utility::send_string_to_node( 0, oss.str() );
			// do not exit immediately as this will cause the MPI process to exit before
			// the head node has a chance to flush its output
		}

		write_output( spec, job_and_result );
	};

	try {
		retrieve_job_result_and_act_and_delete( result_id, deserialize_and_write_output );
		// now tell node0 we are done
		utility::send_integer_to_node( 0, mpi_rank_ );
		utility::send_integer_to_node( 0, mpi_work_pool_jd_output_completed );
		return;
	} catch ( std::string errstr ) {
		utility::send_integer_to_node( 0, mpi_rank_ );
		utility::send_integer_to_node( 0, mpi_work_pool_jd_error );
		std::ostringstream oss;
		oss << "Error on archive node " << mpi_rank_ << " trying to write job result ";
		oss << "for result id ( " << result_id.first << ", " << result_id.second << " )\n";
		oss << "Error message:\n" << errstr << "\n";
		utility::send_string_to_node( 0, oss.str() );
	}
}

/// @details Called by the archive node. The archive node should already have the indicated job
/// result stored on this node.
void
MPIWorkPoolJobDistributor::process_accept_and_output_job_result_request( int remote_node )
{
	// remote node should be node 0
	std::string output_spec_string = utility::receive_string_from_node( remote_node );
	output::OutputSpecificationOP spec;
	try {
		spec = deserialize_output_specification( output_spec_string );
	} catch ( cereal::Exception & e ) {
		utility::send_integer_to_node( 0, mpi_rank_ );
		utility::send_integer_to_node( 0, mpi_work_pool_jd_error );
		std::ostringstream oss;
		oss << "Error on archive node " << mpi_rank_ << " trying to deserialize output specification\n";
		oss << "Error message from cereal library:\n";
		oss << e.what() << "\n";
		utility::send_string_to_node( 0, oss.str() );
		// do not exit immediately as this will cause the MPI process to exit before
		// the head node has a chance to flush its output
		return;

	}
	LarvalJobAndResult job_and_result;
	std::string job_and_result_string = utility::receive_string_from_node( remote_node );
	try {
		job_and_result = deserialize_larval_job_and_result( job_and_result_string );
	} catch ( cereal::Exception & e ) {
		utility::send_integer_to_node( 0, mpi_rank_ );
		utility::send_integer_to_node( 0, mpi_work_pool_jd_error );
		std::ostringstream oss;
		oss << "Error on archive node " << mpi_rank_ << " trying to deserialize larval job and job result";
		oss << " for result id ( " << spec->result_id().first << ", " << spec->result_id().second << " )\n";
		oss << "Error message from cereal library:\n";
		oss << e.what() << "\n";
		TR << oss.str() << std::endl;
		utility::send_string_to_node( 0, oss.str() );
		// do not exit immediately as this will cause the MPI process to exit before
		// the head node has a chance to flush its output
		return;
	}

	write_output( spec, job_and_result );

	// now tell node0 we are done
	utility::send_integer_to_node( 0, mpi_rank_ );
	utility::send_integer_to_node( 0, mpi_work_pool_jd_output_completed );
}

void
MPIWorkPoolJobDistributor::process_retrieve_result_from_archive_and_output_request()
{
	// remote node should be node 0
	Size const remote_node( 0 );
	int const archive_node = utility::receive_integer_from_node( remote_node );
	std::string output_spec_string = utility::receive_string_from_node( remote_node );
	output::OutputSpecificationOP spec;
	try {
		spec = deserialize_output_specification( output_spec_string );
	} catch ( cereal::Exception & e ) {
		utility::send_integer_to_node( 0, mpi_rank_ );
		utility::send_integer_to_node( 0, mpi_work_pool_jd_error );
		std::ostringstream oss;
		oss << "Error on output node " << mpi_rank_ << " trying to deserialize output specification\n";
		oss << "Error message from cereal library:\n";
		oss << e.what() << "\n";
		TR << oss.str() << std::endl;
		utility::send_string_to_node( 0, oss.str() );
		// do not exit immediately as this will cause the MPI process to exit before
		// the head node has a chance to flush its output
		return;

	}

	//TR << "Requesting output result from archive " << archive_node << " on node " << mpi_rank_ << std::endl;

	// Now request the job and result string from the archive.
	utility::send_integer_to_node( archive_node, mpi_rank_ ); // say I need to talk
	utility::send_integer_to_node( archive_node, mpi_work_pool_jd_retrieve_and_discard_job_result );
	utility::send_size_to_node( archive_node, spec->result_id().first );
	utility::send_size_to_node( archive_node, spec->result_id().second );

	int retrieval_success = utility::receive_integer_from_node( archive_node );
	// If we cannot get the job result from the remote, the program needs to exit.
	// The archive node will tell node0 that the result could not be retrieved, and will
	// spin down all the nodes.
	if ( retrieval_success != mpi_work_pool_jd_job_result_retrieved ) return;

	LarvalJobAndResult job_and_result;

	std::string job_and_result_string = utility::receive_string_from_node( archive_node );
	try {
		job_and_result = deserialize_larval_job_and_result( job_and_result_string );
	} catch ( cereal::Exception & e ) {
		utility::send_integer_to_node( 0, mpi_rank_ );
		utility::send_integer_to_node( 0, mpi_work_pool_jd_error );
		std::ostringstream oss;
		oss << "Error on archive node " << mpi_rank_ << " trying to deserialize larval job and job result";
		oss << " for result id ( " << spec->result_id().first << ", " << spec->result_id().second << " )\n";
		oss << "Error message from cereal library:\n";
		oss << e.what() << "\n";
		utility::send_string_to_node( 0, oss.str() );
		// do not exit immediately as this will cause the MPI process to exit before
		// the head node has a chance to flush its output
		return;
	}

	write_output( spec, job_and_result );

	// now tell node0 we are done
	utility::send_integer_to_node( 0, mpi_rank_ );
	utility::send_integer_to_node( 0, mpi_work_pool_jd_output_completed );
}


void
MPIWorkPoolJobDistributor::process_failed_to_retrieve_job_result_request( int archival_node )
{
	// the node making the job-result request, which the archival node was
	// unable to fullfil
	int worker_node = utility::receive_integer_from_node( archival_node );
	Size job_id = utility::receive_size_from_node( archival_node );
	Size result_index = utility::receive_size_from_node( archival_node );
	throw_after_failed_to_retrieve_job_result( worker_node, job_id, result_index, archival_node );
}

/// @details This function unlike the process_retireve_job_result_request function will only
/// be called on an archive node -- and it will be called only by the master node.
/// therefore, we do not need to handle the case when mpi_rank_ == 0.
void
MPIWorkPoolJobDistributor::process_retrieve_and_discard_job_result_request( int worker_node )
{
	using namespace utility;
	Size job_id = receive_size_from_node( worker_node );
	Size result_index = receive_size_from_node( worker_node );

	JobResultID job_result_id = { job_id, result_index };

	auto send_to_node = [&](std::string const & str ) {
		TR.Debug << "Sending result " << job_result_id << " " << result_index << " to node " << worker_node << std::endl;
		send_integer_to_node( worker_node, mpi_work_pool_jd_job_result_retrieved );
		send_string_to_node( worker_node, str );
	};

	try {
		retrieve_job_result_and_act_and_delete(job_result_id, send_to_node);
	} catch (std::string ) {
		send_integer_to_node( worker_node, mpi_work_pool_jd_failed_to_retrieve_job_result );

		// tell node 0 about failed retrieval
		send_integer_to_node( 0, mpi_rank_ );
		send_integer_to_node( 0, mpi_work_pool_jd_failed_to_retrieve_job_result );
		send_integer_to_node( 0, worker_node );
		send_size_to_node( 0, job_id );
		send_size_to_node( 0, result_index );
	}

}

void
MPIWorkPoolJobDistributor::process_discard_job_result_request( int remote_node )
{
	using namespace utility;
	Size job_id = receive_size_from_node( remote_node ); // node 0
	Size result_index = receive_size_from_node( remote_node );

	JobResultID job_result_id = { job_id, result_index };
	delete_job_result(job_result_id);
}

void
MPIWorkPoolJobDistributor::process_archive_job_result_request( int remote_node )
{
	using namespace utility;
	Size job_id = receive_size_from_node( remote_node );
	Size result_index = receive_size_from_node( remote_node );
	JobResultID result_id = { job_id, result_index };
	auto serialized_larval_job_and_result = std::make_shared< std::string >( receive_string_from_node( remote_node ));

	save_job_result( result_id, serialized_larval_job_and_result );

	utility::send_integer_to_node( remote_node, mpi_work_pool_jd_archival_completed );
}

/// @details This function is to be called only when the master node is not already in the
/// middle of communicating with this node about something; the master node should be in the
/// listening loop of go_master when this function is called (or at least it shouldn't be
/// in some other function *and* excepecting this node to send it a message).
void
MPIWorkPoolJobDistributor::send_error_message_to_node0( std::string const & error_message )
{
	utility::send_integer_to_node( 0, mpi_rank_ );
	utility::send_integer_to_node( 0, mpi_work_pool_jd_error );
	utility::send_string_to_node( 0, error_message );
}

bool
MPIWorkPoolJobDistributor::send_output_instruction_to_node( int output_node )
{

	// figure out what output to tell the remote to perform:
	if ( ! jobs_to_output_with_results_stored_on_node0_.empty() ) {
		//TR << "Sending spec stored on master node to " << output_node << std::endl;
		output::OutputSpecificationOP spec = jobs_to_output_with_results_stored_on_node0_.front();
		jobs_to_output_with_results_stored_on_node0_.pop_front();
		send_output_spec_and_job_result_to_remote( output_node, spec );
		update_output_list_empty_bool();
		return true;
	}

	runtime_assert( ! node_is_archive(output_node) ||
		jobs_to_output_for_archives_[ output_node ].empty() );

	if ( node_is_archive(output_node) &&
			( job_extractor_->not_done() ||
			! in_flight_larval_jobs_.empty() ) ) {
		// Archive nodes must not end up in a situation where node A wants
		// a job result from node B and node B wants a job result from
		// node A. This causes deadlock. In fact, any cycle of archives needing
		// job results from other archives could cause deadlock.
		// The only safe time for when any archive can ask another node
		// for one of its results is when there are no results already stored
		// on the archive that will ever be output and when we can
		// guarantee that there will be no other job result could get stored
		// on the archive (and therefore no other archive will ask for its
		// stored result). The only time we can guarantee this is after
		// all jobs have been started (and no more will ever run), and after
		// all jobs that have been started have been completed and their
		// results archived, and any of those results that should be output
		// have already been output. What a mouth full.
		// Currently the only time this function is called for an archive node
		// is after the archive has already output all of the results that
		// are waiting to be output, so that condition is only asserted
		// here and is not checked explicitly.
		// So if all the jobs have finished running and have been archived,
		// then we may assign output_node an output task whose result lives
		// on another archive.
		return false;
	}


	// Tell the worker node to get an output result from the archive that has the most work
	// if any of the archives have any work.

	// Grab the next spec to output off the heap
	int archive_w_most_work = n_outstanding_output_tasks_for_archives_.head_item();
	//TR << "Archive w/ most work? " << archive_w_most_work << " w " << n_outstanding_output_tasks_for_archives_.heap_head() << " work remaining (negative); vs " << jobs_to_output_for_archives_[ archive_w_most_work ].size() << std::endl;
	if ( jobs_to_output_for_archives_[ archive_w_most_work ].empty() ) return false;

	output::OutputSpecificationOP spec = jobs_to_output_for_archives_[ archive_w_most_work ].front();
	jobs_to_output_for_archives_[ archive_w_most_work ].pop_front();
	float remaining = n_outstanding_output_tasks_for_archives_.heap_head() + 1; // negative, so increment instead of decrement
	debug_assert( -1 * remaining == jobs_to_output_for_archives_[ archive_w_most_work ].size() );
	n_outstanding_output_tasks_for_archives_.reset_coval( archive_w_most_work, remaining );

	send_output_spec_and_retrieval_instruction_to_remote( output_node, archive_w_most_work, spec );
	update_output_list_empty_bool();

	return true;

}


/// @throws If the LarvalJob fails to serialize (perhaps an object stored in the
/// const_data_map of the inner-larval-job has not implemented the save & load
/// routines), then this function will throw a EXCN_Msg_Exception
bool
MPIWorkPoolJobDistributor::send_next_job_to_node( int worker_node )
{
	debug_assert( ! job_extractor_->job_queue_empty() );

	if ( ! deallocation_messages_for_node_[ worker_node ].empty() ) {
		send_deallocation_messages_to_node( worker_node );
	}

	// First tell the worker node that we do indeed have a job for them to work on/
	// The other option is that we are going to tell the worker to spin down.
	utility::send_integer_to_node( worker_node, mpi_work_pool_jd_new_job_available );

	// NOTE: Popping jobs_for_current_digraph_node_: we might violate the
	// invariant that this queue is never empty so long as nodes remain in the
	// digraph_nodes_ready_to_be_run_ queue.  Address that below
	LarvalJobOP larval_job;
	while ( ! job_extractor_->job_queue_empty() ) {
		larval_job = job_extractor_->pop_job_from_queue();
		if ( job_queen_->has_job_previously_been_output( larval_job ) ) {
			job_queen_->note_job_completed_and_track( larval_job, jd3_job_previously_executed, 0 );
			job_extractor_->note_job_no_longer_running( larval_job->job_index() );
			larval_job = nullptr;
			continue;
		}
		debug_assert( in_flight_larval_jobs_.count( larval_job->job_index() ) == 0 );
		in_flight_larval_jobs_[ larval_job->job_index() ] = larval_job;
		break;
	}

	if ( ! larval_job ) {
		// i.e. the job queue is empty
		return false;
	}
	TR << "Sending job " << larval_job->job_index() << " to node " << worker_node << std::endl;

	// serialize the LarvalJob and ship it to the worker node.
	// Note that this could leave the worker hanging where it expects
	// a job, but the JD has thrown: this exception must not be caught
	// except to print its contents before exiting.
	std::string serialized_larval_job;
	try {
		serialized_larval_job = serialize_larval_job( larval_job );
	} catch ( cereal::Exception const & e ) {
		// ok, we should throw a utility::excn::EXCN_Msg_Exception letting the user know
		// that the code cannot be used in its current state because some class stored inside
		// the LarvalJob (or perhaps the descendent of the LarvalJob itself) does not implement
		// the save and load serialization funcitons.
		throw CREATE_EXCEPTION(utility::excn::Exception,  "Failed to serialize LarvalJob " + utility::to_string( larval_job->job_index() )
			+ " because it holds an unserializable object.  The following error message comes from the cereal library:\n"
			+ e.what() );
	}
	utility::send_string_to_node( worker_node, serialized_larval_job );

	// Also inform the remote node where to find the JobResults that will serve as inputs to
	// this job.
	utility::vector1< core::Size > archival_nodes_for_job( larval_job->input_job_result_indices().size() );
	for ( core::Size ii = 1; ii <= archival_nodes_for_job.size(); ++ii ) {
		archival_nodes_for_job[ ii ] = job_result_location_map_[ larval_job->input_job_result_indices()[ ii ] ];
	}
	utility::send_sizes_to_node( worker_node, archival_nodes_for_job );

	worker_node_for_job_[ larval_job->job_index() ] = worker_node;
	return true;
}

void
MPIWorkPoolJobDistributor::send_deallocation_messages_to_node( int worker_node )
{
	for ( core::Size which_message : deallocation_messages_for_node_[ worker_node ] ) {
		utility::send_integer_to_node( worker_node, mpi_work_pool_jd_deallocation_message );
		utility::send_string_to_node( worker_node, deallocation_messages_[ which_message ] );
		core::Size n_remaining = --n_remaining_nodes_for_deallocation_message_[ which_message ];
		if ( n_remaining == 0 ) {
			deallocation_messages_[ which_message ] = std::string();
		}
	}
	deallocation_messages_for_node_[ worker_node ].clear();
}

void
MPIWorkPoolJobDistributor::note_job_no_longer_running( Size job_id )
{
	// ok, now remove the completed/failed job from the maps keeping track of
	// outstanding jobs
	worker_node_for_job_.erase( job_id );
	job_extractor_->note_job_no_longer_running( job_id );
	in_flight_larval_jobs_.erase( job_id );

	if ( job_extractor_->retrieve_and_reset_node_recently_completed() ) {
		std::list< deallocation::DeallocationMessageOP > messages = job_queen_->deallocation_messages();
		if ( ! messages.empty() ) {
			store_deallocation_messages( messages );
		}
		if ( ! worker_nodes_waiting_for_jobs_.empty() && ! job_extractor_->job_queue_empty() ) {
			assign_jobs_to_idling_nodes();
		}
	}
}

void
MPIWorkPoolJobDistributor::potentially_output_some_job_results()
{

	// ask the job queen if she wants to output any results
	std::list< output::OutputSpecificationOP > jobs_to_output =
		job_queen_->jobs_that_should_be_output();

	if ( jobs_to_output.empty() ) return;

	std::list< output::OutputSpecificationOP > outputs_for_node0;

	for ( auto spec : jobs_to_output ) {
		JobResultID result_id = spec->result_id();
		auto location_map_iter = job_result_location_map_.find( result_id );
		if ( location_map_iter == job_result_location_map_.end() ) {
			// TO DO: better diagnostic message here
			throw CREATE_EXCEPTION(utility::excn::Exception,  "Failed to find job result (" +
				utility::to_string( result_id.first ) + ", " + utility::to_string( result_id.second ) +
				") for outputting as requested by the JobQeen. Has this job already been" +
				" output or discarded?" );
		}

		// TO DO: instead, wait until after the archive node has completed a job output,
		// and then erase its location from the map; this will merely require that we
		// keep track of which node is outputting which job. For now, mark the job as having
		// been erased.
		Size node_housing_result = location_map_iter->second;
		job_result_location_map_.erase( location_map_iter );

		if ( node_housing_result == 0 ) {
			// ok, we have the result on this node -- we optionally could ship the result
			// to another node and output it there.
			outputs_for_node0.push_back( spec );
		} else {
			if ( ! archive_currently_outputting_[ node_housing_result ] ) {
				TR << "Sending output to archive " << node_housing_result << std::endl;
				send_output_spec_to_remote( node_housing_result, spec );
				archive_currently_outputting_[ node_housing_result ] = true;
			} else {
				TR << "Pushing back output into queue for archive " << node_housing_result << std::endl;
				jobs_to_output_for_archives_[ node_housing_result ].push_back( spec );
			}
		}

		// and now erase the record of where the job had been stored, and,
		// for the sake of diagnosing errors, also store the fact that this job
		// had been output (as opposed to discarded) -- but compactly within the
		// output_jobs_ DIET -- (Note some day, I'll figure out how to do this efficiently.
		// For now, the JD does not track which jobs have already been output.)
	}

	int count_archive( 1 );
	for ( auto spec : outputs_for_node0 ) {

		bool sent( false );
		while ( node_is_archive(count_archive) ) {
			if ( ! archive_currently_outputting_[ count_archive ] ) {
				// send the job to the remote node and have it start outputting it
				send_output_spec_and_job_result_to_remote( count_archive, spec );
				archive_currently_outputting_[ count_archive ] = true;
				sent = true;
				break;
			} else {
				++count_archive;
			}
		}
		if ( ! sent ) {
			jobs_to_output_with_results_stored_on_node0_.push_back( spec );
		}
	}

	// Now update the n_outstanding_output_tasks_for_archives_ heap
	// Negate the coval so that the archives with the most work are at
	// the top of the heap
	for ( int ii = 1; ii <= n_archives_; ++ii ) {
		n_outstanding_output_tasks_for_archives_.reset_coval( ii,
			-1 * float(  jobs_to_output_for_archives_[ ii ].size()  ) );
	}

	update_output_list_empty_bool();
	if ( ! output_list_empty_ ) { assign_output_tasks_to_idling_nodes(); }
}

void
MPIWorkPoolJobDistributor::potentially_discard_some_job_results()
{
	// ask the job queen if she wants to discard any results
	std::list< JobResultID >  jobs_to_discard = job_queen_->job_results_that_should_be_discarded();
	if ( jobs_to_discard.empty() ) return;

	utility::vector0< int > n_discarded_jobs( n_archives_ + 1, 0 );

	for ( JobResultID result_id : jobs_to_discard ) {
#ifndef NDEBUG
		TR << "Discarding JobResultID with job=" << result_id.first << " and resid=" << result_id.second << std::endl;
#endif
		if ( job_result_location_map_.count( result_id ) == 0 ) {
			// TO DO: better diagnostic message here
			throw CREATE_EXCEPTION(utility::excn::Exception,  "Failed to find job result " +
				utility::to_string( result_id.first ) + ", " + utility::to_string( result_id.second ) +
				" for discarding as requested by the JobQeen. Has this job already been" +
				" output or discarded?" );
		}

		Size node_housing_result = job_result_location_map_[ result_id ];
		n_discarded_jobs[ node_housing_result ] += 1;
		LarvalJobAndResult job_and_result;
		if ( node_housing_result == 0 ) {
			// ok, we have the result on this node
			delete_job_result(result_id);
		} else {
			utility::send_integer_to_node( node_housing_result, 0 ); // say "I need to talk to you"
			utility::send_integer_to_node( node_housing_result,
				mpi_work_pool_jd_discard_job_result );
			utility::send_size_to_node( node_housing_result, result_id.first );
			utility::send_size_to_node( node_housing_result, result_id.second );
		}

		// and now delete storage for this job result, and
		// for the sake of diagnosing errors, also store the fact that this job
		// had been discarded (as opposed to being output) -- but compactly within the
		// discarded_jobs_ DIET
		job_result_location_map_.erase( result_id );

		// discarded_jobs_.insert( job_id );

	}

	// update the n_results_per_archive_ heap
	for ( int ii = 0; ii <= n_archives_; ++ii ) {
		if ( n_discarded_jobs[ ii ] == 0 ) continue;
		float new_n_results_for_archive_ii = n_results_per_archive_.coval_for_val( ii )
			- n_discarded_jobs[ ii ];
		n_results_per_archive_.reset_coval( ii, new_n_results_for_archive_ii );
	}
}


void
MPIWorkPoolJobDistributor::send_output_spec_and_job_result_to_remote(
	Size node_id,
	output::OutputSpecificationOP spec
)
{
	TR << "Sending output spec for " << spec->result_id().first << " " << spec->result_id().second << " with output_id " << spec->output_index().primary_output_index << " " << spec->output_index().secondary_output_index << " to node " << node_id << std::endl;

	node_outputter_is_outputting_for_[ node_id ] = 0;

	JobResultID result_id = spec->result_id();
	//std::string serialized_job_and_result;
	//
	//auto job_and_result_iter = job_results_.find( result_id );
	//if ( job_and_result_iter == job_results_.end() ) {
	// std::ostringstream oss;
	// oss << "Error trying to find job result ( " << result_id.first << ", ";
	// oss << result_id.second << " ); node 0 thinks it ought to have this result.\n";
	// throw CREATE_EXCEPTION( utility::excn::Exception, oss.str() );
	//} else {
	// if ( archive_on_disk_ ) {
	//  serialized_job_and_result = get_string_from_disk( result_id.first, result_id.second );
	//  if ( serialized_job_and_result.empty() ) {
	//   // OK -- exit
	//   std::ostringstream oss;
	//   oss << "Error trying to retrieve a serialized job result from disk for job result (";
	//   oss << result_id.first << ", " << result_id.second << " ) which should have been in ";
	//   oss << "\"" << filename_for_archive( result_id.first, result_id.second ) << "\"\n";
	//   throw CREATE_EXCEPTION( utility::excn::Exception, oss.str() );
	//  }
	//  remove( filename_for_archive( result_id.first, result_id.second ).c_str() );
	// } else {
	//  serialized_job_and_result = std::move( *job_and_result_iter->second );
	// }
	// job_results_.erase( job_and_result_iter );
	//}

	auto send_result_and_spec_to_node = [&](std::string const & serialized_job_and_result ) {
		if ( node_id <= (Size) n_archives_ ) {
			utility::send_integer_to_node( node_id, 0 ); // say "I need to talk to you"
		} // else, worker node is already expecting a message from node 0

		utility::send_integer_to_node( node_id,
		mpi_work_pool_jd_accept_and_output_job_result );
		utility::send_string_to_node( node_id, serialize_output_specification( spec ) );
		utility::send_string_to_node( node_id, serialized_job_and_result );
	};

	try {
		retrieve_job_result_and_act_and_delete( result_id, send_result_and_spec_to_node );
	} catch ( std::string const & errstr ) {
		std::ostringstream oss;
		oss << "Failed to retrieve result id " << result_id << "\nError message:" << errstr;
		throw CREATE_EXCEPTION( utility::excn::Exception, oss.str() );
	}
}

void
MPIWorkPoolJobDistributor::send_output_spec_to_remote(
	Size node_id,
	output::OutputSpecificationOP spec
)
{
	node_outputter_is_outputting_for_[ node_id ] = node_id;

	//float new_result_count = n_results_per_archive_.coval_for_val( node_id ) - 1;
	//debug_assert( new_result_count >= 0 );
	//n_results_per_archive_.heap_replace( node_id, new_result_count );

	utility::send_integer_to_node( node_id, 0 ); // say "I need to talk to you"
	utility::send_integer_to_node( node_id,
		mpi_work_pool_jd_output_job_result_already_available );
	utility::send_string_to_node( node_id,
		serialize_output_specification( spec ) );
}

void
MPIWorkPoolJobDistributor::send_output_spec_and_retrieval_instruction_to_remote(
	Size node_id, // node that will perform the output
	Size archive_node, // node where the job lives
	output::OutputSpecificationOP spec
)
{
	//TR << "Sending spec to " << node_id << " for result on archive " << archive_node << std::endl;
	node_outputter_is_outputting_for_[ node_id ] = archive_node;

	// record the fact that there will be one fewer results held on the archive
	// float new_result_count = n_results_per_archive_.coval_for_val( archive_node ) - 1;
	// debug_assert( new_result_count >= 0 );
	// n_results_per_archive_.heap_replace( archive_node, new_result_count );

	if ( node_id <= (Size) n_archives_ ) {
		utility::send_integer_to_node( node_id, 0 ); // say "I need to talk to you"
	}
	// otherwise, we're talking to a worker node who is already waiting for
	//  an instruction from node0 -- we don't need to introduce ourselves


	utility::send_integer_to_node( node_id,
		mpi_work_pool_jd_output_job_result_on_archive );
	utility::send_integer_to_node( node_id, archive_node );
	utility::send_string_to_node( node_id,
		serialize_output_specification( spec ) );
}


void
MPIWorkPoolJobDistributor::update_output_list_empty_bool()
{
	if ( ! jobs_to_output_with_results_stored_on_node0_.empty() ) {
		output_list_empty_ = false;
		return;
	}
	for ( auto const & output_list : jobs_to_output_for_archives_ ) {
		if ( ! output_list.empty() ) {
			output_list_empty_ = false;
			return;
		}
	}
	output_list_empty_ = true;
}

//void
//MPIWorkPoolJobDistributor::queue_jobs_for_next_node_to_run()
//{
// debug_assert( jobs_for_current_digraph_node_.empty() );
// debug_assert( ! digraph_nodes_ready_to_be_run_.empty() );
//
// job_extractor_->queue_jobs_for_next_node_to_run();
//
// if ( ! worker_nodes_waiting_for_jobs_.empty() &&
//   ! job_extractor_->jobs_for_current_digraph_node_empty() ) {
//  assign_jobs_to_idling_nodes();
//  // at the end of this call, either we have run out of jobs to assign
//  // or we have run out of idling nodes
// }
//
//}

//void
//MPIWorkPoolJobDistributor::queue_initial_digraph_nodes_and_jobs()
//{
// runtime_assert( digraph_is_a_DAG( *job_dag_ ) );
//
// // iterate across all nodes in the job_digraph, and note every
// // node with an indegree of 0
// for ( Size ii = 1; ii <= job_dag_->num_nodes(); ++ii ) {
//  if ( job_dag_->get_node(ii)->indegree() == 0 ) {
//   digraph_nodes_ready_to_be_run_.push_back( ii );
//  }
// }
// debug_assert( ! digraph_nodes_ready_to_be_run_.empty() );
// queue_jobs_for_next_node_to_run();
//}

bool
MPIWorkPoolJobDistributor::not_done()
{
	// The JobExtractor may have found a job that's ready to run, so, if there are idling nodes
	// launch that job.
	bool we_are_not_done = job_extractor_->not_done();
	if ( ! worker_nodes_waiting_for_jobs_.empty() && ! job_extractor_->job_queue_empty() ) {
		assign_jobs_to_idling_nodes();
	}

	// Even if there are no more jobs to hand out, if there are any outputs that have not
	// yet been written, we should keep the main listening loop going

	return we_are_not_done || ! output_list_empty_;
}

bool
MPIWorkPoolJobDistributor::any_worker_nodes_not_yet_spun_down()
{
	//TR << "Any worker nodes not yet spun down?" << std::endl;
	//for ( int ii = n_archives_ + 1; ii < mpi_nprocs_; ++ii ) {
	// if ( ! nodes_spun_down_[ ii ] ) {
	//  TR << "   Node " << ii << " has not spun down" << std::endl;
	// }
	//}
	for ( int ii = n_archives_ + 1; ii < mpi_nprocs_; ++ii ) {
		if ( ! nodes_spun_down_[ ii ] ) return true;
	}
	return false;
}

bool
MPIWorkPoolJobDistributor::any_archive_node_still_outputting()
{
	for ( int ii = 1; ii <= n_archives_; ++ii ) {
		if ( archive_currently_outputting_[ ii ] ) return true;
	}
	return false;
}



/// @brief With the invariant that the jobs_for_current_digraph_node_ queue should
/// never have anything in it if the jobs_for_current_digraph_node_ list is
/// empty, then we can simply check whether the jobs_for_current_digraph_node_ queue
/// is empty in order to decide if there are any jobs that are ready to be launched.
bool
MPIWorkPoolJobDistributor::jobs_ready_to_go()
{
	return ! job_extractor_->job_queue_empty();
}

/// @brief Are there any jobs that have not yet been executed, but perhaps are not
/// ready to be submitted because the JobDirectedNode they belong to is downstream
/// of another Node whose jobs are still running?  The JobQueen would not have
/// told the JobDistributor about these jobs yet.  Perhaps the JobQueen has not even
/// told the JobDistributor about the JobDirectedNodes yet.  Basically, we must say
/// "yes" as long as there are jobs that have not yet completed unless we've emptied
/// the digraph_nodes_ready_to_be_run_ queue and then asked the JobQueen to update
/// the job DAG, and she has declined to add any new nodes.
///
/// @details This function relies on the not_done() function to have asked the
/// JobQueen to update the JobDigraph, and then to check the
/// jobs_running_for_digraph_nodes_ map to see if it's still empty.
bool
MPIWorkPoolJobDistributor::jobs_remain()
{
	return ! job_extractor_->complete();
}

void
MPIWorkPoolJobDistributor::note_node_wants_a_job( int worker_node )
{
	worker_nodes_waiting_for_jobs_.push_back( worker_node );
}

void
MPIWorkPoolJobDistributor::send_spin_down_signal_to_node( int worker_node )
{
	TR << "Spinning down node " << worker_node << std::endl;
	utility::send_integer_to_node( worker_node, mpi_work_pool_jd_spin_down );
	nodes_spun_down_[ worker_node ] = true;
}

void
MPIWorkPoolJobDistributor::assign_jobs_to_idling_nodes()
{
	while ( ! worker_nodes_waiting_for_jobs_.empty() && ! job_extractor_->job_queue_empty() ) {
		Size worker_node = worker_nodes_waiting_for_jobs_.front();
		worker_nodes_waiting_for_jobs_.pop_front();
		bool job_sent = send_next_job_to_node( worker_node );
		if ( ! job_sent ) {
			// put this node back into the queue of nodes waiting for jobs
			worker_nodes_waiting_for_jobs_.push_back( worker_node );
		}
	}
}

void
MPIWorkPoolJobDistributor::assign_output_tasks_to_idling_nodes()
{
	for ( auto iter = worker_nodes_waiting_for_jobs_.begin(),
			iter_end = worker_nodes_waiting_for_jobs_.end();
			iter != iter_end; /* no increment */ ) {
		if ( output_list_empty_ ) break;
		auto iter_next = iter;
		++iter_next;
		Size worker = *iter;
		if ( worker <= (Size) max_n_outputters_ ) {
			send_output_instruction_to_node( worker );
			worker_nodes_waiting_for_jobs_.erase( iter );
		}
		iter = iter_next;
	}
}

void
MPIWorkPoolJobDistributor::store_deallocation_messages( std::list< deallocation::DeallocationMessageOP > const & messages )
{
	std::ostringstream oss;
	{ // scope
		cereal::BinaryOutputArchive arc( oss );
		arc( messages );
	}

	deallocation_messages_.push_back( oss.str() );
	n_remaining_nodes_for_deallocation_message_.push_back( mpi_nprocs_ - n_archives_ - 1 );
	for ( core::Size ii = 1+n_archives_; ii < (core::Size) mpi_nprocs_; ++ii ) {
		deallocation_messages_for_node_[ ii ].push_back( deallocation_messages_.size() );
	}

}


void
MPIWorkPoolJobDistributor::throw_after_failed_to_retrieve_job_result(
	int worker_node,
	Size job_id,
	Size result_index,
	int archival_node
)
{
	std::ostringstream oss;
	oss << "Failed to retrieve job result (" << job_id << ", " << result_index << ") which was";
	oss << " requested from node " << archival_node << " by node " << worker_node;
	oss << " but was not present there.\n";

	JobResultID result_id = { job_id, result_index };

	if ( worker_node_for_job_.count( job_id ) ) {
		oss << "Internal Error in the MPIWorkPoolJobDistributor: JobDistrutor thinks that this job is still running on node " <<
			worker_node_for_job_[ job_id ] << "\n";
	} else if ( job_result_location_map_.count( result_id ) ) {
		oss << "JobDistributor on node 0 thinks the result should have been on node ";
		oss << job_result_location_map_[ result_id ] << "\n";
		//} else if ( output_jobs_.member( job_id ) ) {
		// oss << "JobDistributor has already output this job (and does not keep jobs ";
		// oss << "that have already been output).\n";
		//} else if ( discarded_jobs_.member( job_id ) ) {
		// oss << "JobDistributor has discarded this job per the JobQueen's request\n";
	} else {
		oss << "This job is not listed as still running nor as having its JobResult stored on any"
			" archive node; it perhaps has been output or discarded already.\n";
	}

	throw CREATE_EXCEPTION(utility::excn::Exception,  oss.str() );

}

int
MPIWorkPoolJobDistributor::request_job_from_master()
{
	//TR << "Requesting job from master!" << std::endl;
	utility::send_integer_to_node( 0, mpi_rank_ );
	utility::send_integer_to_node( 0, mpi_work_pool_jd_new_job_request );
	return utility::receive_integer_from_node( 0 );
}

void
MPIWorkPoolJobDistributor::retrieve_job_and_run()
{
	std::pair< LarvalJobOP, utility::vector1< JobResultCOP > > job_maturation_data =
		retrieve_job_maturation_data();

	// if the LarvalJobOP is null, then we have failed to retrieve a job result from
	// one of the archival nodes. Quit.
	if ( ! job_maturation_data.first ) return;

	// some convenient names for the two items held in the job_maturation_data class.
	LarvalJobOP & larval_job = job_maturation_data.first;
	utility::vector1< JobResultCOP > & job_results( job_maturation_data.second );

	try {

		bool communicated_w_master = false;
		// if the job's status as it exits is "fail retry", then we'll retry it.
		Size num_retries = larval_job->defines_retry_limit() ? larval_job->retry_limit() : default_retry_limit_;
		for ( core::Size ii = 1; ii <= num_retries; ++ii ) {

			JobOP job = job_queen_->mature_larval_job( larval_job, job_results );

			TR << "Starting job" << std::endl;
			CompletedJobOutput job_output = job->run();
			TR << "Job completed" << std::endl;

			if ( job_output.status == jd3_job_status_failed_retry ) {
				continue;
			} else if ( job_output.status == jd3_job_status_failed_do_not_retry ) {
				worker_send_fail_do_not_retry_message_to_master( larval_job );
				communicated_w_master = true;
				break;
			} else if ( job_output.status == jd3_job_status_inputs_were_bad ) {
				worker_send_fail_bad_inputs_message_to_master( larval_job );
				communicated_w_master = true;
				break;
			} else if ( job_output.status == jd3_job_status_success ) {
				worker_send_job_result_to_master_and_archive( larval_job, job_output );
				communicated_w_master = true;
				break;
			}
		}
		if ( ! communicated_w_master ) {
			worker_send_fail_retry_limit_exceeded( larval_job );
		}

	} catch ( std::ios_base::failure& ex ) {
		std::cerr << "std::IO error detected... exiting..." << std::endl; // We can not longer use Tracer's at this point
		// Using pure exit instead of utility_exit_with_status to avoid infinite recursion
		std::exit( basic::options::option[ basic::options::OptionKeys::out::std_IO_exit_error_code]() );
	} catch ( utility::excn::BadInput & excn ) {
		worker_send_fail_on_bad_inputs_exception_message_to_master( larval_job, excn.msg() );
	} catch ( utility::excn::Exception & excn ) {
		worker_send_fail_w_message_to_master( larval_job, excn.msg() );
	} catch ( ... ) {
		worker_send_fail_w_message_to_master( larval_job, "Unhandled exception!" );
	}
}

void
MPIWorkPoolJobDistributor::retrieve_deallocation_messages_and_new_job_and_run()
{
	// ok, we've already received the deallocation message
	bool more_deallocation_messages_remaining( true );
	while ( more_deallocation_messages_remaining ) {
		std::list< deallocation::DeallocationMessageOP > messages;
		{ // scope
			std::string srlz_deallocation_msg = utility::receive_string_from_node( 0 );
			std::istringstream iss( srlz_deallocation_msg );
			cereal::BinaryInputArchive arc( iss );
			arc( messages );
		}

		for ( auto msg : messages ) {
			job_queen_->process_deallocation_message( msg );
		}

		core::Size next_mpi_msg = utility::receive_integer_from_node( 0 );
		if ( next_mpi_msg == mpi_work_pool_jd_new_job_available ) {
			more_deallocation_messages_remaining = false;
		} else {
			debug_assert( next_mpi_msg == mpi_work_pool_jd_deallocation_message );
		}
	}

	retrieve_job_and_run();
}


std::pair< LarvalJobOP, utility::vector1< JobResultCOP > >
MPIWorkPoolJobDistributor::retrieve_job_maturation_data()
{
	std::pair< LarvalJobOP, utility::vector1< JobResultCOP > > return_val;

	std::string larval_job_string = utility::receive_string_from_node( 0 );
	LarvalJobOP larval_job;
	try {
		larval_job = deserialize_larval_job( larval_job_string );
		TR << "Received job " << larval_job->job_index() << std::endl;
	} catch ( cereal::Exception & e ) {
		std::ostringstream oss;
		oss << "Failed to deserialize larval job on worker node " << mpi_rank_ << ". Exiting.\nError message from cereal library:\n";
		oss << e.what() << "\n";
		send_error_message_to_node0( oss.str() );
		return return_val;
	}

	utility::vector1< core::Size > archival_nodes_for_job_results = utility::receive_sizes_from_node( 0 );
	utility::vector1< JobResultCOP > job_results( larval_job->input_job_result_indices().size() );
	for ( core::Size ii = 1; ii <= job_results.size(); ++ii ) {
		// request job result #(larval_job->input_job_result_indices()[ ii ] )
		// from archival node #(archival_nodes_for_job_results[ii])
		core::Size archive_node = archival_nodes_for_job_results[ ii ];

		TR << "Requesting job result " << larval_job->input_job_result_indices()[ ii ].first << " " << larval_job->input_job_result_indices()[ ii ].second << " from archive " << archive_node << std::endl;
		utility::send_integer_to_node( archive_node, mpi_rank_ ); // tell the other node who is talking ..
		utility::send_integer_to_node( archive_node, mpi_work_pool_jd_retrieve_job_result ); // .. and that we need a job result
		utility::send_size_to_node( archive_node, larval_job->input_job_result_indices()[ ii ].first ); // .. of a particular job
		utility::send_size_to_node( archive_node, larval_job->input_job_result_indices()[ ii ].second ); // .. and a particular result from that job

		int retrieval_success = utility::receive_integer_from_node( archive_node ); // well, does the other node have that job?
		if ( retrieval_success == mpi_work_pool_jd_job_result_retrieved ) {
			std::string job_and_result_string = utility::receive_string_from_node( archive_node ); // good. Now we'll deserialize it
			LarvalJobAndResult job_and_result;
			try {
				job_and_result = deserialize_larval_job_and_result( job_and_result_string );
			} catch ( cereal::Exception & e ) {
				std::ostringstream oss;
				oss << "Failed to deserialize LarvalJob & JobResult pair from job " << larval_job->input_job_result_indices()[ ii ] <<
					" which is required as an input to job " << larval_job->job_index() << "\nError message from cereal library:\n" <<
					e.what() << "\n";
				send_error_message_to_node0( oss.str() );
				return return_val;
			}

			job_results[ ii ] = job_and_result.second;
			TR << "Success" << std::endl;
		} else {
			// ok -- nothing more we're going to do here. We can't run this job.
			//  Node 0 is going to be notified and the job will spin down.
			// perhaps this function should return "false" to indicate that the job
			// could not be run.
			return return_val;
		}
	}
	return_val.first = larval_job;
	return_val.second = job_results;
	return return_val;
}

void
MPIWorkPoolJobDistributor::worker_send_fail_do_not_retry_message_to_master(
	LarvalJobOP larval_job
)
{
	// tell node 0 the job did not succeed.
	utility::send_integer_to_node( 0, mpi_rank_ );
	utility::send_integer_to_node( 0, mpi_work_pool_jd_job_failed_do_not_retry );
	utility::send_size_to_node( 0, larval_job->job_index() );
}

void
MPIWorkPoolJobDistributor::worker_send_fail_bad_inputs_message_to_master(
	LarvalJobOP larval_job
)
{
	utility::send_integer_to_node( 0, mpi_rank_ );
	utility::send_integer_to_node( 0, mpi_work_pool_jd_job_failed_bad_input );
	utility::send_size_to_node( 0, larval_job->job_index() );
}

void
MPIWorkPoolJobDistributor::worker_send_fail_on_bad_inputs_exception_message_to_master(
	LarvalJobOP larval_job,
	std::string const & error_message
)
{
	utility::send_integer_to_node( 0, mpi_rank_ );
	utility::send_integer_to_node( 0, mpi_work_pool_jd_job_failed_w_message );
	utility::send_size_to_node( 0, larval_job->job_index() );
	utility::send_string_to_node( 0, error_message );
}

void
MPIWorkPoolJobDistributor::worker_send_fail_w_message_to_master(
	LarvalJobOP larval_job,
	std::string const & error_message
)
{
	utility::send_integer_to_node( 0, mpi_rank_ );
	utility::send_integer_to_node( 0, mpi_work_pool_jd_job_failed_w_message );
	utility::send_size_to_node( 0, larval_job->job_index() );
	utility::send_string_to_node( 0, error_message );
}

void
MPIWorkPoolJobDistributor::worker_send_fail_retry_limit_exceeded(
	LarvalJobOP larval_job
)
{
	utility::send_integer_to_node( 0, mpi_rank_ );
	utility::send_integer_to_node( 0, mpi_work_pool_jd_job_failed_retry_limit_exceeded );
	utility::send_size_to_node( 0, larval_job->job_index() );
}

void
MPIWorkPoolJobDistributor::worker_send_job_result_to_master_and_archive(
	LarvalJobOP larval_job,
	CompletedJobOutput job_output
)
{
	Size const nresults = job_output.job_results.size();
	utility::vector1< std::string > serialized_larval_job_and_results( nresults );
	try {
		for ( Size ii = 1; ii <= job_output.job_results.size(); ++ii ) {
			serialized_larval_job_and_results[ ii ] = serialize_larval_job_and_result(
				std::make_pair( larval_job, job_output.job_results[ ii ].second ));
		}
	} catch ( cereal::Exception const & e ) {
		// send an error message to node 0 and return
		std::ostringstream oss;
		oss << "Failed to serialize LarvalJob or JobResult; all objects stored in these" <<
			" classes (or derived from them) must implement save & load serialization functions\nJob #" <<
			larval_job->job_index() << " failed. Error message from the cereal library:\n" << e.what() << "\n";
		send_error_message_to_node0( oss.str() );
		return;
	}

	std::string serialized_job_summaries;
	utility::vector1< JobSummaryOP > job_summaries( nresults );
	std::transform( job_output.job_results.begin(), job_output.job_results.end(),
		job_summaries.begin(), []( SingleJobOutputPair const & summ_and_result ) { return summ_and_result.first; } );
	try {
		serialized_job_summaries = serialize_job_summaries( job_summaries );
	} catch ( cereal::Exception const & e ) {
		// send an error message to node 0 and return
		std::ostringstream oss;
		oss << "Failed to serialize JobSummary; all objects stored in this class (or that are derived from" <<
			" it) must implement save & load serialization functions\nJob #" << larval_job->job_index() <<
			" failed. Error message from the cereal library:\n" << e.what() << "\n";
		send_error_message_to_node0( oss.str() );
		return;
	}

	// Ok -- everything that needs to be serialized can be; proceed to report a
	// successful job completion.

	// now, notify node 0 that the job is complete -- node 0 will tell us where
	// to send the JobResult for archival.
	utility::send_integer_to_node( 0, mpi_rank_ );
	utility::send_integer_to_node( 0, mpi_work_pool_jd_job_success );
	utility::send_size_to_node( 0, larval_job->job_index() );
	utility::send_size_to_node( 0, nresults );
	utility::vector1< int > archival_nodes = utility::receive_integers_from_node( 0 );
	for ( Size ii = 1; ii <= nresults; ++ii ) {
		int archival_node = archival_nodes[ ii ];
		TR << "Archiving result " << ii << " of " << nresults << " on node " << archival_node << std::endl;
		utility::send_integer_to_node( archival_node, mpi_rank_ );
		utility::send_integer_to_node( archival_node, mpi_work_pool_jd_archive_job_result );
		utility::send_size_to_node( archival_node, larval_job->job_index() );
		utility::send_size_to_node( archival_node, ii );
		utility::send_string_to_node( archival_node, serialized_larval_job_and_results[ ii ] );

		// now wait to hear from the archival node before proceeding.
		// we need to ensure that the archival nodes have processed the archival events before
		// we send the job status to node 0. Technically, we could proceed to the next archival node,
		// if we were communicating with a different archive for the next job.
		// In any case,  by the time that we send the job status to node 0,
		// the JobResult's archival must have completed to ensure that no node
		// ever asks the archive for a JobResult that has not yet been archived (but that is
		// on its way to being archived). This avoids a race condition where either the
		// archival completes first and the next job that needs that JobResult as an input
		// runs properly, or the job requests the not-yet-archived JobResult, cannot find it,
		// and exits. That'd be a pretty bad race condition to have!
		int archival_completed_message = utility::receive_integer_from_node( archival_node );
		// debug_assert( archival_completed_message == mpi_work_pool_jd_archival_completed );
		if ( archival_completed_message != mpi_work_pool_jd_archival_completed ) {
			// well, damn
			std::ostringstream oss;
			oss << "Failed to archive job result #" << ii << " on archival node " << archival_node << " for job " << larval_job->job_index() << "; exiting" << std::endl;
			send_error_message_to_node0( oss.str() );
			return;
		}
	}

	// now go back to talk again with node 0
	utility::send_integer_to_node( 0, mpi_rank_ );
	utility::send_integer_to_node( 0, mpi_work_pool_jd_job_success_and_archival_complete );

	// Old Implementation: Node0 does not keep the in-flight LarvalJobOPs, so we have to send
	// it back.
	// if ( job_queen_->larval_job_needed_for_note_job_completed() ||
	//   job_queen_->larval_job_needed_for_completed_job_summary() ) {
	//  // Do we need a try/catch block around the call to serialize_larval_job?
	//  // No.
	//  // It's the same larval job that we serialized above, so if it serialized
	//  // before, it will serialize again.
	//  utility::send_string_to_node( 0, serialize_larval_job( larval_job ) );
	// } else {
	//  utility::send_size_to_node( 0, larval_job->job_index() );
	// }

	utility::send_size_to_node( 0, larval_job->job_index() );

	utility::send_integer_to_node( 0, job_output.status );
	utility::send_string_to_node( 0, serialized_job_summaries );
}

void
MPIWorkPoolJobDistributor::write_output(
	output::OutputSpecificationOP spec,
	LarvalJobAndResult const & job_and_result
)
{
	TR << "Writing output for result (" << spec->result_id().first << ", ";
	TR << spec->result_id().second << ") with output index ";
	TR << spec->output_index().primary_output_index << ", ";
	TR << spec->output_index().secondary_output_index << std::endl;

	// This is a two step process. Why? Because the MPIMultithreadedJobDistributor is going
	// to deserializize the job results and perform output in separate threads, so the
	// interaction with the outputters will need to be thread safe. (The JD still guarantees
	// that only one thread will interact with the JobQueen herself at any one point in time).
	if ( max_n_outputters_ > 1 ) {
		spec->jd_output_suffix( mpi_rank_string_ );
	}
	output::ResultOutputterOP outputter = job_queen_->result_outputter( *spec );
	outputter->write_output( *spec, *job_and_result.second );
}


/// @throws Potentially throws a cereal::Exception; it is much more likely that errors are
/// hit during object serialization, rather than objet deserialization, because an object
/// that lacks the save/load routines is embedded in a LarvalJob or JobResult. However,
/// it is possible that the save routine is OK, but the load routine fails, in which case
/// the user ought to be told what was wrong.
LarvalJobOP
MPIWorkPoolJobDistributor::deserialize_larval_job(
	std::string const & larval_job_string
) const
{
	LarvalJobOP larval_job;
	std::istringstream iss( larval_job_string );
	cereal::BinaryInputArchive arc( iss );
	arc( larval_job );
	return larval_job;
}

/// @throws Potentially throws a cereal::Exception if the LarvalJob holds an
/// unserializable object or is itself unserializable
std::string
MPIWorkPoolJobDistributor::serialize_larval_job(
	LarvalJobOP larval_job
) const
{
	std::ostringstream larval_job_oss;
	{
		cereal::BinaryOutputArchive arc( larval_job_oss );
		arc( larval_job );
	}
	return larval_job_oss.str();
}

/// @throws Potentially throws a cereal::Exception; it is much more likely that errors are
/// hit during object serialization, rather than objet deserialization, because an object
/// that lacks the save/load routines is embedded in a LarvalJob or JobResult. However,
/// it is possible that the save routine is OK, but the load routine fails, in which case
/// the user ought to be told what was wrong.
MPIWorkPoolJobDistributor::LarvalJobAndResult
MPIWorkPoolJobDistributor::deserialize_larval_job_and_result(
	std::string const & job_and_result_string
) const
{
	LarvalJobAndResult job_and_result;
	if ( compress_job_results_ ) {
		std::istringstream compressed_ss( job_and_result_string );
		zlib_stream::zip_istream unzipper( compressed_ss );
		cereal::BinaryInputArchive arc( unzipper );
		arc( job_and_result );
	} else {
		std::istringstream iss( job_and_result_string );
		cereal::BinaryInputArchive arc( iss );
		arc( job_and_result );
	}
	return job_and_result;
}

/// @throws Potentially throws a cereal::Exception if the LarvalJob or the JobResult holds an
/// unserializable object or either one is itself unserializable
std::string
MPIWorkPoolJobDistributor::serialize_larval_job_and_result(
	LarvalJobAndResult job_and_result
) const
{
	std::ostringstream oss;
	{
		cereal::BinaryOutputArchive arc( oss );
		arc( job_and_result );
	}
	std::string const uncompressed_string = oss.str();

	if ( compress_job_results_ ) {
		std::ostringstream compressed_ss;
		zlib_stream::zip_ostream zipper( compressed_ss );
		zipper << uncompressed_string;
		zipper.zflush();
		return compressed_ss.str();
	} else {
		return uncompressed_string;
	}
}

/// @throws Potentially throws a cereal::Exception; it is much more likely that errors are
/// hit during object serialization, rather than objet deserialization, because an object
/// that lacks the save/load routines is embedded in a LarvalJob or JobResult. However,
/// it is possible that the save routine is OK, but the load routine fails, in which case
/// the user ought to be told what was wrong.
utility::vector1< JobSummaryOP >
MPIWorkPoolJobDistributor::deserialize_job_summaries(
	std::string const & job_summaries_string
) const
{
	utility::vector1< JobSummaryOP > job_summaries;
	std::istringstream iss( job_summaries_string );
	cereal::BinaryInputArchive arc( iss );
	arc( job_summaries );
	return job_summaries;
}

/// @throws Potentially throws a cereal::Exception if the derived Summary holds an
/// unserializable object or is itself unserializable
std::string
MPIWorkPoolJobDistributor::serialize_job_summaries(
	utility::vector1< JobSummaryOP > const & job_summaries
) const
{
	std::ostringstream job_summaries_oss;
	{
		cereal::BinaryOutputArchive arc( job_summaries_oss );
		arc( job_summaries );
	}
	return job_summaries_oss.str();
}

output::OutputSpecificationOP
MPIWorkPoolJobDistributor::deserialize_output_specification( std::string const & spec_string ) const
{
	output::OutputSpecificationOP spec;
	std::istringstream iss( spec_string );
	cereal::BinaryInputArchive arc( iss );
	arc( spec );
	return spec;

}

std::string
MPIWorkPoolJobDistributor::serialize_output_specification( output::OutputSpecificationOP output_spec ) const
{
	std::ostringstream spec_oss;
	{
		cereal::BinaryOutputArchive arc( spec_oss );
		arc( output_spec );
	}
	return spec_oss.str();
}

std::string MPIWorkPoolJobDistributor::get_string_from_disk( Size job_id, Size result_index ) const {
	return utility::file_contents( filename_for_archive( job_id, result_index ) );
}

std::string MPIWorkPoolJobDistributor::filename_for_archive( Size job_id, Size result_index ) const {
	return archive_dir_name_ + "/archive." + std::to_string( job_id ) + "." + std::to_string( result_index );
}

std::string MPIWorkPoolJobDistributor::checkpoint_file_prefix( Size checkpoint_counter ) const
{
	return checkpoint_directory_ + "/chkpt_" + utility::to_string(checkpoint_counter);
}

std::string MPIWorkPoolJobDistributor::checkpoint_file_name_for_node(
	std::string const & prefix,
	int node_index
) const
{
	return prefix + "_" + utility::to_string(node_index) + ".bin";
}

void MPIWorkPoolJobDistributor::set_checkpoint_period( Size period_seconds )
{
	checkpoint_period_ = period_seconds;
}

/// @details Write out a file with a tiny bit of meta-data: the number of archive nodes.
void MPIWorkPoolJobDistributor::create_checkpoint_sanity_check_file() const
{
	std::string fname = checkpoint_directory_ + "/sanity.txt";
	std::ofstream os(fname.c_str());
	os << "n_archives= " << n_archives_ << "\n";
}

bool MPIWorkPoolJobDistributor::checkpoint_sanity_check() const
{
	if ( !utility::file::is_directory(checkpoint_directory_) ) return false;
	std::string fname = checkpoint_directory_ + "/sanity.txt";

	if ( !utility::file::file_exists(fname) ) return false;

	std::ifstream is(fname.c_str());
	std::string n_archives_str;
	int n_archives(0);

	if ( !is.good() ) return false;
	is >> n_archives_str;
	if ( n_archives_str != "n_archives=" ) {
		TR << "Checkpoint directory sanity check failed: expected to read 'n_archives= " << n_archives_ << "' but found the first line to begin with '" << n_archives_str << "' instead. Sanity file has been corrupted" << std::endl;
		return false;
	}
	if ( !is.good() ) return false;
	is >> n_archives;
	if ( n_archives != n_archives_ ) {
		TR << "Checkpoint directory sanity check failed: expected to read 'n_archives= " << n_archives_ << "' but found " << n_archives << " instead. Restoration from a checkpoint requires that the number of archives be consistent between runs (though, the number of workers is allowed to change)." << std::endl;
		return false;
	}
	return true;
}

void
MPIWorkPoolJobDistributor::delete_checkpoint( Size checkpoint_index ) const {
	if ( checkpoint_index > 0 &&
			! basic::options::option[ basic::options::OptionKeys::jd3::keep_all_checkpoints ] &&
			( ! basic::options::option[ basic::options::OptionKeys::jd3::keep_checkpoint ].user() ||
			basic::options::option[ basic::options::OptionKeys::jd3::keep_checkpoint ] != int(checkpoint_index) ) ) {
		for ( int ii = 0; ii <= n_archives_; ++ii ) {
			std::string fname = checkpoint_file_name_for_node(
				checkpoint_file_prefix(checkpoint_index), ii );
			TR << "Removing previous checkpoint file: " << fname << std::endl;
			// Error checking here?
			// It is entirely possible that the checkpoint file
			// we mean to be deleting has been deleted by the user
			remove( fname.c_str() );
		}
	}

}

void MPIWorkPoolJobDistributor::retrieve_job_result_and_act( JobResultID const & id, std::function< void (std::string const & ) > const & action )
{
	SerializedJobResultMap::const_iterator iter = job_results_.find( id );
	if ( iter == job_results_.end() ) {
		std::ostringstream oss;
		oss << "Requested job result " << id << " does not live on archive " << mpi_rank_ << "\n";
		throw oss.str();
	} else {
		if ( archive_on_disk_ ) {
			std::string const filename = filename_for_archive( id.first, id.second );
			std::ifstream in ( filename );
			if ( ! in.good() ) {
				std::ostringstream oss;
				oss << "Failed to open binary archive on disk: ':" << filename << "'\n";
				throw oss.str();
			} else {
				std::string const content = utility::file_contents( filename );
				action( content );
			}
			in.close();
		} else {
			// ok -- we have found the (serialized) JobResult
			action( * iter->second );
		}
	}
}

void MPIWorkPoolJobDistributor::retrieve_job_result_and_act_and_delete( JobResultID const & id, std::function< void (std::string const & ) > const & action )
{
	SerializedJobResultMap::const_iterator iter = job_results_.find( id );
	if ( iter == job_results_.end() ) {
		std::ostringstream oss;
		oss << "Requested job result " << id << " does not live on archive " << mpi_rank_ << "\n";
		TR << oss.str() << std::endl;
		throw oss.str();
	} else {
		if ( archive_on_disk_ ) {
			std::string const filename = filename_for_archive( id.first, id.second );
			std::ifstream in ( filename );
			if ( ! in.good() ) {
				std::ostringstream oss;
				oss << "Failed to open binary archive on disk: ':" << filename << "'\n";
				TR << oss.str() << std::endl;
				throw oss.str();
			} else {
				std::string const content = utility::file_contents( filename );
				action( content );
			}
			in.close();
			remove( filename.c_str() );
		} else {
			// ok -- we have found the (serialized) JobResult
			action( * iter->second );
		}
	}
	job_results_.erase( iter );
}

void MPIWorkPoolJobDistributor::delete_job_result( JobResultID const & result_id )
{
	auto iter = job_results_.find(result_id);
	if ( iter != job_results_.end() ) {
		job_results_.erase( result_id );
	}
	if ( archive_on_disk_ ) {
		remove( filename_for_archive( result_id.first, result_id.second ).c_str() );
	}
}


/// @throws std::string if the contents could not be archived
void MPIWorkPoolJobDistributor::save_job_result(
	JobResultID const & id,
	utility::pointer::shared_ptr< std::string > const & contents
)
{
	if ( archive_on_disk_ ) {
		job_results_[id] = nullptr;
		std::string const filename = filename_for_archive( id.first, id.second );
		std::ofstream out( filename );
		if ( !out.is_open() ) {
			std::ostringstream oss;
			oss << "Failed to open file for job result " << id << ".\n";
			TR << oss.str() << std::endl;
			throw oss.str();
		}
		out << *contents;
		out.close();
	} else {
		job_results_[id] = contents;
	}

}

}//job_distributors
}//jd3
}//protocols

#endif // SERIALIZATION
