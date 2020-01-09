// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file   test/protocols/jd3/MPIWorkPoolJobDistributor.cxxtest.hh
/// @brief  test suite for protocols::jd3::job_distributors/MPIWorkPoolJobDistributor using the SimulateMPI utilities
/// @author Andy Watkins (amw579@nyu.edu)

// Test headers
#include <cxxtest/TestSuite.h>
#include <test/core/init_util.hh>
#include <test/util/pose_funcs.hh>
#include <test/util/mpi_funcs.hh>

// Unit headers
#include <protocols/jd3/job_distributors/MPIWorkPoolJobDistributor.hh>

// Package headers
#include <protocols/jd3/jobs/MoverJob.hh>
#include <protocols/jd3/Job.hh>
#include <protocols/jd3/JobDigraph.hh>
#include <protocols/jd3/LarvalJob.hh>
#include <protocols/jd3/InnerLarvalJob.hh>
#include <protocols/jd3/JobResult.hh>
#include <protocols/jd3/JobSummary.hh>
#include <protocols/jd3/standard/StandardJobQueen.hh>
#include <protocols/jd3/pose_outputters/PDBPoseOutputter.hh>
#include <protocols/jd3/deallocation/InputPoseDeallocationMessage.hh>
#include <protocols/jd3/output/OutputSpecification.hh>
#include <protocols/jd3/output/ResultOutputter.hh>
#include <protocols/jd3/jobs/MoverJob.hh>
#include <protocols/jd3/job_summaries/EnergyJobSummary.hh>
#include <protocols/jd3/job_summaries/StandardPoseJobSummary.hh>
#include <test/protocols/jd3/DummyOutputter.hh>

/// Project headers
#include <core/types.hh>
#include <core/chemical/AA.hh>
#include <core/conformation/Residue.hh>
#include <core/pose/Pose.hh>
//#include <core/scoring/ScoreFunction.hh>
//#include <core/scoring/ScoreFunctionFactory.hh>
#include <core/import_pose/import_pose.hh>
#include <core/import_pose/import_pose_options.hh>

//#include <core/pack/task/PackerTask.hh>
//#include <core/pack/task/TaskFactory.hh>
//#include <core/pack/scmin/SidechainStateAssignment.hh>
#include <protocols/moves/NullMover.hh>

// Basic headers
#include <basic/datacache/ConstDataMap.hh>
#include <basic/options/option.hh>
#include <basic/options/keys/jd3.OptionKeys.gen.hh>
#include <basic/Tracer.hh>

// Utility headers
#include <utility/SimulateMPI.hh>
#include <utility/mpi_util.hh>
#include <utility/excn/Exceptions.hh>
#include <utility/pointer/owning_ptr.hh>
#include <utility/file/file_sys_util.hh>

// C++ headers
#include <map>
#include <sstream>
#include <utility>
#include <thread>
#include <chrono>

#ifdef SERIALIZATION
// Utility headers
#include <utility/vector1.srlz.hh>

// Cereal headers
#include <cereal/archives/binary.hpp>
#include <cereal/types/utility.hpp>
#include <cereal/types/list.hpp>
#include <cereal/types/memory.hpp>
#include <cereal/types/polymorphic.hpp>

#endif

using namespace utility;
using namespace core;
using namespace protocols::jd3;
using namespace protocols::jd3::job_distributors;
using namespace protocols::jd3::output;
using namespace protocols::jd3::pose_outputters;
using namespace protocols::jd3::standard;
using namespace protocols::jd3::jobs;
using namespace protocols::jd3::job_summaries;


basic::Tracer TR( "test.protocols.jd3.job_distributors.MPIWorkPoolJobDistributorTests" );

// shorthand functions, sp and sp1
std::pair< Size, Size > sp( Size first_in, Size second_in ) { return std::make_pair( first_in, second_in ); }
std::pair< Size, Size > sp1( Size first_in ) { return std::make_pair( first_in, Size(1) ); }

class PoolMnPJob : public MoverJob
{
public:

	CompletedJobOutput
	run() override {
		CompletedJobOutput output = MoverJob::run();
		StandardPoseJobSummaryOP sum = utility::pointer::dynamic_pointer_cast< StandardPoseJobSummary > ( output.job_results[1].first );
		sum->set_energy( 1234 );
		return output;
	}
};

typedef utility::pointer::shared_ptr< PoolMnPJob > PoolMnPJobOP;


// Dummy JobQueen //
class PoolJQ1 : public protocols::jd3::standard::StandardJobQueen
{
public:
	PoolJQ1() :
		njobs_( 1 ),
		job_list_determined_( false ),
		n_jobs_created_so_far_( 0 ),
		results_( new std::map< JobResultID, JobResultOP > ),
		dummy_outputter_( new DummyPoseOutputter( results_ ) )
	{
		do_not_accept_all_pose_outputters_from_factory();
		allow_pose_outputter( PoseOutputterCreatorOP( new DummyOutputterCreator ));
	}

	~PoolJQ1() {}


	LarvalJobs determine_job_list( Size job_node_index, Size max_njobs ) override
	{
		//determine_preliminary_job_list(); //Needed for outputter init
		LarvalJobs jobs;
		if ( job_list_determined_ ) return jobs;

		InnerLarvalJobOP inner_job( new InnerLarvalJob( njobs_, job_node_index ) );
		inner_job->outputter( DummyPoseOutputter::keyname() );
		jobs = expand_job_list( inner_job, max_njobs );
		n_jobs_created_so_far_ += jobs.size();
		if ( n_jobs_created_so_far_ >= njobs_ ) {
			job_list_determined_ = true;
		}
		return jobs;
	}

	protocols::jd3::JobOP
	complete_larval_job_maturation(
		protocols::jd3::LarvalJobCOP job,
		utility::options::OptionCollectionCOP,
		utility::vector1< JobResultCOP > const &
	) override
	{
		using namespace protocols::jd3;
		PoolMnPJobOP mature_job( new PoolMnPJob );

		core::pose::PoseOP pose = create_trpcage_ideal_poseop();
		mature_job->pose( pose );
		mature_job->set_mover( protocols::moves::NullMoverOP( new protocols::moves::NullMover ) );

		jobs_matured_.push_back( job->job_index() );

		return mature_job;
	}

	///@brief Allows this derived SJQ to not need full output initialized through determine_preliminary_job_list
	bool has_job_previously_been_output( protocols::jd3::LarvalJobCOP ) override { return false; }

	void note_job_completed( LarvalJobCOP larval_job, JobStatus status, core::Size nresults  ) override
	{
		status_[ larval_job->job_index() ] = status;
		StandardJobQueen::note_job_completed( larval_job, status, nresults );
	}
	//void completed_job_summary( core::Size job_id, core::Size result_index, JobSummaryOP summary ) override { summaries_[ sp(job_id,result_index) ] = summary; }
	void completed_job_summary( LarvalJobCOP job, core::Size result_index, JobSummaryOP summary ) override { summaries_[ sp(job->job_index(),result_index) ] = summary; }

	output::OutputSpecificationOP
	create_output_specification_for_job_result(
		LarvalJobCOP job,
		utility::options::OptionCollection const & job_options,
		core::Size result_index,
		core::Size nresults
	) override
	{
		JobOutputIndex out_ind = build_output_index( job, result_index, nresults );
		output::OutputSpecificationOP spec = dummy_outputter_->create_output_specification( *job,
			out_ind,
			job_options, utility::tag::TagCOP() );
		spec->result_id( { job->job_index(), result_index } );
		spec->output_index( out_ind );
		return spec;
	}

	output::ResultOutputterOP
	result_outputter( output::OutputSpecification const & ) override
	{
		return dummy_outputter_;
	}

	// void completed_job_result( LarvalJobCOP job, core::Size result_index, JobResultOP job_result ) override { results_[ sp(job->job_index(),result_index) ] = job_result; }

#ifdef SERIALIZATION
	template <class Archive>
	void save(Archive & arc) const {
		arc( cereal::base_class< protocols::jd3::standard::StandardJobQueen >( this ) );
		arc( njobs_ );
		arc( job_list_determined_ );
		arc( n_jobs_created_so_far_ );
		arc( jobs_matured_ );
		arc( status_ );
		arc( summaries_ );
		arc( results_ );
		arc( dummy_outputter_ );
	}
	template <class Archive>
	void load(Archive & arc) {
		arc( cereal::base_class< protocols::jd3::standard::StandardJobQueen >( this ) );
		arc( njobs_ );
		arc( job_list_determined_ );
		arc( n_jobs_created_so_far_ );
		arc( jobs_matured_ );
		arc( status_ );
		arc( summaries_ );
		arc( results_ );
		arc( dummy_outputter_ );
	}
#endif

public:
	core::Size njobs_;

	bool job_list_determined_;
	core::Size n_jobs_created_so_far_;

	mutable utility::vector1< core::Size > jobs_matured_;
	std::map< Size,        JobStatus    > status_;
	std::map< JobResultID, JobSummaryOP > summaries_;
	std::shared_ptr< std::map< JobResultID, JobResultOP > > results_;
	DummyPoseOutputterOP dummy_outputter_;

protected:
	// create a job graph with a single node; skip the SJQ's implementation of this function
	JobDigraphOP create_initial_job_dag() override {
		JobDigraphOP job_graph( new JobDigraph( 1 ) );
		return job_graph;
	}
};

class PoolMnPJob2 : public MoverJob
{
public:
	PoolMnPJob2() : throw_( false ), throw_bad_inputs_( false ), status_( jd3_job_status_success ) {}

	CompletedJobOutput
	run() override {
		CompletedJobOutput output = MoverJob::run();
		if ( throw_ ) {
			throw CREATE_EXCEPTION(utility::excn::Exception,  "PoolMnPJob2 exception" );
		} else if ( throw_bad_inputs_ ) {
			throw CREATE_EXCEPTION(utility::excn::BadInput,  "PoolMnPJob2 bad input exception" );
		} else {
			output.status = status_;
		}
		return output;
	}

	bool throw_;
	bool throw_bad_inputs_;
	JobStatus status_;

};

typedef utility::pointer::shared_ptr< PoolMnPJob2 > PoolMnPJob2OP;

class PoolJQ2 : public PoolJQ1
{
public:
	PoolJQ2() : throw_( false ), throw_bad_inputs_( false ), status_( jd3_job_status_success ) {}

	protocols::jd3::JobOP
	complete_larval_job_maturation(
		protocols::jd3::LarvalJobCOP job,
		utility::options::OptionCollectionCOP,
		utility::vector1< JobResultCOP > const &
	) override
	{
		using namespace protocols::jd3;
		PoolMnPJob2OP mature_job( new PoolMnPJob2 );
		mature_job->throw_ = throw_;
		mature_job->throw_bad_inputs_ = throw_bad_inputs_;
		mature_job->status_ = status_;

		core::pose::PoseOP pose = create_trpcage_ideal_poseop();
		mature_job->pose( pose );
		mature_job->set_mover( protocols::moves::NullMoverOP( new protocols::moves::NullMover ) );

		jobs_matured_.push_back( job->job_index() );

		return mature_job;
	}

	bool throw_;
	bool throw_bad_inputs_;
	JobStatus status_;

};


class PoolJQ3 : public PoolJQ1
{
public:
	PoolJQ3() :
		node_1_njobs_( 3 ),
		node_1_jobs_given_out_( false ),
		node_2_njobs_( 3 ),
		node_2_jobs_given_out_( false ),
		discarded_round_1_results_( false ),
		output_round_2_results_( false )
	{}


	LarvalJobs determine_job_list( Size job_node_index, Size max_njobs ) override
	{
		determine_preliminary_job_list(); //Needed for outputter init
		LarvalJobs job_list;
		if ( job_node_index == 1 ) {
			if ( node_1_jobs_given_out_ ) return job_list;
			node_1_jobs_given_out_ = true;
			InnerLarvalJobOP inner_job( new InnerLarvalJob( node_1_njobs_, job_node_index ));
			inner_job->outputter( DummyPoseOutputter::keyname() );
			job_list = expand_job_list( inner_job, max_njobs );
		} else if ( job_node_index == 2 ) {
			if ( node_2_jobs_given_out_ ) return job_list;
			node_2_jobs_given_out_ = true;
			InnerLarvalJobOP inner_job( new InnerLarvalJob( node_2_njobs_, job_node_index ));
			inner_job->outputter( DummyPoseOutputter::keyname() );
			inner_job->add_input_job_result_index( sp1(1) );
			job_list = expand_job_list( inner_job, max_njobs );

		}
		return job_list;
	}

	std::list< OutputSpecificationOP > jobs_that_should_be_output() override
	{
		std::list< OutputSpecificationOP > output_list;
		if ( node_2_jobs_given_out_ && ! output_round_2_results_ ) {
			bool all_jobs_completed = true;
			for ( Size ii = node_1_njobs_+1; ii <= node_1_njobs_ + node_2_njobs_; ++ii ) {
				if ( summaries_.count( sp1(ii) ) == 0 ) { all_jobs_completed = false; break;  }
			}
			if ( ! all_jobs_completed ) {
				return output_list;
			}
			for ( Size ii = node_1_njobs_+1; ii <= node_1_njobs_ + node_2_njobs_; ++ii ) {
				output_list.push_back( OutputSpecificationOP( new DummyOutputSpecification( sp(ii, 1), JobOutputIndex() ) ));
			}
			output_round_2_results_ = true;
		}
		return output_list;
	}

	std::list< JobResultID > job_results_that_should_be_discarded() override
	{
		// After we have finished round 1, discard all the job results, except those
		// from job 1
		std::list< JobResultID > discard_list;

		if ( node_1_jobs_given_out_ && ! discarded_round_1_results_ ) {
			bool all_jobs_seen = true;
			for ( Size ii = 1; ii <= node_1_njobs_; ++ii ) {
				if ( summaries_.count( sp1(ii) ) == 0 ) { all_jobs_seen = false; break; }
			}
			if ( ! all_jobs_seen ) {
				return discard_list;
			}
			discarded_round_1_results_ = true;
			for ( Size ii = 2; ii <= node_1_njobs_; ++ii ) {
				discard_list.push_back( std::make_pair( ii, core::Size(1) ) );
			}
		}
		return discard_list;
	}



	Size node_1_njobs_;
	bool node_1_jobs_given_out_;

	Size node_2_njobs_;
	bool node_2_jobs_given_out_;

	bool discarded_round_1_results_;
	bool output_round_2_results_;

#ifdef SERIALIZATION
	template <class Archive>
	void save(Archive & arc) const {
		arc( cereal::base_class< PoolJQ1 >( this ) );
		arc( node_1_njobs_ );
		arc( node_1_jobs_given_out_ );
		arc( node_2_njobs_ );
		arc( node_2_jobs_given_out_ );
		arc( discarded_round_1_results_ );
		arc( output_round_2_results_ );
	}
	template <class Archive>
	void load(Archive & arc) {
		arc( cereal::base_class< PoolJQ1 >( this ) );
		arc( node_1_njobs_ );
		arc( node_1_jobs_given_out_ );
		arc( node_2_njobs_ );
		arc( node_2_jobs_given_out_ );
		arc( discarded_round_1_results_ );
		arc( output_round_2_results_ );
	}
#endif

protected:

	JobDigraphOP create_initial_job_dag() override {
		JobDigraphOP job_dag( new JobDigraph( 2 ) );
		job_dag->add_edge( 1, 2 );
		return job_dag;
	}
};

// The idea for this class is to test the various error messages delivered by the JD on node 0
// when a job result on an archive node can't be retrieved. In particular, if the job queen
// discards/outputs a job (removing it from the archive node) in between the time that the job
// distributor on node 0 sends the job out to a remote node to have it worked on and when that
// remote node tries to go and retrieve its results from the archive, then it's possible
// for the archive to legitimately not have the result.
class PoolJQ3b : public PoolJQ3
{
public:
	PoolJQ3b() : node_2_first_job_finished_( false ), discarded_job1_results_( false ), discard_job1_result_( true )
	{
		node_1_njobs_ = node_2_njobs_ = 2;
	}

	void note_job_completed( LarvalJobCOP job, JobStatus status, Size nresults ) override {
		if ( job->job_index() == node_1_njobs_ + 1 ) node_2_first_job_finished_ = true;
		PoolJQ1::note_job_completed( job, status, nresults );
	}

	//void note_job_completed( core::Size job_id, JobStatus status, Size nresults ) override {
	// if ( job_id == node_1_njobs_ + 1 ) node_2_first_job_finished_ = true;
	// PoolJQ1::note_job_completed( job_id, status, nresults );
	//}

	std::list< OutputSpecificationOP > jobs_that_should_be_output() override
	{
		if ( ! discard_job1_result_ && node_2_first_job_finished_ && ! discarded_job1_results_ ) {
			discarded_job1_results_ = true; // that is: job1 will not be stored after it is output
			std::list< OutputSpecificationOP > job1_to_output;
			job1_to_output.push_back( OutputSpecificationOP( new DummyOutputSpecification( sp(1, 1), JobOutputIndex() ) ));
			return job1_to_output;
		} else {
			return PoolJQ3::jobs_that_should_be_output();
		}
	}

	std::list< JobResultID > job_results_that_should_be_discarded() override
	{
		if ( discard_job1_result_ && node_2_first_job_finished_ && ! discarded_job1_results_ ) {
			discarded_job1_results_ = true;
			std::list< JobResultID > job1_to_discard;
			job1_to_discard.push_back( std::make_pair( 1, core::Size(1) ) );
			return job1_to_discard;
		} else {
			return PoolJQ3::job_results_that_should_be_discarded();
		}
	}

	bool node_2_first_job_finished_;
	bool discarded_job1_results_;
	bool discard_job1_result_; // either we ask the job distributor to discard job 1, or we ask the job distributor to output job 1
};

class PoolJQ4 : public PoolJQ1
{
public:
	using PoolJQ1::note_job_completed;
	using PoolJQ1::completed_job_summary;

	void note_job_completed( LarvalJobCOP job, JobStatus status, Size nresults ) override {
		jobs_completed_through_larval_job_interface_.push_back( job->job_index() );
		PoolJQ1::note_job_completed( job, status, nresults );
	}


	void completed_job_summary( LarvalJobCOP job, Size result_index, JobSummaryOP summary ) override {
		summaries_through_larval_job_interface_.push_back( job->job_index() );
		PoolJQ1::completed_job_summary( job, result_index, summary );
	}


	utility::vector1< core::Size > jobs_completed_through_larval_job_interface_;
	utility::vector1< core::Size > summaries_through_larval_job_interface_;

};

class PoolJQ5 : public PoolJQ3
{
public:
	PoolJQ5() : node_3_njobs_( 1 ), node_3_jobs_given_out_( false ) { node_1_njobs_ = 1; node_2_njobs_ = 0; }


	void update_job_dag( JobDigraphUpdater & updater ) override {
		if ( updater.orig_num_nodes() == 1 && updater.job_dag()->get_job_node(1)->all_jobs_completed() ) {
			updater.add_node();
			updater.add_edge_to_new_node( 1, 2 );
		} else if ( updater.orig_num_nodes() == 2 && updater.job_dag()->get_job_node(2)->all_jobs_completed() ) {
			updater.add_node();
			updater.add_edge_to_new_node( 2, 3 );
		}
	}

	LarvalJobs determine_job_list( Size job_node_index, Size max_njobs ) override
	{
		if ( job_node_index < 3 ) return PoolJQ3::determine_job_list( job_node_index, max_njobs );
		LarvalJobs job_list;
		if ( ! node_3_jobs_given_out_ ) {
			node_3_jobs_given_out_ = true;
			InnerLarvalJobOP inner_job( new InnerLarvalJob( node_3_njobs_, job_node_index ));
			inner_job->outputter( DummyPoseOutputter::keyname() );
			job_list = expand_job_list( inner_job, max_njobs );
		}
		return job_list;
	}

	Size node_3_njobs_;
	bool node_3_jobs_given_out_;

protected:
	JobDigraphOP create_initial_job_dag() override {
		JobDigraphOP job_dag( new JobDigraph( 1 ) );
		return job_dag;
	}
};

// ok -- this class should be put into a larval job or
// a job result and when either the master node or the
// worker node goes to serialize it, serialization should
// fail. The JobDistributor should gracefully exit when this
// happens.
class Unserializable : public utility::pointer::ReferenceCount
{
public:
	Unserializable() : myint_( 5 ) {}

	// AMW: Adding a dumb function here so that we don't
	// have an unused private field. (Because to my understanding
	// you need this for the unit test)
	void foo() { myint_++; }

private:
	int myint_;
};

typedef utility::pointer::shared_ptr< Unserializable > UnserializableOP;

class Undeserializable : public utility::pointer::ReferenceCount
{
public:
	Undeserializable() : myint_( 5 ) {}
	int get_my_int() { return myint_; } // appease compiler

#ifdef SERIALIZATION
	template < class Archive >
	void
	save( Archive & arc ) const {
		arc( myint_ );
	}

	// this function causes trouble: it retrieves more data from the
	// archive than was put in
	template < class Archive >
	void
	load( Archive & ) {
		throw cereal::Exception( "Undeserializable could not be deserialized" );
	}
#endif

private:
	int myint_;
};

typedef utility::pointer::shared_ptr< Undeserializable > UndeserializableOP;


// This JQ puts an unserializable object into a LarvalJob.
class PoolJQ6 : public PoolJQ1
{
	// this class will imbed a non-serializable piece of data
	// in the larval job; the master node ought to catch an
	// exception when it tries to serialize the object and
	// send it to another node.
	LarvalJobs determine_job_list( Size job_node_index, Size max_njobs ) override
	{
		determine_preliminary_job_list(); //Needed for outputter init
		LarvalJobs jobs;
		if ( job_list_determined_ ) return jobs;

		InnerLarvalJobOP inner_job( new InnerLarvalJob( njobs_, job_node_index ) );
		inner_job->outputter( DummyPoseOutputter::keyname() );
		inner_job->const_data_map().add( "testing", "testing", UnserializableOP( new Unserializable ));

		jobs = expand_job_list( inner_job, max_njobs );
		n_jobs_created_so_far_ += jobs.size();
		if ( n_jobs_created_so_far_ >= njobs_ ) {
			job_list_determined_ = true;
		}
		return jobs;
	}
};

// This JQ puts an undeserializable object into a LarvalJob.
class PoolJQ7 : public PoolJQ1
{
	// this class will imbed a non-serializable piece of data
	// in the larval job; the master node ought to catch an
	// exception when it tries to serialize the object and
	// send it to another node.
	LarvalJobs determine_job_list( Size, Size max_njobs ) override
	{
		determine_preliminary_job_list(); //Needed for outputter init
		LarvalJobs jobs;
		if ( job_list_determined_ ) return jobs;

		InnerLarvalJobOP inner_job( new InnerLarvalJob( njobs_, 1 ) );
		inner_job->outputter( DummyPoseOutputter::keyname() );
		inner_job->const_data_map().add( "testing", "testing", UndeserializableOP( new Undeserializable ));

		jobs = expand_job_list( inner_job, max_njobs );
		n_jobs_created_so_far_ += jobs.size();
		if ( n_jobs_created_so_far_ >= njobs_ ) {
			job_list_determined_ = true;
		}
		return jobs;
	}
};

// an unserializable job summary
class PoolJobSummary1 : public JobSummary
{
public:
	PoolJobSummary1() : JobSummary(), myint_( 5 ) {}
	int myint_;
};

// an undeserializable job summary
class PoolJobSummary2 : public JobSummary
{
public:
	PoolJobSummary2() : JobSummary(), myint_( 5 ) {}
	int get_my_int() { return myint_; } // appease compiler

#ifdef SERIALIZATION
	template < class Archive >
	void
	save( Archive & arc ) const {
		arc( myint_ );
	}

	// this function causes trouble: it retrieves more data from the
	// archive than was put in
	template < class Archive >
	void
	load( Archive & ) {
		throw cereal::Exception( "PoolJobSummary2 could not be deserialized" );
	}
#endif

	int myint_;
};

#ifdef SERIALIZATION

CEREAL_REGISTER_TYPE( Undeserializable )
CEREAL_REGISTER_TYPE( PoolJobSummary2 )

#endif

class PoolMnPJob3 : public MoverJob
{
public:
	PoolMnPJob3() : MoverJob() {}

	protocols::jd3::CompletedJobOutput
	run() override {
		CompletedJobOutput output = MoverJob::run();
		PoseJobResultOP pose_result = utility::pointer::dynamic_pointer_cast< PoseJobResult > ( output.job_results[1].second );
		pose_result->pose()->set_const_data( "testing", "testing", Unserializable() );
		return output;
	}
};

typedef utility::pointer::shared_ptr< PoolMnPJob3 > PoolMnPJob3OP;

class PoolMnPJob4 : public MoverJob
{
public:
	PoolMnPJob4() : MoverJob() {}

	protocols::jd3::CompletedJobOutput
	run() override {
		CompletedJobOutput output = MoverJob::run();
		output.job_results[1].first = JobSummaryOP( new PoolJobSummary1 );
		return output;
	}
};

typedef utility::pointer::shared_ptr< PoolMnPJob4 > PoolMnPJob4OP;

class PoolMnPJob5 : public MoverJob
{
public:
	PoolMnPJob5() : MoverJob() {}

	CompletedJobOutput
	run() override {
		CompletedJobOutput output = MoverJob::run();
		output.job_results[1].first = JobSummaryOP( new PoolJobSummary2 );
		return output;
	}
};

typedef utility::pointer::shared_ptr< PoolMnPJob5 > PoolMnPJob5OP;

class PoolJQ8 : public PoolJQ1
{

	protocols::jd3::JobOP
	complete_larval_job_maturation(
		protocols::jd3::LarvalJobCOP job,
		utility::options::OptionCollectionCOP,
		utility::vector1< JobResultCOP > const &
	) override
	{
		using namespace protocols::jd3;
		PoolMnPJob3OP mature_job( new PoolMnPJob3 );

		core::pose::PoseOP pose = create_trpcage_ideal_poseop();
		mature_job->pose( pose );
		mature_job->set_mover( protocols::moves::NullMoverOP( new protocols::moves::NullMover ) );

		jobs_matured_.push_back( job->job_index() );

		return mature_job;
	}

};

class PoolJQ9 : public PoolJQ1
{

	protocols::jd3::JobOP
	complete_larval_job_maturation(
		protocols::jd3::LarvalJobCOP job,
		utility::options::OptionCollectionCOP,
		utility::vector1< JobResultCOP > const &
	) override
	{
		using namespace protocols::jd3;
		PoolMnPJob4OP mature_job( new PoolMnPJob4 );

		core::pose::PoseOP pose = create_trpcage_ideal_poseop();
		mature_job->pose( pose );
		mature_job->set_mover( protocols::moves::NullMoverOP( new protocols::moves::NullMover ) );

		jobs_matured_.push_back( job->job_index() );

		return mature_job;
	}

};

class PoolJQ10 : public PoolJQ1
{

	protocols::jd3::JobOP
	complete_larval_job_maturation(
		protocols::jd3::LarvalJobCOP job,
		utility::options::OptionCollectionCOP,
		utility::vector1< JobResultCOP > const &
	) override
	{
		using namespace protocols::jd3;
		PoolMnPJob5OP mature_job( new PoolMnPJob5 );

		core::pose::PoseOP pose = create_trpcage_ideal_poseop();
		mature_job->pose( pose );
		mature_job->set_mover( protocols::moves::NullMoverOP( new protocols::moves::NullMover ) );

		jobs_matured_.push_back( job->job_index() );

		return mature_job;
	}

};

class PoolJQ11 : public PoolJQ3
{
public:

	PoolJQ11() : distributed_deallocation_messages_( false ) {}

	std::list< deallocation::DeallocationMessageOP >
	deallocation_messages() override {
		// ok, let's send a message that two Poses should be deleted
		std::list< deallocation::DeallocationMessageOP > messages;
		if ( distributed_deallocation_messages_ ) return messages;
		distributed_deallocation_messages_ = true;

		typedef protocols::jd3::deallocation::InputPoseDeallocationMessage   PoseDeallocMsg;
		typedef protocols::jd3::deallocation::InputPoseDeallocationMessageOP PoseDeallocMsgOP;
		messages.push_back( PoseDeallocMsgOP( new PoseDeallocMsg( 1 ) ));
		messages.push_back( PoseDeallocMsgOP( new PoseDeallocMsg( 2 ) ));
		return messages;
	}

	void
	process_deallocation_message( deallocation::DeallocationMessageOP message ) override {
		received_messages_.push_back( message );
	}

	bool distributed_deallocation_messages_;
	utility::vector1< deallocation::DeallocationMessageOP > received_messages_;

};

class PoolJQ12 : public PoolJQ3
{
public:
	bool saved_to_archive_;
	bool loaded_from_archive_;

	PoolJQ12() :
		PoolJQ3(),
		saved_to_archive_(false),
		loaded_from_archive_(false)
	{}

	LarvalJobs determine_job_list( Size job_node_index, Size max_njobs ) override
	{
		TR << "DETERMINE JOB LIST: " << job_node_index << " " << node_2_jobs_given_out_ << std::endl;
		if ( job_node_index == 2 && ! node_2_jobs_given_out_ ) {
			TR << "SLEEPING FOR TWO SECONDS" << std::endl;
			std::this_thread::sleep_for(std::chrono::seconds(2));
		}
		return PoolJQ3::determine_job_list(job_node_index, max_njobs);
	}

#ifdef SERIALIZATION
	template <class Archive>
	void save(Archive & arc) const {
		const_cast< bool & > (saved_to_archive_) = true;
		arc( cereal::base_class< PoolJQ3 >( this ) );
		arc( saved_to_archive_ );
		arc( loaded_from_archive_ );
	}
	template <class Archive>
	void load(Archive & arc) {
		arc( cereal::base_class< PoolJQ3 >( this ) );
		arc( saved_to_archive_ );
		arc( loaded_from_archive_ );
		loaded_from_archive_ = true;
	}
#endif

};


typedef utility::pointer::shared_ptr< PoolJQ1 > PoolJQ1OP;
typedef utility::pointer::shared_ptr< PoolJQ2 > PoolJQ2OP;
typedef utility::pointer::shared_ptr< PoolJQ3 > PoolJQ3OP;
typedef utility::pointer::shared_ptr< PoolJQ3b > PoolJQ3bOP;
typedef utility::pointer::shared_ptr< PoolJQ4 > PoolJQ4OP;
typedef utility::pointer::shared_ptr< PoolJQ5 > PoolJQ5OP;
typedef utility::pointer::shared_ptr< PoolJQ6 > PoolJQ6OP;
typedef utility::pointer::shared_ptr< PoolJQ7 > PoolJQ7OP;
typedef utility::pointer::shared_ptr< PoolJQ8 > PoolJQ8OP;
typedef utility::pointer::shared_ptr< PoolJQ9 > PoolJQ9OP;
typedef utility::pointer::shared_ptr< PoolJQ10 > PoolJQ10OP;
typedef utility::pointer::shared_ptr< PoolJQ11 > PoolJQ11OP;
typedef utility::pointer::shared_ptr< PoolJQ12 > PoolJQ12OP;

#ifdef SERIALIZATION
CEREAL_REGISTER_TYPE( PoolJQ1 )
CEREAL_REGISTER_TYPE( PoolJQ3 )
CEREAL_REGISTER_TYPE( PoolJQ12 )
#endif


// --------------- Test Class --------------- //

class MPIWorkPoolJobDistributorTests : public CxxTest::TestSuite {

public:

	typedef core::Size   Size;
	typedef core::Real   Real;

	// Only add the DummyOutputter options once
	bool local_options_added_;

	MPIWorkPoolJobDistributorTests() : local_options_added_( false ) {}

	// --------------- Fixtures --------------- //

	// Define a test fixture (some initial state that several tests share)
	// In CxxTest, setUp()/tearDown() are executed around each test case. If you need a fixture on the test
	// suite level, i.e. something that gets constructed once before all the tests in the test suite are run,
	// suites have to be dynamically created. See CxxTest sample directory for example.

	// Shared initialization goes here.
	void setUp() {
		core_init();
		if ( ! local_options_added_ ) {
			using namespace basic::options;
			option.add( OptionKeys::dummy_outputter_arg, "" ).def(false);
			option.add( OptionKeys::dummy_outputter, "" ).def(false);
			local_options_added_ = true;
		}
		basic::options::option[ basic::options::OptionKeys::jd3::compress_job_results ]( false );
	}

	// Shared finalization goes here.
	void tearDown() {
	}

#ifdef SERIALIZATION

	template < class T >
	std::string
	serialized_T( T const & data ) {
		std::ostringstream oss;
		{
			cereal::BinaryOutputArchive arc( oss );
			arc( data );
		}
		return oss.str();
	}


	template < class T >
	T
	deserialized_T( std::string const & str ) {
		std::istringstream iss( str );
		T return_object;
		cereal::BinaryInputArchive arc( iss );
		arc( return_object );
		return return_object;
	}

	LarvalJobOP
	deserialize_larval_job( std::string const & serialized_larval_job )
	{
		return deserialized_T< LarvalJobOP >( serialized_larval_job );
	}

	LarvalJobOP
	create_larval_job(
		core::Size njobs,
		core::Size nstruct_id,
		core::Size job_index
	)
	{
		InnerLarvalJobOP inner_job( new InnerLarvalJob( njobs, 1 ) );
		inner_job->outputter( DummyPoseOutputter::keyname() );
		return LarvalJobOP( new LarvalJob( inner_job, nstruct_id, job_index ) );
	}

	// create a larval job that lists job 1 as a required input to this job
	LarvalJobOP
	create_larval_job_w_job1_dep(
		core::Size njobs,
		core::Size nstruct_id,
		core::Size job_index
	)
	{
		InnerLarvalJobOP inner_job( new InnerLarvalJob( njobs, 1 ) );
		inner_job->outputter( DummyPoseOutputter::keyname() );
		inner_job->add_input_job_result_index( sp1(1) );
		LarvalJobOP larval_job( new LarvalJob( inner_job, nstruct_id, job_index ) );
		return larval_job;
	}


	std::string
	serialized_larval_job(
		core::Size njobs,
		core::Size nstruct_id,
		core::Size job_index
	)
	{
		LarvalJobOP larval_job = create_larval_job( njobs, nstruct_id, job_index );
		return serialized_T( larval_job );
	}

	std::string
	serialized_larval_job_and_job_result(
		core::Size njobs,
		core::Size nstruct_id,
		core::Size job_index,
		JobResultOP job_result
	)
	{
		return serialized_larval_job_and_job_result(
			create_larval_job( njobs, nstruct_id, job_index ), job_result );
	}

	std::string
	serialized_larval_job_and_job_result( LarvalJobOP larval_job, JobResultOP job_result )
	{
		return serialized_T( std::make_pair( larval_job, job_result ) );
	}

	std::string
	serialized_job_summaries( utility::vector1< JobSummaryOP > summaries )
	{
		return serialized_T( summaries );
	}

	utility::vector1< JobSummaryOP >
	deserialized_job_summaries( std::string const & str  )
	{
		return deserialized_T< utility::vector1< JobSummaryOP > >( str );
	}


	MPIWorkPoolJobDistributor::LarvalJobAndResult
	deserialized_larval_job_and_job_result( std::string const & str )
	{
		try {
			return deserialized_T< MPIWorkPoolJobDistributor::LarvalJobAndResult >( str );
		} catch ( ... ) {
			std::cerr << "Failed to deserialize larval job and job result!" << std::endl;
			TS_ASSERT( false );
			MPIWorkPoolJobDistributor::LarvalJobAndResult dummy_val;
			return dummy_val;
		}
	}

	std::list< deallocation::DeallocationMessageOP >
	deserialize_deallocation_msg_list( std::string const & serialized_msg_list )
	{
		return deserialized_T< std::list< deallocation::DeallocationMessageOP > >( serialized_msg_list );
	}


	std::string
	serialized_input_pose_deallocation_msg_list( std::list< core::Size > const & poses_to_deallocate )
	{
		typedef deallocation::DeallocationMessageOP MsgOP;
		typedef deallocation::InputPoseDeallocationMessage InputPoseMsg;
		std::list< MsgOP > msg_list;
		for ( auto pose_id : poses_to_deallocate ) {
			msg_list.push_back( MsgOP( new InputPoseMsg( pose_id ) ) );
		}
		return serialized_T( msg_list );
	}


	std::string
	serialized_output_specification(
		JobResultID result_id,
		JobOutputIndex output_index
	){
		OutputSpecificationOP spec( new DummyOutputSpecification( result_id, output_index ));
		return serialized_T( spec );
	}

	std::string
	serialized_output_specification(
		JobResultID result_id,
		JobOutputIndex output_index,
		std::string const & outname
	){
		auto spec = std::make_shared< DummyOutputSpecification >( result_id, output_index );
		spec->out_fname( outname );
		OutputSpecificationOP base_spec(spec);
		return serialized_T( base_spec );
	}

	output::OutputSpecificationOP
	deserialized_output_specification( std::string const & serialized_spec )
	{
		return deserialized_T< OutputSpecificationOP >( serialized_spec );
	}


	JobOutputIndex joi( Size primary_index ) {
		JobOutputIndex retval;
		retval.primary_output_index = primary_index;
		return retval;
	}


#endif

	void test_MPIWorkPoolJobDistributor_master_node_vanilla_end_to_end_all_successes() {
		TS_ASSERT( true ); // appease non-serialization builds

#ifdef SERIALIZATION

		EnergyJobSummaryOP trpcage_job_summary( new EnergyJobSummary );
		trpcage_job_summary->energy( 1234 );
		utility::vector1< JobSummaryOP > summaries( 1, trpcage_job_summary );
		std::string serialized_trpcage_job_summaries = serialized_job_summaries( summaries );

		PoseJobResultOP trpcage_pose_result( new PoseJobResult );
		trpcage_pose_result->pose(  create_trpcage_ideal_poseop() );

		//CompletedJobOutput trpcage_job_output;
		//trpcage_job_output.first = trpcage_job_summary;
		//trpcage_job_output.second = trpcage_pose_result;

		//std::ostrinstream oss;
		//{
		// cereal::BinaryOutputArchive arc( oss );
		// arc( trpcage_job_output );
		//}
		//std::string serialized_trpcage_job_output = oss.str();

		SimulateMPI::initialize_simulation( 4 );

		// ok, all the nodes at once start requesting jobs from node 0
		SimulateMPI::set_mpi_rank( 1 );
		send_integer_to_node( 0, 1 );
		SimulateMPI::set_mpi_rank( 2 );
		send_integer_to_node( 0, 2 );
		send_integer_to_node( 0, mpi_work_pool_jd_new_job_request );
		SimulateMPI::set_mpi_rank( 3 );
		send_integer_to_node( 0, 3 );

		// let's start the job at node 1
		SimulateMPI::set_mpi_rank( 1 );
		send_integer_to_node( 0, mpi_work_pool_jd_new_job_request );

		// and now the job at node 3
		SimulateMPI::set_mpi_rank( 3 );
		send_integer_to_node( 0, mpi_work_pool_jd_new_job_request );

		// ok -- now let's pretend that node 2 has finished its job
		SimulateMPI::set_mpi_rank( 2 );
		send_integer_to_node( 0, 2 );

		// ok -- before node 2 can finish its two part job-completion process,
		// node 1 will butt in and start its two part job-completion process,
		SimulateMPI::set_mpi_rank( 1 );
		send_integer_to_node( 0, 1 );

		SimulateMPI::set_mpi_rank( 2 );
		send_integer_to_node( 0, mpi_work_pool_jd_job_success );
		send_size_to_node( 0, 2 ); // job_id
		send_size_to_node( 0, 1 ); // num jobs
		// ok -- node 0 is going to tell it to archive it's results on node 0

		send_integer_to_node( 0, 2 );
		send_integer_to_node( 0, mpi_work_pool_jd_archive_job_result );
		send_size_to_node( 0, 2 ); // job id
		send_size_to_node( 0, 1 ); // result index
		send_string_to_node( 0, serialized_larval_job_and_job_result( 3, 2, 2, trpcage_pose_result ));

		SimulateMPI::set_mpi_rank( 1 );
		send_integer_to_node( 0, mpi_work_pool_jd_job_success );
		send_size_to_node( 0, 1 ); // job id
		send_size_to_node( 0, 1 ); // num jobs

		send_integer_to_node( 0, 1 );
		send_integer_to_node( 0, mpi_work_pool_jd_archive_job_result );
		send_size_to_node( 0, 1 ); // job id
		send_size_to_node( 0, 1 ); // result index
		send_string_to_node( 0, serialized_larval_job_and_job_result( 3, 1, 1, trpcage_pose_result ));

		SimulateMPI::set_mpi_rank( 2 );
		send_integer_to_node( 0, 2 );
		send_integer_to_node( 0, mpi_work_pool_jd_job_success_and_archival_complete );
		send_size_to_node( 0, 2 ); // job id
		send_integer_to_node( 0, jd3_job_status_success );
		send_string_to_node( 0, serialized_trpcage_job_summaries );

		// Now node 2 asks for a new job
		send_integer_to_node( 0, 2 );
		send_integer_to_node( 0, mpi_work_pool_jd_new_job_request );

		SimulateMPI::set_mpi_rank( 3 );
		send_integer_to_node( 0, 3 );
		send_integer_to_node( 0, mpi_work_pool_jd_job_success );
		send_size_to_node( 0, 3 ); // job id
		send_size_to_node( 0, 1 ); // num results

		send_integer_to_node( 0, 3 );
		send_integer_to_node( 0, mpi_work_pool_jd_archive_job_result );
		send_size_to_node( 0, 3 ); // job id
		send_size_to_node( 0, 1 ); // result index
		send_string_to_node( 0, serialized_larval_job_and_job_result( 3, 3, 3, trpcage_pose_result ));

		SimulateMPI::set_mpi_rank( 1 );
		send_integer_to_node( 0, 1 );
		send_integer_to_node( 0, mpi_work_pool_jd_job_success_and_archival_complete );
		send_size_to_node( 0, 1 ); // job id
		send_integer_to_node( 0, jd3_job_status_success );
		send_string_to_node( 0, serialized_trpcage_job_summaries );

		// Now node 1 asks for a new job
		send_integer_to_node( 0, 1 );
		send_integer_to_node( 0, mpi_work_pool_jd_new_job_request );

		SimulateMPI::set_mpi_rank( 3 );
		send_integer_to_node( 0, 3 );
		send_integer_to_node( 0, mpi_work_pool_jd_job_success_and_archival_complete );
		send_size_to_node( 0, 3 ); // job id
		send_integer_to_node( 0, jd3_job_status_success );
		send_string_to_node( 0, serialized_trpcage_job_summaries );

		// Now node 3 asks for a new job
		send_integer_to_node( 0, 3 );
		send_integer_to_node( 0, mpi_work_pool_jd_new_job_request );

		SimulateMPI::set_mpi_rank( 1 );
		//  Node 1 has output the first result it was given, so it asks for a new job
		send_integer_to_node( 0, 1 );
		send_integer_to_node( 0, mpi_work_pool_jd_new_job_request );

		// Node 1 has output the second result it was given, so it asks for a new job
		send_integer_to_node( 0, 1 );
		send_integer_to_node( 0, mpi_work_pool_jd_new_job_request );

		// Node 1 has output the third result it was given, so it asks for a new job
		// but this time, node 0 will tell it that there are no jobs remaining
		send_integer_to_node( 0, 1 );
		send_integer_to_node( 0, mpi_work_pool_jd_new_job_request );

		// ok -- now we create a JQ on node 0, set it up to produce three jobs
		// and then create a job distributor for node 0 and tell it to go go go!

		SimulateMPI::set_mpi_rank( 0 );

		PoolJQ1OP jq( new PoolJQ1 );

		jq->njobs_ = 3;

		MPIWorkPoolJobDistributor jd;

		try {
			jd.go( jq );
		} catch ( ... ) {
			TS_ASSERT( false /*no exception should be thrown*/ );
		}

		// OK!
		// Now we read the mail we got from node 0

		SimulateMPI::set_mpi_rank( 1 );
		ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 1 that it has a job for it to run", mpi_work_pool_jd_new_job_available );
		std::string ser_larv1 = ts_assert_mpi_buffer_has_string( 0, "node 0 sends node 1 the serialized LarvalJob" );
		LarvalJobOP larval_job1 = deserialize_larval_job( ser_larv1 );
		TS_ASSERT_EQUALS( larval_job1->job_index(), 1 );
		TS_ASSERT_EQUALS( larval_job1->nstruct_index(), 1 );
		TS_ASSERT_EQUALS( larval_job1->nstruct_max(), 3 );
		ts_assert_mpi_buffer_has_sizes( 0, "node 0 sends node 1 an empty list of job result indices", utility::vector1< Size >() );

		ts_assert_mpi_buffer_has_integers( 0, "node 0 tells node 1 to archive its results on node 0", utility::vector1< int >( 1, 0 ) );
		ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 1 that archival completed", mpi_work_pool_jd_archival_completed );

		{ // scope -- job2 output
			ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 1 to output job 2", mpi_work_pool_jd_accept_and_output_job_result );
			std::string output_spec2_string = ts_assert_mpi_buffer_has_string( 0, "node 0 sends the serialized output spec #2" );
			OutputSpecificationOP output_spec2 = deserialized_output_specification( output_spec2_string );
			TS_ASSERT_EQUALS( output_spec2->result_id().first, 2 );
			TS_ASSERT_EQUALS( output_spec2->result_id().second, 1 );
			TS_ASSERT_EQUALS( output_spec2->output_index().primary_output_index, 2 );
			TS_ASSERT_EQUALS( output_spec2->output_index().n_primary_outputs, 1000 );
			TS_ASSERT_EQUALS( output_spec2->output_index().secondary_output_index, 1 );
			TS_ASSERT_EQUALS( output_spec2->output_index().n_secondary_outputs, 1 );
			std::string job_result2_string = ts_assert_mpi_buffer_has_string( 0, "node 0 sends the serialized job-result-and-larval-job-pair string" );
			MPIWorkPoolJobDistributor::LarvalJobAndResult job_res2 = deserialized_larval_job_and_job_result( job_result2_string );
			TS_ASSERT( job_res2.first );
			TS_ASSERT( job_res2.second );
			TS_ASSERT_EQUALS( job_res2.first->job_index(), 2 );
			TS_ASSERT_EQUALS( job_res2.first->nstruct_index(), 2 );
			TS_ASSERT_EQUALS( job_res2.first->nstruct_max(), 3 );
			TS_ASSERT_EQUALS( utility::pointer::dynamic_pointer_cast< PoseJobResult > ( job_res2.second )->pose()->total_residue(), 20 );
		}

		{ // scope
			ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 1 to output job 1", mpi_work_pool_jd_accept_and_output_job_result );
			std::string output_spec1_string = ts_assert_mpi_buffer_has_string( 0, "node 0 sends the serialized output spec #1" );
			OutputSpecificationOP output_spec1 = deserialized_output_specification( output_spec1_string );
			TS_ASSERT_EQUALS( output_spec1->result_id().first, 1 );
			TS_ASSERT_EQUALS( output_spec1->result_id().second, 1 );
			TS_ASSERT_EQUALS( output_spec1->output_index().primary_output_index, 1 );
			TS_ASSERT_EQUALS( output_spec1->output_index().n_primary_outputs, 1000 );
			TS_ASSERT_EQUALS( output_spec1->output_index().secondary_output_index, 1 );
			TS_ASSERT_EQUALS( output_spec1->output_index().n_secondary_outputs, 1 );
			std::string job_result1_string = ts_assert_mpi_buffer_has_string( 0, "node 0 sends the serialized job-result-and-larval-job-pair string" );
			MPIWorkPoolJobDistributor::LarvalJobAndResult job_res1 = deserialized_larval_job_and_job_result( job_result1_string );
			TS_ASSERT( job_res1.first );
			TS_ASSERT( job_res1.second );
			TS_ASSERT_EQUALS( job_res1.first->job_index(), 1 );
			TS_ASSERT_EQUALS( job_res1.first->nstruct_index(), 1 );
			TS_ASSERT_EQUALS( job_res1.first->nstruct_max(), 3 );
			TS_ASSERT_EQUALS( utility::pointer::dynamic_pointer_cast< PoseJobResult > ( job_res1.second )->pose()->total_residue(), 20 );
		} // scope

		{ // scope
			ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 1 to output job 3", mpi_work_pool_jd_accept_and_output_job_result );
			std::string output_spec3_string = ts_assert_mpi_buffer_has_string( 0, "node 0 sends the serialized output spec #3" );
			OutputSpecificationOP output_spec3 = deserialized_output_specification( output_spec3_string );
			TS_ASSERT_EQUALS( output_spec3->result_id().first, 3 );
			TS_ASSERT_EQUALS( output_spec3->result_id().second, 1 );
			TS_ASSERT_EQUALS( output_spec3->output_index().primary_output_index, 3 );
			TS_ASSERT_EQUALS( output_spec3->output_index().n_primary_outputs, 1000 );
			TS_ASSERT_EQUALS( output_spec3->output_index().secondary_output_index, 1 );
			TS_ASSERT_EQUALS( output_spec3->output_index().n_secondary_outputs, 1 );
			std::string job_result3_string = ts_assert_mpi_buffer_has_string( 0, "node 0 sends the serialized job-result-and-larval-job-pair string" );
			MPIWorkPoolJobDistributor::LarvalJobAndResult job_res3 = deserialized_larval_job_and_job_result( job_result3_string );
			TS_ASSERT( job_res3.first );
			TS_ASSERT( job_res3.second );
			TS_ASSERT_EQUALS( job_res3.first->job_index(), 3 );
			TS_ASSERT_EQUALS( job_res3.first->nstruct_index(), 3 );
			TS_ASSERT_EQUALS( job_res3.first->nstruct_max(), 3 );
			TS_ASSERT_EQUALS( utility::pointer::dynamic_pointer_cast< PoseJobResult > ( job_res3.second )->pose()->total_residue(), 20 );
		}

		ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 1 that there are no jobs left to run", mpi_work_pool_jd_spin_down );

		SimulateMPI::set_mpi_rank( 2 );
		ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 2 that it has a job for it to run", mpi_work_pool_jd_new_job_available );
		std::string ser_larv2 = ts_assert_mpi_buffer_has_string( 0, "node 0 sends node 2 the serialized LarvalJob" );
		LarvalJobOP larval_job2 = deserialize_larval_job( ser_larv2 );
		TS_ASSERT_EQUALS( larval_job2->job_index(), 2 );
		TS_ASSERT_EQUALS( larval_job2->nstruct_index(), 2 );
		TS_ASSERT_EQUALS( larval_job2->nstruct_max(), 3 );
		ts_assert_mpi_buffer_has_sizes( 0, "node 0 sends node 2 an empty list of job result indices", utility::vector1< Size >() );

		ts_assert_mpi_buffer_has_integers( 0, "node 0 tells node 2 to archive its results on node 0", utility::vector1< int >( 1, 0 ) );
		ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 2 that archival completed", mpi_work_pool_jd_archival_completed );

		ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 2 that there are no jobs left to run", mpi_work_pool_jd_spin_down );

		SimulateMPI::set_mpi_rank( 3 );
		ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 3 that it has a job for it to run", mpi_work_pool_jd_new_job_available );
		std::string ser_larv3 = ts_assert_mpi_buffer_has_string( 0, "node 0 sends node 3 the serialized LarvalJob" );
		LarvalJobOP larval_job3 = deserialize_larval_job( ser_larv3 );
		TS_ASSERT_EQUALS( larval_job3->job_index(), 3 );
		TS_ASSERT_EQUALS( larval_job3->nstruct_index(), 3 );
		TS_ASSERT_EQUALS( larval_job3->nstruct_max(), 3 );
		ts_assert_mpi_buffer_has_sizes( 0, "node 0 sends node 3 an empty list of job result indices", utility::vector1< Size >() );

		ts_assert_mpi_buffer_has_integers( 0, "node 0 tells node 3 to archive its results on node 0", utility::vector1< int >( 1, 0 ) );
		ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 3 that archival completed", mpi_work_pool_jd_archival_completed );

		ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 3 that there are no jobs left to run", mpi_work_pool_jd_spin_down );


		TS_ASSERT_EQUALS( jq->status_.count( 1 ), 1 );
		TS_ASSERT_EQUALS( jq->status_.count( 2 ), 1 );
		TS_ASSERT_EQUALS( jq->status_.count( 3 ), 1 );
		TS_ASSERT_EQUALS( jq->status_[ 1 ], jd3_job_status_success );
		TS_ASSERT_EQUALS( jq->status_[ 2 ], jd3_job_status_success );
		TS_ASSERT_EQUALS( jq->status_[ 3 ], jd3_job_status_success );

		TS_ASSERT_EQUALS( jq->summaries_.count( sp1( 1 ) ), 1 );
		TS_ASSERT_EQUALS( jq->summaries_.count( sp1( 2 ) ), 1 );
		TS_ASSERT_EQUALS( jq->summaries_.count( sp1( 3 ) ), 1 );
		TS_ASSERT_EQUALS( utility::pointer::dynamic_pointer_cast< EnergyJobSummary > ( jq->summaries_[ sp1( 1 ) ])->energy(), 1234 );
		TS_ASSERT_EQUALS( utility::pointer::dynamic_pointer_cast< EnergyJobSummary > ( jq->summaries_[ sp1( 2 ) ])->energy(), 1234 );
		TS_ASSERT_EQUALS( utility::pointer::dynamic_pointer_cast< EnergyJobSummary > ( jq->summaries_[ sp1( 3 ) ])->energy(), 1234 );

		TS_ASSERT_EQUALS( jq->results_->count( sp1( 1 ) ), 0 );
		TS_ASSERT_EQUALS( jq->results_->count( sp1( 2 ) ), 0 );
		TS_ASSERT_EQUALS( jq->results_->count( sp1( 3 ) ), 0 );
		//TS_ASSERT_EQUALS( utility::pointer::dynamic_pointer_cast< PoseJobResult > ( (*jq->results_)[ sp1( 1 ) ])->pose()->total_residue(), 20 );
		//TS_ASSERT_EQUALS( utility::pointer::dynamic_pointer_cast< PoseJobResult > ( (*jq->results_)[ sp1( 2 ) ])->pose()->total_residue(), 20 );
		//TS_ASSERT_EQUALS( utility::pointer::dynamic_pointer_cast< PoseJobResult > ( (*jq->results_)[ sp1( 3 ) ])->pose()->total_residue(), 20 );

		SimulateMPI::set_mpi_rank( 0 );

		TS_ASSERT( SimulateMPI::incoming_message_queue_is_empty() );
		TS_ASSERT( SimulateMPI::outgoing_message_queue_is_empty() );

		//std::cout << "Finished First Unit Test" << std::endl;

#endif
	}


	void test_workpool_jd3_worker_and_outputter_node_vanilla_end_to_end_all_successes() {
		TS_ASSERT( true );

#ifdef SERIALIZATION
		PoseJobResultOP trpcage_pose_result( new PoseJobResult );
		trpcage_pose_result->pose(  create_trpcage_ideal_poseop() );

		SimulateMPI::initialize_simulation( 2 );

		SimulateMPI::set_mpi_rank(0);

		// send 1st job
		send_integer_to_node( 1, mpi_work_pool_jd_new_job_available );
		send_string_to_node( 1, serialized_larval_job( 2, 1, 1 ) );
		send_sizes_to_node( 1, utility::vector1< Size >() );

		// tell node 1 where to send the results of job 1:
		send_integers_to_node( 1, utility::vector1< int >( 1, 0 ) );
		send_integer_to_node( 1, mpi_work_pool_jd_archival_completed );

		// Now send the second job to node 1
		send_integer_to_node( 1, mpi_work_pool_jd_new_job_available );
		send_string_to_node( 1, serialized_larval_job( 2, 2, 2 ) );
		send_sizes_to_node( 1, utility::vector1< Size >() );

		// tell node 1 where to send the results of job 2:
		send_integers_to_node( 1, utility::vector1< int >( 1, 0 ) );
		send_integer_to_node( 1, mpi_work_pool_jd_archival_completed );

		// Next, JD0 will ask node1 to output the jobs, and will send back
		// the serialized job result.
		send_integer_to_node( 1, mpi_work_pool_jd_accept_and_output_job_result );
		send_string_to_node( 1, serialized_output_specification( sp1(1), joi(1) ) );
		send_string_to_node( 1, serialized_larval_job_and_job_result( 2, 1, 1, trpcage_pose_result ) );

		send_integer_to_node( 1, mpi_work_pool_jd_accept_and_output_job_result );
		send_string_to_node( 1, serialized_output_specification( sp1(2), joi(2) ) );
		send_string_to_node( 1, serialized_larval_job_and_job_result( 2, 2, 2, trpcage_pose_result ) );

		// Finally, tell node 1 to spin dow
		send_integer_to_node( 1, mpi_work_pool_jd_spin_down );

		PoolJQ1OP jq1( new PoolJQ1 );
		try {
			SimulateMPI::set_mpi_rank( 1 );
			MPIWorkPoolJobDistributor jd;
			jd.go( jq1 );
		} catch ( ... ) {
			std::cerr << "Exception thrown from worker node in a unit test that should not have thrown an exception!" << std::endl;
			TS_ASSERT( false );
			return;
		}

		// NOW let's make sure that node 1 has been sending its messages the way it should have
		SimulateMPI::set_mpi_rank( 0 );
		// Node 1 starts by requesting a job
		ts_assert_mpi_buffer_has_integer( 1, "node 1 says to node 0: I have a request A", 1 );
		ts_assert_mpi_buffer_has_integer( 1, "node 1 requests a new job of node 0", mpi_work_pool_jd_new_job_request );

		// Then it replies that its job is complete
		ts_assert_mpi_buffer_has_integer( 1, "node 1 says to node 0: I have a request B", 1 );
		ts_assert_mpi_buffer_has_integer( 1, "node 1 has finished its job", mpi_work_pool_jd_job_success );
		ts_assert_mpi_buffer_has_size( 1, "node 1 finished job 1", 1 );
		ts_assert_mpi_buffer_has_size( 1, "node 1 job 1 contained a single result", 1 );

		ts_assert_mpi_buffer_has_integer( 1, "node 1 says to node 0: I have a request B2", 1 );
		ts_assert_mpi_buffer_has_integer( 1, "node 1 wants to archive a job result", mpi_work_pool_jd_archive_job_result );
		ts_assert_mpi_buffer_has_size( 1, "node 1 wants to archive a result for job 1", 1 );
		ts_assert_mpi_buffer_has_size( 1, "node 1 wants to archive the first result for job 1", 1 );
		std::string serialized_job_result1 = ts_assert_mpi_buffer_has_string( 1, "node 1 sends the serialized trpcage pose result" );
		MPIWorkPoolJobDistributor::LarvalJobAndResult job_and_result1 = deserialized_larval_job_and_job_result( serialized_job_result1 );
		TS_ASSERT( job_and_result1.first );
		TS_ASSERT( job_and_result1.second );
		if ( job_and_result1.first ) {
			TS_ASSERT_EQUALS( job_and_result1.first->job_index(), 1 );
			TS_ASSERT_EQUALS( job_and_result1.first->nstruct_index(), 1 );
			TS_ASSERT_EQUALS( job_and_result1.first->nstruct_max(), 2 );
		}
		PoseJobResultOP pose_result1 = utility::pointer::dynamic_pointer_cast< PoseJobResult > ( job_and_result1.second );
		TS_ASSERT( pose_result1 );
		if ( pose_result1 ) {
			TS_ASSERT_EQUALS( pose_result1->pose()->total_residue(), 20 );
		}
		ts_assert_mpi_buffer_has_integer( 1, "node 1 says to node 0: I have a request C", 1 );
		ts_assert_mpi_buffer_has_integer( 1, "node 1 says to node 0: I finished archiving my completed job result -- I'm ready to send a JobSummary", mpi_work_pool_jd_job_success_and_archival_complete );
		ts_assert_mpi_buffer_has_size( 1, "node 1 says to node 0: this was for job index 1", 1 );
		ts_assert_mpi_buffer_has_integer( 1, "node 1 says to node 0: job status is good", jd3_job_status_success );
		std::string serialized_summaries1 = ts_assert_mpi_buffer_has_string( 1, "node 1 says to node 0: here are the summaries" );
		utility::vector1< JobSummaryOP > summaries1 = deserialized_job_summaries( serialized_summaries1 );
		TS_ASSERT_EQUALS( summaries1.size(), 1 );
		if ( summaries1.size() != 1 ) return;
		TS_ASSERT( summaries1[1] );
		StandardPoseJobSummaryOP energy_summary1 = utility::pointer::dynamic_pointer_cast< StandardPoseJobSummary > ( summaries1[1] );
		TS_ASSERT( energy_summary1 );
		if ( energy_summary1 ) {
			TS_ASSERT_EQUALS( energy_summary1->energy(), 1234 );
		}

		// Node 1 then should request a second job
		ts_assert_mpi_buffer_has_integer( 1, "node 1 says to node 0: I have a request D", 1 );
		ts_assert_mpi_buffer_has_integer( 1, "node 1 requests a new job of node 0", mpi_work_pool_jd_new_job_request );

		// Then it says the job is complete
		ts_assert_mpi_buffer_has_integer( 1, "node 1 says to node 0: I have a request E", 1 );
		ts_assert_mpi_buffer_has_integer( 1, "node 1 requests a new job of node 0", mpi_work_pool_jd_job_success );
		ts_assert_mpi_buffer_has_size( 1, "node 1 finished job 2", 2 );
		ts_assert_mpi_buffer_has_size( 1, "node 1 job 2 had one result", 1 );

		ts_assert_mpi_buffer_has_integer( 1, "node 1 says to node 0: I have a request E2", 1 );
		ts_assert_mpi_buffer_has_integer( 1, "node 1 needs to archive a result", mpi_work_pool_jd_archive_job_result );
		ts_assert_mpi_buffer_has_size( 1, "node 1 this is a result from job 2", 2 );
		ts_assert_mpi_buffer_has_size( 1, "node 1 this is the first result from job 2", 1 );
		std::string serialized_job_result2 = ts_assert_mpi_buffer_has_string( 1, "node 1 send the serialized trpcage pose result" );
		MPIWorkPoolJobDistributor::LarvalJobAndResult job_and_result2 = deserialized_larval_job_and_job_result( serialized_job_result2 );
		TS_ASSERT( job_and_result2.first );
		TS_ASSERT( job_and_result2.second );
		if ( job_and_result2.first ) {
			TS_ASSERT_EQUALS( job_and_result2.first->job_index(), 2 );
			TS_ASSERT_EQUALS( job_and_result2.first->nstruct_index(), 2 );
			TS_ASSERT_EQUALS( job_and_result2.first->nstruct_max(), 2 );
		}
		PoseJobResultOP pose_result2 = utility::pointer::dynamic_pointer_cast< PoseJobResult > ( job_and_result2.second );
		TS_ASSERT( pose_result2 );
		if ( pose_result2 ) {
			TS_ASSERT_EQUALS( pose_result2->pose()->total_residue(), 20 );
		}

		ts_assert_mpi_buffer_has_integer( 1, "node 1 says to node 0: I have a request F", 1 );
		ts_assert_mpi_buffer_has_integer( 1, "node 1 says to node 0: I finished archiving my completed job result -- I'm ready to send a JobSummary", mpi_work_pool_jd_job_success_and_archival_complete );
		ts_assert_mpi_buffer_has_size( 1, "node 1 says to node 0: this was for job index 2", 2 );
		ts_assert_mpi_buffer_has_integer( 1, "node 1 says that job 2 ended successfully", jd3_job_status_success );
		std::string serialized_summaries2 = ts_assert_mpi_buffer_has_string( 1, "node 1 says to node 0: here's the summary" );
		utility::vector1< JobSummaryOP > summaries2 = deserialized_job_summaries( serialized_summaries2 );
		TS_ASSERT_EQUALS( summaries2.size(), 1 );
		if ( summaries2.size() != 1 ) return;
		TS_ASSERT( summaries2[1] );
		StandardPoseJobSummaryOP energy_summary2 = utility::pointer::dynamic_pointer_cast< StandardPoseJobSummary > ( summaries2[1] );
		TS_ASSERT( energy_summary2 );
		if ( energy_summary2 ) {
			TS_ASSERT_EQUALS( energy_summary2->energy(), 1234 );
		}

		// OK -- now node 1 should ask for a job, but instead of getting a work work job, it's
		// given an outputting job
		ts_assert_mpi_buffer_has_integer( 1, "node 1 says to node 0: I have a request", 1 );
		ts_assert_mpi_buffer_has_integer( 1, "node 1 requests a new job of node 0", mpi_work_pool_jd_new_job_request );

		ts_assert_mpi_buffer_has_integer( 1, "node 1 says to node 0: I have a request again", 1 );
		ts_assert_mpi_buffer_has_integer( 1, "node 1 says to node 0: I finished outputting job 1", mpi_work_pool_jd_output_completed );

		// OK -- now node 1 should ask for a job, and is again given an output job
		ts_assert_mpi_buffer_has_integer( 1, "node 1 says to node 0: I have a request", 1 );
		ts_assert_mpi_buffer_has_integer( 1, "node 1 requests a new job of node 0", mpi_work_pool_jd_new_job_request );

		ts_assert_mpi_buffer_has_integer( 1, "node 1 says to node 0: I have a request again 2", 1 );
		ts_assert_mpi_buffer_has_integer( 1, "node 1 says to node 0: I finished outputting job 2", mpi_work_pool_jd_output_completed );


		// OK -- now node 1 should ask for a job for a final time
		ts_assert_mpi_buffer_has_integer( 1, "node 1 says to node 0: I have a request", 1 );
		ts_assert_mpi_buffer_has_integer( 1, "node 1 requests a new job of node 0", mpi_work_pool_jd_new_job_request );

		// TO DO
		// make sure the job queen from node 1 has matured two larval jobs

		TS_ASSERT_EQUALS( jq1->results_->count( sp1( 1 ) ), 1 );
		TS_ASSERT_EQUALS( jq1->results_->count( sp1( 2 ) ), 1 );
		TS_ASSERT_EQUALS( utility::pointer::dynamic_pointer_cast< PoseJobResult > ( (*jq1->results_)[ sp1( 1 ) ])->pose()->total_residue(), 20 );
		TS_ASSERT_EQUALS( utility::pointer::dynamic_pointer_cast< PoseJobResult > ( (*jq1->results_)[ sp1( 2 ) ])->pose()->total_residue(), 20 );

		SimulateMPI::set_mpi_rank( 1 );

		TS_ASSERT( SimulateMPI::incoming_message_queue_is_empty() );
		TS_ASSERT( SimulateMPI::outgoing_message_queue_is_empty() );

		//std::cout << "Finished Second Unit Test" << std::endl;


#endif
	}

	void test_workpool_jd3_master_node_job_failed_do_not_retry() {
		TS_ASSERT( true );
#ifdef SERIALIZATION
		SimulateMPI::initialize_simulation( 2 );

		SimulateMPI::set_mpi_rank( 1 );
		send_integer_to_node( 0, 1 ); // I have a request
		send_integer_to_node( 0, mpi_work_pool_jd_new_job_request ); // I want a new job
		send_integer_to_node( 0, 1 ); // I have a request
		send_integer_to_node( 0, mpi_work_pool_jd_job_failed_do_not_retry ); // which is that the job I was running has failed
		send_size_to_node( 0, 1 ); // The job that failed was job #1

		// ask for another job
		send_integer_to_node( 0, 1 ); // I have a request
		send_integer_to_node( 0, mpi_work_pool_jd_new_job_request ); // I want a new job

		SimulateMPI::set_mpi_rank( 0 );
		PoolJQ1OP jq( new PoolJQ1 );
		jq->njobs_ = 1;

		MPIWorkPoolJobDistributor jd;
		try {
			jd.go( jq );
		} catch ( ... ) {
			TS_ASSERT( false /*no exception should be thrown*/ );
		}

		// OK!
		// Now we read the mail we got from node 0

		SimulateMPI::set_mpi_rank( 1 );
		ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 1 that it has a job for it to run", mpi_work_pool_jd_new_job_available );
		std::string ser_larv1 = ts_assert_mpi_buffer_has_string( 0, "node 0 sends node 1 the serialized LarvalJob" );
		LarvalJobOP larval_job1 = deserialize_larval_job( ser_larv1 );
		TS_ASSERT_EQUALS( larval_job1->job_index(), 1 );
		TS_ASSERT_EQUALS( larval_job1->nstruct_index(), 1 );
		TS_ASSERT_EQUALS( larval_job1->nstruct_max(), 1 );
		ts_assert_mpi_buffer_has_sizes( 0, "node 0 sends node 1 an empty list of job result indices", utility::vector1< Size >() );

		ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 1 that there are no jobs left to run", mpi_work_pool_jd_spin_down );

		TS_ASSERT_EQUALS( jq->status_.count( 1 ), 1 );
		TS_ASSERT_EQUALS( jq->status_[ 1 ], jd3_job_status_failed_do_not_retry );
		TS_ASSERT_EQUALS( jq->summaries_.count( sp1(1) ), 0 );
		TS_ASSERT_EQUALS( jq->results_->count( sp1(1) ), 0 );

		SimulateMPI::set_mpi_rank( 0 );

		TS_ASSERT( SimulateMPI::incoming_message_queue_is_empty() );
		TS_ASSERT( SimulateMPI::outgoing_message_queue_is_empty() );

		//std::cout << "Finished third Unit Test" << std::endl;

#endif

	}

	void test_workpool_jd3_master_node_job_failed_bad_input() {
		TS_ASSERT( true );

#ifdef SERIALIZATION
		SimulateMPI::initialize_simulation( 2 );

		SimulateMPI::set_mpi_rank( 1 );
		send_integer_to_node( 0, 1 ); // I have a request
		send_integer_to_node( 0, mpi_work_pool_jd_new_job_request ); // I want a new job
		send_integer_to_node( 0, 1 ); // I have a request
		send_integer_to_node( 0, mpi_work_pool_jd_job_failed_bad_input ); // which is that the job I was running has failed
		send_size_to_node( 0, 1 ); // The job that failed was job #1

		// ask for another job
		send_integer_to_node( 0, 1 ); // I have a request
		send_integer_to_node( 0, mpi_work_pool_jd_new_job_request ); // I want a new job

		SimulateMPI::set_mpi_rank( 0 );
		PoolJQ1OP jq( new PoolJQ1 );
		jq->njobs_ = 1;

		MPIWorkPoolJobDistributor jd;
		try {
			jd.go( jq );
		} catch ( ... ) {
			TS_ASSERT( false /*no exception should be thrown*/ );
		}

		// OK!
		// Now we read the mail we got from node 0

		SimulateMPI::set_mpi_rank( 1 );
		ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 1 that it has a job for it to run", mpi_work_pool_jd_new_job_available );
		std::string ser_larv1 = ts_assert_mpi_buffer_has_string( 0, "node 0 sends node 1 the serialized LarvalJob" );
		LarvalJobOP larval_job1 = deserialize_larval_job( ser_larv1 );
		TS_ASSERT_EQUALS( larval_job1->job_index(), 1 );
		TS_ASSERT_EQUALS( larval_job1->nstruct_index(), 1 );
		TS_ASSERT_EQUALS( larval_job1->nstruct_max(), 1 );
		ts_assert_mpi_buffer_has_sizes( 0, "node 0 sends node 1 an empty list of job result indices", utility::vector1< Size >() );

		ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 1 that there are no jobs left to run", mpi_work_pool_jd_spin_down );

		TS_ASSERT_EQUALS( jq->status_.count( 1 ), 1 );
		TS_ASSERT_EQUALS( jq->status_[ 1 ], jd3_job_status_inputs_were_bad );
		TS_ASSERT_EQUALS( jq->summaries_.count( sp1(1) ), 0 );
		TS_ASSERT_EQUALS( jq->results_->count( sp1(1) ), 0 );

		SimulateMPI::set_mpi_rank( 0 );

		TS_ASSERT( SimulateMPI::incoming_message_queue_is_empty() );
		TS_ASSERT( SimulateMPI::outgoing_message_queue_is_empty() );

		//std::cout << "Finished Fourth Unit Test" << std::endl;

#endif
	}

	void test_workpool_jd3_master_node_job_failed_w_exception() {
		TS_ASSERT( true );

#ifdef SERIALIZATION
		SimulateMPI::initialize_simulation( 2 );

		SimulateMPI::set_mpi_rank( 1 );
		send_integer_to_node( 0, 1 ); // I have a request
		send_integer_to_node( 0, mpi_work_pool_jd_new_job_request ); // I want a new job
		send_integer_to_node( 0, 1 ); // I have a request
		send_integer_to_node( 0, mpi_work_pool_jd_job_failed_w_message ); // which is that the job I was running has failed
		send_size_to_node( 0, 1 ); // The job that failed was job #1
		send_string_to_node( 0, "This is a fake exception message" );

		// ask for another job
		send_integer_to_node( 0, 1 ); // I have a request
		send_integer_to_node( 0, mpi_work_pool_jd_new_job_request ); // I want a new job

		SimulateMPI::set_mpi_rank( 0 );
		PoolJQ1OP jq( new PoolJQ1 );
		jq->njobs_ = 1;

		MPIWorkPoolJobDistributor jd;
		try {
			jd.go( jq );
		} catch ( ... ) {
			TS_ASSERT( false /*no exception should be thrown*/ );
		}

		// OK!
		// Now we read the mail we got from node 0

		SimulateMPI::set_mpi_rank( 1 );
		ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 1 that it has a job for it to run", mpi_work_pool_jd_new_job_available );
		std::string ser_larv1 = ts_assert_mpi_buffer_has_string( 0, "node 0 sends node 1 the serialized LarvalJob" );
		LarvalJobOP larval_job1 = deserialize_larval_job( ser_larv1 );
		TS_ASSERT_EQUALS( larval_job1->job_index(), 1 );
		TS_ASSERT_EQUALS( larval_job1->nstruct_index(), 1 );
		TS_ASSERT_EQUALS( larval_job1->nstruct_max(), 1 );
		ts_assert_mpi_buffer_has_sizes( 0, "node 0 sends node 1 an empty list of job result indices", utility::vector1< Size >() );

		ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 1 that there are no jobs left to run", mpi_work_pool_jd_spin_down );

		TS_ASSERT_EQUALS( jq->status_.count( 1 ), 1 );
		TS_ASSERT_EQUALS( jq->status_[ 1 ], jd3_job_status_failed_w_exception );
		TS_ASSERT_EQUALS( jq->summaries_.count( sp1(1) ), 0 );
		TS_ASSERT_EQUALS( jq->results_->count( sp1(1) ), 0 );

		SimulateMPI::set_mpi_rank( 0 );

		TS_ASSERT( SimulateMPI::incoming_message_queue_is_empty() );
		TS_ASSERT( SimulateMPI::outgoing_message_queue_is_empty() );

		//std::cout << "Finished Fifth Unit Test" << std::endl;

#endif
	}

	void test_workpool_jd3_master_node_job_failed_w_retry_limit_exceeded() {
		TS_ASSERT( true );

#ifdef SERIALIZATION
		SimulateMPI::initialize_simulation( 2 );

		SimulateMPI::set_mpi_rank( 1 );
		send_integer_to_node( 0, 1 ); // I have a request
		send_integer_to_node( 0, mpi_work_pool_jd_new_job_request ); // I want a new job
		send_integer_to_node( 0, 1 ); // I have a request
		send_integer_to_node( 0, mpi_work_pool_jd_job_failed_retry_limit_exceeded ); // which is that the job I was running has failed
		send_size_to_node( 0, 1 ); // The job that failed was job #1

		// ask for another job
		send_integer_to_node( 0, 1 ); // I have a request
		send_integer_to_node( 0, mpi_work_pool_jd_new_job_request ); // I want a new job

		SimulateMPI::set_mpi_rank( 0 );
		PoolJQ1OP jq( new PoolJQ1 );
		jq->njobs_ = 1;

		MPIWorkPoolJobDistributor jd;
		try {
			jd.go( jq );
		} catch ( ... ) {
			TS_ASSERT( false /*no exception should be thrown*/ );
		}

		// OK!
		// Now we read the mail we got from node 0

		SimulateMPI::set_mpi_rank( 1 );
		ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 1 that it has a job for it to run", mpi_work_pool_jd_new_job_available );
		std::string ser_larv1 = ts_assert_mpi_buffer_has_string( 0, "node 0 sends node 1 the serialized LarvalJob" );
		LarvalJobOP larval_job1 = deserialize_larval_job( ser_larv1 );
		TS_ASSERT_EQUALS( larval_job1->job_index(), 1 );
		TS_ASSERT_EQUALS( larval_job1->nstruct_index(), 1 );
		TS_ASSERT_EQUALS( larval_job1->nstruct_max(), 1 );
		ts_assert_mpi_buffer_has_sizes( 0, "node 0 sends node 1 an empty list of job result indices", utility::vector1< Size >() );

		ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 1 that there are no jobs left to run", mpi_work_pool_jd_spin_down );

		TS_ASSERT_EQUALS( jq->status_.count( 1 ), 1 );
		TS_ASSERT_EQUALS( jq->status_[ 1 ], jd3_job_status_failed_max_retries );
		TS_ASSERT_EQUALS( jq->summaries_.count( sp1(1) ), 0 );
		TS_ASSERT_EQUALS( jq->results_->count( sp1(1) ), 0 );

		SimulateMPI::set_mpi_rank( 0 );

		TS_ASSERT( SimulateMPI::incoming_message_queue_is_empty() );
		TS_ASSERT( SimulateMPI::outgoing_message_queue_is_empty() );

		//std::cout << "Finished Sixth Unit Test" << std::endl;

#endif
	}

	void test_workpool_jd3_worker_node_job_failed_do_not_retry() {
		TS_ASSERT( true );


#ifdef SERIALIZATION
		SimulateMPI::initialize_simulation( 2 );

		SimulateMPI::set_mpi_rank( 0 );

		send_integer_to_node( 1, mpi_work_pool_jd_new_job_available );
		send_string_to_node( 1, serialized_larval_job( 1, 1, 1 ) );
		send_sizes_to_node( 1, utility::vector1< Size >() );

		// Now tell node 1 to spin dow
		send_integer_to_node( 1, mpi_work_pool_jd_spin_down );

		try {
			SimulateMPI::set_mpi_rank( 1 );
			PoolJQ2OP jq2( new PoolJQ2 );
			jq2->status_ = jd3_job_status_failed_do_not_retry;
			MPIWorkPoolJobDistributor jd;
			jd.go( jq2 );
		} catch ( ... ) {
			std::cerr << "Exception thrown from worker node in a unit test that should not have thrown an exception!" << std::endl;
			TS_ASSERT( false );
			return;
		}

		// NOW let's make sure that node 1 has been sending its messages the way it should have
		SimulateMPI::set_mpi_rank( 0 );
		// Node 1 starts by requesting a job
		ts_assert_mpi_buffer_has_integer( 1, "node 1 says to node 0: I have a request A", 1 );
		ts_assert_mpi_buffer_has_integer( 1, "node 1 requests a new job of node 0", mpi_work_pool_jd_new_job_request );

		// Now node 1 should have said that the job failed
		ts_assert_mpi_buffer_has_integer( 1, "node 1 says to node 0: I have a request B", 1 );
		ts_assert_mpi_buffer_has_integer( 1, "node 1 says that the job failed with a status 'do not retry'", mpi_work_pool_jd_job_failed_do_not_retry );
		ts_assert_mpi_buffer_has_size( 1, "node 1 says that the job which failed had index 1", 1 );

		// Node 1 requests another job
		ts_assert_mpi_buffer_has_integer( 1, "node 1 says to node 0: I have a request C", 1 );
		ts_assert_mpi_buffer_has_integer( 1, "node 1 requests another job of node 0", mpi_work_pool_jd_new_job_request );

		SimulateMPI::set_mpi_rank( 1 );

		TS_ASSERT( SimulateMPI::incoming_message_queue_is_empty() );
		TS_ASSERT( SimulateMPI::outgoing_message_queue_is_empty() );

		//std::cout << "Finished Seventh Unit Test" << std::endl;

#endif
	}

	void test_workpool_jd3_worker_node_job_failed_bad_input() {
		TS_ASSERT( true );


#ifdef SERIALIZATION
		SimulateMPI::initialize_simulation( 2 );

		SimulateMPI::set_mpi_rank( 0 );

		send_integer_to_node( 1, mpi_work_pool_jd_new_job_available );
		send_string_to_node( 1, serialized_larval_job( 1, 1, 1 ) );
		send_sizes_to_node( 1, utility::vector1< Size >() );

		// Now tell node 1 to spin dow
		send_integer_to_node( 1, mpi_work_pool_jd_spin_down );

		try {
			SimulateMPI::set_mpi_rank( 1 );
			PoolJQ2OP jq2( new PoolJQ2 );
			jq2->status_ = jd3_job_status_inputs_were_bad;
			MPIWorkPoolJobDistributor jd;
			jd.go( jq2 );
		} catch ( ... ) {
			std::cerr << "Exception thrown from worker node in a unit test that should not have thrown an exception!" << std::endl;
			TS_ASSERT( false );
			return;
		}

		// NOW let's make sure that node 1 has been sending its messages the way it should have
		SimulateMPI::set_mpi_rank( 0 );
		// Node 1 starts by requesting a job
		ts_assert_mpi_buffer_has_integer( 1, "node 1 says to node 0: I have a request A", 1 );
		ts_assert_mpi_buffer_has_integer( 1, "node 1 requests a new job of node 0", mpi_work_pool_jd_new_job_request );

		// Now node 1 should have said that the job failed
		ts_assert_mpi_buffer_has_integer( 1, "node 1 says to node 0: I have a request B", 1 );
		ts_assert_mpi_buffer_has_integer( 1, "node 1 says that the job failed with a status 'do not retry'", mpi_work_pool_jd_job_failed_bad_input );
		ts_assert_mpi_buffer_has_size( 1, "node 1 says that the job which failed had index 1", 1 );

		// Node 1 requests another job
		ts_assert_mpi_buffer_has_integer( 1, "node 1 says to node 0: I have a request C", 1 );
		ts_assert_mpi_buffer_has_integer( 1, "node 1 requests another job of node 0", mpi_work_pool_jd_new_job_request );

		SimulateMPI::set_mpi_rank( 1 );

		TS_ASSERT( SimulateMPI::incoming_message_queue_is_empty() );
		TS_ASSERT( SimulateMPI::outgoing_message_queue_is_empty() );

		//std::cout << "Finished Eigth Unit Test" << std::endl;

#endif
	}

	void test_workpool_jd3_worker_node_job_failed_w_exception() {
		TS_ASSERT( true );


#ifdef SERIALIZATION
		SimulateMPI::initialize_simulation( 2 );

		SimulateMPI::set_mpi_rank( 0 );

		send_integer_to_node( 1, mpi_work_pool_jd_new_job_available );
		send_string_to_node( 1, serialized_larval_job( 1, 1, 1 ) );
		send_sizes_to_node( 1, utility::vector1< Size >() );

		// Now tell node 1 to spin dow
		send_integer_to_node( 1, mpi_work_pool_jd_spin_down );

		try {
			SimulateMPI::set_mpi_rank( 1 );
			PoolJQ2OP jq2( new PoolJQ2 );
			jq2->throw_ = true;
			MPIWorkPoolJobDistributor jd;
			jd.go( jq2 );
		} catch ( ... ) {
			std::cerr << "Exception thrown from worker node in a unit test that should not have thrown an exception!" << std::endl;
			TS_ASSERT( false );
			return;
		}

		// NOW let's make sure that node 1 has been sending its messages the way it should have
		SimulateMPI::set_mpi_rank( 0 );
		// Node 1 starts by requesting a job
		ts_assert_mpi_buffer_has_integer( 1, "node 1 says to node 0: I have a request A", 1 );
		ts_assert_mpi_buffer_has_integer( 1, "node 1 requests a new job of node 0", mpi_work_pool_jd_new_job_request );

		// Now node 1 should have said that the job failed
		ts_assert_mpi_buffer_has_integer( 1, "node 1 says to node 0: I have a request B", 1 );
		ts_assert_mpi_buffer_has_integer( 1, "node 1 says that the job failed with exception message'", mpi_work_pool_jd_job_failed_w_message );
		ts_assert_mpi_buffer_has_size( 1, "node 1 says that the job which failed had index 1", 1 );
		ts_assert_mpi_buffer_has_string( 1, "node 1 hands the exception message to node 0", "PoolMnPJob2 exception" );

		// Node 1 requests another job
		ts_assert_mpi_buffer_has_integer( 1, "node 1 says to node 0: I have a request C", 1 );
		ts_assert_mpi_buffer_has_integer( 1, "node 1 requests another job of node 0", mpi_work_pool_jd_new_job_request );

		SimulateMPI::set_mpi_rank( 1 );

		TS_ASSERT( SimulateMPI::incoming_message_queue_is_empty() );
		TS_ASSERT( SimulateMPI::outgoing_message_queue_is_empty() );

		//std::cout << "Finished Ninth Unit Test" << std::endl;

#endif
	}

	void test_workpool_jd3_worker_node_job_failed_w_bad_inputs_exception() {
		TS_ASSERT( true );


#ifdef SERIALIZATION
		SimulateMPI::initialize_simulation( 2 );

		SimulateMPI::set_mpi_rank( 0 );

		send_integer_to_node( 1, mpi_work_pool_jd_new_job_available );
		send_string_to_node( 1, serialized_larval_job( 1, 1, 1 ) );
		send_sizes_to_node( 1, utility::vector1< Size >() );

		// Now tell node 1 to spin dow
		send_integer_to_node( 1, mpi_work_pool_jd_spin_down );

		try {
			SimulateMPI::set_mpi_rank( 1 );
			PoolJQ2OP jq2( new PoolJQ2 );
			jq2->throw_bad_inputs_ = true;
			MPIWorkPoolJobDistributor jd;
			jd.go( jq2 );
		} catch ( ... ) {
			std::cerr << "Exception thrown from worker node in a unit test that should not have thrown an exception!" << std::endl;
			TS_ASSERT( false );
			return;
		}

		// NOW let's make sure that node 1 has been sending its messages the way it should have
		SimulateMPI::set_mpi_rank( 0 );
		// Node 1 starts by requesting a job
		ts_assert_mpi_buffer_has_integer( 1, "node 1 says to node 0: I have a request A", 1 );
		ts_assert_mpi_buffer_has_integer( 1, "node 1 requests a new job of node 0", mpi_work_pool_jd_new_job_request );

		// Now node 1 should have said that the job failed
		ts_assert_mpi_buffer_has_integer( 1, "node 1 says to node 0: I have a request B", 1 );
		ts_assert_mpi_buffer_has_integer( 1, "node 1 says that the job failed with exception message'", mpi_work_pool_jd_job_failed_w_message );
		ts_assert_mpi_buffer_has_size( 1, "node 1 says that the job which failed had index 1", 1 );
		ts_assert_mpi_buffer_has_string( 1, "node 1 hands the exception message to node 0", "PoolMnPJob2 bad input exception" );

		// Node 1 requests another job
		ts_assert_mpi_buffer_has_integer( 1, "node 1 says to node 0: I have a request C", 1 );
		ts_assert_mpi_buffer_has_integer( 1, "node 1 requests another job of node 0", mpi_work_pool_jd_new_job_request );

		SimulateMPI::set_mpi_rank( 1 );

		TS_ASSERT( SimulateMPI::incoming_message_queue_is_empty() );
		TS_ASSERT( SimulateMPI::outgoing_message_queue_is_empty() );

		//std::cout << "Finished Tenth Unit Test" << std::endl;

#endif
	}

	void test_workpool_jd3_worker_node_job_failed_w_retry_limit_exceeded() {
		TS_ASSERT( true );


#ifdef SERIALIZATION
		SimulateMPI::initialize_simulation( 2 );

		SimulateMPI::set_mpi_rank( 0 );

		send_integer_to_node( 1, mpi_work_pool_jd_new_job_available );
		LarvalJobOP multi_pass_larval_job = create_larval_job( 1, 1, 1 );
		multi_pass_larval_job->retry_limit( 5 ); // try this job multiple times
		send_string_to_node( 1, serialized_T( multi_pass_larval_job ) );
		send_sizes_to_node( 1, utility::vector1< Size >() );

		// Now tell node 1 to spin dow
		send_integer_to_node( 1, mpi_work_pool_jd_spin_down );

		try {
			SimulateMPI::set_mpi_rank( 1 );
			PoolJQ2OP jq2( new PoolJQ2 );
			jq2->status_ = jd3_job_status_failed_retry;
			MPIWorkPoolJobDistributor jd;
			jd.go( jq2 );
		} catch ( ... ) {
			std::cerr << "Exception thrown from worker node in a unit test that should not have thrown an exception!" << std::endl;
			TS_ASSERT( false );
			return;
		}

		// NOW let's make sure that node 1 has been sending its messages the way it should have
		SimulateMPI::set_mpi_rank( 0 );
		// Node 1 starts by requesting a job
		ts_assert_mpi_buffer_has_integer( 1, "node 1 says to node 0: I have a request A", 1 );
		ts_assert_mpi_buffer_has_integer( 1, "node 1 requests a new job of node 0", mpi_work_pool_jd_new_job_request );

		// Now node 1 should have said that the job failed
		ts_assert_mpi_buffer_has_integer( 1, "node 1 says to node 0: I have a request B", 1 );
		ts_assert_mpi_buffer_has_integer( 1, "node 1 says that the job failed after hitting its retry limit'", mpi_work_pool_jd_job_failed_retry_limit_exceeded );
		ts_assert_mpi_buffer_has_size( 1, "node 1 says that the job which failed had index 1", 1 );

		// Node 1 requests another job
		ts_assert_mpi_buffer_has_integer( 1, "node 1 says to node 0: I have a request C", 1 );
		ts_assert_mpi_buffer_has_integer( 1, "node 1 requests another job of node 0", mpi_work_pool_jd_new_job_request );

		SimulateMPI::set_mpi_rank( 1 );

		TS_ASSERT( SimulateMPI::incoming_message_queue_is_empty() );
		TS_ASSERT( SimulateMPI::outgoing_message_queue_is_empty() );

		//std::cout << "Finished Eleventh Unit Test" << std::endl;


#endif
	}

	void test_jd3_workpool_manager_two_rounds_no_archive() {


#ifdef SERIALIZATION

		EnergyJobSummaryOP trpcage_job_summary( new EnergyJobSummary );
		trpcage_job_summary->energy( 1234 );
		utility::vector1< JobSummaryOP > summaries( 1, trpcage_job_summary );
		std::string serialized_trpcage_job_summaries = serialized_job_summaries( summaries );

		PoseJobResultOP trpcage_pose_result( new PoseJobResult );
		trpcage_pose_result->pose(  create_trpcage_ideal_poseop() );

		SimulateMPI::initialize_simulation( 4 );

		// ok, all the nodes at once start requesting jobs from node 0
		SimulateMPI::set_mpi_rank( 1 );
		send_integer_to_node( 0, 1 );
		SimulateMPI::set_mpi_rank( 2 );
		send_integer_to_node( 0, 2 );
		send_integer_to_node( 0, mpi_work_pool_jd_new_job_request );
		SimulateMPI::set_mpi_rank( 3 );
		send_integer_to_node( 0, 3 );

		// let's start the job at node 1
		SimulateMPI::set_mpi_rank( 1 );
		send_integer_to_node( 0, mpi_work_pool_jd_new_job_request );

		// and now the job at node 3
		SimulateMPI::set_mpi_rank( 3 );
		send_integer_to_node( 0, mpi_work_pool_jd_new_job_request );

		// ok -- now let's pretend that node 2 has finished its job
		SimulateMPI::set_mpi_rank( 2 );
		send_integer_to_node( 0, 2 );

		// ok -- before node 2 can finish its two part job-completion process,
		// node 1 will butt in and start its two part job-completion process,
		SimulateMPI::set_mpi_rank( 1 );
		send_integer_to_node( 0, 1 );

		SimulateMPI::set_mpi_rank( 2 );
		send_integer_to_node( 0, mpi_work_pool_jd_job_success );
		send_size_to_node( 0, 2 ); // job_id
		send_size_to_node( 0, 1 ); // num jobs
		// ok -- node 0 is going to tell it to archive it's results on node 0

		send_integer_to_node( 0, 2 );
		send_integer_to_node( 0, mpi_work_pool_jd_archive_job_result );
		send_size_to_node( 0, 2 ); // job id
		send_size_to_node( 0, 1 ); // result index
		send_string_to_node( 0, serialized_larval_job_and_job_result( 3, 2, 2, trpcage_pose_result ));

		SimulateMPI::set_mpi_rank( 1 );
		send_integer_to_node( 0, mpi_work_pool_jd_job_success );
		send_size_to_node( 0, 1 ); // job id
		send_size_to_node( 0, 1 ); // num jobs

		send_integer_to_node( 0, 1 );
		send_integer_to_node( 0, mpi_work_pool_jd_archive_job_result );
		send_size_to_node( 0, 1 ); // job id
		send_size_to_node( 0, 1 ); // result index
		send_string_to_node( 0, serialized_larval_job_and_job_result( 3, 1, 1, trpcage_pose_result ));

		SimulateMPI::set_mpi_rank( 2 );
		send_integer_to_node( 0, 2 );
		send_integer_to_node( 0, mpi_work_pool_jd_job_success_and_archival_complete );
		send_size_to_node( 0, 2 ); // job id
		send_integer_to_node( 0, jd3_job_status_success );
		send_string_to_node( 0, serialized_trpcage_job_summaries );

		SimulateMPI::set_mpi_rank( 3 );
		send_integer_to_node( 0, 3 );
		send_integer_to_node( 0, mpi_work_pool_jd_job_success );
		send_size_to_node( 0, 3 ); // job id
		send_size_to_node( 0, 1 ); // num results


		send_integer_to_node( 0, 3 );
		send_integer_to_node( 0, mpi_work_pool_jd_archive_job_result );
		send_size_to_node( 0, 3 ); // job id
		send_size_to_node( 0, 1 ); // result index
		send_string_to_node( 0, serialized_larval_job_and_job_result( 3, 3, 3, trpcage_pose_result ));

		SimulateMPI::set_mpi_rank( 1 );
		send_integer_to_node( 0, 1 );
		send_integer_to_node( 0, mpi_work_pool_jd_job_success_and_archival_complete );
		send_size_to_node( 0, 1 ); // job id
		send_integer_to_node( 0, jd3_job_status_success );
		send_string_to_node( 0, serialized_trpcage_job_summaries );

		// Now node 1 asks for a new job
		send_integer_to_node( 0, 1 );
		send_integer_to_node( 0, mpi_work_pool_jd_new_job_request );

		SimulateMPI::set_mpi_rank( 2 );
		// Now node 2 asks for a new job
		send_integer_to_node( 0, 2 );
		send_integer_to_node( 0, mpi_work_pool_jd_new_job_request );


		SimulateMPI::set_mpi_rank( 3 );
		send_integer_to_node( 0, 3 );
		send_integer_to_node( 0, mpi_work_pool_jd_job_success_and_archival_complete );
		send_size_to_node( 0, 3 ); // job id
		send_integer_to_node( 0, jd3_job_status_success );
		send_string_to_node( 0, serialized_trpcage_job_summaries );

		// Now node 3 asks for a new job
		send_integer_to_node( 0, 3 );
		send_integer_to_node( 0, mpi_work_pool_jd_new_job_request );

		SimulateMPI::set_mpi_rank( 1 );

		// now retrieve the job result for job 1
		send_integer_to_node( 0, 1 );
		send_integer_to_node( 0, mpi_work_pool_jd_retrieve_job_result );
		send_size_to_node( 0, 1 ); // retrieve the result from job #1
		send_size_to_node( 0, 1 ); // result #1 from job #1

		SimulateMPI::set_mpi_rank( 2 );

		// now retrieve the job result for job 1
		send_integer_to_node( 0, 2 );
		send_integer_to_node( 0, mpi_work_pool_jd_retrieve_job_result );
		send_size_to_node( 0, 1 ); // retrieve the result from job #1
		send_size_to_node( 0, 1 ); // result #1 from job #1

		SimulateMPI::set_mpi_rank( 3 );

		// now retrieve the job result for job 1
		send_integer_to_node( 0, 3 );
		send_integer_to_node( 0, mpi_work_pool_jd_retrieve_job_result );
		send_size_to_node( 0, 1 ); // retrieve the result from job #1
		send_size_to_node( 0, 1 ); // result #1 from job #1

		// Now the second round of job results start trickling in

		SimulateMPI::set_mpi_rank( 1 );

		send_integer_to_node( 0, 1 );
		send_integer_to_node( 0, mpi_work_pool_jd_job_success );
		send_size_to_node( 0, 4 ); // job id
		send_size_to_node( 0, 1 ); // num results

		send_integer_to_node( 0, 1 );
		send_integer_to_node( 0, mpi_work_pool_jd_archive_job_result );
		send_size_to_node( 0, 4 ); // job id
		send_size_to_node( 0, 1 ); // result index
		send_string_to_node( 0, serialized_larval_job_and_job_result( 3, 1, 4, trpcage_pose_result ));

		send_integer_to_node( 0, 1 );
		send_integer_to_node( 0, mpi_work_pool_jd_job_success_and_archival_complete );
		send_size_to_node( 0, 4 ); // job id
		send_integer_to_node( 0, jd3_job_status_success );
		send_string_to_node( 0, serialized_trpcage_job_summaries );

		send_integer_to_node( 0, 1 );
		send_integer_to_node( 0, mpi_work_pool_jd_new_job_request );

		SimulateMPI::set_mpi_rank( 3 );
		send_integer_to_node( 0, 3 );
		send_integer_to_node( 0, mpi_work_pool_jd_job_success );
		send_size_to_node( 0, 6 ); // job id
		send_size_to_node( 0, 1 ); // num results

		send_integer_to_node( 0, 3 );
		send_integer_to_node( 0, mpi_work_pool_jd_archive_job_result );
		send_size_to_node( 0, 6 ); // job id
		send_size_to_node( 0, 1 ); // result index
		send_string_to_node( 0, serialized_larval_job_and_job_result( 3, 3, 6, trpcage_pose_result ));

		send_integer_to_node( 0, 3 );
		send_integer_to_node( 0, mpi_work_pool_jd_job_success_and_archival_complete );
		send_size_to_node( 0, 6 ); // job id
		send_integer_to_node( 0, jd3_job_status_success );
		send_string_to_node( 0, serialized_trpcage_job_summaries );

		send_integer_to_node( 0, 3 );
		send_integer_to_node( 0, mpi_work_pool_jd_new_job_request );

		SimulateMPI::set_mpi_rank( 2 );
		send_integer_to_node( 0, 2 );
		send_integer_to_node( 0, mpi_work_pool_jd_job_success );
		send_size_to_node( 0, 5 ); // job id
		send_size_to_node( 0, 1 ); // num results

		send_integer_to_node( 0, 2 );
		send_integer_to_node( 0, mpi_work_pool_jd_archive_job_result );
		send_size_to_node( 0, 5 ); // job id
		send_size_to_node( 0, 1 ); // result index
		send_string_to_node( 0, serialized_larval_job_and_job_result( 3, 2, 5, trpcage_pose_result ));

		send_integer_to_node( 0, 2 );
		send_integer_to_node( 0, mpi_work_pool_jd_job_success_and_archival_complete );
		send_size_to_node( 0, 5 ); // job id
		send_integer_to_node( 0, jd3_job_status_success );
		send_string_to_node( 0, serialized_trpcage_job_summaries );

		send_integer_to_node( 0, 2 );
		send_integer_to_node( 0, mpi_work_pool_jd_new_job_request );

		SimulateMPI::set_mpi_rank( 1 );
		//  Node 1 has output the first result it was given, so it asks for a new job
		send_integer_to_node( 0, 1 );
		send_integer_to_node( 0, mpi_work_pool_jd_new_job_request );

		// Node 1 has output the second result it was given, so it asks for a new job
		send_integer_to_node( 0, 1 );
		send_integer_to_node( 0, mpi_work_pool_jd_new_job_request );

		// Node 1 has output the third result it was given, so it asks for a new job
		// but this time, node 0 will tell it that there are no jobs remaining
		send_integer_to_node( 0, 1 );
		send_integer_to_node( 0, mpi_work_pool_jd_new_job_request );


		//OK! Now create a PoolJQ3 and let 'er rip
		SimulateMPI::set_mpi_rank( 0 );
		PoolJQ3OP jq3( new PoolJQ3 );
		MPIWorkPoolJobDistributor jd;
		try {
			jd.go( jq3 );
		} catch ( ... ) {
			TS_ASSERT( false /*no exception should be thrown*/ );
		}

		// OK!
		// Now we read the mail we got from node 0

		SimulateMPI::set_mpi_rank( 1 );
		ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 1 that it has a job for it to run", mpi_work_pool_jd_new_job_available );
		std::string ser_larv1 = ts_assert_mpi_buffer_has_string( 0, "node 0 sends node 1 the serialized LarvalJob" );
		LarvalJobOP larval_job1 = deserialize_larval_job( ser_larv1 );
		TS_ASSERT_EQUALS( larval_job1->job_index(), 1 );
		TS_ASSERT_EQUALS( larval_job1->nstruct_index(), 1 );
		TS_ASSERT_EQUALS( larval_job1->nstruct_max(), 3 );
		ts_assert_mpi_buffer_has_sizes( 0, "node 0 sends node 1 an empty list of job result indices", utility::vector1< Size >() );

		ts_assert_mpi_buffer_has_integers( 0, "node 0 tells node 1 to archive its results on node 0", utility::vector1< int >( 1, 0 ) );
		ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 1 that archival completed", mpi_work_pool_jd_archival_completed );

		ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 1 that it has another job for it to run", mpi_work_pool_jd_new_job_available );
		std::string ser_larv4 = ts_assert_mpi_buffer_has_string( 0, "node 0 sends node 1 the serialized LarvalJob" );
		LarvalJobOP larval_job4 = deserialize_larval_job( ser_larv4 );
		TS_ASSERT_EQUALS( larval_job4->job_index(), 4 );
		TS_ASSERT_EQUALS( larval_job4->nstruct_index(), 1 );
		TS_ASSERT_EQUALS( larval_job4->nstruct_max(), 3 );
		TS_ASSERT_EQUALS( larval_job4->input_job_result_indices().size(), 1 );
		TS_ASSERT_EQUALS( larval_job4->input_job_result_indices()[ 1 ].first, 1 );
		TS_ASSERT_EQUALS( larval_job4->input_job_result_indices()[ 1 ].second, 1 );
		ts_assert_mpi_buffer_has_sizes( 0, "node 0 sends node 1 a vector1 of job result indices with one element whose value is 0", utility::vector1< Size >( 1, 0 ) );

		ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 1 that it has the job result it was expecting", mpi_work_pool_jd_job_result_retrieved );
		std::string ser_job_res1a = ts_assert_mpi_buffer_has_string( 0, "node 0 sends the serialized larval-job and job-result from job 1" );
		MPIWorkPoolJobDistributor::LarvalJobAndResult job_res1a = deserialized_larval_job_and_job_result( ser_job_res1a );
		TS_ASSERT( job_res1a.first );
		TS_ASSERT( job_res1a.second );
		TS_ASSERT_EQUALS( job_res1a.first->job_index(), 1 );
		TS_ASSERT_EQUALS( job_res1a.first->nstruct_index(), 1 );
		TS_ASSERT_EQUALS( job_res1a.first->nstruct_max(), 3 );
		TS_ASSERT_EQUALS( utility::pointer::dynamic_pointer_cast< PoseJobResult > ( job_res1a.second )->pose()->total_residue(), 20 );

		ts_assert_mpi_buffer_has_integers( 0, "node 0 tells node 1 to archive its results on node 0", utility::vector1< int >( 1, 0 ) );
		ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 1 that archival completed", mpi_work_pool_jd_archival_completed );

		{ // scope -- job4 output
			ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 1 to output job 2", mpi_work_pool_jd_accept_and_output_job_result );
			std::string output_spec4_string = ts_assert_mpi_buffer_has_string( 0, "node 0 sends the serialized output spec #4" );
			OutputSpecificationOP output_spec4 = deserialized_output_specification( output_spec4_string );
			TS_ASSERT_EQUALS( output_spec4->result_id().first, 4 );
			TS_ASSERT_EQUALS( output_spec4->result_id().second, 1 );
			TS_ASSERT_EQUALS( output_spec4->output_index().primary_output_index, 1 ); // probably shouldn't be 1, probably should be 4?
			TS_ASSERT_EQUALS( output_spec4->output_index().n_primary_outputs, 1000 );
			TS_ASSERT_EQUALS( output_spec4->output_index().secondary_output_index, 1 );
			TS_ASSERT_EQUALS( output_spec4->output_index().n_secondary_outputs, 1 );
			std::string job_result4_string = ts_assert_mpi_buffer_has_string( 0, "node 0 sends the serialized job-result-and-larval-job-pair string" );
			MPIWorkPoolJobDistributor::LarvalJobAndResult job_res4 = deserialized_larval_job_and_job_result( job_result4_string );
			TS_ASSERT( job_res4.first );
			TS_ASSERT( job_res4.second );
			TS_ASSERT_EQUALS( job_res4.first->job_index(), 4 );
			TS_ASSERT_EQUALS( job_res4.first->nstruct_index(), 1 );
			TS_ASSERT_EQUALS( job_res4.first->nstruct_max(), 3 );
			TS_ASSERT_EQUALS( utility::pointer::dynamic_pointer_cast< PoseJobResult > ( job_res4.second )->pose()->total_residue(), 20 );
		}


		{ // scope -- job 5 output
			ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 1 to output job 5", mpi_work_pool_jd_accept_and_output_job_result );
			std::string output_spec5_string = ts_assert_mpi_buffer_has_string( 0, "node 0 sends the serialized output spec #5" );
			OutputSpecificationOP output_spec5 = deserialized_output_specification( output_spec5_string );
			TS_ASSERT_EQUALS( output_spec5->result_id().first, 5 );
			TS_ASSERT_EQUALS( output_spec5->result_id().second, 1 );
			TS_ASSERT_EQUALS( output_spec5->output_index().primary_output_index, 1 );
			TS_ASSERT_EQUALS( output_spec5->output_index().n_primary_outputs, 1000 );
			TS_ASSERT_EQUALS( output_spec5->output_index().secondary_output_index, 1 );
			TS_ASSERT_EQUALS( output_spec5->output_index().n_secondary_outputs, 1 );
			std::string job_result5_string = ts_assert_mpi_buffer_has_string( 0, "node 0 sends the serialized job-result-and-larval-job-pair string" );
			MPIWorkPoolJobDistributor::LarvalJobAndResult job_res5 = deserialized_larval_job_and_job_result( job_result5_string );
			TS_ASSERT( job_res5.first );
			TS_ASSERT( job_res5.second );
			TS_ASSERT_EQUALS( job_res5.first->job_index(), 5 );
			TS_ASSERT_EQUALS( job_res5.first->nstruct_index(), 2 );
			TS_ASSERT_EQUALS( job_res5.first->nstruct_max(), 3 );
			TS_ASSERT_EQUALS( utility::pointer::dynamic_pointer_cast< PoseJobResult > ( job_res5.second )->pose()->total_residue(), 20 );
		}

		{ // scope job 6 output
			ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 1 to output job 6", mpi_work_pool_jd_accept_and_output_job_result );
			std::string output_spec6_string = ts_assert_mpi_buffer_has_string( 0, "node 0 sends the serialized output spec #6" );
			OutputSpecificationOP output_spec6 = deserialized_output_specification( output_spec6_string );
			TS_ASSERT_EQUALS( output_spec6->result_id().first, 6 );
			TS_ASSERT_EQUALS( output_spec6->result_id().second, 1 );
			TS_ASSERT_EQUALS( output_spec6->output_index().primary_output_index, 1 );
			TS_ASSERT_EQUALS( output_spec6->output_index().n_primary_outputs, 1000 );
			TS_ASSERT_EQUALS( output_spec6->output_index().secondary_output_index, 1 );
			TS_ASSERT_EQUALS( output_spec6->output_index().n_secondary_outputs, 1 );
			std::string job_result6_string = ts_assert_mpi_buffer_has_string( 0, "node 0 sends the serialized job-result-and-larval-job-pair string" );
			MPIWorkPoolJobDistributor::LarvalJobAndResult job_res6 = deserialized_larval_job_and_job_result( job_result6_string );
			TS_ASSERT( job_res6.first );
			TS_ASSERT( job_res6.second );
			TS_ASSERT_EQUALS( job_res6.first->job_index(), 6 );
			TS_ASSERT_EQUALS( job_res6.first->nstruct_index(), 3 );
			TS_ASSERT_EQUALS( job_res6.first->nstruct_max(), 3 );
			TS_ASSERT_EQUALS( utility::pointer::dynamic_pointer_cast< PoseJobResult > ( job_res6.second )->pose()->total_residue(), 20 );
		} // scope



		ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 1 that there are no jobs left to run", mpi_work_pool_jd_spin_down );

		SimulateMPI::set_mpi_rank( 2 );
		ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 2 that it has a job for it to run", mpi_work_pool_jd_new_job_available );
		std::string ser_larv2 = ts_assert_mpi_buffer_has_string( 0, "node 0 sends node 2 the serialized LarvalJob" );
		LarvalJobOP larval_job2 = deserialize_larval_job( ser_larv2 );
		TS_ASSERT_EQUALS( larval_job2->job_index(), 2 );
		TS_ASSERT_EQUALS( larval_job2->nstruct_index(), 2 );
		TS_ASSERT_EQUALS( larval_job2->nstruct_max(), 3 );
		ts_assert_mpi_buffer_has_sizes( 0, "node 0 sends node 2 an empty list of job result indices", utility::vector1< Size >() );

		ts_assert_mpi_buffer_has_integers( 0, "node 0 tells node 2 to archive its results on node 0", utility::vector1< int >( 1, 0 ) );
		ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 2 that archival completed", mpi_work_pool_jd_archival_completed );

		ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 2 that it has another job for it to run", mpi_work_pool_jd_new_job_available );
		std::string ser_larv5 = ts_assert_mpi_buffer_has_string( 0, "node 0 sends node 2 the serialized LarvalJob" );
		LarvalJobOP larval_job5 = deserialize_larval_job( ser_larv5 );
		TS_ASSERT_EQUALS( larval_job5->job_index(), 5 );
		TS_ASSERT_EQUALS( larval_job5->nstruct_index(), 2 );
		TS_ASSERT_EQUALS( larval_job5->nstruct_max(), 3 );
		TS_ASSERT_EQUALS( larval_job5->input_job_result_indices().size(), 1 );
		TS_ASSERT_EQUALS( larval_job5->input_job_result_indices()[ 1 ].first, 1 );
		TS_ASSERT_EQUALS( larval_job5->input_job_result_indices()[ 1 ].second, 1 );
		ts_assert_mpi_buffer_has_sizes( 0, "node 0 sends node 2 a vector1 of job result indices with one element whose value is 0", utility::vector1< Size >( 1, 0 ) );

		ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 2 that it has the job result it was expecting", mpi_work_pool_jd_job_result_retrieved );
		std::string ser_job_res1b = ts_assert_mpi_buffer_has_string( 0, "node 0 sends the serialized larval-job and job-result from job 1" );
		MPIWorkPoolJobDistributor::LarvalJobAndResult job_res1b = deserialized_larval_job_and_job_result( ser_job_res1b );
		TS_ASSERT( job_res1b.first );
		TS_ASSERT( job_res1b.second );
		TS_ASSERT_EQUALS( job_res1b.first->job_index(), 1 );
		TS_ASSERT_EQUALS( job_res1b.first->nstruct_index(), 1 );
		TS_ASSERT_EQUALS( job_res1b.first->nstruct_max(), 3 );
		TS_ASSERT_EQUALS( utility::pointer::dynamic_pointer_cast< PoseJobResult > ( job_res1b.second )->pose()->total_residue(), 20 );

		ts_assert_mpi_buffer_has_integers( 0, "node 0 tells node 2 to archive its results on node 0", utility::vector1< int >( 1, 0 ) );
		ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 2 that archival completed", mpi_work_pool_jd_archival_completed );

		ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 2 that there are no jobs left to run", mpi_work_pool_jd_spin_down );

		SimulateMPI::set_mpi_rank( 3 );
		ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 3 that it has a job for it to run", mpi_work_pool_jd_new_job_available );
		std::string ser_larv3 = ts_assert_mpi_buffer_has_string( 0, "node 0 sends node 3 the serialized LarvalJob" );
		LarvalJobOP larval_job3 = deserialize_larval_job( ser_larv3 );
		TS_ASSERT_EQUALS( larval_job3->job_index(), 3 );
		TS_ASSERT_EQUALS( larval_job3->nstruct_index(), 3 );
		TS_ASSERT_EQUALS( larval_job3->nstruct_max(), 3 );
		ts_assert_mpi_buffer_has_sizes( 0, "node 0 sends node 3 an empty list of job result indices", utility::vector1< Size >() );

		ts_assert_mpi_buffer_has_integers( 0, "node 0 tells node 3 to archive its results on node 0", utility::vector1< int >( 1, 0 ) );
		ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 3 that archival completed", mpi_work_pool_jd_archival_completed );

		ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 3 that it has another job for it to run", mpi_work_pool_jd_new_job_available );
		std::string ser_larv6 = ts_assert_mpi_buffer_has_string( 0, "node 0 sends node 3 the serialized LarvalJob" );
		LarvalJobOP larval_job6 = deserialize_larval_job( ser_larv6 );
		TS_ASSERT_EQUALS( larval_job6->job_index(), 6 );
		TS_ASSERT_EQUALS( larval_job6->nstruct_index(), 3 );
		TS_ASSERT_EQUALS( larval_job6->nstruct_max(), 3 );
		TS_ASSERT_EQUALS( larval_job6->input_job_result_indices().size(), 1 );
		TS_ASSERT_EQUALS( larval_job6->input_job_result_indices()[ 1 ].first, 1 );
		TS_ASSERT_EQUALS( larval_job6->input_job_result_indices()[ 1 ].second, 1 );
		ts_assert_mpi_buffer_has_sizes( 0, "node 0 sends node 3 a vector1 of job result indices with one element whose value is 0", utility::vector1< Size >( 1, 0 ) );

		ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 3 that it has the job result it was expecting", mpi_work_pool_jd_job_result_retrieved );
		std::string ser_job_res1c = ts_assert_mpi_buffer_has_string( 0, "node 0 sends the serialized larval-job and job-result from job 1" );
		MPIWorkPoolJobDistributor::LarvalJobAndResult job_res1c = deserialized_larval_job_and_job_result( ser_job_res1b );
		TS_ASSERT( job_res1c.first );
		TS_ASSERT( job_res1c.second );
		TS_ASSERT_EQUALS( job_res1c.first->job_index(), 1 );
		TS_ASSERT_EQUALS( job_res1c.first->nstruct_index(), 1 );
		TS_ASSERT_EQUALS( job_res1c.first->nstruct_max(), 3 );
		TS_ASSERT_EQUALS( utility::pointer::dynamic_pointer_cast< PoseJobResult > ( job_res1c.second )->pose()->total_residue(), 20 );

		ts_assert_mpi_buffer_has_integers( 0, "node 0 tells node 3 to archive its results on node 0", utility::vector1< int >( 1, 0 ) );
		ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 3 that archival completed", mpi_work_pool_jd_archival_completed );

		ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 3 that there are no jobs left to run", mpi_work_pool_jd_spin_down );

		TS_ASSERT_EQUALS( jq3->status_.count( 1 ), 1 );
		TS_ASSERT_EQUALS( jq3->status_.count( 2 ), 1 );
		TS_ASSERT_EQUALS( jq3->status_.count( 3 ), 1 );
		TS_ASSERT_EQUALS( jq3->status_.count( 4 ), 1 );
		TS_ASSERT_EQUALS( jq3->status_.count( 5 ), 1 );
		TS_ASSERT_EQUALS( jq3->status_.count( 6 ), 1 );
		TS_ASSERT_EQUALS( jq3->status_[ 1 ], jd3_job_status_success );
		TS_ASSERT_EQUALS( jq3->status_[ 2 ], jd3_job_status_success );
		TS_ASSERT_EQUALS( jq3->status_[ 3 ], jd3_job_status_success );
		TS_ASSERT_EQUALS( jq3->status_[ 4 ], jd3_job_status_success );
		TS_ASSERT_EQUALS( jq3->status_[ 5 ], jd3_job_status_success );
		TS_ASSERT_EQUALS( jq3->status_[ 6 ], jd3_job_status_success );

		TS_ASSERT_EQUALS( jq3->summaries_.count( sp1( 1 ) ), 1 );
		TS_ASSERT_EQUALS( jq3->summaries_.count( sp1( 2 ) ), 1 );
		TS_ASSERT_EQUALS( jq3->summaries_.count( sp1( 3 ) ), 1 );
		TS_ASSERT_EQUALS( jq3->summaries_.count( sp1( 4 ) ), 1 );
		TS_ASSERT_EQUALS( jq3->summaries_.count( sp1( 5 ) ), 1 );
		TS_ASSERT_EQUALS( jq3->summaries_.count( sp1( 6 ) ), 1 );
		TS_ASSERT_EQUALS( utility::pointer::dynamic_pointer_cast< EnergyJobSummary > ( jq3->summaries_[ sp1( 1 ) ])->energy(), 1234 );
		TS_ASSERT_EQUALS( utility::pointer::dynamic_pointer_cast< EnergyJobSummary > ( jq3->summaries_[ sp1( 2 ) ])->energy(), 1234 );
		TS_ASSERT_EQUALS( utility::pointer::dynamic_pointer_cast< EnergyJobSummary > ( jq3->summaries_[ sp1( 3 ) ])->energy(), 1234 );
		TS_ASSERT_EQUALS( utility::pointer::dynamic_pointer_cast< EnergyJobSummary > ( jq3->summaries_[ sp1( 4 ) ])->energy(), 1234 );
		TS_ASSERT_EQUALS( utility::pointer::dynamic_pointer_cast< EnergyJobSummary > ( jq3->summaries_[ sp1( 5 ) ])->energy(), 1234 );
		TS_ASSERT_EQUALS( utility::pointer::dynamic_pointer_cast< EnergyJobSummary > ( jq3->summaries_[ sp1( 6 ) ])->energy(), 1234 );

		TS_ASSERT_EQUALS( jq3->results_->count( sp1( 1 ) ), 0 );
		TS_ASSERT_EQUALS( jq3->results_->count( sp1( 2 ) ), 0 );
		TS_ASSERT_EQUALS( jq3->results_->count( sp1( 3 ) ), 0 );
		TS_ASSERT_EQUALS( jq3->results_->count( sp1( 4 ) ), 0 );
		TS_ASSERT_EQUALS( jq3->results_->count( sp1( 5 ) ), 0 );
		TS_ASSERT_EQUALS( jq3->results_->count( sp1( 6 ) ), 0 );
		//TS_ASSERT_EQUALS( utility::pointer::dynamic_pointer_cast< PoseJobResult > ( (*jq3->results_)[ sp1( 4 ) ])->pose()->total_residue(), 20 );
		//TS_ASSERT_EQUALS( utility::pointer::dynamic_pointer_cast< PoseJobResult > ( (*jq3->results_)[ sp1( 5 ) ])->pose()->total_residue(), 20 );
		//TS_ASSERT_EQUALS( utility::pointer::dynamic_pointer_cast< PoseJobResult > ( (*jq3->results_)[ sp1( 6 ) ])->pose()->total_residue(), 20 );

		SimulateMPI::set_mpi_rank( 0 );

		TS_ASSERT( SimulateMPI::incoming_message_queue_is_empty() );
		TS_ASSERT( SimulateMPI::outgoing_message_queue_is_empty() );

		//std::cout << "Finished Twelfth Unit Test" << std::endl;


#endif
	}

	void test_jd3_workpool_manager_two_rounds_one_archive_no_archiving_on_node_0() {
		TS_ASSERT( true );

#ifdef SERIALIZATION
		core_init_with_additional_options( "-jd3::n_archive_nodes 1 -jd3::do_not_archive_on_node0 -jd3::compress_job_results 0" );

		EnergyJobSummaryOP trpcage_job_summary( new EnergyJobSummary );
		trpcage_job_summary->energy( 1234 );
		utility::vector1< JobSummaryOP > summaries( 1, trpcage_job_summary );
		std::string serialized_trpcage_job_summaries = serialized_job_summaries( summaries );


		PoseJobResultOP trpcage_pose_result( new PoseJobResult );
		trpcage_pose_result->pose(  create_trpcage_ideal_poseop() );

		SimulateMPI::initialize_simulation( 5 );

		// ok, all the nodes at once start requesting jobs from node 0
		SimulateMPI::set_mpi_rank( 2 );
		send_integer_to_node( 0, 2 );
		SimulateMPI::set_mpi_rank( 3 );
		send_integer_to_node( 0, 3 );
		send_integer_to_node( 0, mpi_work_pool_jd_new_job_request );
		SimulateMPI::set_mpi_rank( 4 );
		send_integer_to_node( 0, 4 );

		// let's start the job at node 2
		SimulateMPI::set_mpi_rank( 2 );
		send_integer_to_node( 0, mpi_work_pool_jd_new_job_request );

		// and now the job at node 4
		SimulateMPI::set_mpi_rank( 4 );
		send_integer_to_node( 0, mpi_work_pool_jd_new_job_request );

		// ok -- now let's pretend that node 3 has finished its job
		SimulateMPI::set_mpi_rank( 3 );
		send_integer_to_node( 0, 3 );

		// ok -- before node 3 can finish its two part job-completion process,
		// node 2 will butt in and start its two part job-completion process,
		SimulateMPI::set_mpi_rank( 2 );
		send_integer_to_node( 0, 2 );

		SimulateMPI::set_mpi_rank( 3 );
		send_integer_to_node( 0, mpi_work_pool_jd_job_success );
		send_size_to_node( 0, 2 ); // job_id
		send_size_to_node( 0, 1 ); // num results
		// sent to archive instead -- send_string_to_node( 0, serialized_larval_job_and_job_result( 3, 2, 2, trpcage_pose_result ));

		SimulateMPI::set_mpi_rank( 2 );
		send_integer_to_node( 0, mpi_work_pool_jd_job_success );
		send_size_to_node( 0, 1 ); // job id
		send_size_to_node( 0, 1 ); // num results
		// sent to archive instead -- send_string_to_node( 0, serialized_larval_job_and_job_result( 3, 1, 1, trpcage_pose_result ));

		SimulateMPI::set_mpi_rank( 3 );
		send_integer_to_node( 0, 3 );
		send_integer_to_node( 0, mpi_work_pool_jd_job_success_and_archival_complete );
		send_size_to_node( 0, 2 ); // job id
		send_integer_to_node( 0, jd3_job_status_success );
		send_string_to_node( 0, serialized_trpcage_job_summaries );

		SimulateMPI::set_mpi_rank( 4 );
		send_integer_to_node( 0, 4 );
		send_integer_to_node( 0, mpi_work_pool_jd_job_success );
		send_size_to_node( 0, 3 ); // job id
		send_size_to_node( 0, 1 ); // num results
		// sent to archive instead -- send_string_to_node( 0, serialized_larval_job_and_job_result( 3, 3, 3, trpcage_pose_result ));

		SimulateMPI::set_mpi_rank( 2 );
		send_integer_to_node( 0, 2 );
		send_integer_to_node( 0, mpi_work_pool_jd_job_success_and_archival_complete );
		send_size_to_node( 0, 1 ); // job id
		send_integer_to_node( 0, jd3_job_status_success ); // job id
		send_string_to_node( 0, serialized_trpcage_job_summaries );

		// Now node 2 asks for a new job
		send_integer_to_node( 0, 2 );
		send_integer_to_node( 0, mpi_work_pool_jd_new_job_request );

		SimulateMPI::set_mpi_rank( 3 );
		// Now node 3 asks for a new job
		send_integer_to_node( 0, 3 );
		send_integer_to_node( 0, mpi_work_pool_jd_new_job_request );


		SimulateMPI::set_mpi_rank( 4 );
		send_integer_to_node( 0, 4 );
		send_integer_to_node( 0, mpi_work_pool_jd_job_success_and_archival_complete );
		send_size_to_node( 0, 3 ); // job id
		send_integer_to_node( 0, jd3_job_status_success );
		send_string_to_node( 0, serialized_trpcage_job_summaries );

		// Now node 4 asks for a new job
		send_integer_to_node( 0, 4 );
		send_integer_to_node( 0, mpi_work_pool_jd_new_job_request );

		SimulateMPI::set_mpi_rank( 2 );

		// now retrieve the job result for job 1
		// send_integer_to_node( 0, 2 );
		// send_integer_to_node( 0, mpi_work_pool_jd_retrieve_job_result );
		// send_size_to_node( 0, 1 ); // retrieve the result from job #1

		SimulateMPI::set_mpi_rank( 3 );

		// now retrieve the job result for job 1
		// send_integer_to_node( 0, 3 );
		// send_integer_to_node( 0, mpi_work_pool_jd_retrieve_job_result );
		// send_size_to_node( 0, 1 ); // retrieve the result from job #1

		SimulateMPI::set_mpi_rank( 4 );

		// now retrieve the job result for job 1
		// send_integer_to_node( 0, 4 );
		// send_integer_to_node( 0, mpi_work_pool_jd_retrieve_job_result );
		// send_size_to_node( 0, 1 ); // retrieve the result from job #1

		// Now the second round of job results start trickling in

		SimulateMPI::set_mpi_rank( 2 );

		send_integer_to_node( 0, 2 );
		send_integer_to_node( 0, mpi_work_pool_jd_job_success );
		send_size_to_node( 0, 4 ); // job id
		send_size_to_node( 0, 1 ); // num results
		// sent to archive instead -- send_string_to_node( 0, serialized_larval_job_and_job_result( 3, 1, 4, trpcage_pose_result ));

		send_integer_to_node( 0, 2 );
		send_integer_to_node( 0, mpi_work_pool_jd_job_success_and_archival_complete );
		send_size_to_node( 0, 4 ); // job id
		send_integer_to_node( 0, jd3_job_status_success );
		send_string_to_node( 0, serialized_trpcage_job_summaries );

		send_integer_to_node( 0, 2 );
		send_integer_to_node( 0, mpi_work_pool_jd_new_job_request );

		SimulateMPI::set_mpi_rank( 4 );
		send_integer_to_node( 0, 4 );
		send_integer_to_node( 0, mpi_work_pool_jd_job_success );
		send_size_to_node( 0, 6 ); // job id
		send_size_to_node( 0, 1 ); // num results
		// sent to archive instead -- send_string_to_node( 0, serialized_larval_job_and_job_result( 3, 3, 6, trpcage_pose_result ));

		send_integer_to_node( 0, 4 );
		send_integer_to_node( 0, mpi_work_pool_jd_job_success_and_archival_complete );
		send_size_to_node( 0, 6 ); // job id
		send_integer_to_node( 0, jd3_job_status_success );
		send_string_to_node( 0, serialized_trpcage_job_summaries );

		send_integer_to_node( 0, 4 );
		send_integer_to_node( 0, mpi_work_pool_jd_new_job_request );

		SimulateMPI::set_mpi_rank( 3 );
		send_integer_to_node( 0, 3 );
		send_integer_to_node( 0, mpi_work_pool_jd_job_success );
		send_size_to_node( 0, 5 ); // job id
		send_size_to_node( 0, 1 ); // num results
		// sent to archive instead -- send_string_to_node( 0, serialized_larval_job_and_job_result( 3, 2, 5, trpcage_pose_result ));

		send_integer_to_node( 0, 3 );
		send_integer_to_node( 0, mpi_work_pool_jd_job_success_and_archival_complete );
		send_size_to_node( 0, 5 ); // job id
		send_integer_to_node( 0, jd3_job_status_success );
		send_string_to_node( 0, serialized_trpcage_job_summaries );

		send_integer_to_node( 0, 3 );
		send_integer_to_node( 0, mpi_work_pool_jd_new_job_request );

		SimulateMPI::set_mpi_rank( 1 );
		// ok, now the archive is going to receive a series of job output requests from
		// node 0 and respond by sending back messages that the outputs completed
		send_integer_to_node( 0, 1 );
		send_integer_to_node( 0, mpi_work_pool_jd_output_completed );

		send_integer_to_node( 0, 1 );
		send_integer_to_node( 0, mpi_work_pool_jd_output_completed );

		send_integer_to_node( 0, 1 );
		send_integer_to_node( 0, mpi_work_pool_jd_output_completed );

		//OK! Now create a PoolJQ3 and let 'er rip
		SimulateMPI::set_mpi_rank( 0 );
		PoolJQ3OP jq3( new PoolJQ3 );
		MPIWorkPoolJobDistributor jd;
		try {
			jd.go( jq3 );
		} catch ( ... ) {
			TS_ASSERT( false /*no exception should be thrown*/ );
		}

		// OK!
		// Now we read the mail we got from node 0

		SimulateMPI::set_mpi_rank( 2 );
		ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 2 that it has a job for it to run", mpi_work_pool_jd_new_job_available );
		std::string ser_larv1 = ts_assert_mpi_buffer_has_string( 0, "node 0 sends node 2 the serialized LarvalJob" );
		LarvalJobOP larval_job1 = deserialize_larval_job( ser_larv1 );
		TS_ASSERT_EQUALS( larval_job1->job_index(), 1 );
		TS_ASSERT_EQUALS( larval_job1->nstruct_index(), 1 );
		TS_ASSERT_EQUALS( larval_job1->nstruct_max(), 3 );
		ts_assert_mpi_buffer_has_sizes( 0, "node 0 sends node 2 an empty list of job result indices", utility::vector1< Size >() );

		ts_assert_mpi_buffer_has_integers( 0, "node 0 tells node 2 to archive its results on node 1", utility::vector1< int >( 1, 1 ) );
		// ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 2 that archival completed", mpi_work_pool_jd_archival_completed );

		ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 2 that it has another job for it to run", mpi_work_pool_jd_new_job_available );
		std::string ser_larv4 = ts_assert_mpi_buffer_has_string( 0, "node 0 sends node 2 the serialized LarvalJob" );
		LarvalJobOP larval_job4 = deserialize_larval_job( ser_larv4 );
		TS_ASSERT_EQUALS( larval_job4->job_index(), 4 );
		TS_ASSERT_EQUALS( larval_job4->nstruct_index(), 1 );
		TS_ASSERT_EQUALS( larval_job4->nstruct_max(), 3 );
		TS_ASSERT_EQUALS( larval_job4->input_job_result_indices().size(), 1 );
		TS_ASSERT_EQUALS( larval_job4->input_job_result_indices()[ 1 ].first, 1 );
		TS_ASSERT_EQUALS( larval_job4->input_job_result_indices()[ 1 ].second, 1 );
		ts_assert_mpi_buffer_has_sizes( 0, "node 0 sends node 2 a vector1 of job result indices with one element whose value is 1", utility::vector1< Size >( 1, 1 ) );

		//ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 2 that it has the job result it was expecting", mpi_work_pool_jd_job_result_retrieved );
		// std::string ser_job_res1a = ts_assert_mpi_buffer_has_string( 0, "node 0 sends the serialized larval-job and job-result from job 1" );
		// MPIWorkPoolJobDistributor::LarvalJobAndResult job_res1a = deserialized_larval_job_and_job_result( ser_job_res1a );
		// TS_ASSERT( job_res1a.first );
		// TS_ASSERT( job_res1a.second );
		// TS_ASSERT_EQUALS( job_res1a.first->job_index(), 1 );
		// TS_ASSERT_EQUALS( job_res1a.first->nstruct_index(), 1 );
		// TS_ASSERT_EQUALS( job_res1a.first->nstruct_max(), 3 );
		// TS_ASSERT_EQUALS( utility::pointer::dynamic_pointer_cast< PoseJobResult > ( job_res1a.second )->pose()->total_residue(), 20 );

		ts_assert_mpi_buffer_has_integers( 0, "node 0 tells node 2 to archive its results on node 1", utility::vector1< int >( 1, 1 ) );
		// ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 2 that archival completed", mpi_work_pool_jd_archival_completed );

		ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 2 that there are no jobs left to run", mpi_work_pool_jd_spin_down );

		SimulateMPI::set_mpi_rank( 3 );
		ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 3 that it has a job for it to run", mpi_work_pool_jd_new_job_available );
		std::string ser_larv2 = ts_assert_mpi_buffer_has_string( 0, "node 0 sends node 3 the serialized LarvalJob" );
		LarvalJobOP larval_job2 = deserialize_larval_job( ser_larv2 );
		TS_ASSERT_EQUALS( larval_job2->job_index(), 2 );
		TS_ASSERT_EQUALS( larval_job2->nstruct_index(), 2 );
		TS_ASSERT_EQUALS( larval_job2->nstruct_max(), 3 );
		ts_assert_mpi_buffer_has_sizes( 0, "node 0 sends node 3 an empty list of job result indices", utility::vector1< Size >() );

		ts_assert_mpi_buffer_has_integers( 0, "node 0 tells node 3 to archive its results on node 1", utility::vector1< int >( 1, 1 ) );
		//ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 3 that archival completed", mpi_work_pool_jd_archival_completed );

		ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 3 that it has another job for it to run", mpi_work_pool_jd_new_job_available );
		std::string ser_larv5 = ts_assert_mpi_buffer_has_string( 0, "node 0 sends node 3 the serialized LarvalJob" );
		LarvalJobOP larval_job5 = deserialize_larval_job( ser_larv5 );
		TS_ASSERT_EQUALS( larval_job5->job_index(), 5 );
		TS_ASSERT_EQUALS( larval_job5->nstruct_index(), 2 );
		TS_ASSERT_EQUALS( larval_job5->nstruct_max(), 3 );
		TS_ASSERT_EQUALS( larval_job5->input_job_result_indices().size(), 1 );
		TS_ASSERT_EQUALS( larval_job5->input_job_result_indices()[ 1 ].first, 1 );
		TS_ASSERT_EQUALS( larval_job5->input_job_result_indices()[ 1 ].second, 1 );
		ts_assert_mpi_buffer_has_sizes( 0, "node 0 sends node 3 a vector1 of job result indices with one element whose value is 1", utility::vector1< Size >( 1, 1 ) );

		// ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 3 that it has the job result it was expecting", mpi_work_pool_jd_job_result_retrieved );
		// std::string ser_job_res1b = ts_assert_mpi_buffer_has_string( 0, "node 0 sends the serialized larval-job and job-result from job 1" );
		// MPIWorkPoolJobDistributor::LarvalJobAndResult job_res1b = deserialized_larval_job_and_job_result( ser_job_res1b );
		// TS_ASSERT( job_res1b.first );
		// TS_ASSERT( job_res1b.second );
		// TS_ASSERT_EQUALS( job_res1b.first->job_index(), 1 );
		// TS_ASSERT_EQUALS( job_res1b.first->nstruct_index(), 1 );
		// TS_ASSERT_EQUALS( job_res1b.first->nstruct_max(), 3 );
		// TS_ASSERT_EQUALS( utility::pointer::dynamic_pointer_cast< PoseJobResult > ( job_res1b.second )->pose()->total_residue(), 20 );

		ts_assert_mpi_buffer_has_integers( 0, "node 0 tells node 3 to archive its results on node 1", utility::vector1< int >( 1, 1 ) );
		// ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 3 that archival completed", mpi_work_pool_jd_archival_completed );

		ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 3 that there are no jobs left to run", mpi_work_pool_jd_spin_down );

		SimulateMPI::set_mpi_rank( 4 );
		ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 4 that it has a job for it to run", mpi_work_pool_jd_new_job_available );
		std::string ser_larv3 = ts_assert_mpi_buffer_has_string( 0, "node 0 sends node 4 the serialized LarvalJob" );
		LarvalJobOP larval_job3 = deserialize_larval_job( ser_larv3 );
		TS_ASSERT_EQUALS( larval_job3->job_index(), 3 );
		TS_ASSERT_EQUALS( larval_job3->nstruct_index(), 3 );
		TS_ASSERT_EQUALS( larval_job3->nstruct_max(), 3 );
		ts_assert_mpi_buffer_has_sizes( 0, "node 0 sends node 4 an empty list of job result indices", utility::vector1< Size >() );

		ts_assert_mpi_buffer_has_integers( 0, "node 0 tells node 4 to archive its results on node 1", utility::vector1< int >( 1, 1 ) );
		// ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 4 that archival completed", mpi_work_pool_jd_archival_completed );

		ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 4 that it has another job for it to run", mpi_work_pool_jd_new_job_available );
		std::string ser_larv6 = ts_assert_mpi_buffer_has_string( 0, "node 0 sends node 4 the serialized LarvalJob" );
		LarvalJobOP larval_job6 = deserialize_larval_job( ser_larv6 );
		TS_ASSERT_EQUALS( larval_job6->job_index(), 6 );
		TS_ASSERT_EQUALS( larval_job6->nstruct_index(), 3 );
		TS_ASSERT_EQUALS( larval_job6->nstruct_max(), 3 );
		TS_ASSERT_EQUALS( larval_job6->input_job_result_indices().size(), 1 );
		TS_ASSERT_EQUALS( larval_job6->input_job_result_indices()[ 1 ].first, 1 );
		TS_ASSERT_EQUALS( larval_job6->input_job_result_indices()[ 1 ].second, 1 );
		ts_assert_mpi_buffer_has_sizes( 0, "node 0 sends node 4 a vector1 of job result indices with one element whose value is 1", utility::vector1< Size >( 1, 1 ) );

		// ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 4 that it has the job result it was expecting", mpi_work_pool_jd_job_result_retrieved );
		// std::string ser_job_res1c = ts_assert_mpi_buffer_has_string( 0, "node 0 sends the serialized larval-job and job-result from job 1" );
		// MPIWorkPoolJobDistributor::LarvalJobAndResult job_res1c = deserialized_larval_job_and_job_result( ser_job_res1b );
		// TS_ASSERT( job_res1c.first );
		// TS_ASSERT( job_res1c.second );
		// TS_ASSERT_EQUALS( job_res1c.first->job_index(), 1 );
		// TS_ASSERT_EQUALS( job_res1c.first->nstruct_index(), 1 );
		// TS_ASSERT_EQUALS( job_res1c.first->nstruct_max(), 3 );
		// TS_ASSERT_EQUALS( utility::pointer::dynamic_pointer_cast< PoseJobResult > ( job_res1c.second )->pose()->total_residue(), 20 );

		ts_assert_mpi_buffer_has_integers( 0, "node 0 tells node 4 to archive its results on node 1", utility::vector1< int >( 1, 1 ) );
		// ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 4 that archival completed", mpi_work_pool_jd_archival_completed );

		ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 4 that there are no jobs left to run", mpi_work_pool_jd_spin_down );

		SimulateMPI::set_mpi_rank( 1 ); // the archive node
		// first messages from node 0 are to discard the results from jobs 2 and 3
		ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 1 that it wants its attention A", 0 );
		ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 1 that it needs to discard a job A", mpi_work_pool_jd_discard_job_result );
		ts_assert_mpi_buffer_has_size( 0, "node 0 tells node 1 to discard job 2", 2 );
		ts_assert_mpi_buffer_has_size( 0, "node 0 tells node 1 to discard job 2 result 1", 1 );

		ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 1 that it wants its attention B", 0 );
		ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 1 that it needs to discard a job B", mpi_work_pool_jd_discard_job_result );
		ts_assert_mpi_buffer_has_size( 0, "node 0 tells node 1 to discard job 3", 3 );
		ts_assert_mpi_buffer_has_size( 0, "node 0 tells node 1 to discard job 3 result 1", 1 );

		ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 1 that it wants its attention C", 0 );
		ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 1 that it needs to output a job A", mpi_work_pool_jd_output_job_result_already_available );
		std::string ser_os4 = ts_assert_mpi_buffer_has_string( 0,
			"node 0 sends node 1 the OutputSpecification for job result (4,1)" );
		OutputSpecificationOP os4 = deserialized_output_specification( ser_os4 );
		TS_ASSERT_EQUALS( os4->result_id().first, 4 );
		TS_ASSERT_EQUALS( os4->result_id().second, 1 );

		ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 1 that it wants its attention D", 0 );
		ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 1 that it needs to output a job B", mpi_work_pool_jd_output_job_result_already_available );
		std::string ser_os5 = ts_assert_mpi_buffer_has_string( 0,
			"node 0 sends node 1 the OutputSpecification for job result (5,1)" );
		OutputSpecificationOP os5 = deserialized_output_specification( ser_os5 );
		TS_ASSERT_EQUALS( os5->result_id().first, 5 );
		TS_ASSERT_EQUALS( os5->result_id().second, 1 );

		ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 1 that it wants its attention E", 0 );
		ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 1 that it needs to output a job B", mpi_work_pool_jd_output_job_result_already_available );
		std::string ser_os6 = ts_assert_mpi_buffer_has_string( 0,
			"node 0 sends node 1 the OutputSpecification for job result (6,1)" );
		OutputSpecificationOP os6 = deserialized_output_specification( ser_os6 );
		TS_ASSERT_EQUALS( os6->result_id().first, 6 );
		TS_ASSERT_EQUALS( os6->result_id().second, 1 );

		ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 1 that it wants its attention F", 0 );
		ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 1 to spin down", mpi_work_pool_jd_spin_down );


		// OK -- let's check the state of the JQ

		TS_ASSERT_EQUALS( jq3->status_.count( 1 ), 1 );
		TS_ASSERT_EQUALS( jq3->status_.count( 2 ), 1 );
		TS_ASSERT_EQUALS( jq3->status_.count( 3 ), 1 );
		TS_ASSERT_EQUALS( jq3->status_.count( 4 ), 1 );
		TS_ASSERT_EQUALS( jq3->status_.count( 5 ), 1 );
		TS_ASSERT_EQUALS( jq3->status_.count( 6 ), 1 );
		TS_ASSERT_EQUALS( jq3->status_[ 1 ], jd3_job_status_success );
		TS_ASSERT_EQUALS( jq3->status_[ 2 ], jd3_job_status_success );
		TS_ASSERT_EQUALS( jq3->status_[ 3 ], jd3_job_status_success );
		TS_ASSERT_EQUALS( jq3->status_[ 4 ], jd3_job_status_success );
		TS_ASSERT_EQUALS( jq3->status_[ 5 ], jd3_job_status_success );
		TS_ASSERT_EQUALS( jq3->status_[ 6 ], jd3_job_status_success );

		TS_ASSERT_EQUALS( jq3->summaries_.count( sp1( 1 ) ), 1 );
		TS_ASSERT_EQUALS( jq3->summaries_.count( sp1( 2 ) ), 1 );
		TS_ASSERT_EQUALS( jq3->summaries_.count( sp1( 3 ) ), 1 );
		TS_ASSERT_EQUALS( jq3->summaries_.count( sp1( 4 ) ), 1 );
		TS_ASSERT_EQUALS( jq3->summaries_.count( sp1( 5 ) ), 1 );
		TS_ASSERT_EQUALS( jq3->summaries_.count( sp1( 6 ) ), 1 );
		TS_ASSERT_EQUALS( utility::pointer::dynamic_pointer_cast< EnergyJobSummary > ( jq3->summaries_[ sp1( 1 ) ])->energy(), 1234 );
		TS_ASSERT_EQUALS( utility::pointer::dynamic_pointer_cast< EnergyJobSummary > ( jq3->summaries_[ sp1( 2 ) ])->energy(), 1234 );
		TS_ASSERT_EQUALS( utility::pointer::dynamic_pointer_cast< EnergyJobSummary > ( jq3->summaries_[ sp1( 3 ) ])->energy(), 1234 );
		TS_ASSERT_EQUALS( utility::pointer::dynamic_pointer_cast< EnergyJobSummary > ( jq3->summaries_[ sp1( 4 ) ])->energy(), 1234 );
		TS_ASSERT_EQUALS( utility::pointer::dynamic_pointer_cast< EnergyJobSummary > ( jq3->summaries_[ sp1( 5 ) ])->energy(), 1234 );
		TS_ASSERT_EQUALS( utility::pointer::dynamic_pointer_cast< EnergyJobSummary > ( jq3->summaries_[ sp1( 6 ) ])->energy(), 1234 );

		TS_ASSERT_EQUALS( jq3->results_->count( sp1( 1 ) ), 0 );
		TS_ASSERT_EQUALS( jq3->results_->count( sp1( 2 ) ), 0 );
		TS_ASSERT_EQUALS( jq3->results_->count( sp1( 3 ) ), 0 );
		TS_ASSERT_EQUALS( jq3->results_->count( sp1( 4 ) ), 0 );
		TS_ASSERT_EQUALS( jq3->results_->count( sp1( 5 ) ), 0 );
		TS_ASSERT_EQUALS( jq3->results_->count( sp1( 6 ) ), 0 );


		SimulateMPI::set_mpi_rank( 0 );

		TS_ASSERT( SimulateMPI::incoming_message_queue_is_empty() );
		TS_ASSERT( SimulateMPI::outgoing_message_queue_is_empty() );
		SimulateMPI::print_unprocessed_incoming_messages( std::cerr );
		SimulateMPI::print_unprocessed_outgoing_messages( std::cerr );

		//std::cout << "Finished Thirteenth Unit Test" << std::endl;

#endif
	}

	void test_jd3_workpool_manager_two_rounds_one_archive_one_outputter_no_archiving_on_node_0() {
		TS_ASSERT( true );

#ifdef SERIALIZATION
		core_init_with_additional_options( "-jd3::n_archive_nodes 1 -jd3::do_not_archive_on_node0 -jd3::compress_job_results 0 -jd3::mpi_fraction_outputters 0.4" );

		EnergyJobSummaryOP trpcage_job_summary( new EnergyJobSummary );
		trpcage_job_summary->energy( 1234 );
		utility::vector1< JobSummaryOP > summaries( 1, trpcage_job_summary );
		std::string serialized_trpcage_job_summaries = serialized_job_summaries( summaries );


		PoseJobResultOP trpcage_pose_result( new PoseJobResult );
		trpcage_pose_result->pose(  create_trpcage_ideal_poseop() );

		SimulateMPI::initialize_simulation( 5 );

		// ok, all the nodes at once start requesting jobs from node 0
		SimulateMPI::set_mpi_rank( 2 );
		send_integer_to_node( 0, 2 );
		SimulateMPI::set_mpi_rank( 3 );
		send_integer_to_node( 0, 3 );
		send_integer_to_node( 0, mpi_work_pool_jd_new_job_request );
		SimulateMPI::set_mpi_rank( 4 );
		send_integer_to_node( 0, 4 );

		// let's start the job at node 2
		SimulateMPI::set_mpi_rank( 2 );
		send_integer_to_node( 0, mpi_work_pool_jd_new_job_request );

		// and now the job at node 4
		SimulateMPI::set_mpi_rank( 4 );
		send_integer_to_node( 0, mpi_work_pool_jd_new_job_request );

		// ok -- now let's pretend that node 3 has finished its job
		SimulateMPI::set_mpi_rank( 3 );
		send_integer_to_node( 0, 3 );

		// ok -- before node 3 can finish its two part job-completion process,
		// node 2 will butt in and start its two part job-completion process,
		SimulateMPI::set_mpi_rank( 2 );
		send_integer_to_node( 0, 2 );

		SimulateMPI::set_mpi_rank( 3 );
		send_integer_to_node( 0, mpi_work_pool_jd_job_success );
		send_size_to_node( 0, 2 ); // job_id
		send_size_to_node( 0, 1 ); // num results
		// sent to archive instead -- send_string_to_node( 0, serialized_larval_job_and_job_result( 3, 2, 2, trpcage_pose_result ));

		SimulateMPI::set_mpi_rank( 2 );
		send_integer_to_node( 0, mpi_work_pool_jd_job_success );
		send_size_to_node( 0, 1 ); // job id
		send_size_to_node( 0, 1 ); // num results
		// sent to archive instead -- send_string_to_node( 0, serialized_larval_job_and_job_result( 3, 1, 1, trpcage_pose_result ));

		SimulateMPI::set_mpi_rank( 3 );
		send_integer_to_node( 0, 3 );
		send_integer_to_node( 0, mpi_work_pool_jd_job_success_and_archival_complete );
		send_size_to_node( 0, 2 ); // job id
		send_integer_to_node( 0, jd3_job_status_success );
		send_string_to_node( 0, serialized_trpcage_job_summaries );

		SimulateMPI::set_mpi_rank( 4 );
		send_integer_to_node( 0, 4 );
		send_integer_to_node( 0, mpi_work_pool_jd_job_success );
		send_size_to_node( 0, 3 ); // job id
		send_size_to_node( 0, 1 ); // num results
		// sent to archive instead -- send_string_to_node( 0, serialized_larval_job_and_job_result( 3, 3, 3, trpcage_pose_result ));

		SimulateMPI::set_mpi_rank( 2 );
		send_integer_to_node( 0, 2 );
		send_integer_to_node( 0, mpi_work_pool_jd_job_success_and_archival_complete );
		send_size_to_node( 0, 1 ); // job id
		send_integer_to_node( 0, jd3_job_status_success ); // job id
		send_string_to_node( 0, serialized_trpcage_job_summaries );

		// Now node 2 asks for a new job
		send_integer_to_node( 0, 2 );
		send_integer_to_node( 0, mpi_work_pool_jd_new_job_request );

		SimulateMPI::set_mpi_rank( 3 );
		// Now node 3 asks for a new job
		send_integer_to_node( 0, 3 );
		send_integer_to_node( 0, mpi_work_pool_jd_new_job_request );


		SimulateMPI::set_mpi_rank( 4 );
		send_integer_to_node( 0, 4 );
		send_integer_to_node( 0, mpi_work_pool_jd_job_success_and_archival_complete );
		send_size_to_node( 0, 3 ); // job id
		send_integer_to_node( 0, jd3_job_status_success );
		send_string_to_node( 0, serialized_trpcage_job_summaries );

		// Now node 4 asks for a new job
		send_integer_to_node( 0, 4 );
		send_integer_to_node( 0, mpi_work_pool_jd_new_job_request );

		SimulateMPI::set_mpi_rank( 2 );

		// now retrieve the job result for job 1
		// send_integer_to_node( 0, 2 );
		// send_integer_to_node( 0, mpi_work_pool_jd_retrieve_job_result );
		// send_size_to_node( 0, 1 ); // retrieve the result from job #1

		SimulateMPI::set_mpi_rank( 3 );

		// now retrieve the job result for job 1
		// send_integer_to_node( 0, 3 );
		// send_integer_to_node( 0, mpi_work_pool_jd_retrieve_job_result );
		// send_size_to_node( 0, 1 ); // retrieve the result from job #1

		SimulateMPI::set_mpi_rank( 4 );

		// now retrieve the job result for job 1
		// send_integer_to_node( 0, 4 );
		// send_integer_to_node( 0, mpi_work_pool_jd_retrieve_job_result );
		// send_size_to_node( 0, 1 ); // retrieve the result from job #1

		// Now the second round of job results start trickling in

		SimulateMPI::set_mpi_rank( 2 );

		send_integer_to_node( 0, 2 );
		send_integer_to_node( 0, mpi_work_pool_jd_job_success );
		send_size_to_node( 0, 4 ); // job id
		send_size_to_node( 0, 1 ); // num results
		// sent to archive instead -- send_string_to_node( 0, serialized_larval_job_and_job_result( 3, 1, 4, trpcage_pose_result ));

		send_integer_to_node( 0, 2 );
		send_integer_to_node( 0, mpi_work_pool_jd_job_success_and_archival_complete );
		send_size_to_node( 0, 4 ); // job id
		send_integer_to_node( 0, jd3_job_status_success );
		send_string_to_node( 0, serialized_trpcage_job_summaries );

		send_integer_to_node( 0, 2 );
		send_integer_to_node( 0, mpi_work_pool_jd_new_job_request );

		SimulateMPI::set_mpi_rank( 4 );
		send_integer_to_node( 0, 4 );
		send_integer_to_node( 0, mpi_work_pool_jd_job_success );
		send_size_to_node( 0, 6 ); // job id
		send_size_to_node( 0, 1 ); // num results
		// sent to archive instead -- send_string_to_node( 0, serialized_larval_job_and_job_result( 3, 3, 6, trpcage_pose_result ));

		send_integer_to_node( 0, 4 );
		send_integer_to_node( 0, mpi_work_pool_jd_job_success_and_archival_complete );
		send_size_to_node( 0, 6 ); // job id
		send_integer_to_node( 0, jd3_job_status_success );
		send_string_to_node( 0, serialized_trpcage_job_summaries );

		send_integer_to_node( 0, 4 );
		send_integer_to_node( 0, mpi_work_pool_jd_new_job_request );

		SimulateMPI::set_mpi_rank( 3 );
		send_integer_to_node( 0, 3 );
		send_integer_to_node( 0, mpi_work_pool_jd_job_success );
		send_size_to_node( 0, 5 ); // job id
		send_size_to_node( 0, 1 ); // num results
		// sent to archive instead -- send_string_to_node( 0, serialized_larval_job_and_job_result( 3, 2, 5, trpcage_pose_result ));

		send_integer_to_node( 0, 3 );
		send_integer_to_node( 0, mpi_work_pool_jd_job_success_and_archival_complete );
		send_size_to_node( 0, 5 ); // job id
		send_integer_to_node( 0, jd3_job_status_success );
		send_string_to_node( 0, serialized_trpcage_job_summaries );

		send_integer_to_node( 0, 3 );
		send_integer_to_node( 0, mpi_work_pool_jd_new_job_request );

		// OK -- output time.
		// Have node 2 output two of the jobs and node 1 output one of them
		// to test a race condition.
		// Node 0 will hand both node 1 and node 2 a job to output.
		// Node 2 will report it has finished its output first,
		// Node 0 will assign node 2 a second job to output,
		// and then node 1 will say it has finished.
		// Node 0 should not spin down node 1 until after node 2 has said
		// that it has completed output of the 2nd job.

		SimulateMPI::set_mpi_rank(2);
		send_integer_to_node( 0, 2 );
		send_integer_to_node( 0, mpi_work_pool_jd_output_completed );
		send_integer_to_node( 0, 2 );
		send_integer_to_node( 0, mpi_work_pool_jd_new_job_request );


		SimulateMPI::set_mpi_rank( 1 );
		send_integer_to_node( 0, 1 );
		send_integer_to_node( 0, mpi_work_pool_jd_output_completed );

		SimulateMPI::set_mpi_rank(2);
		send_integer_to_node( 0, 2 );
		send_integer_to_node( 0, mpi_work_pool_jd_output_completed );
		send_integer_to_node( 0, 2 );
		send_integer_to_node( 0, mpi_work_pool_jd_new_job_request );


		//OK! Now create a PoolJQ3 and let 'er rip
		SimulateMPI::set_mpi_rank( 0 );
		PoolJQ3OP jq3( new PoolJQ3 );
		MPIWorkPoolJobDistributor jd;
		try {
			jd.go( jq3 );
		} catch ( ... ) {
			TS_ASSERT( false /*no exception should be thrown*/ );
		}

		// OK!
		// Now we read the mail we got from node 0

		SimulateMPI::set_mpi_rank( 2 );
		ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 2 that it has a job for it to run", mpi_work_pool_jd_new_job_available );
		std::string ser_larv1 = ts_assert_mpi_buffer_has_string( 0, "node 0 sends node 2 the serialized LarvalJob" );
		LarvalJobOP larval_job1 = deserialize_larval_job( ser_larv1 );
		TS_ASSERT_EQUALS( larval_job1->job_index(), 1 );
		TS_ASSERT_EQUALS( larval_job1->nstruct_index(), 1 );
		TS_ASSERT_EQUALS( larval_job1->nstruct_max(), 3 );
		ts_assert_mpi_buffer_has_sizes( 0, "node 0 sends node 2 an empty list of job result indices", utility::vector1< Size >() );

		ts_assert_mpi_buffer_has_integers( 0, "node 0 tells node 2 to archive its results on node 1", utility::vector1< int >( 1, 1 ) );
		// ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 2 that archival completed", mpi_work_pool_jd_archival_completed );

		ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 2 that it has another job for it to run", mpi_work_pool_jd_new_job_available );
		std::string ser_larv4 = ts_assert_mpi_buffer_has_string( 0, "node 0 sends node 2 the serialized LarvalJob" );
		LarvalJobOP larval_job4 = deserialize_larval_job( ser_larv4 );
		TS_ASSERT_EQUALS( larval_job4->job_index(), 4 );
		TS_ASSERT_EQUALS( larval_job4->nstruct_index(), 1 );
		TS_ASSERT_EQUALS( larval_job4->nstruct_max(), 3 );
		TS_ASSERT_EQUALS( larval_job4->input_job_result_indices().size(), 1 );
		TS_ASSERT_EQUALS( larval_job4->input_job_result_indices()[ 1 ].first, 1 );
		TS_ASSERT_EQUALS( larval_job4->input_job_result_indices()[ 1 ].second, 1 );
		ts_assert_mpi_buffer_has_sizes( 0, "node 0 sends node 2 a vector1 of job result indices with one element whose value is 1", utility::vector1< Size >( 1, 1 ) );

		//ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 2 that it has the job result it was expecting", mpi_work_pool_jd_job_result_retrieved );
		// std::string ser_job_res1a = ts_assert_mpi_buffer_has_string( 0, "node 0 sends the serialized larval-job and job-result from job 1" );
		// MPIWorkPoolJobDistributor::LarvalJobAndResult job_res1a = deserialized_larval_job_and_job_result( ser_job_res1a );
		// TS_ASSERT( job_res1a.first );
		// TS_ASSERT( job_res1a.second );
		// TS_ASSERT_EQUALS( job_res1a.first->job_index(), 1 );
		// TS_ASSERT_EQUALS( job_res1a.first->nstruct_index(), 1 );
		// TS_ASSERT_EQUALS( job_res1a.first->nstruct_max(), 3 );
		// TS_ASSERT_EQUALS( utility::pointer::dynamic_pointer_cast< PoseJobResult > ( job_res1a.second )->pose()->total_residue(), 20 );

		ts_assert_mpi_buffer_has_integers( 0, "node 0 tells node 2 to archive its results on node 1", utility::vector1< int >( 1, 1 ) );
		// ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 2 that archival completed", mpi_work_pool_jd_archival_completed );

		ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 2 that there is a job that it needs to output", mpi_work_pool_jd_output_job_result_on_archive );
		ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 2 that the job result is archived on node 1", 1);
		std::string ser_os5 = ts_assert_mpi_buffer_has_string( 0, "node 0 then sends node 2 the serialized job output specification for job result (5,1)" );
		OutputSpecificationOP os5 = deserialized_output_specification( ser_os5 );
		TS_ASSERT_EQUALS( os5->result_id().first, 5 );
		TS_ASSERT_EQUALS( os5->result_id().second, 1 );

		ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 2 that there is another job that it needs to output", mpi_work_pool_jd_output_job_result_on_archive );
		ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 2 that the job result is archived on node 1", 1);
		std::string ser_os6 = ts_assert_mpi_buffer_has_string( 0, "node 0 then sends node 2 the serialized job output specification for job result (6,1)" );
		OutputSpecificationOP os6 = deserialized_output_specification( ser_os6 );
		TS_ASSERT_EQUALS( os6->result_id().first, 6 );
		TS_ASSERT_EQUALS( os6->result_id().second, 1 );

		// Finally, spin down
		Size spin_down_node2_message_ind = utility::SimulateMPI::index_of_next_message();
		ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 2 that there are no jobs left to run", mpi_work_pool_jd_spin_down );

		SimulateMPI::set_mpi_rank( 3 );
		ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 3 that it has a job for it to run", mpi_work_pool_jd_new_job_available );
		std::string ser_larv2 = ts_assert_mpi_buffer_has_string( 0, "node 0 sends node 3 the serialized LarvalJob" );
		LarvalJobOP larval_job2 = deserialize_larval_job( ser_larv2 );
		TS_ASSERT_EQUALS( larval_job2->job_index(), 2 );
		TS_ASSERT_EQUALS( larval_job2->nstruct_index(), 2 );
		TS_ASSERT_EQUALS( larval_job2->nstruct_max(), 3 );
		ts_assert_mpi_buffer_has_sizes( 0, "node 0 sends node 3 an empty list of job result indices", utility::vector1< Size >() );

		ts_assert_mpi_buffer_has_integers( 0, "node 0 tells node 3 to archive its results on node 1", utility::vector1< int >( 1, 1 ) );
		//ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 3 that archival completed", mpi_work_pool_jd_archival_completed );

		ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 3 that it has another job for it to run", mpi_work_pool_jd_new_job_available );
		std::string ser_larv5 = ts_assert_mpi_buffer_has_string( 0, "node 0 sends node 3 the serialized LarvalJob" );
		LarvalJobOP larval_job5 = deserialize_larval_job( ser_larv5 );
		TS_ASSERT_EQUALS( larval_job5->job_index(), 5 );
		TS_ASSERT_EQUALS( larval_job5->nstruct_index(), 2 );
		TS_ASSERT_EQUALS( larval_job5->nstruct_max(), 3 );
		TS_ASSERT_EQUALS( larval_job5->input_job_result_indices().size(), 1 );
		TS_ASSERT_EQUALS( larval_job5->input_job_result_indices()[ 1 ].first, 1 );
		TS_ASSERT_EQUALS( larval_job5->input_job_result_indices()[ 1 ].second, 1 );
		ts_assert_mpi_buffer_has_sizes( 0, "node 0 sends node 3 a vector1 of job result indices with one element whose value is 1", utility::vector1< Size >( 1, 1 ) );

		// ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 3 that it has the job result it was expecting", mpi_work_pool_jd_job_result_retrieved );
		// std::string ser_job_res1b = ts_assert_mpi_buffer_has_string( 0, "node 0 sends the serialized larval-job and job-result from job 1" );
		// MPIWorkPoolJobDistributor::LarvalJobAndResult job_res1b = deserialized_larval_job_and_job_result( ser_job_res1b );
		// TS_ASSERT( job_res1b.first );
		// TS_ASSERT( job_res1b.second );
		// TS_ASSERT_EQUALS( job_res1b.first->job_index(), 1 );
		// TS_ASSERT_EQUALS( job_res1b.first->nstruct_index(), 1 );
		// TS_ASSERT_EQUALS( job_res1b.first->nstruct_max(), 3 );
		// TS_ASSERT_EQUALS( utility::pointer::dynamic_pointer_cast< PoseJobResult > ( job_res1b.second )->pose()->total_residue(), 20 );

		ts_assert_mpi_buffer_has_integers( 0, "node 0 tells node 3 to archive its results on node 1", utility::vector1< int >( 1, 1 ) );
		// ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 3 that archival completed", mpi_work_pool_jd_archival_completed );

		ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 3 that there are no jobs left to run", mpi_work_pool_jd_spin_down );

		SimulateMPI::set_mpi_rank( 4 );
		ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 4 that it has a job for it to run", mpi_work_pool_jd_new_job_available );
		std::string ser_larv3 = ts_assert_mpi_buffer_has_string( 0, "node 0 sends node 4 the serialized LarvalJob" );
		LarvalJobOP larval_job3 = deserialize_larval_job( ser_larv3 );
		TS_ASSERT_EQUALS( larval_job3->job_index(), 3 );
		TS_ASSERT_EQUALS( larval_job3->nstruct_index(), 3 );
		TS_ASSERT_EQUALS( larval_job3->nstruct_max(), 3 );
		ts_assert_mpi_buffer_has_sizes( 0, "node 0 sends node 4 an empty list of job result indices", utility::vector1< Size >() );

		ts_assert_mpi_buffer_has_integers( 0, "node 0 tells node 4 to archive its results on node 1", utility::vector1< int >( 1, 1 ) );
		// ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 4 that archival completed", mpi_work_pool_jd_archival_completed );

		ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 4 that it has another job for it to run", mpi_work_pool_jd_new_job_available );
		std::string ser_larv6 = ts_assert_mpi_buffer_has_string( 0, "node 0 sends node 4 the serialized LarvalJob" );
		LarvalJobOP larval_job6 = deserialize_larval_job( ser_larv6 );
		TS_ASSERT_EQUALS( larval_job6->job_index(), 6 );
		TS_ASSERT_EQUALS( larval_job6->nstruct_index(), 3 );
		TS_ASSERT_EQUALS( larval_job6->nstruct_max(), 3 );
		TS_ASSERT_EQUALS( larval_job6->input_job_result_indices().size(), 1 );
		TS_ASSERT_EQUALS( larval_job6->input_job_result_indices()[ 1 ].first, 1 );
		TS_ASSERT_EQUALS( larval_job6->input_job_result_indices()[ 1 ].second, 1 );
		ts_assert_mpi_buffer_has_sizes( 0, "node 0 sends node 4 a vector1 of job result indices with one element whose value is 1", utility::vector1< Size >( 1, 1 ) );

		// ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 4 that it has the job result it was expecting", mpi_work_pool_jd_job_result_retrieved );
		// std::string ser_job_res1c = ts_assert_mpi_buffer_has_string( 0, "node 0 sends the serialized larval-job and job-result from job 1" );
		// MPIWorkPoolJobDistributor::LarvalJobAndResult job_res1c = deserialized_larval_job_and_job_result( ser_job_res1b );
		// TS_ASSERT( job_res1c.first );
		// TS_ASSERT( job_res1c.second );
		// TS_ASSERT_EQUALS( job_res1c.first->job_index(), 1 );
		// TS_ASSERT_EQUALS( job_res1c.first->nstruct_index(), 1 );
		// TS_ASSERT_EQUALS( job_res1c.first->nstruct_max(), 3 );
		// TS_ASSERT_EQUALS( utility::pointer::dynamic_pointer_cast< PoseJobResult > ( job_res1c.second )->pose()->total_residue(), 20 );

		ts_assert_mpi_buffer_has_integers( 0, "node 0 tells node 4 to archive its results on node 1", utility::vector1< int >( 1, 1 ) );
		// ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 4 that archival completed", mpi_work_pool_jd_archival_completed );

		ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 4 that there are no jobs left to run", mpi_work_pool_jd_spin_down );

		SimulateMPI::set_mpi_rank( 1 ); // the archive node
		// first messages from node 0 are to discard the results from jobs 2 and 3
		ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 1 that it wants its attention A", 0 );
		ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 1 that it needs to discard a job A", mpi_work_pool_jd_discard_job_result );
		ts_assert_mpi_buffer_has_size( 0, "node 0 tells node 1 to discard job 2", 2 );
		ts_assert_mpi_buffer_has_size( 0, "node 0 tells node 1 to discard job 2 result 1", 1 );

		ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 1 that it wants its attention B", 0 );
		ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 1 that it needs to discard a job B", mpi_work_pool_jd_discard_job_result );
		ts_assert_mpi_buffer_has_size( 0, "node 0 tells node 1 to discard job 3", 3 );
		ts_assert_mpi_buffer_has_size( 0, "node 0 tells node 1 to discard job 3 result 1", 1 );

		ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 1 that it wants its attention C", 0 );
		ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 1 that it needs to output a job A", mpi_work_pool_jd_output_job_result_already_available );
		std::string ser_os4 = ts_assert_mpi_buffer_has_string( 0,
			"node 0 sends node 1 the OutputSpecification for job result (4,1)" );
		OutputSpecificationOP os4 = deserialized_output_specification( ser_os4 );
		TS_ASSERT_EQUALS( os4->result_id().first, 4 );
		TS_ASSERT_EQUALS( os4->result_id().second, 1 );

		ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 1 that it wants its attention D", 0 );
		Size spin_down_node1_message_ind = SimulateMPI::index_of_next_message();
		ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 1 that there are no jobs left to run", mpi_work_pool_jd_spin_down );

		// CRITICAL! The job distributor must not spin down node 1 until
		// node 2 has said that it has completed outputting everything
		// or you get a node 2 waiting forever for node 1 to reply to
		// a request for the result that node 2 is tasked with outputting
		// (which node 1 won't do because it has left its listener loop)
		// std::cout << "Spin down messages: spin_down_node2_message_ind " << spin_down_node2_message_ind << " spin_down_node1_message_ind " << spin_down_node1_message_ind << std::endl;
		TS_ASSERT( spin_down_node2_message_ind < spin_down_node1_message_ind );

		TS_ASSERT_EQUALS( jq3->status_.count( 1 ), 1 );
		TS_ASSERT_EQUALS( jq3->status_.count( 2 ), 1 );
		TS_ASSERT_EQUALS( jq3->status_.count( 3 ), 1 );
		TS_ASSERT_EQUALS( jq3->status_.count( 4 ), 1 );
		TS_ASSERT_EQUALS( jq3->status_.count( 5 ), 1 );
		TS_ASSERT_EQUALS( jq3->status_.count( 6 ), 1 );
		TS_ASSERT_EQUALS( jq3->status_[ 1 ], jd3_job_status_success );
		TS_ASSERT_EQUALS( jq3->status_[ 2 ], jd3_job_status_success );
		TS_ASSERT_EQUALS( jq3->status_[ 3 ], jd3_job_status_success );
		TS_ASSERT_EQUALS( jq3->status_[ 4 ], jd3_job_status_success );
		TS_ASSERT_EQUALS( jq3->status_[ 5 ], jd3_job_status_success );
		TS_ASSERT_EQUALS( jq3->status_[ 6 ], jd3_job_status_success );

		TS_ASSERT_EQUALS( jq3->summaries_.count( sp1( 1 ) ), 1 );
		TS_ASSERT_EQUALS( jq3->summaries_.count( sp1( 2 ) ), 1 );
		TS_ASSERT_EQUALS( jq3->summaries_.count( sp1( 3 ) ), 1 );
		TS_ASSERT_EQUALS( jq3->summaries_.count( sp1( 4 ) ), 1 );
		TS_ASSERT_EQUALS( jq3->summaries_.count( sp1( 5 ) ), 1 );
		TS_ASSERT_EQUALS( jq3->summaries_.count( sp1( 6 ) ), 1 );
		TS_ASSERT_EQUALS( utility::pointer::dynamic_pointer_cast< EnergyJobSummary > ( jq3->summaries_[ sp1( 1 ) ])->energy(), 1234 );
		TS_ASSERT_EQUALS( utility::pointer::dynamic_pointer_cast< EnergyJobSummary > ( jq3->summaries_[ sp1( 2 ) ])->energy(), 1234 );
		TS_ASSERT_EQUALS( utility::pointer::dynamic_pointer_cast< EnergyJobSummary > ( jq3->summaries_[ sp1( 3 ) ])->energy(), 1234 );
		TS_ASSERT_EQUALS( utility::pointer::dynamic_pointer_cast< EnergyJobSummary > ( jq3->summaries_[ sp1( 4 ) ])->energy(), 1234 );
		TS_ASSERT_EQUALS( utility::pointer::dynamic_pointer_cast< EnergyJobSummary > ( jq3->summaries_[ sp1( 5 ) ])->energy(), 1234 );
		TS_ASSERT_EQUALS( utility::pointer::dynamic_pointer_cast< EnergyJobSummary > ( jq3->summaries_[ sp1( 6 ) ])->energy(), 1234 );

		TS_ASSERT_EQUALS( jq3->results_->count( sp1( 1 ) ), 0 );
		TS_ASSERT_EQUALS( jq3->results_->count( sp1( 2 ) ), 0 );
		TS_ASSERT_EQUALS( jq3->results_->count( sp1( 3 ) ), 0 );
		TS_ASSERT_EQUALS( jq3->results_->count( sp1( 4 ) ), 0 );
		TS_ASSERT_EQUALS( jq3->results_->count( sp1( 5 ) ), 0 );
		TS_ASSERT_EQUALS( jq3->results_->count( sp1( 6 ) ), 0 );


		SimulateMPI::set_mpi_rank( 0 );

		TS_ASSERT( SimulateMPI::incoming_message_queue_is_empty() );
		TS_ASSERT( SimulateMPI::outgoing_message_queue_is_empty() );

		SimulateMPI::print_unprocessed_incoming_messages( std::cerr );
		SimulateMPI::print_unprocessed_outgoing_messages( std::cerr );

		//std::cout << "Finished Thirteenth Unit Test" << std::endl;

#endif
	}


	void test_jd3_workpool_archive_two_rounds_one_archive_no_archiving_on_node_0() {
		TS_ASSERT( true );

#ifdef SERIALIZATION
		core_init_with_additional_options( "-jd3::n_archive_nodes 1 -jd3::do_not_archive_on_node0 -jd3::compress_job_results 0" );

		EnergyJobSummaryOP trpcage_job_summary( new EnergyJobSummary );
		trpcage_job_summary->energy( 1234 );
		utility::vector1< JobSummaryOP > summaries( 1, trpcage_job_summary );
		std::string serialized_trpcage_job_summaries = serialized_job_summaries( summaries );

		PoseJobResultOP trpcage_pose_result( new PoseJobResult );
		trpcage_pose_result->pose(  create_trpcage_ideal_poseop() );

		SimulateMPI::initialize_simulation( 5 );

		SimulateMPI::set_mpi_rank( 3 );
		send_integer_to_node( 1, 3 ); // node 3 says "I have a message for you, archive"
		send_integer_to_node( 1, mpi_work_pool_jd_archive_job_result ); // "please archive this job result"
		send_size_to_node( 1, 2 ); // job_id
		send_size_to_node( 1, 1 ); // result index
		send_string_to_node( 1, serialized_larval_job_and_job_result( 3, 2, 2, trpcage_pose_result ));

		SimulateMPI::set_mpi_rank( 2 );
		send_integer_to_node( 1, 2 ); // node 2 says "I have a message for you, archive"
		send_integer_to_node( 1, mpi_work_pool_jd_archive_job_result ); // "please archive this job result"
		send_size_to_node( 1, 1 ); // job_id
		send_size_to_node( 1, 1 ); // result index
		send_string_to_node( 1, serialized_larval_job_and_job_result( 3, 1, 1, trpcage_pose_result ));

		SimulateMPI::set_mpi_rank( 4 );
		send_integer_to_node( 1, 4 ); // node 4 says "I have a message for you, archive"
		send_integer_to_node( 1, mpi_work_pool_jd_archive_job_result ); // "please archive this job result"
		send_size_to_node( 1, 3 ); // job_id
		send_size_to_node( 1, 1 ); // result index
		send_string_to_node( 1, serialized_larval_job_and_job_result( 3, 3, 3, trpcage_pose_result ));


		SimulateMPI::set_mpi_rank( 0 );
		// master node says "discard job results 2 and 3
		send_integer_to_node( 1, 0 ); // node 0 says to archive node: "hey buddy, we need to talk"
		send_integer_to_node( 1, mpi_work_pool_jd_discard_job_result ); // "I need you to discard a job result"
		send_size_to_node( 1, 2 ); // discard job 2
		send_size_to_node( 1, 1 ); // discard job 2 result index 1

		send_integer_to_node( 1, 0 ); // node 0 says to archive node: "hey buddy, we need to talk"
		send_integer_to_node( 1, mpi_work_pool_jd_discard_job_result ); // "I need you to discard a job result"
		send_size_to_node( 1, 3 ); // discard job 3
		send_size_to_node( 1, 1 ); // discard job 3 result index 1

		SimulateMPI::set_mpi_rank( 2 );

		// now retrieve the job result for job 1
		send_integer_to_node( 1, 2 );
		send_integer_to_node( 1, mpi_work_pool_jd_retrieve_job_result );
		send_size_to_node( 1, 1 ); // retrieve the result from job #1
		send_size_to_node( 1, 1 ); // retrieve the result from job #1 result index #1

		SimulateMPI::set_mpi_rank( 3 );

		// now retrieve the job result for job 1
		send_integer_to_node( 1, 3 );
		send_integer_to_node( 1, mpi_work_pool_jd_retrieve_job_result );
		send_size_to_node( 1, 1 ); // retrieve the result from job #1
		send_size_to_node( 1, 1 ); // retrieve the result from job #1 result index #1

		SimulateMPI::set_mpi_rank( 4 );

		// now retrieve the job result for job 1
		send_integer_to_node( 1, 4 );
		send_integer_to_node( 1, mpi_work_pool_jd_retrieve_job_result );
		send_size_to_node( 1, 1 ); // retrieve the result from job #1
		send_size_to_node( 1, 1 ); // retrieve the result from job #1 result index #1

		SimulateMPI::set_mpi_rank( 2 );
		send_integer_to_node( 1, 2 ); // node 2 says "I have a message for you, archive"
		send_integer_to_node( 1, mpi_work_pool_jd_archive_job_result ); // "please archive this job result"
		send_size_to_node( 1, 4 ); // job_id
		send_size_to_node( 1, 1 ); // result index #1
		send_string_to_node( 1, serialized_larval_job_and_job_result( 3, 1, 4, trpcage_pose_result ));

		SimulateMPI::set_mpi_rank( 3 );
		send_integer_to_node( 1, 3 ); // node 3 says "I have a message for you, archive"
		send_integer_to_node( 1, mpi_work_pool_jd_archive_job_result ); // "please archive this job result"
		send_size_to_node( 1, 5 ); // job_id
		send_size_to_node( 1, 1 ); // result index #1
		send_string_to_node( 1, serialized_larval_job_and_job_result( 3, 2, 5, trpcage_pose_result ));

		SimulateMPI::set_mpi_rank( 4 );
		send_integer_to_node( 1, 4 ); // node 4 says "I have a message for you, archive"
		send_integer_to_node( 1, mpi_work_pool_jd_archive_job_result ); // "please archive this job result"
		send_size_to_node( 1, 6 ); // job_id
		send_size_to_node( 1, 1 ); // result index #1
		send_string_to_node( 1, serialized_larval_job_and_job_result( 3, 3, 6, trpcage_pose_result ));

		// Finally, node 0 asks the archive for job result

		SimulateMPI::set_mpi_rank( 0 );

		send_integer_to_node( 1, 0 ); // node 0 says "hey, I need to talk to you"
		send_integer_to_node( 1, mpi_work_pool_jd_output_job_result_already_available );
		send_string_to_node( 1, serialized_output_specification( JobResultID( 4, 1 ), joi(4) ) );

		send_integer_to_node( 1, 0 ); // node 0 says "hey, I need to talk to you"
		send_integer_to_node( 1, mpi_work_pool_jd_output_job_result_already_available );
		send_string_to_node( 1, serialized_output_specification( JobResultID( 5, 1 ), joi(5) ));

		send_integer_to_node( 1, 0 ); // node 0 says "hey, I need to talk to you"
		send_integer_to_node( 1, mpi_work_pool_jd_output_job_result_already_available ); // I need a job result
		//send_size_to_node( 1, 6 ); // job id
		//send_size_to_node( 1, 1 ); // result index
		send_string_to_node( 1, serialized_output_specification( JobResultID( 6, 1 ), joi(6) ));

		send_integer_to_node( 1, 0 ); // node 0 says "hey, I need to talk to you"
		send_integer_to_node( 1, mpi_work_pool_jd_spin_down ); // time to shut down

		//OK! Now create a PoolJQ3 and let 'er rip
		SimulateMPI::set_mpi_rank( 1 );
		PoolJQ3OP jq3( new PoolJQ3 );
		MPIWorkPoolJobDistributor jd;
		try {
			jd.go( jq3 );
		} catch ( ... ) {
			TS_ASSERT( false /*no exception should be thrown*/ );
		}

		// At the end, the jd should have outputs now from jobs 4-6
		TS_ASSERT_EQUALS( jq3->results_->count( sp1( 1 ) ), 0 );
		TS_ASSERT_EQUALS( jq3->results_->count( sp1( 2 ) ), 0 );
		TS_ASSERT_EQUALS( jq3->results_->count( sp1( 3 ) ), 0 );
		TS_ASSERT_EQUALS( jq3->results_->count( sp1( 4 ) ), 1 );
		TS_ASSERT_EQUALS( jq3->results_->count( sp1( 5 ) ), 1 );
		TS_ASSERT_EQUALS( jq3->results_->count( sp1( 6 ) ), 1 );
		TS_ASSERT_EQUALS( utility::pointer::dynamic_pointer_cast< PoseJobResult > ( (*jq3->results_)[ sp1( 4 ) ])->pose()->total_residue(), 20 );
		TS_ASSERT_EQUALS( utility::pointer::dynamic_pointer_cast< PoseJobResult > ( (*jq3->results_)[ sp1( 5 ) ])->pose()->total_residue(), 20 );
		TS_ASSERT_EQUALS( utility::pointer::dynamic_pointer_cast< PoseJobResult > ( (*jq3->results_)[ sp1( 6 ) ])->pose()->total_residue(), 20 );


		// OK!
		// Now we read the mail we got from node 1

		SimulateMPI::set_mpi_rank( 2 );

		ts_assert_mpi_buffer_has_integer( 1, "node 1 tells node 2 that archival completed (for job 2)", mpi_work_pool_jd_archival_completed );

		ts_assert_mpi_buffer_has_integer( 1, "node 1 tells node 2 that it has the job result it was expecting", mpi_work_pool_jd_job_result_retrieved );
		std::string ser_job_res1a = ts_assert_mpi_buffer_has_string( 1, "node 1 sends the serialized larval-job and job-result from job 1" );
		MPIWorkPoolJobDistributor::LarvalJobAndResult job_res1a = deserialized_larval_job_and_job_result( ser_job_res1a );
		TS_ASSERT( job_res1a.first );
		TS_ASSERT( job_res1a.second );
		TS_ASSERT_EQUALS( job_res1a.first->job_index(), 1 );
		TS_ASSERT_EQUALS( job_res1a.first->nstruct_index(), 1 );
		TS_ASSERT_EQUALS( job_res1a.first->nstruct_max(), 3 );
		TS_ASSERT_EQUALS( utility::pointer::dynamic_pointer_cast< PoseJobResult > ( job_res1a.second )->pose()->total_residue(), 20 );

		ts_assert_mpi_buffer_has_integer( 1, "node 1 tells node 2 that archival completed (for job 4)", mpi_work_pool_jd_archival_completed );

		SimulateMPI::set_mpi_rank( 3 );
		ts_assert_mpi_buffer_has_integer( 1, "node 1 tells node 3 that archival completed (for job 1)", mpi_work_pool_jd_archival_completed );

		ts_assert_mpi_buffer_has_integer( 1, "node 1 tells node 3 that it has the job result it was expecting", mpi_work_pool_jd_job_result_retrieved );
		std::string ser_job_res1b = ts_assert_mpi_buffer_has_string( 1, "node 1 sends the serialized larval-job and job-result from job 1" );
		MPIWorkPoolJobDistributor::LarvalJobAndResult job_res1b = deserialized_larval_job_and_job_result( ser_job_res1b );
		TS_ASSERT( job_res1b.first );
		TS_ASSERT( job_res1b.second );
		TS_ASSERT_EQUALS( job_res1b.first->job_index(), 1 );
		TS_ASSERT_EQUALS( job_res1b.first->nstruct_index(), 1 );
		TS_ASSERT_EQUALS( job_res1b.first->nstruct_max(), 3 );
		TS_ASSERT_EQUALS( utility::pointer::dynamic_pointer_cast< PoseJobResult > ( job_res1b.second )->pose()->total_residue(), 20 );

		ts_assert_mpi_buffer_has_integer( 1, "node 1 tells node 3 that archival completed (for job 5)", mpi_work_pool_jd_archival_completed );

		SimulateMPI::set_mpi_rank( 4 );
		ts_assert_mpi_buffer_has_integer( 1, "node 1 tells node 4 that archival completed (for job 3)", mpi_work_pool_jd_archival_completed );

		ts_assert_mpi_buffer_has_integer( 1, "node 1 tells node 4 that it has the job result it was expecting", mpi_work_pool_jd_job_result_retrieved );
		std::string ser_job_res1c = ts_assert_mpi_buffer_has_string( 1, "node 1 sends the serialized larval-job and job-result from job 1" );
		MPIWorkPoolJobDistributor::LarvalJobAndResult job_res1c = deserialized_larval_job_and_job_result( ser_job_res1b );
		TS_ASSERT( job_res1c.first );
		TS_ASSERT( job_res1c.second );
		TS_ASSERT_EQUALS( job_res1c.first->job_index(), 1 );
		TS_ASSERT_EQUALS( job_res1c.first->nstruct_index(), 1 );
		TS_ASSERT_EQUALS( job_res1c.first->nstruct_max(), 3 );
		TS_ASSERT_EQUALS( utility::pointer::dynamic_pointer_cast< PoseJobResult > ( job_res1c.second )->pose()->total_residue(), 20 );

		ts_assert_mpi_buffer_has_integer( 1, "node 1 tells node 4 that archival completed", mpi_work_pool_jd_archival_completed );

		SimulateMPI::set_mpi_rank( 0 ); // the master node

		ts_assert_mpi_buffer_has_integer( 1, "node 1 says that it has a message", 1 );
		ts_assert_mpi_buffer_has_integer( 1, "node 1 says that it has output job result 4", mpi_work_pool_jd_output_completed );

		ts_assert_mpi_buffer_has_integer( 1, "node 1 says that it has a message, b", 1 );
		ts_assert_mpi_buffer_has_integer( 1, "node 1 says that it has output job result 5", mpi_work_pool_jd_output_completed );

		ts_assert_mpi_buffer_has_integer( 1, "node 1 says that it has a message, c", 1 );
		ts_assert_mpi_buffer_has_integer( 1, "node 1 says that it has output job result 6", mpi_work_pool_jd_output_completed );

#endif
	}

	void test_jd3_workpool_worker_two_rounds_one_archive_no_archiving_on_node_0() {
		TS_ASSERT( true );

#ifdef SERIALIZATION
		core_init_with_additional_options( "-jd3::n_archive_nodes 1 -jd3::do_not_archive_on_node0 -jd3::compress_job_results 0" );

		EnergyJobSummaryOP trpcage_job_summary( new EnergyJobSummary );
		trpcage_job_summary->energy( 1234 );
		utility::vector1< JobSummaryOP > summaries( 1, trpcage_job_summary );
		std::string serialized_trpcage_job_summaries = serialized_job_summaries( summaries );

		PoseJobResultOP trpcage_pose_result( new PoseJobResult );
		trpcage_pose_result->pose(  create_trpcage_ideal_poseop() );

		SimulateMPI::initialize_simulation( 3 );

		SimulateMPI::set_mpi_rank( 0 );

		// send 1st job
		send_integer_to_node( 2, mpi_work_pool_jd_new_job_available );
		send_string_to_node( 2, serialized_larval_job( 1, 1, 1 ) );
		send_sizes_to_node( 2, utility::vector1< Size >() );

		// tell node 2 where to send the results of job 1:
		send_integers_to_node( 2, utility::vector1< int > (1,1) );

		SimulateMPI::set_mpi_rank( 1 );
		// ok, node 2 will say that it needs to send us a job, and it'll send it to node 1 (the archive)
		// and then the archive needs to reply that the archival was successful
		send_integer_to_node( 2, mpi_work_pool_jd_archival_completed );

		SimulateMPI::set_mpi_rank( 0 );
		// send 2nd job, which lists job 1 as a required input
		send_integer_to_node( 2, mpi_work_pool_jd_new_job_available );
		send_string_to_node( 2, serialized_T( create_larval_job_w_job1_dep( 1, 1, 2 ) ));
		send_sizes_to_node( 2, utility::vector1< Size >( 1, 1 ) ); // say that job 1's results live on node 1

		SimulateMPI::set_mpi_rank( 1 );
		send_integer_to_node( 2, mpi_work_pool_jd_job_result_retrieved );
		send_string_to_node( 2, serialized_larval_job_and_job_result( 1, 1, 1, trpcage_pose_result ));

		SimulateMPI::set_mpi_rank( 0 );
		// tell node 2 where to send the results of job 2:
		send_integers_to_node( 2, utility::vector1< int > (1,1) );

		SimulateMPI::set_mpi_rank( 1 );
		// ok, node 2 will say that it needs to send us a job, and it'll send it to node 1 (the archive)
		// and then the archive needs to reply that the archival was successful
		send_integer_to_node( 2, mpi_work_pool_jd_archival_completed );

		SimulateMPI::set_mpi_rank( 0 );
		send_integer_to_node( 2, mpi_work_pool_jd_spin_down ); // no more jobs to hand out

		//OK! Now create a PoolJQ3 and let 'er rip
		SimulateMPI::set_mpi_rank( 2 );
		PoolJQ3OP jq3( new PoolJQ3 );
		MPIWorkPoolJobDistributor jd;
		try {
			jd.go( jq3 );
		} catch ( ... ) {
			TS_ASSERT( false /*no exception should be thrown*/ );
		}

		// OK!
		// Now we read the mail we got from node 2

		SimulateMPI::set_mpi_rank( 0 );
		// Node 2 starts by requesting a job
		ts_assert_mpi_buffer_has_integer( 2, "node 2 says to node 0: I have a request A", 2 );
		ts_assert_mpi_buffer_has_integer( 2, "node 2 requests a new job of node 0", mpi_work_pool_jd_new_job_request );

		// Then it replies that its job is complete
		ts_assert_mpi_buffer_has_integer( 2, "node 2 says to node 0: I have a request B", 2 );
		ts_assert_mpi_buffer_has_integer( 2, "node 2 has finished its job", mpi_work_pool_jd_job_success );
		ts_assert_mpi_buffer_has_size( 2, "node 2 finished job #1", 1 );
		ts_assert_mpi_buffer_has_size( 2, "job #1 generated 1 result", 1 );

		ts_assert_mpi_buffer_has_integer( 2, "node 2 says to node 0: I have a request C", 2 );
		ts_assert_mpi_buffer_has_integer( 2, "node 2 says to node 0: I finished archiving my completed job result -- I'm ready to send a JobSummary", mpi_work_pool_jd_job_success_and_archival_complete );
		ts_assert_mpi_buffer_has_size( 2, "node 2 says to node 0: this was for job index 1", 1 );
		ts_assert_mpi_buffer_has_integer( 2, "node 2 reports the job status to node 0: success", jd3_job_status_success );
		std::string serialized_summaries1 = ts_assert_mpi_buffer_has_string( 2, "node 2 says to node 0: here's the summary" );
		utility::vector1< JobSummaryOP > summaries1 = deserialized_job_summaries( serialized_summaries1 );
		TS_ASSERT_EQUALS( summaries1.size(), 1 );
		if ( summaries1.size() != 1 ) return;
		TS_ASSERT( summaries1[1] );
		StandardPoseJobSummaryOP energy_summary1 = utility::pointer::dynamic_pointer_cast< StandardPoseJobSummary > ( summaries1[1] );
		TS_ASSERT( energy_summary1 );
		if ( energy_summary1 ) {
			TS_ASSERT_EQUALS( energy_summary1->energy(), 1234 );
		}

		// Now Node 2 requests another job
		ts_assert_mpi_buffer_has_integer( 2, "node 2 says to node 0: I have a request D", 2 );
		ts_assert_mpi_buffer_has_integer( 2, "node 2 requests a new job of node 0", mpi_work_pool_jd_new_job_request );

		// Then it replies that its job is complete
		ts_assert_mpi_buffer_has_integer( 2, "node 2 says to node 0: I have a request E", 2 );
		ts_assert_mpi_buffer_has_integer( 2, "node 2 has finished its job", mpi_work_pool_jd_job_success );
		ts_assert_mpi_buffer_has_size( 2, "node 2 finished job #2", 2 );
		ts_assert_mpi_buffer_has_size( 2, "job #2 produced one result", 1 );

		ts_assert_mpi_buffer_has_integer( 2, "node 2 says to node 0: I have a request F", 2 );
		ts_assert_mpi_buffer_has_integer( 2, "node 2 says to node 0: I finished archiving my completed job result -- I'm ready to send a JobSummary", mpi_work_pool_jd_job_success_and_archival_complete );
		ts_assert_mpi_buffer_has_size( 2, "node 2 says to node 0: this was for job index 2", 2 );
		ts_assert_mpi_buffer_has_integer( 2, "job 2 was a success", jd3_job_status_success );
		std::string serialized_summaries2 = ts_assert_mpi_buffer_has_string( 2, "node 2 says to node 0: here are the summaries" );
		utility::vector1< JobSummaryOP > summaries2 = deserialized_job_summaries( serialized_summaries2 );
		TS_ASSERT_EQUALS( summaries2.size(), 1 );
		if ( summaries2.size() != 1 ) return;
		TS_ASSERT( summaries2[ 1 ] );
		StandardPoseJobSummaryOP energy_summary2 = utility::pointer::dynamic_pointer_cast< StandardPoseJobSummary > ( summaries2[1] );
		TS_ASSERT( energy_summary2 );
		if ( energy_summary2 ) {
			TS_ASSERT_EQUALS( energy_summary2->energy(), 1234 );
		}

		// Now Node 2 requests another job, and it will turn out that there are no more, so node 0 will send a spin down signal.
		ts_assert_mpi_buffer_has_integer( 2, "node 2 says to node 0: I have a request G", 2 );
		ts_assert_mpi_buffer_has_integer( 2, "node 2 requests a new job of node 0", mpi_work_pool_jd_new_job_request );

		SimulateMPI::set_mpi_rank( 1 );

		// Let's see what node 2 sent to the archive node.
		ts_assert_mpi_buffer_has_integer( 2, "node 2 says \"I have a message for you, archive\"", 2 ); // node 2 says "I have a message for you, archive"
		ts_assert_mpi_buffer_has_integer( 2, "node 2 says \"please archive this job result\"", mpi_work_pool_jd_archive_job_result ); // "please archive this job result"
		ts_assert_mpi_buffer_has_size( 2, "node 2 says this job result is for job #1", 1 ); // job_id
		ts_assert_mpi_buffer_has_size( 2, "node 2 says this job result is for job #1 result #1", 1 ); // result index
		std::string serialized_job_result1 = ts_assert_mpi_buffer_has_string( 2, "node 2 sends the serialized trpcage pose result for job 1" );
		MPIWorkPoolJobDistributor::LarvalJobAndResult job_and_result1 = deserialized_larval_job_and_job_result( serialized_job_result1 );
		TS_ASSERT( job_and_result1.first );
		TS_ASSERT( job_and_result1.second );
		if ( job_and_result1.first ) {
			TS_ASSERT_EQUALS( job_and_result1.first->job_index(), 1 );
			TS_ASSERT_EQUALS( job_and_result1.first->nstruct_index(), 1 );
			TS_ASSERT_EQUALS( job_and_result1.first->nstruct_max(), 1 );
		}
		PoseJobResultOP pose_result1 = utility::pointer::dynamic_pointer_cast< PoseJobResult > ( job_and_result1.second );
		TS_ASSERT( pose_result1 );
		if ( pose_result1 ) {
			TS_ASSERT_EQUALS( pose_result1->pose()->total_residue(), 20 );
		}

		// node 2 then asks for the job results from job 1
		ts_assert_mpi_buffer_has_integer( 2, "node 2 says to the archive that it has a request", 2 );
		ts_assert_mpi_buffer_has_integer( 2, "node 2 says that it needs a job result",  mpi_work_pool_jd_retrieve_job_result );
		ts_assert_mpi_buffer_has_size( 2, "node 2 says that it wants the job with index 1", 1 ); // retrieve the result from job #1
		ts_assert_mpi_buffer_has_size( 2, "node 2 says that it wants the job with index 1 result #1", 1 ); // result index 1

		// finally, node 2 sends the job results from job 2
		ts_assert_mpi_buffer_has_integer( 2, "node 2 says \"I have a message for you, archive\" B", 2 ); // node 2 says "I have a message for you, archive"
		ts_assert_mpi_buffer_has_integer( 2, "node 2 says \"please archive this job result\" B", mpi_work_pool_jd_archive_job_result ); // "please archive this job result"
		ts_assert_mpi_buffer_has_size( 2, "node 2 says that the job id is 2", 2 ); // job_ib
		ts_assert_mpi_buffer_has_size( 2, "node 2 says that the job id is 2 result index 1", 1 ); // result index
		std::string serialized_job_result2 = ts_assert_mpi_buffer_has_string( 2, "node 2 sends the serialized trpcage pose result for job 2" );
		MPIWorkPoolJobDistributor::LarvalJobAndResult job_and_result2 = deserialized_larval_job_and_job_result( serialized_job_result2 );
		TS_ASSERT( job_and_result2.first );
		TS_ASSERT( job_and_result2.second );
		if ( job_and_result2.first ) {
			TS_ASSERT_EQUALS( job_and_result2.first->job_index(), 2 );
			TS_ASSERT_EQUALS( job_and_result2.first->nstruct_index(), 1 );
			TS_ASSERT_EQUALS( job_and_result2.first->nstruct_max(), 1 );
			TS_ASSERT_EQUALS( job_and_result2.first->input_job_result_indices().size(), 1 );
			TS_ASSERT_EQUALS( job_and_result2.first->input_job_result_indices()[ 1 ].first, 1 );
			TS_ASSERT_EQUALS( job_and_result2.first->input_job_result_indices()[ 1 ].second, 1 );
		}
		PoseJobResultOP pose_result2 = utility::pointer::dynamic_pointer_cast< PoseJobResult > ( job_and_result2.second );
		TS_ASSERT( pose_result2 );
		if ( pose_result2 ) {
			TS_ASSERT_EQUALS( pose_result2->pose()->total_residue(), 20 );
		}

#endif
	}

	// Removing the idea that the remote node needs to send back the LarvalJobOP to node0
	void dont_test_MPIWorkPoolJobDistributor_master_node_vanilla_end_to_end_all_successes_jq_requires_larval_jobs_for_summaries() {
		TS_ASSERT( true ); // appease non-serialization builds


#ifdef SERIALIZATION

		EnergyJobSummaryOP trpcage_job_summary( new EnergyJobSummary );
		trpcage_job_summary->energy( 1234 );
		utility::vector1< JobSummaryOP > summaries( 1, trpcage_job_summary );
		std::string serialized_trpcage_job_summaries = serialized_job_summaries( summaries );

		PoseJobResultOP trpcage_pose_result( new PoseJobResult );
		trpcage_pose_result->pose(  create_trpcage_ideal_poseop() );

		SimulateMPI::initialize_simulation( 2 );

		// node 1 requests a job
		SimulateMPI::set_mpi_rank( 1 );
		send_integer_to_node( 0, 1 );
		send_integer_to_node( 0, mpi_work_pool_jd_new_job_request );

		// node 1 starts its two part job-completion process,
		SimulateMPI::set_mpi_rank( 1 );
		send_integer_to_node( 0, 1 );
		send_integer_to_node( 0, mpi_work_pool_jd_job_success );
		send_size_to_node( 0, 1 ); // job id
		send_size_to_node( 0, 1 ); // num results

		send_integer_to_node( 0, 1 );
		send_integer_to_node( 0, mpi_work_pool_jd_archive_job_result );
		send_size_to_node( 0, 1 ); // job id
		send_size_to_node( 0, 1 ); // result id
		send_string_to_node( 0, serialized_larval_job_and_job_result( 1, 1, 1, trpcage_pose_result ));

		send_integer_to_node( 0, 1 );
		send_integer_to_node( 0, mpi_work_pool_jd_job_success_and_archival_complete );
		send_string_to_node( 0, serialized_larval_job( 1, 1, 1 ) ); // NEW! Node 1 has to send the serialized larval job back to node 0
		send_integer_to_node( 0, jd3_job_status_success );
		send_string_to_node( 0, serialized_trpcage_job_summaries );

		// Now node 1 asks for a new job
		send_integer_to_node( 0, 1 );
		send_integer_to_node( 0, mpi_work_pool_jd_new_job_request );

		// ok -- now we create a JQ on node 0, set it up to produce three jobs
		// and then create a job distributor for node 0 and tell it to go go go!

		SimulateMPI::set_mpi_rank( 0 );
		PoolJQ4OP jq( new PoolJQ4 );
		jq->njobs_ = 1;

		MPIWorkPoolJobDistributor jd;
		try {
			jd.go( jq );
		} catch ( ... ) {
			TS_ASSERT( false /*no exception should be thrown*/ );
		}

		// OK!
		// Now we read the mail we got from node 0

		SimulateMPI::set_mpi_rank( 1 );
		ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 1 that it has a job for it to run", mpi_work_pool_jd_new_job_available );
		std::string ser_larv1 = ts_assert_mpi_buffer_has_string( 0, "node 0 sends node 1 the serialized LarvalJob" );
		LarvalJobOP larval_job1 = deserialize_larval_job( ser_larv1 );
		TS_ASSERT_EQUALS( larval_job1->job_index(), 1 );
		TS_ASSERT_EQUALS( larval_job1->nstruct_index(), 1 );
		TS_ASSERT_EQUALS( larval_job1->nstruct_max(), 1 );
		ts_assert_mpi_buffer_has_sizes( 0, "node 0 sends node 1 an empty list of job result indices", utility::vector1< Size >() );

		ts_assert_mpi_buffer_has_integers( 0, "node 0 tells node 1 to archive its results on node 0", utility::vector1< int > ( 1, 0 ) );
		ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 1 that archival completed", mpi_work_pool_jd_archival_completed );

		ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 1 that there are no jobs left to run", mpi_work_pool_jd_spin_down );

		TS_ASSERT_EQUALS( jq->status_.count( 1 ), 1 );
		TS_ASSERT_EQUALS( jq->status_[ 1 ], jd3_job_status_success );

		// The job distributor should have used the note_job_completed function that takes
		// a LarvalJobOP as its first argument (and not the one that takes a Size as its first
		// argument
		TS_ASSERT_EQUALS( jq->jobs_completed_through_larval_job_interface_.size(), 1 );
		TS_ASSERT_EQUALS( jq->jobs_completed_through_larval_job_interface_[ 1 ], 1 );

		// The job distributor should have used the completed_job_summary function that takes
		// a LarvalJobOP as its first argument (and not the one that takes a Size as its first
		// argument
		TS_ASSERT_EQUALS( jq->summaries_through_larval_job_interface_.size(), 1 );
		TS_ASSERT_EQUALS( jq->summaries_through_larval_job_interface_[ 1 ], 1 );

		TS_ASSERT_EQUALS( jq->summaries_.count( sp1(1) ), 1 );
		TS_ASSERT_EQUALS( utility::pointer::dynamic_pointer_cast< EnergyJobSummary > ( jq->summaries_[ sp1(1) ])->energy(), 1234 );

		TS_ASSERT_EQUALS( jq->results_->count( sp1(1) ), 1 );
		TS_ASSERT_EQUALS( utility::pointer::dynamic_pointer_cast< PoseJobResult > ( (*jq->results_)[ sp1(1) ])->pose()->total_residue(), 20 );

#endif
	}

	void dont_test_workpool_jd3_worker_node_vanilla_end_to_end_all_successes_jq_requires_larval_jobs_for_summaries() {
		TS_ASSERT( true );


#ifdef SERIALIZATION
		SimulateMPI::initialize_simulation( 2 );

		SimulateMPI::set_mpi_rank(0);

		// send 1st job
		send_integer_to_node( 1, mpi_work_pool_jd_new_job_available );
		send_string_to_node( 1, serialized_larval_job( 1, 1, 1 ) );
		send_sizes_to_node( 1, utility::vector1< Size >() );

		// tell node 1 where to send the results of job 1:
		send_integers_to_node( 1, utility::vector1< int >( 1, 0 ) );
		send_integer_to_node( 1, mpi_work_pool_jd_archival_completed );

		// Now tell node 1 to spin dow
		send_integer_to_node( 1, mpi_work_pool_jd_spin_down );

		try {
			SimulateMPI::set_mpi_rank( 1 );
			PoolJQ4OP jq4( new PoolJQ4 );
			MPIWorkPoolJobDistributor jd;
			jd.go( jq4 );
		} catch ( ... ) {
			std::cerr << "Exception thrown from worker node in a unit test that should not have thrown an exception!" << std::endl;
			TS_ASSERT( false );
			return;
		}

		// NOW let's make sure that node 1 has been sending its messages the way it should have
		SimulateMPI::set_mpi_rank( 0 );
		// Node 1 starts by requesting a job
		ts_assert_mpi_buffer_has_integer( 1, "node 1 says to node 0: I have a request A", 1 );
		ts_assert_mpi_buffer_has_integer( 1, "node 1 requests a new job of node 0", mpi_work_pool_jd_new_job_request );

		// Then it replies that its job is complete
		ts_assert_mpi_buffer_has_integer( 1, "node 1 says to node 0: I have a request B", 1 );
		ts_assert_mpi_buffer_has_integer( 1, "node 1 has finished its job", mpi_work_pool_jd_job_success );
		ts_assert_mpi_buffer_has_size( 1, "node 1 finished job 1", 1 );
		ts_assert_mpi_buffer_has_size( 1, "node 1 finished job 1 producing 1 result", 1 );

		ts_assert_mpi_buffer_has_integer( 1, "node 1 says to node 0: I have a request C", 1 );
		ts_assert_mpi_buffer_has_integer( 1, "node 1 has finished its job", mpi_work_pool_jd_archive_job_result );
		ts_assert_mpi_buffer_has_size( 1, "node 1 job 1", 1 );
		ts_assert_mpi_buffer_has_size( 1, "node 1 job 1 result 1", 1 );
		std::string serialized_job_result1 = ts_assert_mpi_buffer_has_string( 1, "node 1 send the serialized trpcage pose result" );
		MPIWorkPoolJobDistributor::LarvalJobAndResult job_and_result1 = deserialized_larval_job_and_job_result( serialized_job_result1 );
		TS_ASSERT( job_and_result1.first );
		TS_ASSERT( job_and_result1.second );
		if ( job_and_result1.first ) {
			TS_ASSERT_EQUALS( job_and_result1.first->job_index(), 1 );
			TS_ASSERT_EQUALS( job_and_result1.first->nstruct_index(), 1 );
			TS_ASSERT_EQUALS( job_and_result1.first->nstruct_max(), 1 );
		}
		PoseJobResultOP pose_result1 = utility::pointer::dynamic_pointer_cast< PoseJobResult > ( job_and_result1.second );
		TS_ASSERT( pose_result1 );
		if ( pose_result1 ) {
			TS_ASSERT_EQUALS( pose_result1->pose()->total_residue(), 20 );
		}

		ts_assert_mpi_buffer_has_integer( 1, "node 1 says to node 0: I have a request D", 1 );
		ts_assert_mpi_buffer_has_integer( 1, "node 1 says to node 0: I finished archiving my completed job result -- I'm ready to send a JobSummary", mpi_work_pool_jd_job_success_and_archival_complete );
		std::string serialized_larval_job1 = ts_assert_mpi_buffer_has_string( 1, "node 1 finished this particular larval job (for job 1)" );
		LarvalJobOP larval_job1 = deserialize_larval_job( serialized_larval_job1 );
		TS_ASSERT( larval_job1 );
		if ( larval_job1 ) {
			TS_ASSERT_EQUALS( larval_job1->job_index(), 1 );
			TS_ASSERT_EQUALS( larval_job1->nstruct_index(), 1 );
			TS_ASSERT_EQUALS( larval_job1->nstruct_max(), 1 );
		}

		ts_assert_mpi_buffer_has_integer( 1, "node 1 is reporting the job status for job 1", jd3_job_status_success );
		std::string serialized_summary1 = ts_assert_mpi_buffer_has_string( 1, "node 1 says to node 0: here's the summary" );
		utility::vector1< JobSummaryOP > summaries1 = deserialized_job_summaries( serialized_summary1 );
		TS_ASSERT_EQUALS( summaries1.size(), 1 );
		if ( summaries1.size() != 1 ) return;
		TS_ASSERT( summaries1[1] );
		EnergyJobSummaryOP energy_summary1 = utility::pointer::dynamic_pointer_cast< EnergyJobSummary > ( summaries1[1] );
		TS_ASSERT( energy_summary1 );
		if ( energy_summary1 ) {
			TS_ASSERT_EQUALS( energy_summary1->energy(), 1234 );
		}

		// OK -- now node 1 should ask for a job for a final time
		ts_assert_mpi_buffer_has_integer( 1, "node 1 says to node 0: I have a request", 1 );
		ts_assert_mpi_buffer_has_integer( 1, "node 1 requests a new job of node 0", mpi_work_pool_jd_new_job_request );


#endif
	}

	void test_jd3_workpool_archive_output_result_from_other_archive() {
		TS_ASSERT( true );

#ifdef SERIALIZATION
		core_init_with_additional_options( "-jd3::n_archive_nodes 2 -jd3::do_not_archive_on_node0 -jd3::compress_job_results 0" );

		EnergyJobSummaryOP trpcage_job_summary( new EnergyJobSummary );
		trpcage_job_summary->energy( 1234 );
		utility::vector1< JobSummaryOP > summaries( 1, trpcage_job_summary );
		std::string serialized_trpcage_job_summaries = serialized_job_summaries( summaries );

		PoseJobResultOP trpcage_pose_result( new PoseJobResult );
		trpcage_pose_result->pose(  create_trpcage_ideal_poseop() );

		SimulateMPI::initialize_simulation( 5 );

		SimulateMPI::set_mpi_rank( 0 );
		send_integer_to_node( 1, 0 ); // master node says "I have a message for you, archive"
		send_integer_to_node( 1, mpi_work_pool_jd_output_job_result_on_archive ); // "please output a job result that lives on another node"
		send_integer_to_node( 1, 2 );
		send_string_to_node( 1, serialized_output_specification( JobResultID( 6, 1 ), joi(6), "dummy_out.pdb" ));

		SimulateMPI::set_mpi_rank(2);
		send_integer_to_node( 1, mpi_work_pool_jd_job_result_retrieved );
		send_string_to_node( 1, serialized_larval_job_and_job_result( 1, 1, 6, trpcage_pose_result ) );

		SimulateMPI::set_mpi_rank(0);
		send_integer_to_node( 1, 0 ); // node 0 says "hey, I need to talk to you"
		send_integer_to_node( 1, mpi_work_pool_jd_spin_down ); // time to shut down

		//OK! Now create a PoolJQ3 and let 'er rip
		SimulateMPI::set_mpi_rank( 1 );
		PoolJQ1OP jq1( new PoolJQ1 );
		MPIWorkPoolJobDistributor jd;
		try {
			jd.go( jq1 );
		} catch ( ... ) {
			TS_ASSERT( false /*no exception should be thrown*/ );
		}

		TS_ASSERT_EQUALS( jq1->dummy_outputter_->named_poses_.count( "dummy_out.pdb" ), 1 );

		SimulateMPI::set_mpi_rank( 0 ); // the master node

		ts_assert_mpi_buffer_has_integer( 1, "node 1 says that it has a message", 1 );
		ts_assert_mpi_buffer_has_integer( 1, "node 1 says that it has output job result 1", mpi_work_pool_jd_output_completed );

		SimulateMPI::set_mpi_rank( 2 ); // the seconda archive
		ts_assert_mpi_buffer_has_integer( 1, "node 1 says that it has a message, 2", 1 );
		ts_assert_mpi_buffer_has_integer( 1, "node 1 say that it needs a job result", mpi_work_pool_jd_retrieve_and_discard_job_result );
		ts_assert_mpi_buffer_has_size( 1, "node 1 says the job result it needs is from job 6", 6 );
		ts_assert_mpi_buffer_has_size( 1, "node 1 says the job result for job 6 it needs is #1", 1 );

		SimulateMPI::set_mpi_rank( 1 ); // the archive

		TS_ASSERT( SimulateMPI::incoming_message_queue_is_empty() );
		TS_ASSERT( SimulateMPI::outgoing_message_queue_is_empty() );

#endif
	}


	void test_master_node_response_to_failure_to_retrieve_job_result_from_archive()
	{
		TS_ASSERT( true );


#ifdef SERIALIZATION

		core_init_with_additional_options( "-jd3::n_archive_nodes 1 -jd3::do_not_archive_on_node0 -jd3::compress_job_results 0" );

		SimulateMPI::initialize_simulation( 3 );

		// ok -- the set up will be that node 1 is going to ask the archive for job result #1
		// and the archive is going to say that the job result isn't there.
		// In this test, the master node will think that the archive should have the job result.

		EnergyJobSummaryOP trpcage_job_summary( new EnergyJobSummary );
		trpcage_job_summary->energy( 1234 );
		utility::vector1< JobSummaryOP > summaries( 1, trpcage_job_summary );
		std::string serialized_trpcage_job_summaries = serialized_job_summaries( summaries );


		PoseJobResultOP trpcage_pose_result( new PoseJobResult );
		trpcage_pose_result->pose(  create_trpcage_ideal_poseop() );

		// node 1 requests a job
		SimulateMPI::set_mpi_rank( 2 );
		send_integer_to_node( 0, 2 );
		send_integer_to_node( 0, mpi_work_pool_jd_new_job_request );

		// node 2 starts its two part job-completion process,
		send_integer_to_node( 0, 2 );
		send_integer_to_node( 0, mpi_work_pool_jd_job_success );
		send_size_to_node( 0, 1 ); // job id
		send_size_to_node( 0, 1 ); // num results
		// and node 0 should say that node 2 ought to archive its result on node 1

		send_integer_to_node( 0, 2 );
		send_integer_to_node( 0, mpi_work_pool_jd_job_success_and_archival_complete );
		send_size_to_node( 0, 1 ); // node 1 says it finished job #1
		send_integer_to_node( 0, jd3_job_status_success );
		send_string_to_node( 0, serialized_trpcage_job_summaries );

		// Now node 2 asks for a new job
		send_integer_to_node( 0, 2 );
		send_integer_to_node( 0, mpi_work_pool_jd_new_job_request );

		// node 2 requests a job
		send_integer_to_node( 0, 2 );
		send_integer_to_node( 0, mpi_work_pool_jd_new_job_request );

		// node 1 tells node 0 about a failure to retrieve a job result
		SimulateMPI::set_mpi_rank( 1 );
		send_integer_to_node( 0, 1 );
		send_integer_to_node( 0, mpi_work_pool_jd_failed_to_retrieve_job_result );
		send_integer_to_node( 0, 2 ); // node 2 was the one who requested a job
		send_size_to_node( 0, 1 ); // it was job #1 that node 2 requested.
		send_size_to_node( 0, 1 ); // it was job #1 result #1 that node 2 requested.

		SimulateMPI::set_mpi_rank( 0 );

		PoolJQ3OP jq3( new PoolJQ3 );
		jq3->node_1_njobs_ = jq3->node_2_njobs_ = 1;
		MPIWorkPoolJobDistributor jd;
		try {
			jd.go( jq3 );
			TS_ASSERT( false ); // this should not be reached.
		} catch (utility::excn::Exception & e ) {
			std::string expected_err_msg = "Failed to retrieve job result (1, 1) which was requested from node 1 by node 2"
				" but was not present there.\nJobDistributor on node 0 thinks the result should have been on node 1";
			TS_ASSERT( e.msg().find(expected_err_msg) != std::string::npos );
		}

#endif
	}

	void test_master_node_response_to_failure_to_retrieve_job_result_from_archive_job_result_discarded()
	{
		TS_ASSERT( true );


#ifdef SERIALIZATION

		core_init_with_additional_options( "-jd3::n_archive_nodes 1 -jd3::do_not_archive_on_node0 -jd3::compress_job_results 0" );

		SimulateMPI::initialize_simulation( 4 );

		// there will be two rounds: round 1 has two jobs, round 2 has two jobs.
		// After round 1, job #1 will be preserved on the archive, and job #2 will be discarded.
		// Jobs 3 and 4 will list job #1's output as a required input.
		// Job 3 will run and finish, and then the job queen will tell the job distributor
		// to discard the result from job 1.
		// THEN Job 4 will request job 1 from the archive and the archive will tell node 0
		// that it doesn't have it.
		// The test is to make sure that the JobDistributor can provide a useful output
		// message saying that job 1 had been discarded.

		EnergyJobSummaryOP trpcage_job_summary( new EnergyJobSummary );
		trpcage_job_summary->energy( 1234 );
		utility::vector1< JobSummaryOP > summaries( 1, trpcage_job_summary );
		std::string serialized_trpcage_job_summaries = serialized_job_summaries( summaries );

		PoseJobResultOP trpcage_pose_result( new PoseJobResult );
		trpcage_pose_result->pose(  create_trpcage_ideal_poseop() );

		// node 1 requests a job
		SimulateMPI::set_mpi_rank( 2 );
		send_integer_to_node( 0, 2 );
		send_integer_to_node( 0, mpi_work_pool_jd_new_job_request );

		SimulateMPI::set_mpi_rank( 3 );
		send_integer_to_node( 0, 3 );
		send_integer_to_node( 0, mpi_work_pool_jd_new_job_request );

		// node 2 starts its two part job-completion process,
		SimulateMPI::set_mpi_rank( 2 );
		send_integer_to_node( 0, 2 );
		send_integer_to_node( 0, mpi_work_pool_jd_job_success );
		send_size_to_node( 0, 1 ); // job id
		send_size_to_node( 0, 1 ); // num results
		// and node 0 should say that node 2 ought to archive its result on node 1

		// node 3 starts its two part job-completion process,
		SimulateMPI::set_mpi_rank( 3 );
		send_integer_to_node( 0, 3 );
		send_integer_to_node( 0, mpi_work_pool_jd_job_success );
		send_size_to_node( 0, 2 ); // job id
		send_size_to_node( 0, 1 ); // num results
		// and node 0 should say that node 2 ought to archive its result on node 1

		SimulateMPI::set_mpi_rank( 2 );
		send_integer_to_node( 0, 2 );
		send_integer_to_node( 0, mpi_work_pool_jd_job_success_and_archival_complete );
		send_size_to_node( 0, 1 ); // node 2 says it finished job #1
		send_integer_to_node( 0, jd3_job_status_success ); // job #1 completed successfully
		send_string_to_node( 0, serialized_trpcage_job_summaries );

		SimulateMPI::set_mpi_rank( 3 );
		send_integer_to_node( 0, 3 );
		send_integer_to_node( 0, mpi_work_pool_jd_job_success_and_archival_complete );
		send_size_to_node( 0, 2 ); // node 3 says it finished job #1
		send_integer_to_node( 0, jd3_job_status_success ); // job #2 completed successfully
		send_string_to_node( 0, serialized_trpcage_job_summaries );

		SimulateMPI::set_mpi_rank( 2 );
		// Now node 2 asks for a new job
		send_integer_to_node( 0, 2 );
		send_integer_to_node( 0, mpi_work_pool_jd_new_job_request );

		SimulateMPI::set_mpi_rank( 3 );
		// Now node 3 asks for a new job
		send_integer_to_node( 0, 3 );
		send_integer_to_node( 0, mpi_work_pool_jd_new_job_request );


		// now node 2 is going to say it has finished its job
		SimulateMPI::set_mpi_rank( 2 );
		send_integer_to_node( 0, 2 );
		send_integer_to_node( 0, mpi_work_pool_jd_job_success );
		send_size_to_node( 0, 3 ); // job id
		send_size_to_node( 0, 1 ); // num results

		send_integer_to_node( 0, 2 );
		send_integer_to_node( 0, mpi_work_pool_jd_job_success_and_archival_complete );
		send_size_to_node( 0, 3 ); // node 1 says it finished job #3
		send_integer_to_node( 0, jd3_job_status_success ); // job #3 completed successfully
		send_string_to_node( 0, serialized_trpcage_job_summaries );

		// And we pretend that node 3 is only just now getting started in retrieving
		// the job inputs from the archive but oh no! the job inputs it needs aren't there
		// any more
		// node 1 tells node 0 about a failure to retrieve a job result
		SimulateMPI::set_mpi_rank( 1 );
		send_integer_to_node( 0, 1 );
		send_integer_to_node( 0, mpi_work_pool_jd_failed_to_retrieve_job_result );
		send_integer_to_node( 0, 3 ); // node 3 was the one who requested a job
		send_size_to_node( 0, 1 ); // it was job #1 that node 3 requested.
		send_size_to_node( 0, 1 ); // it was job #1 result #1 that node 3 requested.

		SimulateMPI::set_mpi_rank( 0 );

		PoolJQ3bOP jq3b( new PoolJQ3b );
		MPIWorkPoolJobDistributor jd;
		try {
			jd.go( jq3b );
			TS_ASSERT( false ); // this should not be reached.
		} catch (utility::excn::Exception & e ) {
			std::string expected_err_msg = "Failed to retrieve job result (1, 1) which was requested from node 1 by node 3"
				" but was not present there.\nThis job is not listed as still running nor as having its JobResult stored on any archive node; it perhaps has been output or discarded already.";
			TS_ASSERT( e.msg().find(expected_err_msg) != std::string::npos );
		}

#endif
	}

	void test_master_node_response_to_failure_to_retrieve_job_result_from_archive_job_previously_output()
	{
		TS_ASSERT( true );


#ifdef SERIALIZATION

		core_init_with_additional_options( "-jd3::n_archive_nodes 1 -jd3::do_not_archive_on_node0 -jd3::compress_job_results 0" );

		SimulateMPI::initialize_simulation( 4 );

		// there will be two rounds: round 1 has two jobs, round 2 has two jobs.
		// After round 1, job #1 will be preserved on the archive, and job #2 will be discarded.
		// Jobs 3 and 4 will list job #1's output as a required input.
		// Job 3 will run and finish, and then the job queen will tell the job distributor
		// to output the result from job 1 (removing it from the archive).
		// THEN Job 4 will request job 1 from the archive and the archive will tell node 0
		// that it doesn't have it.
		// The test is to make sure that the JobDistributor can provide a useful output
		// message saying that job 1 had been discarded.


		EnergyJobSummaryOP trpcage_job_summary( new EnergyJobSummary );
		trpcage_job_summary->energy( 1234 );
		utility::vector1< JobSummaryOP > summaries( 1, trpcage_job_summary );
		std::string serialized_trpcage_job_summaries = serialized_job_summaries( summaries );

		PoseJobResultOP trpcage_pose_result( new PoseJobResult );
		trpcage_pose_result->pose(  create_trpcage_ideal_poseop() );

		// node 1 requests a job
		SimulateMPI::set_mpi_rank( 2 );
		send_integer_to_node( 0, 2 );
		send_integer_to_node( 0, mpi_work_pool_jd_new_job_request );

		SimulateMPI::set_mpi_rank( 3 );
		send_integer_to_node( 0, 3 );
		send_integer_to_node( 0, mpi_work_pool_jd_new_job_request );

		// node 2 starts its two part job-completion process,
		SimulateMPI::set_mpi_rank( 2 );
		send_integer_to_node( 0, 2 );
		send_integer_to_node( 0, mpi_work_pool_jd_job_success );
		send_size_to_node( 0, 1 ); // job id
		send_size_to_node( 0, 1 ); // num results
		// and node 0 should say that node 2 ought to archive its result on node 1

		// node 3 starts its two part job-completion process,
		SimulateMPI::set_mpi_rank( 3 );
		send_integer_to_node( 0, 3 );
		send_integer_to_node( 0, mpi_work_pool_jd_job_success );
		send_size_to_node( 0, 2 ); // job id
		send_size_to_node( 0, 1 ); // num results
		// and node 0 should say that node 2 ought to archive its result on node 1

		SimulateMPI::set_mpi_rank( 2 );
		send_integer_to_node( 0, 2 );
		send_integer_to_node( 0, mpi_work_pool_jd_job_success_and_archival_complete );
		send_size_to_node( 0, 1 ); // node 2 says it finished job #1
		send_integer_to_node( 0, jd3_job_status_success ); // node 2 says it finished job #1 successfully
		send_string_to_node( 0, serialized_trpcage_job_summaries );

		SimulateMPI::set_mpi_rank( 3 );
		send_integer_to_node( 0, 3 );
		send_integer_to_node( 0, mpi_work_pool_jd_job_success_and_archival_complete );
		send_size_to_node( 0, 2 ); // node 3 says it finished job #2
		send_integer_to_node( 0, jd3_job_status_success ); // node 3 says it finished job #2 successfully
		send_string_to_node( 0, serialized_trpcage_job_summaries );

		SimulateMPI::set_mpi_rank( 2 );
		// Now node 2 asks for a new job
		send_integer_to_node( 0, 2 );
		send_integer_to_node( 0, mpi_work_pool_jd_new_job_request );

		SimulateMPI::set_mpi_rank( 3 );
		// Now node 3 asks for a new job
		send_integer_to_node( 0, 3 );
		send_integer_to_node( 0, mpi_work_pool_jd_new_job_request );


		// now node 2 is going to say it has finished its job
		SimulateMPI::set_mpi_rank( 2 );
		send_integer_to_node( 0, 2 );
		send_integer_to_node( 0, mpi_work_pool_jd_job_success );
		send_size_to_node( 0, 3 ); // job id
		send_size_to_node( 0, 1 ); // num results

		send_integer_to_node( 0, 2 );
		send_integer_to_node( 0, mpi_work_pool_jd_job_success_and_archival_complete );
		send_size_to_node( 0, 3 ); // node 2 says it finished job #3
		send_integer_to_node( 0, jd3_job_status_success ); // node 2 says it finished job #3 successfully
		send_string_to_node( 0, serialized_trpcage_job_summaries );

		SimulateMPI::set_mpi_rank( 1 );
		// ok, now the archive is going to receive a job output requests from
		// node 0 for job 1
		send_integer_to_node( 0, 1 );
		send_integer_to_node( 0, mpi_work_pool_jd_output_completed );

		// And we pretend that node 3 is only just now getting started in retrieving
		// the job inputs from the archive .. so node 3 and node 1 start talking (but we don't get
		// to see their conversation) but oh no! the job inputs it needs aren't there on the archive
		// any more!
		// The archive then tells node 0 about a failure to retrieve a job result
		SimulateMPI::set_mpi_rank( 1 );
		send_integer_to_node( 0, 1 );
		send_integer_to_node( 0, mpi_work_pool_jd_failed_to_retrieve_job_result );
		send_integer_to_node( 0, 3 ); // node 3 was the one who requested a job
		send_size_to_node( 0, 1 ); // it was job #1 that node 3 requested.
		send_size_to_node( 0, 1 ); // it was job #1 result #1 that node 3 requested.

		SimulateMPI::set_mpi_rank( 0 );

		PoolJQ3bOP jq3b( new PoolJQ3b );
		jq3b->discard_job1_result_ = false; // instead, output job 1's result (removing it from the archive)
		MPIWorkPoolJobDistributor jd;
		try {
			jd.go( jq3b );
			TS_ASSERT( false ); // this should not be reached.
		} catch (utility::excn::Exception & e ) {
			std::string expected_err_msg = "Failed to retrieve job result (1, 1) which was requested from node 1 by node 3"
				" but was not present there.\nThis job is not listed as still running nor as having its JobResult stored on any archive node; it perhaps has been output or discarded already.";
			TS_ASSERT( e.msg().find(expected_err_msg) != std::string::npos );
		}

#endif
	}


	void test_work_pool_jd_archive_asked_for_result_it_doesnt_have()
	{
		TS_ASSERT( true );


#ifdef SERIALIZATION

		core_init_with_additional_options( "-jd3::n_archive_nodes 1 -jd3::do_not_archive_on_node0 -jd3::compress_job_results 0" );

		SimulateMPI::initialize_simulation( 4 );

		SimulateMPI::set_mpi_rank( 2 );

		// now retrieve the job result for job 1
		send_integer_to_node( 1, 2 );
		send_integer_to_node( 1, mpi_work_pool_jd_retrieve_job_result );
		send_size_to_node( 1, 1 ); // retrieve the result from job #1
		send_size_to_node( 1, 1 ); // retrieve the result from job #1

		// the archive doesn't directly exit -- it expects the JobDistributor on node 0 to exit once it has
		// recieved the error message (and when one MPI process exits, the whole job dies), so we actually
		// have to send a spin down signal to the archive to get the archive to exit it's listener loop
		SimulateMPI::set_mpi_rank( 0 );
		send_integer_to_node( 1, 0 );
		send_integer_to_node( 1, mpi_work_pool_jd_spin_down );


		// OK -- let's see what the archive node does
		SimulateMPI::set_mpi_rank( 1 );
		PoolJQ3bOP jq( new PoolJQ3b );
		MPIWorkPoolJobDistributor jd;
		try {
			jd.go( jq );
		} catch ( ... ) {
			TS_ASSERT( false /*no exception should be thrown*/ );
		}

		// OK -- what message did nodes 0 and 2 receive?
		SimulateMPI::set_mpi_rank( 0 );
		ts_assert_mpi_buffer_has_integer( 1, "node 1 tells node 0 that it needs to talk with it", 1 );
		ts_assert_mpi_buffer_has_integer( 1, "node 1 says that it failed to retrieve a job result", mpi_work_pool_jd_failed_to_retrieve_job_result );
		ts_assert_mpi_buffer_has_integer( 1, "node 1 says that it was node 2 that asked for the job result", 2 ); // node 2 did the requesting
		ts_assert_mpi_buffer_has_size( 1, "node 1 says that node 2 was asking for job 1", 1 ); // it was job 1 whose result was requested
		ts_assert_mpi_buffer_has_size( 1, "node 1 says that node 2 was asking for job 1 result 1 ", 1 ); // it was job 1 result 1 that was requested

		SimulateMPI::set_mpi_rank( 2 );
		ts_assert_mpi_buffer_has_integer( 1, "node 2 says that it could not retrieve the result that node 2 had requested", mpi_work_pool_jd_failed_to_retrieve_job_result );

#endif
	}

	void test_work_pool_jd_manager_asked_for_result_it_doesnt_have()
	{
		TS_ASSERT( true );


#ifdef SERIALIZATION

		SimulateMPI::initialize_simulation( 2 );

		SimulateMPI::set_mpi_rank( 1 );

		// just cut right to the chase: before even running a single job, node 1 is going to ask node 0 for a job result
		// that node 0 does not have.
		// now retrieve the job result for job 1
		send_integer_to_node( 0, 1 );
		send_integer_to_node( 0, mpi_work_pool_jd_retrieve_job_result );
		send_size_to_node( 0, 1 ); // retrieve the result from job #1
		send_size_to_node( 0, 1 ); // retrieve the result from job #1 result #1

		SimulateMPI::set_mpi_rank( 0 );

		PoolJQ3bOP jq3b( new PoolJQ3b );
		MPIWorkPoolJobDistributor jd;
		try {
			jd.go( jq3b );
			TS_ASSERT( false ); // this should not be reached.
		} catch (utility::excn::Exception & e ) {
			std::string expected_err_msg = "Failed to retrieve job result (1, 1) which was requested from node 0 by node 1"
				" but was not present there.\nThis job is not listed"
				" as still running nor as having its JobResult stored on any archive node; it perhaps has been output or discarded already.";
			TS_ASSERT( e.msg().find(expected_err_msg) != std::string::npos );
		}
#endif
	}

	void test_send_bad_message_to_master_node_should_never_happen()
	{
		TS_ASSERT( true );


#ifdef SERIALIZATION

		SimulateMPI::initialize_simulation( 2 );

		SimulateMPI::set_mpi_rank( 1 );

		// just cut right to the chase: before even running a single job, node 1 is going to send a spin_down
		// signal to node 0, and node 0 just doesn't take that kind of instruction from a worker.
		send_integer_to_node( 0, 1 );
		send_integer_to_node( 0, mpi_work_pool_jd_spin_down );

		SimulateMPI::set_mpi_rank( 0 );

		PoolJQ3bOP jq3b( new PoolJQ3b );
		MPIWorkPoolJobDistributor jd;
		try {
			jd.go( jq3b );
			TS_ASSERT( false ); // this should not be reached.
		} catch (utility::excn::Exception & e ) {
			std::string expected_err_msg = "Internal Error in the MPIWorkPoolJobDistributor: "
				"received inappropriate signal on node 0 from node 1: " + utility::to_string( mpi_work_pool_jd_spin_down );
			TS_ASSERT( e.msg().find(expected_err_msg) != std::string::npos );
		}
#endif
	}

	void test_work_pool_jd_job_digraph_updater_and_job_node_w_zero_jobs()
	{
		TS_ASSERT( true );

#ifdef SERIALIZATION

		SimulateMPI::initialize_simulation( 2 );

		EnergyJobSummaryOP trpcage_job_summary( new EnergyJobSummary );
		trpcage_job_summary->energy( 1234 );
		utility::vector1< JobSummaryOP > summaries( 1, trpcage_job_summary );
		std::string serialized_trpcage_job_summaries = serialized_job_summaries( summaries );

		PoseJobResultOP trpcage_pose_result( new PoseJobResult );
		trpcage_pose_result->pose(  create_trpcage_ideal_poseop() );

		// node 1 requests a job
		SimulateMPI::set_mpi_rank( 1 );
		send_integer_to_node( 0, 1 );
		send_integer_to_node( 0, mpi_work_pool_jd_new_job_request );

		// node 1 starts its two part job-completion process,
		send_integer_to_node( 0, 1 );
		send_integer_to_node( 0, mpi_work_pool_jd_job_success );
		send_size_to_node( 0, 1 ); // job id
		send_size_to_node( 0, 1 ); // num results

		send_integer_to_node( 0, 1 );
		send_integer_to_node( 0, mpi_work_pool_jd_archive_job_result );
		send_size_to_node( 0, 1 ); // job id
		send_size_to_node( 0, 1 ); // result id
		send_string_to_node( 0, serialized_larval_job_and_job_result( 1, 1, 1, trpcage_pose_result ));

		send_integer_to_node( 0, 1 );
		send_integer_to_node( 0, mpi_work_pool_jd_job_success_and_archival_complete );
		send_size_to_node( 0, 1 ); // job id
		send_integer_to_node( 0, jd3_job_status_success ); // node 1 says it finished job #1 successfully
		send_string_to_node( 0, serialized_trpcage_job_summaries );

		send_integer_to_node( 0, 1 );
		send_integer_to_node( 0, mpi_work_pool_jd_new_job_request );

		// node 1 starts its two part job-completion process,
		send_integer_to_node( 0, 1 );
		send_integer_to_node( 0, mpi_work_pool_jd_job_success );
		send_size_to_node( 0, 2 ); // job id
		send_size_to_node( 0, 1 ); // num results

		send_integer_to_node( 0, 1 );
		send_integer_to_node( 0, mpi_work_pool_jd_archive_job_result );
		send_size_to_node( 0, 2 ); // job id
		send_size_to_node( 0, 1 ); // num results
		send_string_to_node( 0, serialized_larval_job_and_job_result( 1, 1, 2, trpcage_pose_result ));

		send_integer_to_node( 0, 1 );
		send_integer_to_node( 0, mpi_work_pool_jd_job_success_and_archival_complete );
		send_size_to_node( 0, 2 ); // job id
		send_integer_to_node( 0, jd3_job_status_success ); // node 1 says it finished job #2 successfully
		send_string_to_node( 0, serialized_trpcage_job_summaries );

		// Now node 1 asks for a new job, but won't get one
		send_integer_to_node( 0, 1 );
		send_integer_to_node( 0, mpi_work_pool_jd_new_job_request );

		// Now let's create a JQ that will use the JobDigraphUpdater
		SimulateMPI::set_mpi_rank( 0 );
		PoolJQ5OP jq( new PoolJQ5 );

		MPIWorkPoolJobDistributor jd;
		try {
			jd.go( jq );
		} catch ( ... ) {
			TS_ASSERT( false /*no exception should be thrown*/ );
		}

		// And now we read the mail we got from node 0
		SimulateMPI::set_mpi_rank( 1 );

		ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 1 that it has a job for it to run", mpi_work_pool_jd_new_job_available );
		std::string ser_larv1 = ts_assert_mpi_buffer_has_string( 0, "node 0 sends node 1 the serialized LarvalJob" );
		LarvalJobOP larval_job1 = deserialize_larval_job( ser_larv1 );
		TS_ASSERT_EQUALS( larval_job1->job_index(), 1 );
		TS_ASSERT_EQUALS( larval_job1->nstruct_index(), 1 );
		TS_ASSERT_EQUALS( larval_job1->nstruct_max(), 1 );
		ts_assert_mpi_buffer_has_sizes( 0, "node 0 sends node 1 an empty list of job result indices", utility::vector1< Size >() );

		ts_assert_mpi_buffer_has_integers( 0, "node 0 tells node 1 to archive its results on node 0", utility::vector1< int >( 1, 0 ) );
		ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 1 that archival completed", mpi_work_pool_jd_archival_completed );

		ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 1 that it has a job for it to run", mpi_work_pool_jd_new_job_available );
		std::string ser_larv2 = ts_assert_mpi_buffer_has_string( 0, "node 0 sends node 1 the serialized LarvalJob" );
		LarvalJobOP larval_job2 = deserialize_larval_job( ser_larv2 );
		TS_ASSERT_EQUALS( larval_job2->job_index(), 2 );
		TS_ASSERT_EQUALS( larval_job2->nstruct_index(), 1 );
		TS_ASSERT_EQUALS( larval_job2->nstruct_max(), 1 );
		ts_assert_mpi_buffer_has_sizes( 0, "node 0 sends node 1 an empty list of job result indices", utility::vector1< Size >() );

		ts_assert_mpi_buffer_has_integers( 0, "node 0 tells node 1 to archive its results on node 0", utility::vector1< int >( 1, 0 ) );
		ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 1 that archival completed", mpi_work_pool_jd_archival_completed );

		ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 1 that there are no jobs left to run", mpi_work_pool_jd_spin_down );


#endif
	}

	void test_work_pool_jd_master_tries_to_serialize_larval_job_w_unserializable_data()
	{
		TS_ASSERT( true );

#ifdef SERIALIZATION
		SimulateMPI::initialize_simulation( 2 );

		EnergyJobSummaryOP trpcage_job_summary( new EnergyJobSummary );
		trpcage_job_summary->energy( 1234 );
		utility::vector1< JobSummaryOP > summaries( 1, trpcage_job_summary );
		std::string serialized_trpcage_job_summaries = serialized_job_summaries( summaries );

		PoseJobResultOP trpcage_pose_result( new PoseJobResult );
		trpcage_pose_result->pose(  create_trpcage_ideal_poseop() );

		// node 1 requests a job
		SimulateMPI::set_mpi_rank( 1 );
		send_integer_to_node( 0, 1 );
		send_integer_to_node( 0, mpi_work_pool_jd_new_job_request );

		// Now let's create a JQ that creates unserializable larval jobs
		SimulateMPI::set_mpi_rank( 0 );
		PoolJQ6OP jq( new PoolJQ6 );

		MPIWorkPoolJobDistributor jd;
		try {
			jd.go( jq );
			TS_ASSERT( false ); // we should not reach here
		} catch (utility::excn::Exception & e ) {
			std::ostringstream oss;
			oss << "Failed to serialize LarvalJob 1 because it holds an unserializable object.  The following error message comes from the cereal library:\n" <<
				"Trying to save an unregistered polymorphic type (Unserializable).\n" <<
				"Make sure your type is registered with CEREAL_REGISTER_TYPE and that the archive you are" <<
				" using was included (and registered with CEREAL_REGISTER_ARCHIVE) prior to calling CEREAL_REGISTER_TYPE.\n" <<
				"If your type is already registered and you still see this error, you may need to use CEREAL_REGISTER_DYNAMIC_INIT.";

			//std::cout << "Caught exception\n" << e.msg() << "----\n";
			TS_ASSERT( e.msg().find(oss.str()) != std::string::npos );
		}

#endif
	}

	void test_work_pool_jd_worker_tries_to_deserialize_larval_job_w_undeserializable_data()
	{
		TS_ASSERT( true );

#ifdef SERIALIZATION
		SimulateMPI::initialize_simulation( 2 );

		EnergyJobSummaryOP trpcage_job_summary( new EnergyJobSummary );
		trpcage_job_summary->energy( 1234 );
		utility::vector1< JobSummaryOP > summaries( 1, trpcage_job_summary );
		std::string serialized_trpcage_job_summaries = serialized_job_summaries( summaries );

		PoseJobResultOP trpcage_pose_result( new PoseJobResult );
		trpcage_pose_result->pose(  create_trpcage_ideal_poseop() );

		InnerLarvalJobOP inner_job( new InnerLarvalJob( 2, 1 ) );
		inner_job->outputter( DummyPoseOutputter::keyname() );
		inner_job->const_data_map().add( "testing", "testing", UndeserializableOP( new Undeserializable ));
		LarvalJobOP larval_job( new LarvalJob( inner_job, 1, 1 ));

		std::string undeserializable_larval_job;
		try {
			undeserializable_larval_job = serialized_T( larval_job );
		} catch ( cereal::Exception & e ) {
			TS_ASSERT( false );
			std::cerr << "Failed to serialize a LarvalJob holding a Undeserializable object\n" << e.what() << std::endl;
			return;
		}

		// node 0 has a job for node 1
		SimulateMPI::set_mpi_rank( 0 );
		send_integer_to_node( 1, mpi_work_pool_jd_new_job_available );
		send_string_to_node( 1, undeserializable_larval_job );

		// ok -- the worker node will be shut down when the mpi process exits; and this will happen when node 0
		// has output the error message that we sent and then exits; so for this unit test, just send node 1 a
		// spin-down signal
		send_integer_to_node( 1, mpi_work_pool_jd_spin_down );

		// Now let's create a JQ that creates unserializable larval jobs
		SimulateMPI::set_mpi_rank( 1 );
		PoolJQ1OP jq( new PoolJQ1 );

		MPIWorkPoolJobDistributor jd;
		try {
			jd.go( jq );
		} catch ( ... ) {
			TS_ASSERT( false );
		}

		// NOW let's make sure that node 1 has been sending its messages the way it should have
		SimulateMPI::set_mpi_rank( 0 );
		// Node 1 starts by requesting a job
		ts_assert_mpi_buffer_has_integer( 1, "node 1 says to node 0: I have a request A", 1 );
		ts_assert_mpi_buffer_has_integer( 1, "node 1 requests a new job of node 0", mpi_work_pool_jd_new_job_request );

		// Node 1 then says that it was unable to deserialize the larval job
		ts_assert_mpi_buffer_has_integer( 1, "node 1 says to node 0: I have a request A", 1 );
		ts_assert_mpi_buffer_has_integer( 1, "node 1 requests a new job of node 0", mpi_work_pool_jd_error );
		ts_assert_mpi_buffer_has_string( 1, "node 1 sent an error message",
			"Failed to deserialize larval job on worker node 1. Exiting.\nError message from cereal library:\n"
			"Undeserializable could not be deserialized\n" );

#endif
	}

	void test_work_pool_jd_worker_tries_to_deserialize_job_result_w_undeserializable_data()
	{
		TS_ASSERT( true );

#ifdef SERIALIZATION
		SimulateMPI::initialize_simulation( 2 );

		EnergyJobSummaryOP trpcage_job_summary( new EnergyJobSummary );
		trpcage_job_summary->energy( 1234 );
		utility::vector1< JobSummaryOP > summaries( 1, trpcage_job_summary );
		std::string serialized_trpcage_job_summaries = serialized_job_summaries( summaries );

		PoseJobResultOP trpcage_pose_result( new PoseJobResult );
		core::pose::PoseOP trpcage = create_trpcage_ideal_poseop();
		trpcage->set_const_data< Undeserializable >( "testing", "testing", Undeserializable());
		trpcage_pose_result->pose( trpcage );

		// node 0 has a job for node 1
		SimulateMPI::set_mpi_rank( 0 );
		send_integer_to_node( 1, mpi_work_pool_jd_new_job_available );
		send_string_to_node( 1, serialized_T( create_larval_job_w_job1_dep( 1, 1, 2 ) ));
		send_sizes_to_node( 1, utility::vector1< Size >( 1, 0 ));

		// now node 0 sends an un-deserializable job result
		send_integer_to_node( 1, mpi_work_pool_jd_job_result_retrieved );
		send_string_to_node( 1, serialized_larval_job_and_job_result( 1, 1, 1, trpcage_pose_result ));

		// ok -- the worker node will be shut down when the mpi process exits; and this will happen when node 0
		// has output the error message that we sent and then exits; so for this unit test, just send node 1 a
		// spin-down signal
		send_integer_to_node( 1, mpi_work_pool_jd_spin_down );

		// Now let's create a JQ that creates unserializable larval jobs
		SimulateMPI::set_mpi_rank( 1 );
		PoolJQ1OP jq( new PoolJQ1 );

		MPIWorkPoolJobDistributor jd;
		try {
			jd.go( jq );
		} catch ( ... ) {
			TS_ASSERT( false );
		}

		// NOW let's make sure that node 1 has been sending its messages the way it should have
		SimulateMPI::set_mpi_rank( 0 );
		// Node 1 starts by requesting a job
		ts_assert_mpi_buffer_has_integer( 1, "node 1 says to node 0: I have a request A", 1 );
		ts_assert_mpi_buffer_has_integer( 1, "node 1 requests a new job of node 0", mpi_work_pool_jd_new_job_request );
		ts_assert_mpi_buffer_has_integer( 1, "node 1 says to node 0: I have a request B", 1 );
		ts_assert_mpi_buffer_has_integer( 1, "node 1 requests a new job of node 0", mpi_work_pool_jd_retrieve_job_result );
		ts_assert_mpi_buffer_has_size( 1, "node 1 requests job #1", 1 );
		ts_assert_mpi_buffer_has_size( 1, "node 1 requests job #1 result #1", 1 );

		// Node 1 then says that it was unable to deserialize the job result
		ts_assert_mpi_buffer_has_integer( 1, "node 1 says to node 0: I have a request A", 1 );
		ts_assert_mpi_buffer_has_integer( 1, "node 1 requests a new job of node 0", mpi_work_pool_jd_error );
		ts_assert_mpi_buffer_has_string( 1, "node 1 sent an error message",
			"Failed to deserialize LarvalJob & JobResult pair from job (1, 1) which is required as an input to job 2\n"
			"Error message from cereal library:\n"
			"Undeserializable could not be deserialized\n" );

#endif
	}

	void test_work_pool_jd_worker_tries_to_serialize_job_result_w_unserializable_data()
	{
		TS_ASSERT( true );

#ifdef SERIALIZATION
		SimulateMPI::initialize_simulation( 2 );

		// node 0 has a job for node 1
		SimulateMPI::set_mpi_rank( 0 );
		send_integer_to_node( 1, mpi_work_pool_jd_new_job_available );
		send_string_to_node( 1, serialized_larval_job( 1, 1, 1 ) );
		send_sizes_to_node( 1, utility::vector1< Size >() );

		// ok -- the worker node will be shut down when the mpi process exits; and this will happen when node 0
		// has output the error message that we sent and then exits; so for this unit test, just send node 1 a
		// spin-down signal
		send_integer_to_node( 1, mpi_work_pool_jd_spin_down );

		// Now let's create a JQ that creates unserializable larval jobs
		SimulateMPI::set_mpi_rank( 1 );
		PoolJQ8OP jq( new PoolJQ8 );

		MPIWorkPoolJobDistributor jd;
		try {
			jd.go( jq );
		} catch ( ... ) {
			TS_ASSERT( false );
		}

		// NOW let's make sure that node 1 has been sending its messages the way it should have
		SimulateMPI::set_mpi_rank( 0 );
		// Node 1 starts by requesting a job
		ts_assert_mpi_buffer_has_integer( 1, "node 1 says to node 0: I have a request A", 1 );
		ts_assert_mpi_buffer_has_integer( 1, "node 1 requests a new job of node 0", mpi_work_pool_jd_new_job_request );

		// Node 1 then says that it was unable to serialize the job result after job execution completed
		ts_assert_mpi_buffer_has_integer( 1, "node 1 says to node 0: I have a request A", 1 );
		ts_assert_mpi_buffer_has_integer( 1, "node 1 requests a new job of node 0", mpi_work_pool_jd_error );
		ts_assert_mpi_buffer_has_string( 1, "node 1 sent an error message",
			"Failed to serialize LarvalJob or JobResult; all objects stored in these"
			" classes (or derived from them) must implement save & load serialization functions\nJob #1"
			" failed. Error message from the cereal library:\n"
			"Trying to save an unregistered polymorphic type (Unserializable).\n"
			"Make sure your type is registered with CEREAL_REGISTER_TYPE and that the archive you"
			" are using was included (and registered with CEREAL_REGISTER_ARCHIVE) prior to calling CEREAL_REGISTER_TYPE.\n"
			"If your type is already registered and you still see this error, you may need to use CEREAL_REGISTER_DYNAMIC_INIT.\n" );


#endif
	}

	void test_work_pool_jd_worker_tries_to_serialize_job_summary_w_unserializable_data()
	{
		TS_ASSERT( true );

#ifdef SERIALIZATION
		SimulateMPI::initialize_simulation( 2 );

		// node 0 has a job for node 1
		SimulateMPI::set_mpi_rank( 0 );
		send_integer_to_node( 1, mpi_work_pool_jd_new_job_available );
		send_string_to_node( 1, serialized_larval_job( 1, 1, 1 ) );
		send_sizes_to_node( 1, utility::vector1< Size >() );

		// ok -- the worker node will be shut down when the mpi process exits; and this will happen when node 0
		// has output the error message that we sent and then exits; so for this unit test, just send node 1 a
		// spin-down signal
		send_integer_to_node( 1, mpi_work_pool_jd_spin_down );

		// Now let's create a JQ that creates unserializable larval jobs
		SimulateMPI::set_mpi_rank( 1 );
		PoolJQ9OP jq( new PoolJQ9 );

		MPIWorkPoolJobDistributor jd;
		try {
			jd.go( jq );
		} catch ( ... ) {
			TS_ASSERT( false );
		}

		// NOW let's make sure that node 1 has been sending its messages the way it should have
		SimulateMPI::set_mpi_rank( 0 );
		// Node 1 starts by requesting a job
		ts_assert_mpi_buffer_has_integer( 1, "node 1 says to node 0: I have a request A", 1 );
		ts_assert_mpi_buffer_has_integer( 1, "node 1 requests a new job of node 0", mpi_work_pool_jd_new_job_request );

		// Node 1 then says that it was unable to deserialize the job summary
		ts_assert_mpi_buffer_has_integer( 1, "node 1 says to node 0: I have a request A", 1 );
		ts_assert_mpi_buffer_has_integer( 1, "node 1 requests a new job of node 0", mpi_work_pool_jd_error );
		ts_assert_mpi_buffer_has_string( 1, "node 1 sent an error message",
			"Failed to serialize JobSummary; all objects stored in this class (or that are derived from it) must implement save & load serialization functions\n"
			"Job #1 failed. Error message from the cereal library:\n"
			"Trying to save an unregistered polymorphic type (PoolJobSummary1).\n"
			"Make sure your type is registered with CEREAL_REGISTER_TYPE and that the archive you "
			"are using was included (and registered with CEREAL_REGISTER_ARCHIVE) prior to calling CEREAL_REGISTER_TYPE.\n"
			"If your type is already registered and you still see this error, you may need to use CEREAL_REGISTER_DYNAMIC_INIT.\n" );


#endif
	}

	void test_work_pool_jd_master_node_fails_to_deserialize_undeserializable_job_summary()
	{
		TS_ASSERT( true ); // appease non-serialization builds


#ifdef SERIALIZATION


		JobSummaryOP trpcage_job_summary( new PoolJobSummary2 );
		utility::vector1< JobSummaryOP > summaries( 1, trpcage_job_summary );
		std::string serialized_trpcage_job_summaries = serialized_job_summaries( summaries );

		PoseJobResultOP trpcage_pose_result( new PoseJobResult );
		trpcage_pose_result->pose(  create_trpcage_ideal_poseop() );

		SimulateMPI::initialize_simulation( 2 );

		// node 1 requests a job
		SimulateMPI::set_mpi_rank( 1 );
		send_integer_to_node( 0, 1 );
		send_integer_to_node( 0, mpi_work_pool_jd_new_job_request );

		// node 1 starts its two part job-completion process,
		SimulateMPI::set_mpi_rank( 1 );
		send_integer_to_node( 0, 1 );
		send_integer_to_node( 0, mpi_work_pool_jd_job_success );
		send_size_to_node( 0, 1 ); // job id
		send_size_to_node( 0, 1 ); // num results

		send_integer_to_node( 0, 1 );
		send_integer_to_node( 0, mpi_work_pool_jd_archive_job_result );
		send_size_to_node( 0, 1 ); // job id
		send_size_to_node( 0, 1 ); // result index
		send_string_to_node( 0, serialized_larval_job_and_job_result( 1, 1, 1, trpcage_pose_result ));

		send_integer_to_node( 0, 1 );
		send_integer_to_node( 0, mpi_work_pool_jd_job_success_and_archival_complete );
		send_size_to_node( 0, 1 ); // job id
		send_integer_to_node( 0, jd3_job_status_success );
		send_string_to_node( 0, serialized_trpcage_job_summaries );

		// ok -- now we create a JQ on node 0, set it up to produce three jobs
		// and then create a job distributor for node 0 and tell it to go go go!

		SimulateMPI::set_mpi_rank( 0 );
		PoolJQ1OP jq( new PoolJQ1 );
		jq->njobs_ = 1;

		MPIWorkPoolJobDistributor jd;
		try {
			jd.go( jq );
			TS_ASSERT( false );
		} catch (utility::excn::Exception & e ) {
			//std::cerr << e.msg() << std::endl;
			std::string expected_err_msg = "Failed to deserialize the JobSummary array for job #1\nError message from"
				" the cereal library:\nPoolJobSummary2 could not be deserialized";
			TS_ASSERT( e.msg().find(expected_err_msg) != std::string::npos );
		}

#endif
	}

	// Do I need to reinstate this bug? The master node no longer desearializes job results
	void dont_test_work_pool_jd_master_node_fails_to_deserialize_undeserializable_job_result()
	{
		TS_ASSERT( true ); // appease non-serialization builds

#ifdef SERIALIZATION

		EnergyJobSummaryOP trpcage_job_summary( new EnergyJobSummary );
		trpcage_job_summary->energy( 1234 );
		utility::vector1< JobSummaryOP > summaries( 1, trpcage_job_summary );
		std::string serialized_trpcage_job_summaries = serialized_job_summaries( summaries );

		PoseJobResultOP trpcage_pose_result( new PoseJobResult );
		trpcage_pose_result->pose(  create_trpcage_ideal_poseop() );
		trpcage_pose_result->pose()->set_const_data< Undeserializable >( "testing", "testing", Undeserializable());

		SimulateMPI::initialize_simulation( 2 );

		// node 1 requests a job
		SimulateMPI::set_mpi_rank( 1 );
		send_integer_to_node( 0, 1 );
		send_integer_to_node( 0, mpi_work_pool_jd_new_job_request );

		// node 1 starts its two part job-completion process,
		SimulateMPI::set_mpi_rank( 1 );
		send_integer_to_node( 0, 1 );
		send_integer_to_node( 0, mpi_work_pool_jd_job_success );
		send_size_to_node( 0, 1 ); // job id
		send_size_to_node( 0, 1 ); // num results

		send_integer_to_node( 0, 1 );
		send_integer_to_node( 0, mpi_work_pool_jd_archive_job_result );
		send_size_to_node( 0, 1 ); // job id
		send_size_to_node( 0, 1 ); // result index
		send_string_to_node( 0, serialized_larval_job_and_job_result( 1, 1, 1, trpcage_pose_result ));

		send_integer_to_node( 0, 1 );
		send_integer_to_node( 0, mpi_work_pool_jd_job_success_and_archival_complete );
		send_size_to_node( 0, 1 ); // job id
		send_integer_to_node( 0, jd3_job_status_success );
		send_string_to_node( 0, serialized_trpcage_job_summaries );

		send_integer_to_node( 0, 1 );
		send_integer_to_node( 0, mpi_work_pool_jd_new_job_request );

		// ok -- now we create a JQ on node 0, set it up to produce three jobs
		// and then create a job distributor for node 0 and tell it to go go go!

		SimulateMPI::set_mpi_rank( 0 );
		PoolJQ1OP jq( new PoolJQ1 );
		jq->njobs_ = 1;

		MPIWorkPoolJobDistributor jd;
		try {
			jd.go( jq );
			TS_ASSERT( false );
		} catch (utility::excn::Exception & e ) {
			//std::cerr << e.msg() << std::endl;
			std::string expected_err_msg = "Failed to deserialize LarvalJob & JobResult pair for job #1 result index #1\nError message from"
				" the cereal library:\nUndeserializable could not be deserialized";
			TS_ASSERT( e.msg().find(expected_err_msg) != std::string::npos );
		}

#endif
	}

	/////////////// Test the deallocation system
	void test_jd3_workpool_manager_deallocation_messages_sent_to_remote_nodes() {

#ifdef SERIALIZATION
		EnergyJobSummaryOP trpcage_job_summary( new EnergyJobSummary );
		trpcage_job_summary->energy( 1234 );
		utility::vector1< JobSummaryOP > summaries( 1, trpcage_job_summary );
		std::string serialized_trpcage_job_summaries = serialized_job_summaries( summaries );

		PoseJobResultOP trpcage_pose_result( new PoseJobResult );
		trpcage_pose_result->pose(  create_trpcage_ideal_poseop() );

		SimulateMPI::initialize_simulation( 4 );

		// ok, all the nodes at once start requesting jobs from node 0
		SimulateMPI::set_mpi_rank( 1 );
		send_integer_to_node( 0, 1 );
		SimulateMPI::set_mpi_rank( 2 );
		send_integer_to_node( 0, 2 );
		send_integer_to_node( 0, mpi_work_pool_jd_new_job_request );
		SimulateMPI::set_mpi_rank( 3 );
		send_integer_to_node( 0, 3 );

		// let's start the job at node 1
		SimulateMPI::set_mpi_rank( 1 );
		send_integer_to_node( 0, mpi_work_pool_jd_new_job_request );

		// and now the job at node 3
		SimulateMPI::set_mpi_rank( 3 );
		send_integer_to_node( 0, mpi_work_pool_jd_new_job_request );

		// ok -- now let's pretend that node 2 has finished its job
		SimulateMPI::set_mpi_rank( 2 );
		send_integer_to_node( 0, 2 );

		// ok -- before node 2 can finish its two part job-completion process,
		// node 1 will butt in and start its two part job-completion process,
		SimulateMPI::set_mpi_rank( 1 );
		send_integer_to_node( 0, 1 );

		SimulateMPI::set_mpi_rank( 2 );
		send_integer_to_node( 0, mpi_work_pool_jd_job_success );
		send_size_to_node( 0, 2 ); // job_id
		send_size_to_node( 0, 1 ); // num results

		send_integer_to_node( 0, 2 );
		send_integer_to_node( 0, mpi_work_pool_jd_archive_job_result );
		send_size_to_node( 0, 2 ); // job_id
		send_size_to_node( 0, 1 ); // result index
		send_string_to_node( 0, serialized_larval_job_and_job_result( 3, 2, 2, trpcage_pose_result ));

		SimulateMPI::set_mpi_rank( 1 );
		send_integer_to_node( 0, mpi_work_pool_jd_job_success );
		send_size_to_node( 0, 1 ); // job id
		send_size_to_node( 0, 1 ); // num results

		send_integer_to_node( 0, 1 );
		send_integer_to_node( 0, mpi_work_pool_jd_archive_job_result );
		send_size_to_node( 0, 1 ); // job id
		send_size_to_node( 0, 1 ); // result index
		send_string_to_node( 0, serialized_larval_job_and_job_result( 3, 1, 1, trpcage_pose_result ));

		SimulateMPI::set_mpi_rank( 2 );
		send_integer_to_node( 0, 2 );
		send_integer_to_node( 0, mpi_work_pool_jd_job_success_and_archival_complete );
		send_size_to_node( 0, 2 ); // job id
		send_integer_to_node( 0, jd3_job_status_success );
		send_string_to_node( 0, serialized_trpcage_job_summaries );

		SimulateMPI::set_mpi_rank( 3 );
		send_integer_to_node( 0, 3 );
		send_integer_to_node( 0, mpi_work_pool_jd_job_success );
		send_size_to_node( 0, 3 ); // job id
		send_size_to_node( 0, 1 ); // num results

		send_integer_to_node( 0, 3 );
		send_integer_to_node( 0, mpi_work_pool_jd_archive_job_result );
		send_size_to_node( 0, 3 ); // job id
		send_size_to_node( 0, 1 ); // result index
		send_string_to_node( 0, serialized_larval_job_and_job_result( 3, 3, 3, trpcage_pose_result ));

		SimulateMPI::set_mpi_rank( 1 );
		send_integer_to_node( 0, 1 );
		send_integer_to_node( 0, mpi_work_pool_jd_job_success_and_archival_complete );
		send_size_to_node( 0, 1 ); // job id
		send_integer_to_node( 0, jd3_job_status_success );
		send_string_to_node( 0, serialized_trpcage_job_summaries );

		// Now node 1 asks for a new job
		send_integer_to_node( 0, 1 );
		send_integer_to_node( 0, mpi_work_pool_jd_new_job_request );

		SimulateMPI::set_mpi_rank( 2 );
		// Now node 2 asks for a new job
		send_integer_to_node( 0, 2 );
		send_integer_to_node( 0, mpi_work_pool_jd_new_job_request );


		SimulateMPI::set_mpi_rank( 3 );
		send_integer_to_node( 0, 3 );
		send_integer_to_node( 0, mpi_work_pool_jd_job_success_and_archival_complete );
		send_size_to_node( 0, 3 ); // job id
		send_integer_to_node( 0, jd3_job_status_success );
		send_string_to_node( 0, serialized_trpcage_job_summaries );

		// Now node 3 asks for a new job
		send_integer_to_node( 0, 3 );
		send_integer_to_node( 0, mpi_work_pool_jd_new_job_request );

		SimulateMPI::set_mpi_rank( 1 );

		// now retrieve the job result for job 1
		send_integer_to_node( 0, 1 );
		send_integer_to_node( 0, mpi_work_pool_jd_retrieve_job_result );
		send_size_to_node( 0, 1 ); // retrieve the result from job #1
		send_size_to_node( 0, 1 ); // retrieve the result from job #1 result #1

		SimulateMPI::set_mpi_rank( 2 );

		// now retrieve the job result for job 1
		send_integer_to_node( 0, 2 );
		send_integer_to_node( 0, mpi_work_pool_jd_retrieve_job_result );
		send_size_to_node( 0, 1 ); // retrieve the result from job #1
		send_size_to_node( 0, 1 ); // retrieve the result from job #1 result #1

		SimulateMPI::set_mpi_rank( 3 );

		// now retrieve the job result for job 1
		send_integer_to_node( 0, 3 );
		send_integer_to_node( 0, mpi_work_pool_jd_retrieve_job_result );
		send_size_to_node( 0, 1 ); // retrieve the result from job #1
		send_size_to_node( 0, 1 ); // retrieve the result from job #1 result #1

		// Now the second round of job results start trickling in

		SimulateMPI::set_mpi_rank( 1 );

		send_integer_to_node( 0, 1 );
		send_integer_to_node( 0, mpi_work_pool_jd_job_success );
		send_size_to_node( 0, 4 ); // job id
		send_size_to_node( 0, 1 ); // num results

		send_integer_to_node( 0, 1 );
		send_integer_to_node( 0, mpi_work_pool_jd_archive_job_result );
		send_size_to_node( 0, 4 ); // job id
		send_size_to_node( 0, 1 ); // result index
		send_string_to_node( 0, serialized_larval_job_and_job_result( 3, 1, 4, trpcage_pose_result ));

		send_integer_to_node( 0, 1 );
		send_integer_to_node( 0, mpi_work_pool_jd_job_success_and_archival_complete );
		send_size_to_node( 0, 4 ); // job id
		send_integer_to_node( 0, jd3_job_status_success );
		send_string_to_node( 0, serialized_trpcage_job_summaries );

		send_integer_to_node( 0, 1 );
		send_integer_to_node( 0, mpi_work_pool_jd_new_job_request );

		SimulateMPI::set_mpi_rank( 3 );
		send_integer_to_node( 0, 3 );
		send_integer_to_node( 0, mpi_work_pool_jd_job_success );
		send_size_to_node( 0, 6 ); // job id
		send_size_to_node( 0, 1 ); // num results

		send_integer_to_node( 0, 3 );
		send_integer_to_node( 0, mpi_work_pool_jd_archive_job_result );
		send_size_to_node( 0, 6 ); // job id
		send_size_to_node( 0, 1 ); // num results
		send_string_to_node( 0, serialized_larval_job_and_job_result( 3, 3, 6, trpcage_pose_result ));

		send_integer_to_node( 0, 3 );
		send_integer_to_node( 0, mpi_work_pool_jd_job_success_and_archival_complete );
		send_size_to_node( 0, 6 ); // job id
		send_integer_to_node( 0, jd3_job_status_success );
		send_string_to_node( 0, serialized_trpcage_job_summaries );

		send_integer_to_node( 0, 3 );
		send_integer_to_node( 0, mpi_work_pool_jd_new_job_request );

		SimulateMPI::set_mpi_rank( 2 );
		send_integer_to_node( 0, 2 );
		send_integer_to_node( 0, mpi_work_pool_jd_job_success );
		send_size_to_node( 0, 5 ); // job id
		send_size_to_node( 0, 1 ); // num results

		send_integer_to_node( 0, 2 );
		send_integer_to_node( 0, mpi_work_pool_jd_archive_job_result );
		send_size_to_node( 0, 5 ); // job id
		send_size_to_node( 0, 1 ); // result index
		send_string_to_node( 0, serialized_larval_job_and_job_result( 3, 2, 5, trpcage_pose_result ));

		send_integer_to_node( 0, 2 );
		send_integer_to_node( 0, mpi_work_pool_jd_job_success_and_archival_complete );
		send_size_to_node( 0, 5 ); // job id
		send_integer_to_node( 0, jd3_job_status_success );
		send_string_to_node( 0, serialized_trpcage_job_summaries );

		send_integer_to_node( 0, 2 );
		send_integer_to_node( 0, mpi_work_pool_jd_new_job_request );

		SimulateMPI::set_mpi_rank( 1 );
		//  Node 1 has output the first result it was given, so it asks for a new job
		send_integer_to_node( 0, 1 );
		send_integer_to_node( 0, mpi_work_pool_jd_new_job_request );

		// Node 1 has output the second result it was given, so it asks for a new job
		send_integer_to_node( 0, 1 );
		send_integer_to_node( 0, mpi_work_pool_jd_new_job_request );

		// Node 1 has output the third result it was given, so it asks for a new job
		// but this time, node 0 will tell it that there are no jobs remaining
		send_integer_to_node( 0, 1 );
		send_integer_to_node( 0, mpi_work_pool_jd_new_job_request );

		//OK! Now create a PoolJQ11 and let 'er rip
		SimulateMPI::set_mpi_rank( 0 );
		PoolJQ11OP jq11( new PoolJQ11 );
		MPIWorkPoolJobDistributor jd;
		try {
			jd.go( jq11 );
		} catch ( ... ) {
			TS_ASSERT( false /*no exception should be thrown*/ );
		}

		// OK!
		// Now we read the mail we got from node 0

		SimulateMPI::set_mpi_rank( 1 );
		ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 1 that it has a job for it to run", mpi_work_pool_jd_new_job_available );
		std::string ser_larv1 = ts_assert_mpi_buffer_has_string( 0, "node 0 sends node 1 the serialized LarvalJob" );
		LarvalJobOP larval_job1 = deserialize_larval_job( ser_larv1 );
		TS_ASSERT_EQUALS( larval_job1->job_index(), 1 );
		TS_ASSERT_EQUALS( larval_job1->nstruct_index(), 1 );
		TS_ASSERT_EQUALS( larval_job1->nstruct_max(), 3 );
		ts_assert_mpi_buffer_has_sizes( 0, "node 0 sends node 1 an empty list of job result indices", utility::vector1< Size >() );

		ts_assert_mpi_buffer_has_integers( 0, "node 0 tells node 1 to archive its results on node 0", utility::vector1< int >( 1, 0 ) );
		ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 1 that archival completed", mpi_work_pool_jd_archival_completed );

		ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 1 that there are deallocations that should be made", mpi_work_pool_jd_deallocation_message );
		std::string node1_dealloc_msgs_str = ts_assert_mpi_buffer_has_string( 0, "node 0 sends node 1 the serialized deallocation message list" );
		std::list< deallocation::DeallocationMessageOP > node1_dealloc_msgs = deserialize_deallocation_msg_list( node1_dealloc_msgs_str );
		TS_ASSERT_EQUALS( node1_dealloc_msgs.size(), 2 );
		utility::vector1< deallocation::DeallocationMessageOP > node1_dealloc_msgs_vect( 2 );
		std::copy( node1_dealloc_msgs.begin(), node1_dealloc_msgs.end(), node1_dealloc_msgs_vect.begin() );
		deallocation::InputPoseDeallocationMessageOP n1msg1 = utility::pointer::dynamic_pointer_cast< deallocation::InputPoseDeallocationMessage > ( node1_dealloc_msgs_vect[1] );
		deallocation::InputPoseDeallocationMessageOP n1msg2 = utility::pointer::dynamic_pointer_cast< deallocation::InputPoseDeallocationMessage > ( node1_dealloc_msgs_vect[2] );
		TS_ASSERT( n1msg1 );
		TS_ASSERT( n1msg2 );
		TS_ASSERT_EQUALS( n1msg1->pose_id(), 1 );
		TS_ASSERT_EQUALS( n1msg2->pose_id(), 2 );

		ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 1 that it has another job for it to run", mpi_work_pool_jd_new_job_available );
		std::string ser_larv4 = ts_assert_mpi_buffer_has_string( 0, "node 0 sends node 1 the serialized LarvalJob" );
		LarvalJobOP larval_job4 = deserialize_larval_job( ser_larv4 );
		TS_ASSERT_EQUALS( larval_job4->job_index(), 4 );
		TS_ASSERT_EQUALS( larval_job4->nstruct_index(), 1 );
		TS_ASSERT_EQUALS( larval_job4->nstruct_max(), 3 );
		TS_ASSERT_EQUALS( larval_job4->input_job_result_indices().size(), 1 );
		TS_ASSERT_EQUALS( larval_job4->input_job_result_indices()[ 1 ].first, 1 );
		TS_ASSERT_EQUALS( larval_job4->input_job_result_indices()[ 1 ].second, 1 );
		ts_assert_mpi_buffer_has_sizes( 0, "node 0 sends node 1 a vector1 of job result indices with one element whose value is 0", utility::vector1< Size >( 1, 0 ) );

		ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 1 that it has the job result it was expecting", mpi_work_pool_jd_job_result_retrieved );
		std::string ser_job_res1a = ts_assert_mpi_buffer_has_string( 0, "node 0 sends the serialized larval-job and job-result from job 1" );
		MPIWorkPoolJobDistributor::LarvalJobAndResult job_res1a = deserialized_larval_job_and_job_result( ser_job_res1a );
		TS_ASSERT( job_res1a.first );
		TS_ASSERT( job_res1a.second );
		TS_ASSERT_EQUALS( job_res1a.first->job_index(), 1 );
		TS_ASSERT_EQUALS( job_res1a.first->nstruct_index(), 1 );
		TS_ASSERT_EQUALS( job_res1a.first->nstruct_max(), 3 );
		TS_ASSERT_EQUALS( utility::pointer::dynamic_pointer_cast< PoseJobResult > ( job_res1a.second )->pose()->total_residue(), 20 );

		ts_assert_mpi_buffer_has_integers( 0, "node 0 tells node 1 to archive its results on node 0", utility::vector1< int >( 1, 0 ) );
		ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 1 that archival completed", mpi_work_pool_jd_archival_completed );

		{ // scope -- job4 output
			ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 1 to output job 2", mpi_work_pool_jd_accept_and_output_job_result );
			std::string output_spec4_string = ts_assert_mpi_buffer_has_string( 0, "node 0 sends the serialized output spec #4" );
			OutputSpecificationOP output_spec4 = deserialized_output_specification( output_spec4_string );
			TS_ASSERT_EQUALS( output_spec4->result_id().first, 4 );
			TS_ASSERT_EQUALS( output_spec4->result_id().second, 1 );
			TS_ASSERT_EQUALS( output_spec4->output_index().primary_output_index, 1 );
			TS_ASSERT_EQUALS( output_spec4->output_index().n_primary_outputs, 1000 );
			TS_ASSERT_EQUALS( output_spec4->output_index().secondary_output_index, 1 );
			TS_ASSERT_EQUALS( output_spec4->output_index().n_secondary_outputs, 1 );
			std::string job_result4_string = ts_assert_mpi_buffer_has_string( 0, "node 0 sends the serialized job-result-and-larval-job-pair string" );
			MPIWorkPoolJobDistributor::LarvalJobAndResult job_res4 = deserialized_larval_job_and_job_result( job_result4_string );
			TS_ASSERT( job_res4.first );
			TS_ASSERT( job_res4.second );
			TS_ASSERT_EQUALS( job_res4.first->job_index(), 4 );
			TS_ASSERT_EQUALS( job_res4.first->nstruct_index(), 1 );
			TS_ASSERT_EQUALS( job_res4.first->nstruct_max(), 3 );
			TS_ASSERT_EQUALS( utility::pointer::dynamic_pointer_cast< PoseJobResult > ( job_res4.second )->pose()->total_residue(), 20 );
		}


		{ // scope -- job 5 output
			ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 1 to output job 5", mpi_work_pool_jd_accept_and_output_job_result );
			std::string output_spec5_string = ts_assert_mpi_buffer_has_string( 0, "node 0 sends the serialized output spec #5" );
			OutputSpecificationOP output_spec5 = deserialized_output_specification( output_spec5_string );
			TS_ASSERT_EQUALS( output_spec5->result_id().first, 5 );
			TS_ASSERT_EQUALS( output_spec5->result_id().second, 1 );
			TS_ASSERT_EQUALS( output_spec5->output_index().primary_output_index, 1 );
			TS_ASSERT_EQUALS( output_spec5->output_index().n_primary_outputs, 1000 );
			TS_ASSERT_EQUALS( output_spec5->output_index().secondary_output_index, 1 );
			TS_ASSERT_EQUALS( output_spec5->output_index().n_secondary_outputs, 1 );
			std::string job_result5_string = ts_assert_mpi_buffer_has_string( 0, "node 0 sends the serialized job-result-and-larval-job-pair string" );
			MPIWorkPoolJobDistributor::LarvalJobAndResult job_res5 = deserialized_larval_job_and_job_result( job_result5_string );
			TS_ASSERT( job_res5.first );
			TS_ASSERT( job_res5.second );
			TS_ASSERT_EQUALS( job_res5.first->job_index(), 5 );
			TS_ASSERT_EQUALS( job_res5.first->nstruct_index(), 2 );
			TS_ASSERT_EQUALS( job_res5.first->nstruct_max(), 3 );
			TS_ASSERT_EQUALS( utility::pointer::dynamic_pointer_cast< PoseJobResult > ( job_res5.second )->pose()->total_residue(), 20 );
		}


		{ // scope job 6 output
			ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 1 to output job 6", mpi_work_pool_jd_accept_and_output_job_result );
			std::string output_spec6_string = ts_assert_mpi_buffer_has_string( 0, "node 0 sends the serialized output spec #6" );
			OutputSpecificationOP output_spec6 = deserialized_output_specification( output_spec6_string );
			TS_ASSERT_EQUALS( output_spec6->result_id().first, 6 );
			TS_ASSERT_EQUALS( output_spec6->result_id().second, 1 );
			TS_ASSERT_EQUALS( output_spec6->output_index().primary_output_index, 1 );
			TS_ASSERT_EQUALS( output_spec6->output_index().n_primary_outputs, 1000 );
			TS_ASSERT_EQUALS( output_spec6->output_index().secondary_output_index, 1 );
			TS_ASSERT_EQUALS( output_spec6->output_index().n_secondary_outputs, 1 );
			std::string job_result6_string = ts_assert_mpi_buffer_has_string( 0, "node 0 sends the serialized job-result-and-larval-job-pair string" );
			MPIWorkPoolJobDistributor::LarvalJobAndResult job_res6 = deserialized_larval_job_and_job_result( job_result6_string );
			TS_ASSERT( job_res6.first );
			TS_ASSERT( job_res6.second );
			TS_ASSERT_EQUALS( job_res6.first->job_index(), 6 );
			TS_ASSERT_EQUALS( job_res6.first->nstruct_index(), 3 );
			TS_ASSERT_EQUALS( job_res6.first->nstruct_max(), 3 );
			TS_ASSERT_EQUALS( utility::pointer::dynamic_pointer_cast< PoseJobResult > ( job_res6.second )->pose()->total_residue(), 20 );
		} // scope

		ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 1 that there are no jobs left to run", mpi_work_pool_jd_spin_down );

		SimulateMPI::set_mpi_rank( 2 );
		ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 2 that it has a job for it to run", mpi_work_pool_jd_new_job_available );
		std::string ser_larv2 = ts_assert_mpi_buffer_has_string( 0, "node 0 sends node 2 the serialized LarvalJob" );
		LarvalJobOP larval_job2 = deserialize_larval_job( ser_larv2 );
		TS_ASSERT_EQUALS( larval_job2->job_index(), 2 );
		TS_ASSERT_EQUALS( larval_job2->nstruct_index(), 2 );
		TS_ASSERT_EQUALS( larval_job2->nstruct_max(), 3 );
		ts_assert_mpi_buffer_has_sizes( 0, "node 0 sends node 2 an empty list of job result indices", utility::vector1< Size >() );

		ts_assert_mpi_buffer_has_integers( 0, "node 0 tells node 2 to archive its results on node 0", utility::vector1< int >( 1, 0 ) );
		ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 2 that archival completed", mpi_work_pool_jd_archival_completed );

		ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 2 that there are deallocations that should be made", mpi_work_pool_jd_deallocation_message );
		std::string node2_dealloc_msgs_str = ts_assert_mpi_buffer_has_string( 0, "node 0 sends node 2 the serialized deallocation message list" );
		std::list< deallocation::DeallocationMessageOP > node2_dealloc_msgs = deserialize_deallocation_msg_list( node2_dealloc_msgs_str );
		TS_ASSERT_EQUALS( node2_dealloc_msgs.size(), 2 );
		utility::vector1< deallocation::DeallocationMessageOP > node2_dealloc_msgs_vect( 2 );
		std::copy( node2_dealloc_msgs.begin(), node2_dealloc_msgs.end(), node2_dealloc_msgs_vect.begin() );
		deallocation::InputPoseDeallocationMessageOP n2msg1 = utility::pointer::dynamic_pointer_cast< deallocation::InputPoseDeallocationMessage > ( node2_dealloc_msgs_vect[1] );
		deallocation::InputPoseDeallocationMessageOP n2msg2 = utility::pointer::dynamic_pointer_cast< deallocation::InputPoseDeallocationMessage > ( node2_dealloc_msgs_vect[2] );
		TS_ASSERT( n2msg1 );
		TS_ASSERT( n2msg2 );
		TS_ASSERT_EQUALS( n2msg1->pose_id(), 1 );
		TS_ASSERT_EQUALS( n2msg2->pose_id(), 2 );

		ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 2 that it has another job for it to run", mpi_work_pool_jd_new_job_available );
		std::string ser_larv5 = ts_assert_mpi_buffer_has_string( 0, "node 0 sends node 2 the serialized LarvalJob" );
		LarvalJobOP larval_job5 = deserialize_larval_job( ser_larv5 );
		TS_ASSERT_EQUALS( larval_job5->job_index(), 5 );
		TS_ASSERT_EQUALS( larval_job5->nstruct_index(), 2 );
		TS_ASSERT_EQUALS( larval_job5->nstruct_max(), 3 );
		TS_ASSERT_EQUALS( larval_job5->input_job_result_indices().size(), 1 );
		TS_ASSERT_EQUALS( larval_job5->input_job_result_indices()[ 1 ].first, 1 );
		TS_ASSERT_EQUALS( larval_job5->input_job_result_indices()[ 1 ].second, 1 );
		ts_assert_mpi_buffer_has_sizes( 0, "node 0 sends node 2 a vector1 of job result indices with one element whose value is 0", utility::vector1< Size >( 1, 0 ) );

		ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 2 that it has the job result it was expecting", mpi_work_pool_jd_job_result_retrieved );
		std::string ser_job_res1b = ts_assert_mpi_buffer_has_string( 0, "node 0 sends the serialized larval-job and job-result from job 1" );
		MPIWorkPoolJobDistributor::LarvalJobAndResult job_res1b = deserialized_larval_job_and_job_result( ser_job_res1b );
		TS_ASSERT( job_res1b.first );
		TS_ASSERT( job_res1b.second );
		TS_ASSERT_EQUALS( job_res1b.first->job_index(), 1 );
		TS_ASSERT_EQUALS( job_res1b.first->nstruct_index(), 1 );
		TS_ASSERT_EQUALS( job_res1b.first->nstruct_max(), 3 );
		TS_ASSERT_EQUALS( utility::pointer::dynamic_pointer_cast< PoseJobResult > ( job_res1b.second )->pose()->total_residue(), 20 );

		ts_assert_mpi_buffer_has_integers( 0, "node 0 tells node 2 to archive its results on node 0", utility::vector1< int >( 1, 0 ) );
		ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 2 that archival completed", mpi_work_pool_jd_archival_completed );

		ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 2 that there are no jobs left to run", mpi_work_pool_jd_spin_down );

		SimulateMPI::set_mpi_rank( 3 );
		ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 3 that it has a job for it to run", mpi_work_pool_jd_new_job_available );
		std::string ser_larv3 = ts_assert_mpi_buffer_has_string( 0, "node 0 sends node 3 the serialized LarvalJob" );
		LarvalJobOP larval_job3 = deserialize_larval_job( ser_larv3 );
		TS_ASSERT_EQUALS( larval_job3->job_index(), 3 );
		TS_ASSERT_EQUALS( larval_job3->nstruct_index(), 3 );
		TS_ASSERT_EQUALS( larval_job3->nstruct_max(), 3 );
		ts_assert_mpi_buffer_has_sizes( 0, "node 0 sends node 3 an empty list of job result indices", utility::vector1< Size >() );

		ts_assert_mpi_buffer_has_integers( 0, "node 0 tells node 3 to archive its results on node 0", utility::vector1< int >( 1, 0 ) );
		ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 3 that archival completed", mpi_work_pool_jd_archival_completed );

		ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 3 that there are deallocations that should be made", mpi_work_pool_jd_deallocation_message );
		std::string node3_dealloc_msgs_str = ts_assert_mpi_buffer_has_string( 0, "node 0 sends node 3 the serialized deallocation message list" );
		std::list< deallocation::DeallocationMessageOP > node3_dealloc_msgs = deserialize_deallocation_msg_list( node3_dealloc_msgs_str );
		TS_ASSERT_EQUALS( node3_dealloc_msgs.size(), 2 );
		utility::vector1< deallocation::DeallocationMessageOP > node3_dealloc_msgs_vect( 2 );
		std::copy( node3_dealloc_msgs.begin(), node3_dealloc_msgs.end(), node3_dealloc_msgs_vect.begin() );
		deallocation::InputPoseDeallocationMessageOP n3msg1 = utility::pointer::dynamic_pointer_cast< deallocation::InputPoseDeallocationMessage > ( node3_dealloc_msgs_vect[1] );
		deallocation::InputPoseDeallocationMessageOP n3msg2 = utility::pointer::dynamic_pointer_cast< deallocation::InputPoseDeallocationMessage > ( node3_dealloc_msgs_vect[2] );
		TS_ASSERT( n3msg1 );
		TS_ASSERT( n3msg2 );
		TS_ASSERT_EQUALS( n3msg1->pose_id(), 1 );
		TS_ASSERT_EQUALS( n3msg2->pose_id(), 2 );

		ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 3 that it has another job for it to run", mpi_work_pool_jd_new_job_available );
		std::string ser_larv6 = ts_assert_mpi_buffer_has_string( 0, "node 0 sends node 3 the serialized LarvalJob" );
		LarvalJobOP larval_job6 = deserialize_larval_job( ser_larv6 );
		TS_ASSERT_EQUALS( larval_job6->job_index(), 6 );
		TS_ASSERT_EQUALS( larval_job6->nstruct_index(), 3 );
		TS_ASSERT_EQUALS( larval_job6->nstruct_max(), 3 );
		TS_ASSERT_EQUALS( larval_job6->input_job_result_indices().size(), 1 );
		TS_ASSERT_EQUALS( larval_job6->input_job_result_indices()[ 1 ].first, 1 );
		TS_ASSERT_EQUALS( larval_job6->input_job_result_indices()[ 1 ].second, 1 );
		ts_assert_mpi_buffer_has_sizes( 0, "node 0 sends node 3 a vector1 of job result indices with one element whose value is 0", utility::vector1< Size >( 1, 0 ) );

		ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 3 that it has the job result it was expecting", mpi_work_pool_jd_job_result_retrieved );
		std::string ser_job_res1c = ts_assert_mpi_buffer_has_string( 0, "node 0 sends the serialized larval-job and job-result from job 1" );
		MPIWorkPoolJobDistributor::LarvalJobAndResult job_res1c = deserialized_larval_job_and_job_result( ser_job_res1b );
		TS_ASSERT( job_res1c.first );
		TS_ASSERT( job_res1c.second );
		TS_ASSERT_EQUALS( job_res1c.first->job_index(), 1 );
		TS_ASSERT_EQUALS( job_res1c.first->nstruct_index(), 1 );
		TS_ASSERT_EQUALS( job_res1c.first->nstruct_max(), 3 );
		TS_ASSERT_EQUALS( utility::pointer::dynamic_pointer_cast< PoseJobResult > ( job_res1c.second )->pose()->total_residue(), 20 );

		ts_assert_mpi_buffer_has_integers( 0, "node 0 tells node 3 to archive its results on node 0", utility::vector1< int >( 1, 0 ) );
		ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 3 that archival completed", mpi_work_pool_jd_archival_completed );

		ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 3 that there are no jobs left to run", mpi_work_pool_jd_spin_down );

		TS_ASSERT_EQUALS( jq11->status_.count( 1 ), 1 );
		TS_ASSERT_EQUALS( jq11->status_.count( 2 ), 1 );
		TS_ASSERT_EQUALS( jq11->status_.count( 3 ), 1 );
		TS_ASSERT_EQUALS( jq11->status_.count( 4 ), 1 );
		TS_ASSERT_EQUALS( jq11->status_.count( 5 ), 1 );
		TS_ASSERT_EQUALS( jq11->status_.count( 6 ), 1 );
		TS_ASSERT_EQUALS( jq11->status_[ 1 ], jd3_job_status_success );
		TS_ASSERT_EQUALS( jq11->status_[ 2 ], jd3_job_status_success );
		TS_ASSERT_EQUALS( jq11->status_[ 3 ], jd3_job_status_success );
		TS_ASSERT_EQUALS( jq11->status_[ 4 ], jd3_job_status_success );
		TS_ASSERT_EQUALS( jq11->status_[ 5 ], jd3_job_status_success );
		TS_ASSERT_EQUALS( jq11->status_[ 6 ], jd3_job_status_success );

		TS_ASSERT_EQUALS( jq11->summaries_.count( sp1( 1 ) ), 1 );
		TS_ASSERT_EQUALS( jq11->summaries_.count( sp1( 2 ) ), 1 );
		TS_ASSERT_EQUALS( jq11->summaries_.count( sp1( 3 ) ), 1 );
		TS_ASSERT_EQUALS( jq11->summaries_.count( sp1( 4 ) ), 1 );
		TS_ASSERT_EQUALS( jq11->summaries_.count( sp1( 5 ) ), 1 );
		TS_ASSERT_EQUALS( jq11->summaries_.count( sp1( 6 ) ), 1 );
		TS_ASSERT_EQUALS( utility::pointer::dynamic_pointer_cast< EnergyJobSummary > ( jq11->summaries_[ sp1( 1 ) ])->energy(), 1234 );
		TS_ASSERT_EQUALS( utility::pointer::dynamic_pointer_cast< EnergyJobSummary > ( jq11->summaries_[ sp1( 2 ) ])->energy(), 1234 );
		TS_ASSERT_EQUALS( utility::pointer::dynamic_pointer_cast< EnergyJobSummary > ( jq11->summaries_[ sp1( 3 ) ])->energy(), 1234 );
		TS_ASSERT_EQUALS( utility::pointer::dynamic_pointer_cast< EnergyJobSummary > ( jq11->summaries_[ sp1( 4 ) ])->energy(), 1234 );
		TS_ASSERT_EQUALS( utility::pointer::dynamic_pointer_cast< EnergyJobSummary > ( jq11->summaries_[ sp1( 5 ) ])->energy(), 1234 );
		TS_ASSERT_EQUALS( utility::pointer::dynamic_pointer_cast< EnergyJobSummary > ( jq11->summaries_[ sp1( 6 ) ])->energy(), 1234 );

		TS_ASSERT_EQUALS( jq11->results_->count( sp1( 1 ) ), 0 );
		TS_ASSERT_EQUALS( jq11->results_->count( sp1( 2 ) ), 0 );
		TS_ASSERT_EQUALS( jq11->results_->count( sp1( 3 ) ), 0 );
		TS_ASSERT_EQUALS( jq11->results_->count( sp1( 4 ) ), 0 );
		TS_ASSERT_EQUALS( jq11->results_->count( sp1( 5 ) ), 0 );
		TS_ASSERT_EQUALS( jq11->results_->count( sp1( 6 ) ), 0 );
		//TS_ASSERT_EQUALS( utility::pointer::dynamic_pointer_cast< PoseJobResult > ( (*jq11->results_)[ sp1( 4 ) ])->pose()->total_residue(), 20 );
		//TS_ASSERT_EQUALS( utility::pointer::dynamic_pointer_cast< PoseJobResult > ( (*jq11->results_)[ sp1( 5 ) ])->pose()->total_residue(), 20 );
		//TS_ASSERT_EQUALS( utility::pointer::dynamic_pointer_cast< PoseJobResult > ( (*jq11->results_)[ sp1( 6 ) ])->pose()->total_residue(), 20 );

		SimulateMPI::set_mpi_rank( 0 );

		TS_ASSERT( SimulateMPI::incoming_message_queue_is_empty() );
		TS_ASSERT( SimulateMPI::outgoing_message_queue_is_empty() );

		//std::cout << "Finished XXth Unit Test" << std::endl;


#endif
	}

	void test_jd3_workpool_worker_deallocation_messages_sent_to_remote_nodes() {
		TS_ASSERT( true );

#ifdef SERIALIZATION
		core_init_with_additional_options( "-jd3::n_archive_nodes 1 -jd3::do_not_archive_on_node0 -jd3::compress_job_results 0" );

		EnergyJobSummaryOP trpcage_job_summary( new EnergyJobSummary );
		trpcage_job_summary->energy( 1234 );
		utility::vector1< JobSummaryOP > summaries( 1, trpcage_job_summary );
		std::string serialized_trpcage_job_summaries = serialized_job_summaries( summaries );

		PoseJobResultOP trpcage_pose_result( new PoseJobResult );
		trpcage_pose_result->pose(  create_trpcage_ideal_poseop() );

		SimulateMPI::initialize_simulation( 3 );

		SimulateMPI::set_mpi_rank( 0 );

		// send 1st job
		send_integer_to_node( 2, mpi_work_pool_jd_new_job_available );
		send_string_to_node( 2, serialized_larval_job( 1, 1, 1 ) );
		send_sizes_to_node( 2, utility::vector1< Size >() );

		// tell node 2 where to send the results of job 1:
		send_integers_to_node( 2, utility::vector1< int >( 1, 1 ) );

		SimulateMPI::set_mpi_rank( 1 );
		// ok, node 2 will say that it needs to send us a job, and it'll send it to node 1 (the archive)
		// and then the archive needs to reply that the archival was successful
		send_integer_to_node( 2, mpi_work_pool_jd_archival_completed );

		SimulateMPI::set_mpi_rank( 0 );
		// send deallocation message
		send_integer_to_node( 2, mpi_work_pool_jd_deallocation_message );
		std::list< core::Size > pose_ids{ 1, 2 };
		send_string_to_node( 2, serialized_input_pose_deallocation_msg_list( pose_ids ) );

		// send 2nd job, which lists job 1 as a required input
		send_integer_to_node( 2, mpi_work_pool_jd_new_job_available );
		send_string_to_node( 2, serialized_T( create_larval_job_w_job1_dep( 1, 1, 2 ) ));
		send_sizes_to_node( 2, utility::vector1< Size >( 1, 1 ) ); // say that job 1's results live on node 1

		SimulateMPI::set_mpi_rank( 1 );
		send_integer_to_node( 2, mpi_work_pool_jd_job_result_retrieved );
		send_string_to_node( 2, serialized_larval_job_and_job_result( 1, 1, 1, trpcage_pose_result ));

		SimulateMPI::set_mpi_rank( 0 );
		// tell node 2 where to send the results of job 2:
		send_integers_to_node( 2, utility::vector1< int >( 1, 1 ) );

		SimulateMPI::set_mpi_rank( 1 );
		// ok, node 2 will say that it needs to send us a job, and it'll send it to node 1 (the archive)
		// and then the archive needs to reply that the archival was successful
		send_integer_to_node( 2, mpi_work_pool_jd_archival_completed );

		SimulateMPI::set_mpi_rank( 0 );
		send_integer_to_node( 2, mpi_work_pool_jd_spin_down ); // no more jobs to hand out

		//OK! Now create a PoolJQ11 and let 'er rip
		SimulateMPI::set_mpi_rank( 2 );
		PoolJQ11OP jq11( new PoolJQ11 );
		MPIWorkPoolJobDistributor jd;
		try {
			jd.go( jq11 );
		} catch ( ... ) {
			TS_ASSERT( false /*no exception should be thrown*/ );
		}

		// OK!
		// Now we read the mail we got from node 2

		SimulateMPI::set_mpi_rank( 0 );
		// Node 2 starts by requesting a job
		ts_assert_mpi_buffer_has_integer( 2, "node 2 says to node 0: I have a request A", 2 );
		ts_assert_mpi_buffer_has_integer( 2, "node 2 requests a new job of node 0", mpi_work_pool_jd_new_job_request );

		// Then it replies that its job is complete
		ts_assert_mpi_buffer_has_integer( 2, "node 2 says to node 0: I have a request B", 2 );
		ts_assert_mpi_buffer_has_integer( 2, "node 2 has finished its job", mpi_work_pool_jd_job_success );
		ts_assert_mpi_buffer_has_size( 2, "node 2 finished job #1", 1 );
		ts_assert_mpi_buffer_has_size( 2, "node 2 finished job #1 with 1 result", 1 );

		ts_assert_mpi_buffer_has_integer( 2, "node 2 says to node 0: I have a request C", 2 );
		ts_assert_mpi_buffer_has_integer( 2, "node 2 says to node 0: I finished archiving my completed job result -- I'm ready to send a JobSummary", mpi_work_pool_jd_job_success_and_archival_complete );
		ts_assert_mpi_buffer_has_size( 2, "node 2 says to node 0: this was for job index 1", 1 );
		ts_assert_mpi_buffer_has_integer( 2, "node 2 says to node 0: job 1 was successful", jd3_job_status_success );
		std::string serialized_summary1 = ts_assert_mpi_buffer_has_string( 2, "node 2 says to node 0: here's the summary" );
		utility::vector1< JobSummaryOP > summaries1 = deserialized_job_summaries( serialized_summary1 );
		TS_ASSERT_EQUALS( summaries1.size(), 1 );
		if ( summaries1.size() != 1 ) return;
		TS_ASSERT( summaries1[ 1 ] );
		StandardPoseJobSummaryOP energy_summary1 = utility::pointer::dynamic_pointer_cast< StandardPoseJobSummary > ( summaries1[ 1 ] );
		TS_ASSERT( energy_summary1 );
		if ( energy_summary1 ) {
			TS_ASSERT_EQUALS( energy_summary1->energy(), 1234 );
		}

		// Now Node 2 requests another job
		ts_assert_mpi_buffer_has_integer( 2, "node 2 says to node 0: I have a request D", 2 );
		ts_assert_mpi_buffer_has_integer( 2, "node 2 requests a new job of node 0", mpi_work_pool_jd_new_job_request );

		// Then it replies that its job is complete
		ts_assert_mpi_buffer_has_integer( 2, "node 2 says to node 0: I have a request E", 2 );
		ts_assert_mpi_buffer_has_integer( 2, "node 2 has finished its job", mpi_work_pool_jd_job_success );
		ts_assert_mpi_buffer_has_size( 2, "node 2 finished job #2", 2 );
		ts_assert_mpi_buffer_has_size( 2, "node 2 finished job #2 which generated one result", 1 );

		ts_assert_mpi_buffer_has_integer( 2, "node 2 says to node 0: I have a request F", 2 );
		ts_assert_mpi_buffer_has_integer( 2, "node 2 says to node 0: I finished archiving my completed job result -- I'm ready to send a JobSummary", mpi_work_pool_jd_job_success_and_archival_complete );
		ts_assert_mpi_buffer_has_size( 2, "node 2 says to node 0: this was for job index 2", 2 );
		ts_assert_mpi_buffer_has_integer( 2, "node 2 says to node 0: job 2 was successful", jd3_job_status_success );
		std::string serialized_summary2 = ts_assert_mpi_buffer_has_string( 2, "node 2 says to node 0: here's the summary" );
		utility::vector1< JobSummaryOP > summaries2 = deserialized_job_summaries( serialized_summary2 );
		TS_ASSERT_EQUALS( summaries2.size(), 1 );
		if ( summaries2.size() != 1 ) return;
		TS_ASSERT( summaries2[1] );
		StandardPoseJobSummaryOP energy_summary2 = utility::pointer::dynamic_pointer_cast< StandardPoseJobSummary > ( summaries2[1] );
		TS_ASSERT( energy_summary2 );
		if ( energy_summary2 ) {
			TS_ASSERT_EQUALS( energy_summary2->energy(), 1234 );
		}

		// Now Node 2 requests another job, and it will turn out that there are no more, so node 0 will send a spin down signal.
		ts_assert_mpi_buffer_has_integer( 2, "node 2 says to node 0: I have a request G", 2 );
		ts_assert_mpi_buffer_has_integer( 2, "node 2 requests a new job of node 0", mpi_work_pool_jd_new_job_request );

		SimulateMPI::set_mpi_rank( 1 );

		// Let's see what node 2 sent to the archive node.
		ts_assert_mpi_buffer_has_integer( 2, "node 2 says \"I have a message for you, archive\"", 2 ); // node 2 says "I have a message for you, archive"
		ts_assert_mpi_buffer_has_integer( 2, "node 2 says \"please archive this job result\"", mpi_work_pool_jd_archive_job_result ); // "please archive this job result"
		ts_assert_mpi_buffer_has_size( 2, "node 2 says this job result is for job #1", 1 ); // job_id
		ts_assert_mpi_buffer_has_size( 2, "node 2 says this job result is for job #1 result #1", 1 ); // result index
		std::string serialized_job_result1 = ts_assert_mpi_buffer_has_string( 2, "node 2 sends the serialized trpcage pose result for job 1" );
		MPIWorkPoolJobDistributor::LarvalJobAndResult job_and_result1 = deserialized_larval_job_and_job_result( serialized_job_result1 );
		TS_ASSERT( job_and_result1.first );
		TS_ASSERT( job_and_result1.second );
		if ( job_and_result1.first ) {
			TS_ASSERT_EQUALS( job_and_result1.first->job_index(), 1 );
			TS_ASSERT_EQUALS( job_and_result1.first->nstruct_index(), 1 );
			TS_ASSERT_EQUALS( job_and_result1.first->nstruct_max(), 1 );
		}
		PoseJobResultOP pose_result1 = utility::pointer::dynamic_pointer_cast< PoseJobResult > ( job_and_result1.second );
		TS_ASSERT( pose_result1 );
		if ( pose_result1 ) {
			TS_ASSERT_EQUALS( pose_result1->pose()->total_residue(), 20 );
		}

		// node 2 then asks for the job results from job 1
		ts_assert_mpi_buffer_has_integer( 2, "node 2 says to the archive that it has a request", 2 );
		ts_assert_mpi_buffer_has_integer( 2, "node 2 says that it needs a job result",  mpi_work_pool_jd_retrieve_job_result );
		ts_assert_mpi_buffer_has_size( 2, "node 2 says that it wants the job 1", 1 ); // retrieve the result from job #1
		ts_assert_mpi_buffer_has_size( 2, "node 2 says that it wants the job 1 with result index 1", 1 ); // retrieve the result from job #1 result #1

		// finally, node 2 sends the job results from job 2
		ts_assert_mpi_buffer_has_integer( 2, "node 2 says \"I have a message for you, archive\" B", 2 ); // node 2 says "I have a message for you, archive"
		ts_assert_mpi_buffer_has_integer( 2, "node 2 says \"please archive this job result\" B", mpi_work_pool_jd_archive_job_result ); // "please archive this job result"
		ts_assert_mpi_buffer_has_size( 2, "node 2 says that the job id is 2", 2 ); // job_ib
		ts_assert_mpi_buffer_has_size( 2, "node 2 says that the job id is 2", 1 ); // result index 1
		std::string serialized_job_result2 = ts_assert_mpi_buffer_has_string( 2, "node 2 sends the serialized trpcage pose result for job 2" );
		MPIWorkPoolJobDistributor::LarvalJobAndResult job_and_result2 = deserialized_larval_job_and_job_result( serialized_job_result2 );
		TS_ASSERT( job_and_result2.first );
		TS_ASSERT( job_and_result2.second );
		if ( job_and_result2.first ) {
			TS_ASSERT_EQUALS( job_and_result2.first->job_index(), 2 );
			TS_ASSERT_EQUALS( job_and_result2.first->nstruct_index(), 1 );
			TS_ASSERT_EQUALS( job_and_result2.first->nstruct_max(), 1 );
			TS_ASSERT_EQUALS( job_and_result2.first->input_job_result_indices().size(), 1 );
			TS_ASSERT_EQUALS( job_and_result2.first->input_job_result_indices()[ 1 ].first, 1 );
			TS_ASSERT_EQUALS( job_and_result2.first->input_job_result_indices()[ 1 ].second, 1 );
		}
		PoseJobResultOP pose_result2 = utility::pointer::dynamic_pointer_cast< PoseJobResult > ( job_and_result2.second );
		TS_ASSERT( pose_result2 );
		if ( pose_result2 ) {
			TS_ASSERT_EQUALS( pose_result2->pose()->total_residue(), 20 );
		}

		TS_ASSERT_EQUALS( jq11->received_messages_.size(), 2 );
		typedef deallocation::InputPoseDeallocationMessageOP DeallocPoseOP;
		typedef deallocation::InputPoseDeallocationMessage DeallocPose;
		DeallocPoseOP msg1 = utility::pointer::dynamic_pointer_cast< DeallocPose >( jq11->received_messages_[ 1 ] );
		DeallocPoseOP msg2 = utility::pointer::dynamic_pointer_cast< DeallocPose >( jq11->received_messages_[ 2 ] );
		TS_ASSERT( msg1 );
		TS_ASSERT( msg2 );
		TS_ASSERT_EQUALS( msg1->pose_id(), 1 );
		TS_ASSERT_EQUALS( msg2->pose_id(), 2 );

#endif
	}

	void test_jd3_workpool_jd_checkpoint_master_node() {
		// OK -- I'll define here a JQ that takes several seconds to accomplish
		// a particular step so that I can rest assured that a serialization event
		// has occurred after the first job from the second JobDAG node has been
		// handed out to a worker
		// A lot of this code is bascially copy and pasted from
		// test_jd3_workpool_archive_two_rounds_one_archive_no_archiving_on_node_0
		// except that right after the JD asks for the first job for round two,
		// it's going to declare a checkpoint event and communicate with the archive
		// (node 1) saying that the archive needs to create a checkpoint file.
		//
		// So the archive needs to send a message "back" to node 0 that says "I have
		// completed my archival"

		TS_ASSERT( true );

#ifdef SERIALIZATION
		core_init_with_additional_options( "-jd3::n_archive_nodes 1 -jd3::do_not_archive_on_node0 -jd3::compress_job_results 0 -jd3::mpi_fraction_outputters 0.4 -jd3::checkpoint_directory wpjd_checkpoints_1 -jd3::keep_all_checkpoints" );

		EnergyJobSummaryOP trpcage_job_summary( new EnergyJobSummary );
		trpcage_job_summary->energy( 1234 );
		utility::vector1< JobSummaryOP > summaries( 1, trpcage_job_summary );
		std::string serialized_trpcage_job_summaries = serialized_job_summaries( summaries );


		PoseJobResultOP trpcage_pose_result( new PoseJobResult );
		trpcage_pose_result->pose(  create_trpcage_ideal_poseop() );

		SimulateMPI::initialize_simulation( 5 );

		// ok, all the nodes at once start requesting jobs from node 0
		SimulateMPI::set_mpi_rank( 2 );
		send_integer_to_node( 0, 2 );
		SimulateMPI::set_mpi_rank( 3 );
		send_integer_to_node( 0, 3 );
		send_integer_to_node( 0, mpi_work_pool_jd_new_job_request );
		SimulateMPI::set_mpi_rank( 4 );
		send_integer_to_node( 0, 4 );

		// let's start the job at node 2
		SimulateMPI::set_mpi_rank( 2 );
		send_integer_to_node( 0, mpi_work_pool_jd_new_job_request );

		// and now the job at node 4
		SimulateMPI::set_mpi_rank( 4 );
		send_integer_to_node( 0, mpi_work_pool_jd_new_job_request );

		// ok -- now let's pretend that node 3 has finished its job
		SimulateMPI::set_mpi_rank( 3 );
		send_integer_to_node( 0, 3 );

		// ok -- before node 3 can finish its two part job-completion process,
		// node 2 will butt in and start its two part job-completion process,
		SimulateMPI::set_mpi_rank( 2 );
		send_integer_to_node( 0, 2 );

		SimulateMPI::set_mpi_rank( 3 );
		send_integer_to_node( 0, mpi_work_pool_jd_job_success );
		send_size_to_node( 0, 2 ); // job_id
		send_size_to_node( 0, 1 ); // num results
		// sent to archive instead -- send_string_to_node( 0, serialized_larval_job_and_job_result( 3, 2, 2, trpcage_pose_result ));

		SimulateMPI::set_mpi_rank( 2 );
		send_integer_to_node( 0, mpi_work_pool_jd_job_success );
		send_size_to_node( 0, 1 ); // job id
		send_size_to_node( 0, 1 ); // num results
		// sent to archive instead -- send_string_to_node( 0, serialized_larval_job_and_job_result( 3, 1, 1, trpcage_pose_result ));

		SimulateMPI::set_mpi_rank( 3 );
		send_integer_to_node( 0, 3 );
		send_integer_to_node( 0, mpi_work_pool_jd_job_success_and_archival_complete );
		send_size_to_node( 0, 2 ); // job id
		send_integer_to_node( 0, jd3_job_status_success );
		send_string_to_node( 0, serialized_trpcage_job_summaries );

		SimulateMPI::set_mpi_rank( 4 );
		send_integer_to_node( 0, 4 );
		send_integer_to_node( 0, mpi_work_pool_jd_job_success );
		send_size_to_node( 0, 3 ); // job id
		send_size_to_node( 0, 1 ); // num results
		// sent to archive instead -- send_string_to_node( 0, serialized_larval_job_and_job_result( 3, 3, 3, trpcage_pose_result ));

		SimulateMPI::set_mpi_rank( 2 );
		send_integer_to_node( 0, 2 );
		send_integer_to_node( 0, mpi_work_pool_jd_job_success_and_archival_complete );
		send_size_to_node( 0, 1 ); // job id
		send_integer_to_node( 0, jd3_job_status_success ); // job id
		send_string_to_node( 0, serialized_trpcage_job_summaries );

		// Now node 2 asks for a new job
		send_integer_to_node( 0, 2 );
		send_integer_to_node( 0, mpi_work_pool_jd_new_job_request );

		SimulateMPI::set_mpi_rank( 3 );
		// Now node 3 asks for a new job
		send_integer_to_node( 0, 3 );
		send_integer_to_node( 0, mpi_work_pool_jd_new_job_request );


		SimulateMPI::set_mpi_rank( 4 );
		send_integer_to_node( 0, 4 );
		send_integer_to_node( 0, mpi_work_pool_jd_job_success_and_archival_complete );
		send_size_to_node( 0, 3 ); // job id
		send_integer_to_node( 0, jd3_job_status_success );
		send_string_to_node( 0, serialized_trpcage_job_summaries );

		// CHECKPOINT!
		// After the last job from DAG-node 1has completed, the JQ12 will pause for two seconds
		// so that when the JD0 returns to the top of its listening loop, enough time will have
		// passed so that it decides to create a checkpoint. At this stage, JD0 will send a
		// message to the archive and the archive will have to reply that it successfully
		// checkpointed its data.
		SimulateMPI::set_mpi_rank(1);
		send_integer_to_node(0, mpi_work_pool_jd_checkpointing_complete );


		SimulateMPI::set_mpi_rank(4);
		// Now node 4 asks for a new job
		send_integer_to_node( 0, 4 );
		send_integer_to_node( 0, mpi_work_pool_jd_new_job_request );

		SimulateMPI::set_mpi_rank( 2 );

		// now retrieve the job result for job 1
		// send_integer_to_node( 0, 2 );
		// send_integer_to_node( 0, mpi_work_pool_jd_retrieve_job_result );
		// send_size_to_node( 0, 1 ); // retrieve the result from job #1

		SimulateMPI::set_mpi_rank( 3 );

		// now retrieve the job result for job 1
		// send_integer_to_node( 0, 3 );
		// send_integer_to_node( 0, mpi_work_pool_jd_retrieve_job_result );
		// send_size_to_node( 0, 1 ); // retrieve the result from job #1

		SimulateMPI::set_mpi_rank( 4 );

		// now retrieve the job result for job 1
		// send_integer_to_node( 0, 4 );
		// send_integer_to_node( 0, mpi_work_pool_jd_retrieve_job_result );
		// send_size_to_node( 0, 1 ); // retrieve the result from job #1

		// Now the second round of job results start trickling in

		SimulateMPI::set_mpi_rank( 2 );

		send_integer_to_node( 0, 2 );
		send_integer_to_node( 0, mpi_work_pool_jd_job_success );
		send_size_to_node( 0, 4 ); // job id
		send_size_to_node( 0, 1 ); // num results
		// sent to archive instead -- send_string_to_node( 0, serialized_larval_job_and_job_result( 3, 1, 4, trpcage_pose_result ));

		send_integer_to_node( 0, 2 );
		send_integer_to_node( 0, mpi_work_pool_jd_job_success_and_archival_complete );
		send_size_to_node( 0, 4 ); // job id
		send_integer_to_node( 0, jd3_job_status_success );
		send_string_to_node( 0, serialized_trpcage_job_summaries );

		send_integer_to_node( 0, 2 );
		send_integer_to_node( 0, mpi_work_pool_jd_new_job_request );

		SimulateMPI::set_mpi_rank( 4 );
		send_integer_to_node( 0, 4 );
		send_integer_to_node( 0, mpi_work_pool_jd_job_success );
		send_size_to_node( 0, 6 ); // job id
		send_size_to_node( 0, 1 ); // num results
		// sent to archive instead -- send_string_to_node( 0, serialized_larval_job_and_job_result( 3, 3, 6, trpcage_pose_result ));

		send_integer_to_node( 0, 4 );
		send_integer_to_node( 0, mpi_work_pool_jd_job_success_and_archival_complete );
		send_size_to_node( 0, 6 ); // job id
		send_integer_to_node( 0, jd3_job_status_success );
		send_string_to_node( 0, serialized_trpcage_job_summaries );

		send_integer_to_node( 0, 4 );
		send_integer_to_node( 0, mpi_work_pool_jd_new_job_request );

		SimulateMPI::set_mpi_rank( 3 );
		send_integer_to_node( 0, 3 );
		send_integer_to_node( 0, mpi_work_pool_jd_job_success );
		send_size_to_node( 0, 5 ); // job id
		send_size_to_node( 0, 1 ); // num results
		// sent to archive instead -- send_string_to_node( 0, serialized_larval_job_and_job_result( 3, 2, 5, trpcage_pose_result ));

		send_integer_to_node( 0, 3 );
		send_integer_to_node( 0, mpi_work_pool_jd_job_success_and_archival_complete );
		send_size_to_node( 0, 5 ); // job id
		send_integer_to_node( 0, jd3_job_status_success );
		send_string_to_node( 0, serialized_trpcage_job_summaries );

		send_integer_to_node( 0, 3 );
		send_integer_to_node( 0, mpi_work_pool_jd_new_job_request );

		// OK -- output time.
		// Have node 2 output two of the jobs and node 1 output one of them
		// to test a race condition.
		// Node 0 will hand both node 1 and node 2 a job to output.
		// Node 2 will report it has finished its output first,
		// Node 0 will assign node 2 a second job to output,
		// and then node 1 will say it has finished.
		// Node 0 should not spin down node 1 until after node 2 has said
		// that it has completed output of the 2nd job.

		SimulateMPI::set_mpi_rank(2);
		send_integer_to_node( 0, 2 );
		send_integer_to_node( 0, mpi_work_pool_jd_output_completed );
		send_integer_to_node( 0, 2 );
		send_integer_to_node( 0, mpi_work_pool_jd_new_job_request );


		SimulateMPI::set_mpi_rank( 1 );
		send_integer_to_node( 0, 1 );
		send_integer_to_node( 0, mpi_work_pool_jd_output_completed );

		SimulateMPI::set_mpi_rank(2);
		send_integer_to_node( 0, 2 );
		send_integer_to_node( 0, mpi_work_pool_jd_output_completed );
		send_integer_to_node( 0, 2 );
		send_integer_to_node( 0, mpi_work_pool_jd_new_job_request );


		//OK! Now create a PoolJQ12
		SimulateMPI::set_mpi_rank( 0 );
		PoolJQ12OP jq12( new PoolJQ12 );
		MPIWorkPoolJobDistributor jd;
		jd.set_checkpoint_period(1); // 1 second between checkpoints.
		try {
			jd.go( jq12 );
		} catch ( utility::excn::Exception & e) {
			TR << "Unexpected exception " << e.msg() << std::endl;
			TS_ASSERT( false /*no exception should be thrown*/ );
		} catch ( ... ) {
			TR << "Uncaught and unexpected exception" << std::endl;
			TS_ASSERT( false /*no exception should be thrown*/ );
		}

		// OK!
		// Now we read the mail we got from node 0

		SimulateMPI::set_mpi_rank( 2 );
		ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 2 that it has a job for it to run", mpi_work_pool_jd_new_job_available );
		std::string ser_larv1 = ts_assert_mpi_buffer_has_string( 0, "node 0 sends node 2 the serialized LarvalJob" );
		LarvalJobOP larval_job1 = deserialize_larval_job( ser_larv1 );
		TS_ASSERT_EQUALS( larval_job1->job_index(), 1 );
		TS_ASSERT_EQUALS( larval_job1->nstruct_index(), 1 );
		TS_ASSERT_EQUALS( larval_job1->nstruct_max(), 3 );
		ts_assert_mpi_buffer_has_sizes( 0, "node 0 sends node 2 an empty list of job result indices", utility::vector1< Size >() );

		ts_assert_mpi_buffer_has_integers( 0, "node 0 tells node 2 to archive its results on node 1", utility::vector1< int >( 1, 1 ) );
		// ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 2 that archival completed", mpi_work_pool_jd_archival_completed );

		ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 2 that it has another job for it to run", mpi_work_pool_jd_new_job_available );
		std::string ser_larv4 = ts_assert_mpi_buffer_has_string( 0, "node 0 sends node 2 the serialized LarvalJob" );
		LarvalJobOP larval_job4 = deserialize_larval_job( ser_larv4 );
		TS_ASSERT_EQUALS( larval_job4->job_index(), 4 );
		TS_ASSERT_EQUALS( larval_job4->nstruct_index(), 1 );
		TS_ASSERT_EQUALS( larval_job4->nstruct_max(), 3 );
		TS_ASSERT_EQUALS( larval_job4->input_job_result_indices().size(), 1 );
		TS_ASSERT_EQUALS( larval_job4->input_job_result_indices()[ 1 ].first, 1 );
		TS_ASSERT_EQUALS( larval_job4->input_job_result_indices()[ 1 ].second, 1 );
		ts_assert_mpi_buffer_has_sizes( 0, "node 0 sends node 2 a vector1 of job result indices with one element whose value is 1", utility::vector1< Size >( 1, 1 ) );

		//ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 2 that it has the job result it was expecting", mpi_work_pool_jd_job_result_retrieved );
		// std::string ser_job_res1a = ts_assert_mpi_buffer_has_string( 0, "node 0 sends the serialized larval-job and job-result from job 1" );
		// MPIWorkPoolJobDistributor::LarvalJobAndResult job_res1a = deserialized_larval_job_and_job_result( ser_job_res1a );
		// TS_ASSERT( job_res1a.first );
		// TS_ASSERT( job_res1a.second );
		// TS_ASSERT_EQUALS( job_res1a.first->job_index(), 1 );
		// TS_ASSERT_EQUALS( job_res1a.first->nstruct_index(), 1 );
		// TS_ASSERT_EQUALS( job_res1a.first->nstruct_max(), 3 );
		// TS_ASSERT_EQUALS( utility::pointer::dynamic_pointer_cast< PoseJobResult > ( job_res1a.second )->pose()->total_residue(), 20 );

		ts_assert_mpi_buffer_has_integers( 0, "node 0 tells node 2 to archive its results on node 1", utility::vector1< int >( 1, 1 ) );
		// ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 2 that archival completed", mpi_work_pool_jd_archival_completed );

		ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 2 that there is a job that it needs to output", mpi_work_pool_jd_output_job_result_on_archive );
		ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 2 that the job result is archived on node 1", 1);
		std::string ser_os5 = ts_assert_mpi_buffer_has_string( 0, "node 0 then sends node 2 the serialized job output specification for job result (5,1)" );
		OutputSpecificationOP os5 = deserialized_output_specification( ser_os5 );
		TS_ASSERT_EQUALS( os5->result_id().first, 5 );
		TS_ASSERT_EQUALS( os5->result_id().second, 1 );

		ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 2 that there is another job that it needs to output", mpi_work_pool_jd_output_job_result_on_archive );
		ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 2 that the job result is archived on node 1", 1);
		std::string ser_os6 = ts_assert_mpi_buffer_has_string( 0, "node 0 then sends node 2 the serialized job output specification for job result (6,1)" );
		OutputSpecificationOP os6 = deserialized_output_specification( ser_os6 );
		TS_ASSERT_EQUALS( os6->result_id().first, 6 );
		TS_ASSERT_EQUALS( os6->result_id().second, 1 );

		// Finally, spin down
		Size spin_down_node2_message_ind = utility::SimulateMPI::index_of_next_message();
		ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 2 that there are no jobs left to run", mpi_work_pool_jd_spin_down );

		SimulateMPI::set_mpi_rank( 3 );
		ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 3 that it has a job for it to run", mpi_work_pool_jd_new_job_available );
		std::string ser_larv2 = ts_assert_mpi_buffer_has_string( 0, "node 0 sends node 3 the serialized LarvalJob" );
		LarvalJobOP larval_job2 = deserialize_larval_job( ser_larv2 );
		TS_ASSERT_EQUALS( larval_job2->job_index(), 2 );
		TS_ASSERT_EQUALS( larval_job2->nstruct_index(), 2 );
		TS_ASSERT_EQUALS( larval_job2->nstruct_max(), 3 );
		ts_assert_mpi_buffer_has_sizes( 0, "node 0 sends node 3 an empty list of job result indices", utility::vector1< Size >() );

		ts_assert_mpi_buffer_has_integers( 0, "node 0 tells node 3 to archive its results on node 1", utility::vector1< int >( 1, 1 ) );
		//ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 3 that archival completed", mpi_work_pool_jd_archival_completed );

		ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 3 that it has another job for it to run", mpi_work_pool_jd_new_job_available );
		std::string ser_larv5 = ts_assert_mpi_buffer_has_string( 0, "node 0 sends node 3 the serialized LarvalJob" );
		LarvalJobOP larval_job5 = deserialize_larval_job( ser_larv5 );
		TS_ASSERT_EQUALS( larval_job5->job_index(), 5 );
		TS_ASSERT_EQUALS( larval_job5->nstruct_index(), 2 );
		TS_ASSERT_EQUALS( larval_job5->nstruct_max(), 3 );
		TS_ASSERT_EQUALS( larval_job5->input_job_result_indices().size(), 1 );
		TS_ASSERT_EQUALS( larval_job5->input_job_result_indices()[ 1 ].first, 1 );
		TS_ASSERT_EQUALS( larval_job5->input_job_result_indices()[ 1 ].second, 1 );
		ts_assert_mpi_buffer_has_sizes( 0, "node 0 sends node 3 a vector1 of job result indices with one element whose value is 1", utility::vector1< Size >( 1, 1 ) );

		// ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 3 that it has the job result it was expecting", mpi_work_pool_jd_job_result_retrieved );
		// std::string ser_job_res1b = ts_assert_mpi_buffer_has_string( 0, "node 0 sends the serialized larval-job and job-result from job 1" );
		// MPIWorkPoolJobDistributor::LarvalJobAndResult job_res1b = deserialized_larval_job_and_job_result( ser_job_res1b );
		// TS_ASSERT( job_res1b.first );
		// TS_ASSERT( job_res1b.second );
		// TS_ASSERT_EQUALS( job_res1b.first->job_index(), 1 );
		// TS_ASSERT_EQUALS( job_res1b.first->nstruct_index(), 1 );
		// TS_ASSERT_EQUALS( job_res1b.first->nstruct_max(), 3 );
		// TS_ASSERT_EQUALS( utility::pointer::dynamic_pointer_cast< PoseJobResult > ( job_res1b.second )->pose()->total_residue(), 20 );

		ts_assert_mpi_buffer_has_integers( 0, "node 0 tells node 3 to archive its results on node 1", utility::vector1< int >( 1, 1 ) );
		// ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 3 that archival completed", mpi_work_pool_jd_archival_completed );

		ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 3 that there are no jobs left to run", mpi_work_pool_jd_spin_down );

		SimulateMPI::set_mpi_rank( 4 );
		ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 4 that it has a job for it to run", mpi_work_pool_jd_new_job_available );
		std::string ser_larv3 = ts_assert_mpi_buffer_has_string( 0, "node 0 sends node 4 the serialized LarvalJob" );
		LarvalJobOP larval_job3 = deserialize_larval_job( ser_larv3 );
		TS_ASSERT_EQUALS( larval_job3->job_index(), 3 );
		TS_ASSERT_EQUALS( larval_job3->nstruct_index(), 3 );
		TS_ASSERT_EQUALS( larval_job3->nstruct_max(), 3 );
		ts_assert_mpi_buffer_has_sizes( 0, "node 0 sends node 4 an empty list of job result indices", utility::vector1< Size >() );

		ts_assert_mpi_buffer_has_integers( 0, "node 0 tells node 4 to archive its results on node 1", utility::vector1< int >( 1, 1 ) );
		// ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 4 that archival completed", mpi_work_pool_jd_archival_completed );

		ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 4 that it has another job for it to run", mpi_work_pool_jd_new_job_available );
		std::string ser_larv6 = ts_assert_mpi_buffer_has_string( 0, "node 0 sends node 4 the serialized LarvalJob" );
		LarvalJobOP larval_job6 = deserialize_larval_job( ser_larv6 );
		TS_ASSERT_EQUALS( larval_job6->job_index(), 6 );
		TS_ASSERT_EQUALS( larval_job6->nstruct_index(), 3 );
		TS_ASSERT_EQUALS( larval_job6->nstruct_max(), 3 );
		TS_ASSERT_EQUALS( larval_job6->input_job_result_indices().size(), 1 );
		TS_ASSERT_EQUALS( larval_job6->input_job_result_indices()[ 1 ].first, 1 );
		TS_ASSERT_EQUALS( larval_job6->input_job_result_indices()[ 1 ].second, 1 );
		ts_assert_mpi_buffer_has_sizes( 0, "node 0 sends node 4 a vector1 of job result indices with one element whose value is 1", utility::vector1< Size >( 1, 1 ) );

		// ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 4 that it has the job result it was expecting", mpi_work_pool_jd_job_result_retrieved );
		// std::string ser_job_res1c = ts_assert_mpi_buffer_has_string( 0, "node 0 sends the serialized larval-job and job-result from job 1" );
		// MPIWorkPoolJobDistributor::LarvalJobAndResult job_res1c = deserialized_larval_job_and_job_result( ser_job_res1b );
		// TS_ASSERT( job_res1c.first );
		// TS_ASSERT( job_res1c.second );
		// TS_ASSERT_EQUALS( job_res1c.first->job_index(), 1 );
		// TS_ASSERT_EQUALS( job_res1c.first->nstruct_index(), 1 );
		// TS_ASSERT_EQUALS( job_res1c.first->nstruct_max(), 3 );
		// TS_ASSERT_EQUALS( utility::pointer::dynamic_pointer_cast< PoseJobResult > ( job_res1c.second )->pose()->total_residue(), 20 );

		ts_assert_mpi_buffer_has_integers( 0, "node 0 tells node 4 to archive its results on node 1", utility::vector1< int >( 1, 1 ) );
		// ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 4 that archival completed", mpi_work_pool_jd_archival_completed );

		ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 4 that there are no jobs left to run", mpi_work_pool_jd_spin_down );

		SimulateMPI::set_mpi_rank( 1 ); // the archive node
		// first messages from node 0 are to discard the results from jobs 2 and 3
		ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 1 that it wants its attention A", 0 );
		ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 1 that it needs to discard a job A", mpi_work_pool_jd_discard_job_result );
		ts_assert_mpi_buffer_has_size( 0, "node 0 tells node 1 to discard job 2", 2 );
		ts_assert_mpi_buffer_has_size( 0, "node 0 tells node 1 to discard job 2 result 1", 1 );

		ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 1 that it wants its attention B", 0 );
		ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 1 that it needs to discard a job B", mpi_work_pool_jd_discard_job_result );
		ts_assert_mpi_buffer_has_size( 0, "node 0 tells node 1 to discard job 3", 3 );
		ts_assert_mpi_buffer_has_size( 0, "node 0 tells node 1 to discard job 3 result 1", 1 );

		//CHECKPOINT!
		ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 1 that it wants its attention CHPT1", 0 );
		ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 1 that it's time to checkpoint", mpi_work_pool_jd_begin_checkpointing );
		ts_assert_mpi_buffer_has_string( 0, "node 0 tells node 1 that the file it should write to has the prefix 'wpjd_checkpoints_1/chkpt_1'", "wpjd_checkpoints_1/chkpt_1" );

		ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 1 that it wants its attention C", 0 );
		ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 1 that it needs to output a job A", mpi_work_pool_jd_output_job_result_already_available );
		std::string ser_os4 = ts_assert_mpi_buffer_has_string( 0,
			"node 0 sends node 1 the OutputSpecification for job result (4,1)" );
		OutputSpecificationOP os4 = deserialized_output_specification( ser_os4 );
		TS_ASSERT_EQUALS( os4->result_id().first, 4 );
		TS_ASSERT_EQUALS( os4->result_id().second, 1 );

		ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 1 that it wants its attention D", 0 );
		Size spin_down_node1_message_ind = SimulateMPI::index_of_next_message();
		ts_assert_mpi_buffer_has_integer( 0, "node 0 tells node 1 that there are no jobs left to run", mpi_work_pool_jd_spin_down );

		// CRITICAL! The job distributor must not spin down node 1 until
		// node 2 has said that it has completed outputting everything
		// or you get a node 2 waiting forever for node 1 to reply to
		// a request for the result that node 2 is tasked with outputting
		// (which node 1 won't do because it has left its listener loop)
		// std::cout << "Spin down messages: spin_down_node2_message_ind " << spin_down_node2_message_ind << " spin_down_node1_message_ind " << spin_down_node1_message_ind << std::endl;
		TS_ASSERT( spin_down_node2_message_ind < spin_down_node1_message_ind );

		TS_ASSERT_EQUALS( jq12->status_.count( 1 ), 1 );
		TS_ASSERT_EQUALS( jq12->status_.count( 2 ), 1 );
		TS_ASSERT_EQUALS( jq12->status_.count( 3 ), 1 );
		TS_ASSERT_EQUALS( jq12->status_.count( 4 ), 1 );
		TS_ASSERT_EQUALS( jq12->status_.count( 5 ), 1 );
		TS_ASSERT_EQUALS( jq12->status_.count( 6 ), 1 );
		TS_ASSERT_EQUALS( jq12->status_[ 1 ], jd3_job_status_success );
		TS_ASSERT_EQUALS( jq12->status_[ 2 ], jd3_job_status_success );
		TS_ASSERT_EQUALS( jq12->status_[ 3 ], jd3_job_status_success );
		TS_ASSERT_EQUALS( jq12->status_[ 4 ], jd3_job_status_success );
		TS_ASSERT_EQUALS( jq12->status_[ 5 ], jd3_job_status_success );
		TS_ASSERT_EQUALS( jq12->status_[ 6 ], jd3_job_status_success );

		TS_ASSERT_EQUALS( jq12->summaries_.count( sp1( 1 ) ), 1 );
		TS_ASSERT_EQUALS( jq12->summaries_.count( sp1( 2 ) ), 1 );
		TS_ASSERT_EQUALS( jq12->summaries_.count( sp1( 3 ) ), 1 );
		TS_ASSERT_EQUALS( jq12->summaries_.count( sp1( 4 ) ), 1 );
		TS_ASSERT_EQUALS( jq12->summaries_.count( sp1( 5 ) ), 1 );
		TS_ASSERT_EQUALS( jq12->summaries_.count( sp1( 6 ) ), 1 );
		TS_ASSERT_EQUALS( utility::pointer::dynamic_pointer_cast< EnergyJobSummary > ( jq12->summaries_[ sp1( 1 ) ])->energy(), 1234 );
		TS_ASSERT_EQUALS( utility::pointer::dynamic_pointer_cast< EnergyJobSummary > ( jq12->summaries_[ sp1( 2 ) ])->energy(), 1234 );
		TS_ASSERT_EQUALS( utility::pointer::dynamic_pointer_cast< EnergyJobSummary > ( jq12->summaries_[ sp1( 3 ) ])->energy(), 1234 );
		TS_ASSERT_EQUALS( utility::pointer::dynamic_pointer_cast< EnergyJobSummary > ( jq12->summaries_[ sp1( 4 ) ])->energy(), 1234 );
		TS_ASSERT_EQUALS( utility::pointer::dynamic_pointer_cast< EnergyJobSummary > ( jq12->summaries_[ sp1( 5 ) ])->energy(), 1234 );
		TS_ASSERT_EQUALS( utility::pointer::dynamic_pointer_cast< EnergyJobSummary > ( jq12->summaries_[ sp1( 6 ) ])->energy(), 1234 );

		TS_ASSERT_EQUALS( jq12->results_->count( sp1( 1 ) ), 0 );
		TS_ASSERT_EQUALS( jq12->results_->count( sp1( 2 ) ), 0 );
		TS_ASSERT_EQUALS( jq12->results_->count( sp1( 3 ) ), 0 );
		TS_ASSERT_EQUALS( jq12->results_->count( sp1( 4 ) ), 0 );
		TS_ASSERT_EQUALS( jq12->results_->count( sp1( 5 ) ), 0 );
		TS_ASSERT_EQUALS( jq12->results_->count( sp1( 6 ) ), 0 );


		SimulateMPI::set_mpi_rank( 0 );

		TS_ASSERT( SimulateMPI::incoming_message_queue_is_empty() );
		TS_ASSERT( SimulateMPI::outgoing_message_queue_is_empty() );

		SimulateMPI::print_unprocessed_incoming_messages( std::cerr );
		SimulateMPI::print_unprocessed_outgoing_messages( std::cerr );
		TS_ASSERT( jq12->saved_to_archive_ );
		TS_ASSERT( ! jq12->loaded_from_archive_ );

		TS_ASSERT( utility::file::file_exists( "wpjd_checkpoints_1/sanity.txt" ));
		TS_ASSERT( utility::file::file_exists( "wpjd_checkpoints_1/chkpt_1_0.bin" ));

		// OK!!!!
		// Now we need to restore from the checkpoint file we've created
		// and rerun the job and we should see that the JD picks up where it
		// left off at the time of checkpoint creation!

		{ // scope 2nd half

			core_init_with_additional_options( "-jd3::n_archive_nodes 1 -jd3::do_not_archive_on_node0 -jd3::compress_job_results 0 -jd3::mpi_fraction_outputters 0.4 -jd3::checkpoint_directory wpjd_checkpoints_1 -restore_from_checkpoint" );

			// Create a faux binary file for the archive so that JD0 thinks that the full complement
			// of checkpoint files exists.
			{
				std::ofstream os( "wpjd_checkpoints_1/chkpt_1_1.bin" );
				os << "Hello!" << std::endl;
			}

			// ok, all the nodes at once start requesting jobs from node 0
			SimulateMPI::set_mpi_rank( 3 );
			send_integer_to_node( 0, 3 );
			send_integer_to_node( 0, mpi_work_pool_jd_new_job_request );
			SimulateMPI::set_mpi_rank( 2 );
			send_integer_to_node( 0, 2 );
			send_integer_to_node( 0, mpi_work_pool_jd_new_job_request );
			SimulateMPI::set_mpi_rank( 4 );
			send_integer_to_node( 0, 4 );
			send_integer_to_node( 0, mpi_work_pool_jd_new_job_request );


			SimulateMPI::set_mpi_rank( 3 );
			send_integer_to_node( 0, 3 );
			send_integer_to_node( 0, mpi_work_pool_jd_job_success );
			send_size_to_node( 0, 5 ); // job id
			send_size_to_node( 0, 1 ); // num results

			send_integer_to_node( 0, 3 );
			send_integer_to_node( 0, mpi_work_pool_jd_job_success_and_archival_complete );
			send_size_to_node( 0, 5 ); // job id
			send_integer_to_node( 0, jd3_job_status_success );
			send_string_to_node( 0, serialized_trpcage_job_summaries );

			send_integer_to_node( 0, 3 );
			send_integer_to_node( 0, mpi_work_pool_jd_new_job_request );

			SimulateMPI::set_mpi_rank( 4 );
			send_integer_to_node( 0, 4 );
			send_integer_to_node( 0, mpi_work_pool_jd_job_success );
			send_size_to_node( 0, 6 ); // job id
			send_size_to_node( 0, 1 ); // num results
			// sent to archive instead -- send_string_to_node( 0, serialized_larval_job_and_job_result( 3, 3, 6, trpcage_pose_result ));

			send_integer_to_node( 0, 4 );
			send_integer_to_node( 0, mpi_work_pool_jd_job_success_and_archival_complete );
			send_size_to_node( 0, 6 ); // job id
			send_integer_to_node( 0, jd3_job_status_success );
			send_string_to_node( 0, serialized_trpcage_job_summaries );

			send_integer_to_node( 0, 4 );
			send_integer_to_node( 0, mpi_work_pool_jd_new_job_request );

			SimulateMPI::set_mpi_rank( 2 );

			send_integer_to_node( 0, 2 );
			send_integer_to_node( 0, mpi_work_pool_jd_job_success );
			send_size_to_node( 0, 4 ); // job id
			send_size_to_node( 0, 1 ); // num results
			// sent to archive instead -- send_string_to_node( 0, serialized_larval_job_and_job_result( 3, 1, 4, trpcage_pose_result ));

			send_integer_to_node( 0, 2 );
			send_integer_to_node( 0, mpi_work_pool_jd_job_success_and_archival_complete );
			send_size_to_node( 0, 4 ); // job id
			send_integer_to_node( 0, jd3_job_status_success );
			send_string_to_node( 0, serialized_trpcage_job_summaries );

			send_integer_to_node( 0, 2 );
			send_integer_to_node( 0, mpi_work_pool_jd_new_job_request );

			// OK -- output time.
			// Have node 2 output two of the jobs and node 1 output one of them
			// to test a race condition.
			// Node 0 will hand both node 1 and node 2 a job to output.
			// Node 2 will report it has finished its output first,
			// Node 0 will assign node 2 a second job to output,
			// and then node 1 will say it has finished.
			// Node 0 should not spin down node 1 until after node 2 has said
			// that it has completed output of the 2nd job.

			SimulateMPI::set_mpi_rank(2);
			send_integer_to_node( 0, 2 );
			send_integer_to_node( 0, mpi_work_pool_jd_output_completed );
			send_integer_to_node( 0, 2 );
			send_integer_to_node( 0, mpi_work_pool_jd_new_job_request );


			SimulateMPI::set_mpi_rank( 1 );
			send_integer_to_node( 0, 1 );
			send_integer_to_node( 0, mpi_work_pool_jd_output_completed );

			SimulateMPI::set_mpi_rank(2);
			send_integer_to_node( 0, 2 );
			send_integer_to_node( 0, mpi_work_pool_jd_output_completed );
			send_integer_to_node( 0, 2 );
			send_integer_to_node( 0, mpi_work_pool_jd_new_job_request );

			// OK! Now create another PoolJQ12 -- this one will be replaced when JD0 deserializes
			// the checkpoint data so we'll have to get it back out of the JobDistributor
			// to interrogate it.
			SimulateMPI::set_mpi_rank( 0 );
			PoolJQ12OP jq12b( new PoolJQ12 );
			MPIWorkPoolJobDistributor jd_b;
			jd_b.set_checkpoint_period(10); // long enough that we won't checkpoint again.
			try {
				jd_b.go( jq12b );
			} catch ( utility::excn::Exception & e) {
				TR << "Unexpected exception " << e.msg() << std::endl;
				TS_ASSERT( false /*no exception should be thrown*/ );
			} catch ( ... ) {
				TR << "Uncaught and unexpected exception" << std::endl;
				TS_ASSERT( false /*no exception should be thrown*/ );
			}

			// OK!
			// Now we read the mail we got from node 0

			SimulateMPI::set_mpi_rank( 2 );
			ts_assert_mpi_buffer_has_integer( 0, "2nd half: node 0 tells node 2 that it has another job for it to run", mpi_work_pool_jd_new_job_available );
			std::string ser_larv4 = ts_assert_mpi_buffer_has_string( 0, "2nd half: node 0 sends node 2 the serialized LarvalJob" );
			LarvalJobOP larval_job4 = deserialize_larval_job( ser_larv4 );
			TS_ASSERT_EQUALS( larval_job4->job_index(), 4 );
			TS_ASSERT_EQUALS( larval_job4->nstruct_index(), 1 );
			TS_ASSERT_EQUALS( larval_job4->nstruct_max(), 3 );
			TS_ASSERT_EQUALS( larval_job4->input_job_result_indices().size(), 1 );
			TS_ASSERT_EQUALS( larval_job4->input_job_result_indices()[ 1 ].first, 1 );
			TS_ASSERT_EQUALS( larval_job4->input_job_result_indices()[ 1 ].second, 1 );
			ts_assert_mpi_buffer_has_sizes( 0, "2nd half: node 0 sends node 2 a vector1 of job result indices with one element whose value is 1", utility::vector1< Size >( 1, 1 ) );

			ts_assert_mpi_buffer_has_integers( 0, "2nd half: node 0 tells node 2 to archive its results on node 1", utility::vector1< int >( 1, 1 ) );

			ts_assert_mpi_buffer_has_integer( 0, "2nd half: node 0 tells node 2 that there is a job that it needs to output", mpi_work_pool_jd_output_job_result_on_archive );
			ts_assert_mpi_buffer_has_integer( 0, "2nd half: node 0 tells node 2 that the job result is archived on node 1", 1);
			std::string ser_os5 = ts_assert_mpi_buffer_has_string( 0, "2nd half: node 0 then sends node 2 the serialized job output specification for job result (5,1)" );
			OutputSpecificationOP os5 = deserialized_output_specification( ser_os5 );
			TS_ASSERT_EQUALS( os5->result_id().first, 5 );
			TS_ASSERT_EQUALS( os5->result_id().second, 1 );

			ts_assert_mpi_buffer_has_integer( 0, "2nd half: node 0 tells node 2 that there is another job that it needs to output", mpi_work_pool_jd_output_job_result_on_archive );
			ts_assert_mpi_buffer_has_integer( 0, "2nd half: node 0 tells node 2 that the job result is archived on node 1", 1);
			std::string ser_os6 = ts_assert_mpi_buffer_has_string( 0, "2nd half: node 0 then sends node 2 the serialized job output specification for job result (6,1)" );
			OutputSpecificationOP os6 = deserialized_output_specification( ser_os6 );
			TS_ASSERT_EQUALS( os6->result_id().first, 6 );
			TS_ASSERT_EQUALS( os6->result_id().second, 1 );

			// Finally, spin down
			Size spin_down_node2_message_ind = utility::SimulateMPI::index_of_next_message();
			ts_assert_mpi_buffer_has_integer( 0, "2nd half: node 0 tells node 2 that there are no jobs left to run", mpi_work_pool_jd_spin_down );

			SimulateMPI::set_mpi_rank( 3 );
			ts_assert_mpi_buffer_has_integer( 0, "2nd half: node 0 tells node 3 that it has another job for it to run", mpi_work_pool_jd_new_job_available );
			std::string ser_larv5 = ts_assert_mpi_buffer_has_string( 0, "2nd half: node 0 sends node 3 the serialized LarvalJob" );
			LarvalJobOP larval_job5 = deserialize_larval_job( ser_larv5 );
			TS_ASSERT_EQUALS( larval_job5->job_index(), 5 );
			TS_ASSERT_EQUALS( larval_job5->nstruct_index(), 2 );
			TS_ASSERT_EQUALS( larval_job5->nstruct_max(), 3 );
			TS_ASSERT_EQUALS( larval_job5->input_job_result_indices().size(), 1 );
			TS_ASSERT_EQUALS( larval_job5->input_job_result_indices()[ 1 ].first, 1 );
			TS_ASSERT_EQUALS( larval_job5->input_job_result_indices()[ 1 ].second, 1 );
			ts_assert_mpi_buffer_has_sizes( 0, "2nd half: node 0 sends node 3 a vector1 of job result indices with one element whose value is 1", utility::vector1< Size >( 1, 1 ) );

			ts_assert_mpi_buffer_has_integers( 0, "2nd half: node 0 tells node 3 to archive its results on node 1", utility::vector1< int >( 1, 1 ) );
			// ts_assert_mpi_buffer_has_integer( 0, "2nd half: node 0 tells node 3 that archival completed", mpi_work_pool_jd_archival_completed );

			ts_assert_mpi_buffer_has_integer( 0, "2nd half: node 0 tells node 3 that there are no jobs left to run", mpi_work_pool_jd_spin_down );

			SimulateMPI::set_mpi_rank( 4 );
			ts_assert_mpi_buffer_has_integer( 0, "2nd half: node 0 tells node 4 that it has another job for it to run", mpi_work_pool_jd_new_job_available );
			std::string ser_larv6 = ts_assert_mpi_buffer_has_string( 0, "2nd half: node 0 sends node 4 the serialized LarvalJob" );
			LarvalJobOP larval_job6 = deserialize_larval_job( ser_larv6 );
			TS_ASSERT_EQUALS( larval_job6->job_index(), 6 );
			TS_ASSERT_EQUALS( larval_job6->nstruct_index(), 3 );
			TS_ASSERT_EQUALS( larval_job6->nstruct_max(), 3 );
			TS_ASSERT_EQUALS( larval_job6->input_job_result_indices().size(), 1 );
			TS_ASSERT_EQUALS( larval_job6->input_job_result_indices()[ 1 ].first, 1 );
			TS_ASSERT_EQUALS( larval_job6->input_job_result_indices()[ 1 ].second, 1 );
			ts_assert_mpi_buffer_has_sizes( 0, "2nd half: node 0 sends node 4 a vector1 of job result indices with one element whose value is 1", utility::vector1< Size >( 1, 1 ) );

			ts_assert_mpi_buffer_has_integers( 0, "2nd half: node 0 tells node 4 to archive its results on node 1", utility::vector1< int >( 1, 1 ) );

			ts_assert_mpi_buffer_has_integer( 0, "2nd half: node 0 tells node 4 that there are no jobs left to run", mpi_work_pool_jd_spin_down );

			SimulateMPI::set_mpi_rank( 1 ); // the archive node
			//CHECKPOINT!
			ts_assert_mpi_buffer_has_integer( 0, "2nd half: node 0 tells node 1 that it wants its attention CHPT2", 0 );
			ts_assert_mpi_buffer_has_integer( 0, "2nd half: node 0 tells node 1 that it's time to checkpoint", mpi_work_pool_jd_restore_from_checkpoint );
			ts_assert_mpi_buffer_has_string( 0, "2nd half: node 0 tells node 1 that the file it should write to has the prefix 'wpjd_checkpoints_1/chkpt_1'", "wpjd_checkpoints_1/chkpt_1" );
			ts_assert_mpi_buffer_has_sizes( 0, "2nd half: node 0 sends node 1 a vector1 of job result indices #1 with one element whose value is 1", utility::vector1< Size >( 1, 1 ) );
			ts_assert_mpi_buffer_has_sizes( 0, "2nd half: node 0 sends node 1 a vector1 of job result indices #2 with one element whose value is 1", utility::vector1< Size >( 1, 1 ) );

			ts_assert_mpi_buffer_has_integer( 0, "2nd half: node 0 tells node 1 that it wants its attention C", 0 );
			ts_assert_mpi_buffer_has_integer( 0, "2nd half: node 0 tells node 1 that it needs to output a job A", mpi_work_pool_jd_output_job_result_already_available );
			std::string ser_os4 = ts_assert_mpi_buffer_has_string( 0,
				"2nd half: node 0 sends node 1 the OutputSpecification for job result (4,1)" );
			OutputSpecificationOP os4 = deserialized_output_specification( ser_os4 );
			TS_ASSERT_EQUALS( os4->result_id().first, 4 );
			TS_ASSERT_EQUALS( os4->result_id().second, 1 );

			ts_assert_mpi_buffer_has_integer( 0, "2nd half: node 0 tells node 1 that it wants its attention D", 0 );
			Size spin_down_node1_message_ind = SimulateMPI::index_of_next_message();
			ts_assert_mpi_buffer_has_integer( 0, "2nd half: node 0 tells node 1 that there are no jobs left to run", mpi_work_pool_jd_spin_down );

			// CRITICAL! The job distributor must not spin down node 1 until
			// node 2 has said that it has completed outputting everything
			// or you get a node 2 waiting forever for node 1 to reply to
			// a request for the result that node 2 is tasked with outputting
			// (which node 1 won't do because it has left its listener loop)
			// std::cout << "Spin down messages: spin_down_node2_message_ind " << spin_down_node2_message_ind << " spin_down_node1_message_ind " << spin_down_node1_message_ind << std::endl;
			TS_ASSERT( spin_down_node2_message_ind < spin_down_node1_message_ind );


			SimulateMPI::set_mpi_rank( 0 );

			TS_ASSERT( SimulateMPI::incoming_message_queue_is_empty() );
			TS_ASSERT( SimulateMPI::outgoing_message_queue_is_empty() );

			SimulateMPI::print_unprocessed_incoming_messages( std::cerr );
			SimulateMPI::print_unprocessed_outgoing_messages( std::cerr );

			//std::cout << "Finished Checkpointing Unit Test" << std::endl;

			jq12b = utility::pointer::dynamic_pointer_cast< PoolJQ12 > (jd_b.job_queen());
			TS_ASSERT( jq12b );
			if ( ! jq12b ) return;

			TS_ASSERT_EQUALS( jq12b->status_.count( 1 ), 1 );
			TS_ASSERT_EQUALS( jq12b->status_.count( 2 ), 1 );
			TS_ASSERT_EQUALS( jq12b->status_.count( 3 ), 1 );
			TS_ASSERT_EQUALS( jq12b->status_.count( 4 ), 1 );
			TS_ASSERT_EQUALS( jq12b->status_.count( 5 ), 1 );
			TS_ASSERT_EQUALS( jq12b->status_.count( 6 ), 1 );
			TS_ASSERT_EQUALS( jq12b->status_[ 1 ], jd3_job_status_success );
			TS_ASSERT_EQUALS( jq12b->status_[ 2 ], jd3_job_status_success );
			TS_ASSERT_EQUALS( jq12b->status_[ 3 ], jd3_job_status_success );
			TS_ASSERT_EQUALS( jq12b->status_[ 4 ], jd3_job_status_success );
			TS_ASSERT_EQUALS( jq12b->status_[ 5 ], jd3_job_status_success );
			TS_ASSERT_EQUALS( jq12b->status_[ 6 ], jd3_job_status_success );

			TS_ASSERT_EQUALS( jq12b->summaries_.count( sp1( 1 ) ), 1 );
			TS_ASSERT_EQUALS( jq12b->summaries_.count( sp1( 2 ) ), 1 );
			TS_ASSERT_EQUALS( jq12b->summaries_.count( sp1( 3 ) ), 1 );
			TS_ASSERT_EQUALS( jq12b->summaries_.count( sp1( 4 ) ), 1 );
			TS_ASSERT_EQUALS( jq12b->summaries_.count( sp1( 5 ) ), 1 );
			TS_ASSERT_EQUALS( jq12b->summaries_.count( sp1( 6 ) ), 1 );
			TS_ASSERT_EQUALS( utility::pointer::dynamic_pointer_cast< EnergyJobSummary > ( jq12b->summaries_[ sp1( 1 ) ])->energy(), 1234 );
			TS_ASSERT_EQUALS( utility::pointer::dynamic_pointer_cast< EnergyJobSummary > ( jq12b->summaries_[ sp1( 2 ) ])->energy(), 1234 );
			TS_ASSERT_EQUALS( utility::pointer::dynamic_pointer_cast< EnergyJobSummary > ( jq12b->summaries_[ sp1( 3 ) ])->energy(), 1234 );
			TS_ASSERT_EQUALS( utility::pointer::dynamic_pointer_cast< EnergyJobSummary > ( jq12b->summaries_[ sp1( 4 ) ])->energy(), 1234 );
			TS_ASSERT_EQUALS( utility::pointer::dynamic_pointer_cast< EnergyJobSummary > ( jq12b->summaries_[ sp1( 5 ) ])->energy(), 1234 );
			TS_ASSERT_EQUALS( utility::pointer::dynamic_pointer_cast< EnergyJobSummary > ( jq12b->summaries_[ sp1( 6 ) ])->energy(), 1234 );

			TS_ASSERT_EQUALS( jq12b->results_->count( sp1( 1 ) ), 0 );
			TS_ASSERT_EQUALS( jq12b->results_->count( sp1( 2 ) ), 0 );
			TS_ASSERT_EQUALS( jq12b->results_->count( sp1( 3 ) ), 0 );
			TS_ASSERT_EQUALS( jq12b->results_->count( sp1( 4 ) ), 0 );
			TS_ASSERT_EQUALS( jq12b->results_->count( sp1( 5 ) ), 0 );
			TS_ASSERT_EQUALS( jq12b->results_->count( sp1( 6 ) ), 0 );

			TS_ASSERT( jq12b->saved_to_archive_ );
			TS_ASSERT( jq12b->loaded_from_archive_ );

		} // 2nd half scope


		// CLEANUP!
		utility::vector1< std::string > fnames;
		utility::file::list_dir( "wpjd_checkpoints_1", fnames );
		for ( auto fname: fnames ) {
			auto full_name = "wpjd_checkpoints_1/" + fname;
			remove( full_name.c_str() );
		}
		remove( "wpjd_checkpoints_1" );
#endif

	}


	void test_jd3_workpool_jd_checkpoint_archive_node() {
		TS_ASSERT( true );

#ifdef SERIALIZATION
		core_init_with_additional_options( "-jd3::n_archive_nodes 1 -jd3::do_not_archive_on_node0 -jd3::compress_job_results 0 -jd3::checkpoint_directory wpjd_checkpoints_2 -jd3::keep_all_checkpoints" );

		utility::file::create_directory( "wpjd_checkpoints_2" );

		EnergyJobSummaryOP trpcage_job_summary( new EnergyJobSummary );
		trpcage_job_summary->energy( 1234 );
		utility::vector1< JobSummaryOP > summaries( 1, trpcage_job_summary );
		std::string serialized_trpcage_job_summaries = serialized_job_summaries( summaries );

		PoseJobResultOP trpcage_pose_result( new PoseJobResult );
		trpcage_pose_result->pose(  create_trpcage_ideal_poseop() );

		SimulateMPI::initialize_simulation( 5 );

		SimulateMPI::set_mpi_rank( 3 );
		send_integer_to_node( 1, 3 ); // node 3 says "I have a message for you, archive"
		send_integer_to_node( 1, mpi_work_pool_jd_archive_job_result ); // "please archive this job result"
		send_size_to_node( 1, 2 ); // job_id
		send_size_to_node( 1, 1 ); // result index
		send_string_to_node( 1, serialized_larval_job_and_job_result( 3, 2, 2, trpcage_pose_result ));

		SimulateMPI::set_mpi_rank( 2 );
		send_integer_to_node( 1, 2 ); // node 2 says "I have a message for you, archive"
		send_integer_to_node( 1, mpi_work_pool_jd_archive_job_result ); // "please archive this job result"
		send_size_to_node( 1, 1 ); // job_id
		send_size_to_node( 1, 1 ); // result index
		send_string_to_node( 1, serialized_larval_job_and_job_result( 3, 1, 1, trpcage_pose_result ));

		SimulateMPI::set_mpi_rank( 4 );
		send_integer_to_node( 1, 4 ); // node 4 says "I have a message for you, archive"
		send_integer_to_node( 1, mpi_work_pool_jd_archive_job_result ); // "please archive this job result"
		send_size_to_node( 1, 3 ); // job_id
		send_size_to_node( 1, 1 ); // result index
		send_string_to_node( 1, serialized_larval_job_and_job_result( 3, 3, 3, trpcage_pose_result ));


		SimulateMPI::set_mpi_rank( 0 );
		// master node says "discard job results 2 and 3
		send_integer_to_node( 1, 0 ); // node 0 says to archive node: "hey buddy, we need to talk"
		send_integer_to_node( 1, mpi_work_pool_jd_discard_job_result ); // "I need you to discard a job result"
		send_size_to_node( 1, 2 ); // discard job 2
		send_size_to_node( 1, 1 ); // discard job 2 result index 1

		send_integer_to_node( 1, 0 ); // node 0 says to archive node: "hey buddy, we need to talk"
		send_integer_to_node( 1, mpi_work_pool_jd_discard_job_result ); // "I need you to discard a job result"
		send_size_to_node( 1, 3 ); // discard job 3
		send_size_to_node( 1, 1 ); // discard job 3 result index 1

		send_integer_to_node( 1, 0 );
		send_integer_to_node( 1, mpi_work_pool_jd_begin_checkpointing );
		send_string_to_node( 1, "wpjd_checkpoints_2/chkpt_1" );

		SimulateMPI::set_mpi_rank( 2 );

		// now retrieve the job result for job 1
		send_integer_to_node( 1, 2 );
		send_integer_to_node( 1, mpi_work_pool_jd_retrieve_job_result );
		send_size_to_node( 1, 1 ); // retrieve the result from job #1
		send_size_to_node( 1, 1 ); // retrieve the result from job #1 result index #1

		SimulateMPI::set_mpi_rank( 3 );

		// now retrieve the job result for job 1
		send_integer_to_node( 1, 3 );
		send_integer_to_node( 1, mpi_work_pool_jd_retrieve_job_result );
		send_size_to_node( 1, 1 ); // retrieve the result from job #1
		send_size_to_node( 1, 1 ); // retrieve the result from job #1 result index #1

		SimulateMPI::set_mpi_rank( 4 );

		// now retrieve the job result for job 1
		send_integer_to_node( 1, 4 );
		send_integer_to_node( 1, mpi_work_pool_jd_retrieve_job_result );
		send_size_to_node( 1, 1 ); // retrieve the result from job #1
		send_size_to_node( 1, 1 ); // retrieve the result from job #1 result index #1

		SimulateMPI::set_mpi_rank( 2 );
		send_integer_to_node( 1, 2 ); // node 2 says "I have a message for you, archive"
		send_integer_to_node( 1, mpi_work_pool_jd_archive_job_result ); // "please archive this job result"
		send_size_to_node( 1, 4 ); // job_id
		send_size_to_node( 1, 1 ); // result index #1
		send_string_to_node( 1, serialized_larval_job_and_job_result( 3, 1, 4, trpcage_pose_result ));

		SimulateMPI::set_mpi_rank( 3 );
		send_integer_to_node( 1, 3 ); // node 3 says "I have a message for you, archive"
		send_integer_to_node( 1, mpi_work_pool_jd_archive_job_result ); // "please archive this job result"
		send_size_to_node( 1, 5 ); // job_id
		send_size_to_node( 1, 1 ); // result index #1
		send_string_to_node( 1, serialized_larval_job_and_job_result( 3, 2, 5, trpcage_pose_result ));

		SimulateMPI::set_mpi_rank( 4 );
		send_integer_to_node( 1, 4 ); // node 4 says "I have a message for you, archive"
		send_integer_to_node( 1, mpi_work_pool_jd_archive_job_result ); // "please archive this job result"
		send_size_to_node( 1, 6 ); // job_id
		send_size_to_node( 1, 1 ); // result index #1
		send_string_to_node( 1, serialized_larval_job_and_job_result( 3, 3, 6, trpcage_pose_result ));

		// Finally, node 0 asks the archive for job result

		SimulateMPI::set_mpi_rank( 0 );

		send_integer_to_node( 1, 0 ); // node 0 says "hey, I need to talk to you"
		send_integer_to_node( 1, mpi_work_pool_jd_output_job_result_already_available );
		send_string_to_node( 1, serialized_output_specification( JobResultID( 4, 1 ), joi(4) ) );

		send_integer_to_node( 1, 0 ); // node 0 says "hey, I need to talk to you"
		send_integer_to_node( 1, mpi_work_pool_jd_output_job_result_already_available );
		send_string_to_node( 1, serialized_output_specification( JobResultID( 5, 1 ), joi(5) ));

		send_integer_to_node( 1, 0 ); // node 0 says "hey, I need to talk to you"
		send_integer_to_node( 1, mpi_work_pool_jd_output_job_result_already_available ); // I need a job result
		//send_size_to_node( 1, 6 ); // job id
		//send_size_to_node( 1, 1 ); // result index
		send_string_to_node( 1, serialized_output_specification( JobResultID( 6, 1 ), joi(6) ));

		send_integer_to_node( 1, 0 ); // node 0 says "hey, I need to talk to you"
		send_integer_to_node( 1, mpi_work_pool_jd_spin_down ); // time to shut down

		//OK! Now create a PoolJQ3 and let 'er rip
		SimulateMPI::set_mpi_rank( 1 );
		PoolJQ3OP jq3( new PoolJQ3 );
		MPIWorkPoolJobDistributor jd;
		try {
			jd.go( jq3 );
		} catch ( ... ) {
			TS_ASSERT( false /*no exception should be thrown*/ );
		}

		// At the end, the jd should have outputs now from jobs 4-6
		TS_ASSERT_EQUALS( jq3->results_->count( sp1( 1 ) ), 0 );
		TS_ASSERT_EQUALS( jq3->results_->count( sp1( 2 ) ), 0 );
		TS_ASSERT_EQUALS( jq3->results_->count( sp1( 3 ) ), 0 );
		TS_ASSERT_EQUALS( jq3->results_->count( sp1( 4 ) ), 1 );
		TS_ASSERT_EQUALS( jq3->results_->count( sp1( 5 ) ), 1 );
		TS_ASSERT_EQUALS( jq3->results_->count( sp1( 6 ) ), 1 );
		TS_ASSERT_EQUALS( utility::pointer::dynamic_pointer_cast< PoseJobResult > ( (*jq3->results_)[ sp1( 4 ) ])->pose()->total_residue(), 20 );
		TS_ASSERT_EQUALS( utility::pointer::dynamic_pointer_cast< PoseJobResult > ( (*jq3->results_)[ sp1( 5 ) ])->pose()->total_residue(), 20 );
		TS_ASSERT_EQUALS( utility::pointer::dynamic_pointer_cast< PoseJobResult > ( (*jq3->results_)[ sp1( 6 ) ])->pose()->total_residue(), 20 );


		// OK!
		// Now we read the mail we got from node 1

		SimulateMPI::set_mpi_rank( 2 );

		ts_assert_mpi_buffer_has_integer( 1, "node 1 tells node 2 that archival completed (for job 2)", mpi_work_pool_jd_archival_completed );

		ts_assert_mpi_buffer_has_integer( 1, "node 1 tells node 2 that it has the job result it was expecting", mpi_work_pool_jd_job_result_retrieved );
		std::string ser_job_res1a = ts_assert_mpi_buffer_has_string( 1, "node 1 sends the serialized larval-job and job-result from job 1" );
		MPIWorkPoolJobDistributor::LarvalJobAndResult job_res1a = deserialized_larval_job_and_job_result( ser_job_res1a );
		TS_ASSERT( job_res1a.first );
		TS_ASSERT( job_res1a.second );
		TS_ASSERT_EQUALS( job_res1a.first->job_index(), 1 );
		TS_ASSERT_EQUALS( job_res1a.first->nstruct_index(), 1 );
		TS_ASSERT_EQUALS( job_res1a.first->nstruct_max(), 3 );
		TS_ASSERT_EQUALS( utility::pointer::dynamic_pointer_cast< PoseJobResult > ( job_res1a.second )->pose()->total_residue(), 20 );

		ts_assert_mpi_buffer_has_integer( 1, "node 1 tells node 2 that archival completed (for job 4)", mpi_work_pool_jd_archival_completed );

		SimulateMPI::set_mpi_rank( 3 );
		ts_assert_mpi_buffer_has_integer( 1, "node 1 tells node 3 that archival completed (for job 1)", mpi_work_pool_jd_archival_completed );

		ts_assert_mpi_buffer_has_integer( 1, "node 1 tells node 3 that it has the job result it was expecting", mpi_work_pool_jd_job_result_retrieved );
		std::string ser_job_res1b = ts_assert_mpi_buffer_has_string( 1, "node 1 sends the serialized larval-job and job-result from job 1" );
		MPIWorkPoolJobDistributor::LarvalJobAndResult job_res1b = deserialized_larval_job_and_job_result( ser_job_res1b );
		TS_ASSERT( job_res1b.first );
		TS_ASSERT( job_res1b.second );
		TS_ASSERT_EQUALS( job_res1b.first->job_index(), 1 );
		TS_ASSERT_EQUALS( job_res1b.first->nstruct_index(), 1 );
		TS_ASSERT_EQUALS( job_res1b.first->nstruct_max(), 3 );
		TS_ASSERT_EQUALS( utility::pointer::dynamic_pointer_cast< PoseJobResult > ( job_res1b.second )->pose()->total_residue(), 20 );

		ts_assert_mpi_buffer_has_integer( 1, "node 1 tells node 3 that archival completed (for job 5)", mpi_work_pool_jd_archival_completed );

		SimulateMPI::set_mpi_rank( 4 );
		ts_assert_mpi_buffer_has_integer( 1, "node 1 tells node 4 that archival completed (for job 3)", mpi_work_pool_jd_archival_completed );

		ts_assert_mpi_buffer_has_integer( 1, "node 1 tells node 4 that it has the job result it was expecting", mpi_work_pool_jd_job_result_retrieved );
		std::string ser_job_res1c = ts_assert_mpi_buffer_has_string( 1, "node 1 sends the serialized larval-job and job-result from job 1" );
		MPIWorkPoolJobDistributor::LarvalJobAndResult job_res1c = deserialized_larval_job_and_job_result( ser_job_res1b );
		TS_ASSERT( job_res1c.first );
		TS_ASSERT( job_res1c.second );
		TS_ASSERT_EQUALS( job_res1c.first->job_index(), 1 );
		TS_ASSERT_EQUALS( job_res1c.first->nstruct_index(), 1 );
		TS_ASSERT_EQUALS( job_res1c.first->nstruct_max(), 3 );
		TS_ASSERT_EQUALS( utility::pointer::dynamic_pointer_cast< PoseJobResult > ( job_res1c.second )->pose()->total_residue(), 20 );

		ts_assert_mpi_buffer_has_integer( 1, "node 1 tells node 4 that archival completed", mpi_work_pool_jd_archival_completed );

		SimulateMPI::set_mpi_rank( 0 ); // the master node

		ts_assert_mpi_buffer_has_integer( 1, "node 1 tells node 0 that it succeeded at checkpointing itself", mpi_work_pool_jd_checkpointing_complete );

		ts_assert_mpi_buffer_has_integer( 1, "node 1 says that it has a message", 1 );
		ts_assert_mpi_buffer_has_integer( 1, "node 1 says that it has output job result 4", mpi_work_pool_jd_output_completed );

		ts_assert_mpi_buffer_has_integer( 1, "node 1 says that it has a message, b", 1 );
		ts_assert_mpi_buffer_has_integer( 1, "node 1 says that it has output job result 5", mpi_work_pool_jd_output_completed );

		ts_assert_mpi_buffer_has_integer( 1, "node 1 says that it has a message, c", 1 );
		ts_assert_mpi_buffer_has_integer( 1, "node 1 says that it has output job result 6", mpi_work_pool_jd_output_completed );

		SimulateMPI::set_mpi_rank( 1 );

		TS_ASSERT( SimulateMPI::incoming_message_queue_is_empty() );
		TS_ASSERT( SimulateMPI::outgoing_message_queue_is_empty() );

		SimulateMPI::print_unprocessed_incoming_messages( std::cerr );
		SimulateMPI::print_unprocessed_outgoing_messages( std::cerr );

		// SECOND HALF!
		// After the checkpoint was generated in the first half, restore from that
		// checkpoint. All the behavior should then be the same for the second half.
		{ // scope 2nd half
			core_init_with_additional_options( "-jd3::n_archive_nodes 1 -jd3::do_not_archive_on_node0 -jd3::compress_job_results 0 -jd3::checkpoint_directory wpjd_checkpoints_2 -jd3::restore_from_checkpoint" );

			SimulateMPI::set_mpi_rank( 0 );

			send_integer_to_node( 1, 0 );
			send_integer_to_node( 1, mpi_work_pool_jd_restore_from_checkpoint );
			send_string_to_node( 1, "wpjd_checkpoints_2/chkpt_1" );
			send_sizes_to_node( 1, utility::vector1< Size >( 1, 1) );
			send_sizes_to_node( 1, utility::vector1< Size >( 1, 1) );

			SimulateMPI::set_mpi_rank( 2 );

			// now retrieve the job result for job 1
			send_integer_to_node( 1, 2 );
			send_integer_to_node( 1, mpi_work_pool_jd_retrieve_job_result );
			send_size_to_node( 1, 1 ); // retrieve the result from job #1
			send_size_to_node( 1, 1 ); // retrieve the result from job #1 result index #1

			SimulateMPI::set_mpi_rank( 3 );

			// now retrieve the job result for job 1
			send_integer_to_node( 1, 3 );
			send_integer_to_node( 1, mpi_work_pool_jd_retrieve_job_result );
			send_size_to_node( 1, 1 ); // retrieve the result from job #1
			send_size_to_node( 1, 1 ); // retrieve the result from job #1 result index #1

			SimulateMPI::set_mpi_rank( 4 );

			// now retrieve the job result for job 1
			send_integer_to_node( 1, 4 );
			send_integer_to_node( 1, mpi_work_pool_jd_retrieve_job_result );
			send_size_to_node( 1, 1 ); // retrieve the result from job #1
			send_size_to_node( 1, 1 ); // retrieve the result from job #1 result index #1

			SimulateMPI::set_mpi_rank( 2 );
			send_integer_to_node( 1, 2 ); // node 2 says "I have a message for you, archive"
			send_integer_to_node( 1, mpi_work_pool_jd_archive_job_result ); // "please archive this job result"
			send_size_to_node( 1, 4 ); // job_id
			send_size_to_node( 1, 1 ); // result index #1
			send_string_to_node( 1, serialized_larval_job_and_job_result( 3, 1, 4, trpcage_pose_result ));

			SimulateMPI::set_mpi_rank( 3 );
			send_integer_to_node( 1, 3 ); // node 3 says "I have a message for you, archive"
			send_integer_to_node( 1, mpi_work_pool_jd_archive_job_result ); // "please archive this job result"
			send_size_to_node( 1, 5 ); // job_id
			send_size_to_node( 1, 1 ); // result index #1
			send_string_to_node( 1, serialized_larval_job_and_job_result( 3, 2, 5, trpcage_pose_result ));

			SimulateMPI::set_mpi_rank( 4 );
			send_integer_to_node( 1, 4 ); // node 4 says "I have a message for you, archive"
			send_integer_to_node( 1, mpi_work_pool_jd_archive_job_result ); // "please archive this job result"
			send_size_to_node( 1, 6 ); // job_id
			send_size_to_node( 1, 1 ); // result index #1
			send_string_to_node( 1, serialized_larval_job_and_job_result( 3, 3, 6, trpcage_pose_result ));

			// Finally, node 0 asks the archive for job result

			SimulateMPI::set_mpi_rank( 0 );

			send_integer_to_node( 1, 0 ); // node 0 says "hey, I need to talk to you"
			send_integer_to_node( 1, mpi_work_pool_jd_output_job_result_already_available );
			send_string_to_node( 1, serialized_output_specification( JobResultID( 4, 1 ), joi(4) ) );

			send_integer_to_node( 1, 0 ); // node 0 says "hey, I need to talk to you"
			send_integer_to_node( 1, mpi_work_pool_jd_output_job_result_already_available );
			send_string_to_node( 1, serialized_output_specification( JobResultID( 5, 1 ), joi(5) ));

			send_integer_to_node( 1, 0 ); // node 0 says "hey, I need to talk to you"
			send_integer_to_node( 1, mpi_work_pool_jd_output_job_result_already_available ); // I need a job result
			//send_size_to_node( 1, 6 ); // job id
			//send_size_to_node( 1, 1 ); // result index
			send_string_to_node( 1, serialized_output_specification( JobResultID( 6, 1 ), joi(6) ));

			send_integer_to_node( 1, 0 ); // node 0 says "hey, I need to talk to you"
			send_integer_to_node( 1, mpi_work_pool_jd_spin_down ); // time to shut down

			//OK! Now create a PoolJQ3 and let 'er rip
			SimulateMPI::set_mpi_rank( 1 );
			PoolJQ3OP jq3( new PoolJQ3 );
			MPIWorkPoolJobDistributor jd;
			try {
				jd.go( jq3 );
			} catch ( ... ) {
				TS_ASSERT( false /*no exception should be thrown*/ );
			}

			// At the end, the jd should have outputs now from jobs 4-6
			TS_ASSERT_EQUALS( jq3->results_->count( sp1( 1 ) ), 0 );
			TS_ASSERT_EQUALS( jq3->results_->count( sp1( 2 ) ), 0 );
			TS_ASSERT_EQUALS( jq3->results_->count( sp1( 3 ) ), 0 );
			TS_ASSERT_EQUALS( jq3->results_->count( sp1( 4 ) ), 1 );
			TS_ASSERT_EQUALS( jq3->results_->count( sp1( 5 ) ), 1 );
			TS_ASSERT_EQUALS( jq3->results_->count( sp1( 6 ) ), 1 );
			TS_ASSERT_EQUALS( utility::pointer::dynamic_pointer_cast< PoseJobResult > ( (*jq3->results_)[ sp1( 4 ) ])->pose()->total_residue(), 20 );
			TS_ASSERT_EQUALS( utility::pointer::dynamic_pointer_cast< PoseJobResult > ( (*jq3->results_)[ sp1( 5 ) ])->pose()->total_residue(), 20 );
			TS_ASSERT_EQUALS( utility::pointer::dynamic_pointer_cast< PoseJobResult > ( (*jq3->results_)[ sp1( 6 ) ])->pose()->total_residue(), 20 );


			// OK!
			// Now we read the mail we got from node 1

			SimulateMPI::set_mpi_rank( 2 );

			ts_assert_mpi_buffer_has_integer( 1, "2nd half: node 1 tells node 2 that it has the job result it was expecting", mpi_work_pool_jd_job_result_retrieved );
			std::string ser_job_res1a = ts_assert_mpi_buffer_has_string( 1, "2nd half: node 1 sends the serialized larval-job and job-result from job 1" );
			MPIWorkPoolJobDistributor::LarvalJobAndResult job_res1a = deserialized_larval_job_and_job_result( ser_job_res1a );
			TS_ASSERT( job_res1a.first );
			TS_ASSERT( job_res1a.second );
			TS_ASSERT_EQUALS( job_res1a.first->job_index(), 1 );
			TS_ASSERT_EQUALS( job_res1a.first->nstruct_index(), 1 );
			TS_ASSERT_EQUALS( job_res1a.first->nstruct_max(), 3 );
			TS_ASSERT_EQUALS( utility::pointer::dynamic_pointer_cast< PoseJobResult > ( job_res1a.second )->pose()->total_residue(), 20 );

			ts_assert_mpi_buffer_has_integer( 1, "2nd half: node 1 tells node 2 that archival completed (for job 4)", mpi_work_pool_jd_archival_completed );

			SimulateMPI::set_mpi_rank( 3 );

			ts_assert_mpi_buffer_has_integer( 1, "2nd half: node 1 tells node 3 that it has the job result it was expecting", mpi_work_pool_jd_job_result_retrieved );
			std::string ser_job_res1b = ts_assert_mpi_buffer_has_string( 1, "2nd half: node 1 sends the serialized larval-job and job-result from job 1" );
			MPIWorkPoolJobDistributor::LarvalJobAndResult job_res1b = deserialized_larval_job_and_job_result( ser_job_res1b );
			TS_ASSERT( job_res1b.first );
			TS_ASSERT( job_res1b.second );
			TS_ASSERT_EQUALS( job_res1b.first->job_index(), 1 );
			TS_ASSERT_EQUALS( job_res1b.first->nstruct_index(), 1 );
			TS_ASSERT_EQUALS( job_res1b.first->nstruct_max(), 3 );
			TS_ASSERT_EQUALS( utility::pointer::dynamic_pointer_cast< PoseJobResult > ( job_res1b.second )->pose()->total_residue(), 20 );

			ts_assert_mpi_buffer_has_integer( 1, "2nd half: node 1 tells node 3 that archival completed (for job 5)", mpi_work_pool_jd_archival_completed );

			SimulateMPI::set_mpi_rank( 4 );

			ts_assert_mpi_buffer_has_integer( 1, "2nd half: node 1 tells node 4 that it has the job result it was expecting", mpi_work_pool_jd_job_result_retrieved );
			std::string ser_job_res1c = ts_assert_mpi_buffer_has_string( 1, "2nd half: node 1 sends the serialized larval-job and job-result from job 1" );
			MPIWorkPoolJobDistributor::LarvalJobAndResult job_res1c = deserialized_larval_job_and_job_result( ser_job_res1b );
			TS_ASSERT( job_res1c.first );
			TS_ASSERT( job_res1c.second );
			TS_ASSERT_EQUALS( job_res1c.first->job_index(), 1 );
			TS_ASSERT_EQUALS( job_res1c.first->nstruct_index(), 1 );
			TS_ASSERT_EQUALS( job_res1c.first->nstruct_max(), 3 );
			TS_ASSERT_EQUALS( utility::pointer::dynamic_pointer_cast< PoseJobResult > ( job_res1c.second )->pose()->total_residue(), 20 );

			ts_assert_mpi_buffer_has_integer( 1, "2nd half: node 1 tells node 4 that archival completed", mpi_work_pool_jd_archival_completed );

			SimulateMPI::set_mpi_rank( 0 ); // the master node

			ts_assert_mpi_buffer_has_integer( 1, "2nd half: node 1 says that it has a message", 1 );
			ts_assert_mpi_buffer_has_integer( 1, "2nd half: node 1 says that it has output job result 4", mpi_work_pool_jd_output_completed );

			ts_assert_mpi_buffer_has_integer( 1, "2nd half: node 1 says that it has a message, b", 1 );
			ts_assert_mpi_buffer_has_integer( 1, "2nd half: node 1 says that it has output job result 5", mpi_work_pool_jd_output_completed );

			ts_assert_mpi_buffer_has_integer( 1, "2nd half: node 1 says that it has a message, c", 1 );
			ts_assert_mpi_buffer_has_integer( 1, "2nd half: node 1 says that it has output job result 6", mpi_work_pool_jd_output_completed );

			SimulateMPI::set_mpi_rank( 1 );

			TS_ASSERT( SimulateMPI::incoming_message_queue_is_empty() );
			TS_ASSERT( SimulateMPI::outgoing_message_queue_is_empty() );

			SimulateMPI::print_unprocessed_incoming_messages( std::cerr );
			SimulateMPI::print_unprocessed_outgoing_messages( std::cerr );

			TS_ASSERT( utility::file::file_exists( "wpjd_checkpoints_2/chkpt_1_1.bin" ));

		} // end scope 2nd half

		// CLEANUP!
		utility::vector1< std::string > fnames;
		utility::file::list_dir( "wpjd_checkpoints_2", fnames );
		for ( auto fname: fnames ) {
			auto full_name = "wpjd_checkpoints_2/" + fname;
			remove( full_name.c_str() );
		}
		rmdir( "wpjd_checkpoints_2" );

#endif
	}



};
