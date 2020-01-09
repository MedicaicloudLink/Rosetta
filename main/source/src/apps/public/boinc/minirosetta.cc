// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file
/// @brief

#ifdef BOINC
#include <utility/io/izstream.hh>
#include <utility/io/ozstream.hh>
#include <utility/boinc/boinc_util.hh>
#include <utility/file/file_sys_util.hh>
#include <protocols/boinc/boinc.hh>
#include "boinc_zip.h"
#endif // BOINC

/// Must have this after BOINC stuff to avoid windows build error
#include <core/io/silent/util.hh>
#include <protocols/abinitio/AbrelaxApplication.hh>
#include <protocols/abinitio/IterativeAbrelax.hh>
#include <protocols/cyclic_peptide_predict/SimpleCycpepPredictApplication.hh>
#include <protocols/jd2/archive/ArchiveManager.hh>
#include <protocols/abinitio/BrokerMain.hh>
#include <protocols/boinc/util.hh>
#include <protocols/comparative_modeling/cm_main.hh>
#include <protocols/medal/MedalMain.hh>
#include <protocols/symmetric_docking/SymDockProtocol.hh>
#include <protocols/rbsegment_relax/RBSegmentRelax_main.hh>
#include <protocols/loop_build/LoopBuild.hh>
#include <protocols/loophash/Mover_LoopHashRefine.hh>
#include <protocols/abinitio/vs_test.hh>
#include <protocols/jd2/JobDistributor.hh>
#include <protocols/ligand_docking/ligand_dock_impl.hh>
#include <protocols/flxbb/FlxbbDesign_main.hh>
#include <protocols/ddg/ddG_main.hh>
#include <protocols/canonical_sampling/CanonicalSamplingApplication.hh>
#include <protocols/canonical_sampling/CanonicalSamplingMover.hh>
#include <protocols/frag_picker/nonlocal/NonlocalFragsMain.hh>
#include <protocols/star/StarAbinitioMain.hh>

#ifdef BOINC
#include <protocols/boinc/boinc.hh>
#include <devel/init.hh>
#endif

#ifdef WIN32
#include <devel/init.hh>
#include <core/import_pose/import_pose.hh>
#include <core/io/pdb/pdb_writer.hh>
#endif

#include <core/types.hh>

#include <devel/init.hh>

#ifdef BOINC
#include <core/pose/Pose.hh>
#include <core/pose/extra_pose_info_util.hh>
#include <core/import_pose/import_pose.hh>
#include <core/io/pdb/pdb_writer.hh>
#include <core/io/silent/util.hh>
#include <core/pose/util.hh>
#endif

#include <basic/options/option.hh>
#include <utility/exit.hh>
#include <utility/excn/Exceptions.hh>

// option key includes
#include <basic/options/keys/in.OptionKeys.gen.hh>
#include <basic/options/keys/run.OptionKeys.gen.hh>
#include <basic/options/keys/jd2.OptionKeys.gen.hh>
#include <basic/options/keys/boinc.OptionKeys.gen.hh>

#include <protocols/checkpoint/Checkpoint.hh>
#include <protocols/jd2/BOINCJobDistributor.hh>
#include <protocols/relax/relax_main.hh>
#include <utility/vector0.hh>
#include <utility/vector1.hh>

class DummyMover : public protocols::moves::Mover {
public:
	DummyMover() : protocols::moves::Mover( "DummyMover" ) {}
	~DummyMover() override= default;
	void apply( core::pose::Pose & ) override {
		std::cerr << "DummyMover::apply() should never have been called!"
			<< " (JobDistributor/Parser should have replaced DummyMover.)" << std::endl;
		utility_exit_with_message("Function not implemented.");
	}
	std::string get_name() const override { return "DummyMover"; }
};

