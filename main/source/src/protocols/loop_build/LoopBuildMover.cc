// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

#include <protocols/loop_build/LoopBuildMover.hh>

#include <basic/Tracer.hh>
#include <core/scoring/ScoreFunction.hh>
#include <core/pose/util.hh>
#include <core/pose/extra_pose_info_util.hh>


#include <utility/exit.hh>

#include <core/scoring/electron_density/util.hh>

#include <protocols/evaluation/PoseEvaluator.hh>
#include <protocols/evaluation/util.hh>
#include <protocols/evaluation/EvaluatorFactory.hh>

#include <protocols/jd2/util.hh>
#include <protocols/loops/loops_main.hh>

#include <basic/options/keys/out.OptionKeys.gen.hh>
#include <basic/options/keys/loops.OptionKeys.gen.hh>
#include <basic/options/keys/in.OptionKeys.gen.hh>
#include <basic/options/keys/edensity.OptionKeys.gen.hh>

//#include <basic/resource_manager/ResourceManager.hh>
//#include <basic/resource_manager/util.hh>

#include <protocols/comparative_modeling/LoopRelaxMover.hh>

#include <core/import_pose/import_pose.hh>
#include <protocols/electron_density/SetupForDensityScoringMover.hh>
#include <utility/vector1.hh>
#include <basic/options/option.hh>

#include <protocols/loops/util.hh>
#include <protocols/loops/Loops.hh>
#include <protocols/loops/LoopsFileIO.hh>

#include <protocols/symmetry/SetupForSymmetryMover.hh>
#include <basic/options/keys/symmetry.OptionKeys.gen.hh>

#include <protocols/simple_filters/RmsdEvaluator.hh>

#include <numeric/random/random.hh>

#ifdef WIN32
#include <ctime>
#endif