int
main( int argc, char * argv [] )
{
	try {

		using namespace basic::options;
		using namespace basic::options::OptionKeys;
		using std::string;
		using utility::vector1;

#ifdef BOINC // BOINC STUFF

		// do not modify argv, it's not safe and can cause weird errors on Android

		// initialize boinc
		using namespace protocols::boinc;
		Boinc boinc_wu = Boinc::instance();
		boinc_wu.initialize_worker(); //The shared memory gets created here.

		// lets not keep writing this to stderr after restarts to conserve IO/data.
		// stderr.txt exists as a file link even before the BOINC client runs the app so use a tmp file.
		if (!utility::file::file_exists("rosetta_tmp.txt")) {
			// print command to stderr
			std::cerr << "command:";
			for ( int i=0; i< argc; ++i ) {
				std::cerr << ' ' <<  argv[i];
			}
			std::cerr  << std::endl; std::cerr.flush();
			utility::io::ozstream data( "rosetta_tmp.txt" );
		}
#endif

		// has to be called before devel::init. Which is really stupid.
		protocols::abinitio::AbrelaxApplication::register_options();
		protocols::cyclic_peptide_predict::SimpleCycpepPredictApplication::register_options();
		protocols::abinitio::IterativeAbrelax::register_options();
		protocols::jd2::archive::ArchiveManager::register_options();
		protocols::canonical_sampling::CanonicalSamplingMover::register_options();
		protocols::abinitio::register_options_broker();

		devel::init( argc, argv );

#ifdef BOINC // BOINC STUFF

#ifdef BOINC_GRAPHICS
	// read the work unit description file if one exists
	Boinc::set_wu_desc();
	Boinc::set_randomly_cycle_appearance( option[run::protocol]() != "simple_cycpep_predict" );  // Set whether the app should randomly cycle the graphics
#endif

	// unzip an archive?
	if (option[ in::file::zip ].user()) {
		std::string resolvedfile = option[ in::file::zip ]();
		bool is_database = false;
		bool skip_extraction = false;
		if (resolvedfile == "minirosetta_database.zip") {
			is_database = true;
			if (utility::file::file_exists( "minirosetta_database.zip.is_extracted" )) skip_extraction = true;
		}
		if (!skip_extraction) {
			utility::boinc::resolve_filename( resolvedfile );
			if (!utility::file::file_exists( resolvedfile )) {
				utility_exit_with_message("in::file::zip "+
					option[ in::file::zip ]()+" does not exist!");
			} else {
				boinc_zip(UNZIP_IT, resolvedfile, "./");
				if (is_database) {
					utility::io::ozstream data( "minirosetta_database.zip.is_extracted" );
				}
			}
		}
	}

	// unzip an archive of files specific to a given BOINC workunit.
	if ( option[ in::file::boinc_wu_zip ].user() ) {
		vector1< string > files = option[ in::file::boinc_wu_zip ]();
		for( vector1< string >::const_iterator it = files.begin(), end = files.end(); it != end; ++it ) {
			std::string resolvedfile = *it;
			utility::boinc::resolve_filename( resolvedfile );
			if ( !utility::file::file_exists( resolvedfile ) ) {
				utility_exit_with_message( "in::file::boinc_wu_zip " +
					*it + " does not exist!"
				);
			} else {
				boinc_zip(UNZIP_IT, resolvedfile, "./");
			}
		}
	}

	// override database option and set to current directory
	option[in::path::database].value("minirosetta_database");

#endif
		if ( option[ run::checkpoint ] || option[ run::checkpoint_interval ].user() ) {
			protocols::checkpoint::checkpoint_with_interval( option[ run::checkpoint_interval ] );
		}

#ifdef BOINC_GRAPHICS
		// set native for graphics
		if ( option[ in::file::native ].user() ) {
			core::pose::PoseOP native_pose_( new core::pose::Pose );
			core::import_pose::pose_from_file( *native_pose_, option[ in::file::native ]() , core::import_pose::PDB_file);
			if ( native_pose_->size() <= protocols::boinc::MAX_NATIVE_POSE_RESIDUES ) {
				core::pose::set_ss_from_phipsi( *native_pose_ );
				protocols::boinc::Boinc::set_graphics_native_pose( *native_pose_ );
			}
		}

		protocols::boinc::Boinc::set_shared_memory_fully_initialized(); //Signal that the shared memory is fully initialized at this point, and can be used by the graphics app.
#endif


		// RUN PROTOCOL
		// catch *any* exception.
		try{
			if ( option[ run::protocol ]() == "abrelax" ) {
				protocols::abinitio::AbrelaxApplication abrelax;
				abrelax.run();
			} else if ( option[run::protocol]() == "simple_cycpep_predict" ) {
				protocols::cyclic_peptide_predict::SimpleCycpepPredictApplication peppredict;
				peppredict.run();
			} else if ( option[ run::protocol ]() == "symdock" ) {
				protocols::symmetric_docking::SymDock_main();
			} else if ( option[ run::protocol ]() == "broker" ) {
				protocols::abinitio::Broker_main();
			} else if ( option[ run::protocol ]() == "loophash" ) {
				protocols::loophash::loophash_main();
			} else if ( option[ run::protocol ]() == "ligand_dock" ) {
				ligand_dock_main();
			} else if ( option[ run::protocol ]() == "relax" ) {
				protocols::relax::Relax_main( true );
			} else if ( option[ run::protocol ]() == "looprelax" ) {
				protocols::loop_build::LoopBuild_main( true );
			} else if ( option[ run::protocol ]() == "threading" ) {
				protocols::comparative_modeling::cm_main();
			} else if ( option[ run::protocol ]() == "medal" ) {
				protocols::medal::Medal_main(nullptr);
			} else if ( option[ run::protocol ]() == "medal_exchange" ) {
				protocols::medal::MedalExchange_main(nullptr);
			} else if ( option[ run::protocol ]() == "star" ) {
				protocols::star::StarAbinitio_main(nullptr);
			} else if ( option[ run::protocol ]() == "rbsegmentrelax" ) {
				protocols::RBSegmentRelax_main( );
			} else if ( option[ run::protocol ]() == "boinc_debug" ) {
				protocols::abinitio::run_boinc_debug();
			} else if ( option[ run::protocol ]() == "flxbb" ) {
				protocols::flxbb::FlxbbDesign_main();
			} else if ( option[ run::protocol ]() == "jd2_scripting" ) {
				protocols::moves::MoverOP mover( new DummyMover );
				option[ jd2::dd_parser ].value( true ); // This option MUST be set true if we're using rosetta_scripts.
				// To avoid accidental crashes, we'll do so programatically
				protocols::jd2::BOINCJobDistributor::get_instance()->go( mover );
			} else if ( option[run::protocol]() == "ddg" ) {
				protocols::ddG_main();
			} else if ( option[run::protocol]() == "canonical_sampling" ) {
				protocols::canonical_sampling::canonical_sampling_main();
			} else if ( option[run::protocol]() == "nonlocal_frags" ) {
				protocols::frag_picker::nonlocal::NonlocalFrags_main();
			} else {
				utility_exit_with_message(
					"Invalid protocol requested: "+ option[ run::protocol ]()
				);
				return 0; // makes compiler happy
			}
			//---code to do a in spot score cut necessary for running centroid abinitio through boinc
#ifdef BOINC
	double runtime = protocols::boinc::Boinc::get_boinc_wu_cpu_time();
#else
			double runtime = -1; //-1 does not do filtering
#endif
			if ( option[boinc::score_cut_pct].user() ) {
				double minTimePerModel = 61;  //seconds per model min time for boinc.
				protocols::boinc::boincOutputFilter(runtime,minTimePerModel); //ideally score cut alone, but an additional filter that allows only 1 structure every 61 seconds.
			}
#ifdef BOINC

	// gzip the output silent files.
	core::io::silent::gzip();

	// ideally these would be called in the dtor but the way we have the singleton pattern set up the dtors don't get
	// called
	protocols::boinc::Boinc::worker_finish_summary( protocols::boinc::Boinc::decoy_count(), protocols::boinc::Boinc::decoy_count(), 1 );
	protocols::boinc::Boinc::worker_shutdown(); // Does not return.
	utility_exit_with_message( "reached end of minirosetta::main() after worker_shutdown(); " );
#endif
		} catch (utility::excn::Exception& excn ) {
			excn.display();
#ifdef USEMPI
		MPI_Abort( MPI_COMM_WORLD, 911 );
#endif
			return 1;    // MUST return non-0 - otherwise BOINC does not abort!
		}

	} catch (utility::excn::Exception const & e ) {
		e.display();
		return -1;
	}
	return 0;
}

#ifdef BOINC
#ifdef _WIN32




/*******************************************************
 * Windows: Unix applications begin with main() while Windows applications
 * begin with WinMain, so this just makes WinMain() process the command line
 * and then invoke main()
 */
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrevInst,
									 LPSTR Args, int WinMode)
{
		LPSTR command_line;
		char* argv[1024];
		int argc;

		command_line = GetCommandLine();
		argc = parse_command_line( command_line, argv );
		return main(argc, argv);
}
#endif
#endif