namespace protocols {
namespace loop_build {


LoopBuildMover::LoopBuildMover(protocols::comparative_modeling::LoopRelaxMover loop_relax_mover) :
	loop_relax_mover_( loop_relax_mover)
{}


void LoopBuildMover::apply(core::pose::Pose & pose){

	setup_loop_definition();
	bool psipred_ss2_ok = loops::set_secstruct_from_psipred_ss2( pose );
	if ( !psipred_ss2_ok ) {
		std::string dssp_name( basic::options::option[ basic::options::OptionKeys::in::file::dssp ]().name() );
		bool dssp_ok = loops::set_secstruct_from_dssp(pose, dssp_name);
		if ( !dssp_ok ) {
			core::pose::set_ss_from_phipsi( pose );
		}
	}

	// symmetrize start pose & loopfile
	if ( basic::options::option[ basic::options::OptionKeys::symmetry::symmetry_definition ].user() )  {
		protocols::symmetry::SetupForSymmetryMover pre_mover;
		pre_mover.apply( pose );
	}

	// bit of a hack for looprelax-into-density
	// set pose for density scoring if a map was input
	// (potentially) dock map into density -- do this here so we only need to
	// dock the pose once
	if ( basic::options::option[ basic::options::OptionKeys::edensity::mapfile ].user() ) {
		protocols::electron_density::SetupForDensityScoringMover pre_mover;
		pre_mover.apply( pose );
	}

	core::pose::Pose native_pose;
	if ( basic::options::option[ basic::options::OptionKeys::in::file::native ].user() ) {
		core::import_pose::pose_from_file(
			native_pose, basic::options::option[ basic::options::OptionKeys::in::file::native ](),
			core::import_pose::PDB_file
		);
		core::pose::set_ss_from_phipsi( native_pose );
	} else {
		native_pose = pose;
	}

	evaluation::MetaPoseEvaluatorOP evaluator( new evaluation::MetaPoseEvaluator );
	evaluation::EvaluatorFactory::get_instance()->add_all_evaluators(*evaluator);
	evaluator->add_evaluation(
		utility::pointer::make_shared< simple_filters::SelectRmsdEvaluator >( native_pose, "_native" )
	);

	static basic::Tracer TR( "protocols.loop_build.LoopBuildMover" );

	TR << "Annotated sequence of pose: "
		<< pose.annotated_sequence(true) << std::endl;

	std::string const refine       ( basic::options::option[ basic::options::OptionKeys::loops::refine ]() );
	std::string const remodel      ( basic::options::option[ basic::options::OptionKeys::loops::remodel ]() );
	bool const keep_time      ( basic::options::option[ basic::options::OptionKeys::loops::timer ]() );
	std::string curr_job_tag = protocols::jd2::current_input_tag();
	core::Size curr_nstruct = protocols::jd2::current_nstruct_index();
	clock_t starttime = clock();

	loop_relax_mover_.set_current_tag(curr_job_tag);

	// add density wts from cmd line to looprelax scorefunctions
	if ( basic::options::option[ basic::options::OptionKeys::edensity::mapfile ].user() ) {
		core::scoring::ScoreFunctionOP lr_cen_scorefxn = loops::get_cen_scorefxn();
		core::scoring::electron_density::add_dens_scores_from_cmdline_to_scorefxn( *lr_cen_scorefxn );
		core::scoring::ScoreFunctionOP lr_fa_scorefxn = loops::get_fa_scorefxn();
		core::scoring::electron_density::add_dens_scores_from_cmdline_to_scorefxn( *lr_fa_scorefxn );
		loop_relax_mover_.scorefxns( lr_cen_scorefxn, lr_fa_scorefxn );
	}

	loop_relax_mover_.apply( pose );

	//////////////////////////////////////////////////////////////////////////////
	////
	////   Filter
	////
	core::Real final_score( 0.0 );
	using std::string;
	using core::pose::getPoseExtraScore;
	if ( core::pose::getPoseExtraScore(
			pose, std::string("final_looprelax_score"), final_score
			)
			) {
		if ( basic::options::option[ basic::options::OptionKeys::loops::final_score_filter ].user() &&
				final_score > basic::options::option[ basic::options::OptionKeys::loops::final_score_filter ]()
				) {
			TR.Debug <<  "FailedFilter " << final_score << " > "
				<< basic::options::option[  basic::options::OptionKeys::loops::final_score_filter ]() << std::endl;
			return;
		}
	}

	/////////////////////////////////////////////////////////////////////////////
	////
	////   Output
	////

	if ( ! basic::options::option[ basic::options::OptionKeys::out::file::silent ].user() ) {

		// if closure failed don't output
		if ( remodel == "perturb_kic" || remodel == "perturb_kic_refactor" || remodel == "perturb_kic_with_fragments" ) {
			set_last_move_status( loop_relax_mover_.get_last_move_status() );
			if ( get_last_move_status() != protocols::moves::MS_SUCCESS ) {
				TR << "Initial kinematic closure failed. Not outputting."
					<< std::endl;
				return;
			}
		}
		if ( loop_relax_mover_.compute_rmsd() ) {
			if ( remodel != "no" ) {

				if ( remodel == "perturb_kic_refactor" ) {
					core::Real rebuild_looprms=0.0;
					getPoseExtraScore( pose, "rebuild_looprms", rebuild_looprms );
					protocols::jd2::add_string_real_pair_to_current_job("loop_rebuildrms ",rebuild_looprms );
					// mirror to tracer
					TR << "loop_rebuildrms: " << rebuild_looprms << std::endl;
				}

				core::Real cen_looprms=0.0;
				getPoseExtraScore( pose, "cen_looprms", cen_looprms );
				protocols::jd2::add_string_real_pair_to_current_job("loop_cenrms ",cen_looprms );
				// mirror to tracer
				TR << "loop_cenrms: " << cen_looprms << std::endl;
			}
			if ( refine != "no" ) {
				core::Real final_looprms=0.0;
				core::Real final_score=0.0;
				core::Real final_chainbreak=0.0;
				getPoseExtraScore( pose, "looprms", final_looprms );
				getPoseExtraScore( pose, "final_looprelax_score", final_score );
				getPoseExtraScore( pose, "final_chainbreak", final_chainbreak );
				protocols::jd2::add_string_real_pair_to_current_job("loop_rms ", final_looprms);
				protocols::jd2::add_string_real_pair_to_current_job("total_energy ", final_score);
				protocols::jd2::add_string_real_pair_to_current_job("chainbreak ", final_chainbreak);
				// mirror to tracer
				TR << "loop_rms " << final_looprms << std::endl;
				TR << "total_energy " << final_score << std::endl;
				TR << "chainbreak " << final_chainbreak << std::endl;
			}
		}
	}
	clock_t stoptime = clock();
	if ( keep_time ) {
		TR << "Job " << curr_nstruct << " took "<< ((double) stoptime - starttime )/CLOCKS_PER_SEC
			<< " seconds" << std::endl;
	}
}

std::string LoopBuildMover::get_name() const
{
	return "LoopBuildMover";
}
void LoopBuildMover::setup_loop_definition()
{
	//using namespace basic::resource_manager;
	// load loopfile
	//if ( ! ResourceManager::get_instance()->has_resource_with_description( "LoopsFile" ) ) {
	// throw CREATE_EXCEPTION(utility::excn::Exception,  "No loop file specified." );
	//}
	//protocols::loops::LoopsFileDataOP loops_from_file = get_resource< protocols::loops::LoopsFileData >( "LoopsFile" );
	//loop_relax_mover_.loops_file_data( *loops_from_file );

	// TO DO: Put this functionality into its own function since it seem like it could
	// be used in more than this one location?
	utility::vector1< std::string > loops_files = basic::options::option[ basic::options::OptionKeys::loops::loop_file ].value_or( utility::vector1< std::string >() );
	if ( ! loops_files.size() ) {
		throw CREATE_EXCEPTION(utility::excn::Exception, "The -loop_file flag has not been specified on the command line");
	}
	core::Size const which_loops_file( loops_files.size() == 1 ? 1 : core::Size( numeric::random::rg().random_range(1,( loops_files.size() ))));
	std::string loop_file = loops_files[ which_loops_file ];
	loops::LoopsFileIO lfio;
	loops::LoopsFileDataOP lfd = lfio.read_loop_file( loop_file, true /*prohibit_single_residue_loops*/ );
	loop_relax_mover_.loops_file_data( *lfd );

}

}//loop_build
}//protocols


