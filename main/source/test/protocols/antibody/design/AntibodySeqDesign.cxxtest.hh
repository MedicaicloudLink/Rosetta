// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file   protocols/antibody/constraints/AntibodyConstraintTests.cxxtest.hh
/// @brief Test creation of TF and TaskOps by SeqDesignTFCreator
/// @author Jared Adolf-Bryfogle

//#define private public
//#define protected public

// Test headers
#include <test/UMoverTest.hh>
#include <test/UTracer.hh>
#include <cxxtest/TestSuite.h>
#include <test/protocols/antibody/utilities.hh>

// Project Headers
#include <protocols/antibody/AntibodyInfo.hh>
#include <protocols/antibody/util.hh>
#include <protocols/antibody/AntibodyEnum.hh>
#include <protocols/antibody/clusters/CDRClusterEnum.hh>

#include <protocols/antibody/design/CDRGraftDesignOptions.hh>
#include <protocols/antibody/design/CDRSeqDesignOptions.hh>
#include <protocols/antibody/database/CDRSetOptions.hh>
#include <protocols/antibody/database/CDRSetOptionsParser.hh>
#include <protocols/antibody/database/AntibodyDatabaseManager.hh>
#include <protocols/antibody/design/AntibodySeqDesignTFCreator.hh>
#include <protocols/antibody/design/util.hh>

#include <protocols/simple_task_operations/RestrictToLoopsAndNeighbors.hh>

// Core Headers
#include <core/pose/Pose.hh>
#include <core/pose/PDBInfo.hh>
#include <core/import_pose/import_pose.hh>
#include <core/pack/task/TaskFactory.hh>
#include <core/pack/task/operation/TaskOperations.hh>
#include <core/pack/task/PackerTask.hh>
#include <core/scoring/ScoreFunction.hh>
#include <core/scoring/ScoreFunctionFactory.hh>

#include <basic/Tracer.hh>
#include <boost/foreach.hpp>
#include <utility/vector1.hh>
#include <iostream>

#define BFE BOOST_FOREACH

using namespace protocols::antibody;
using namespace protocols::antibody::design;
using namespace core::pack::task::operation;
using namespace core::pack::task;
using namespace protocols::simple_task_operations;

using utility::vector1;

static basic::Tracer TR("protocols.antibody.AntibodySeqDesign");

class AntibodySeqDesign: public CxxTest::TestSuite {

public:

	core::pose::Pose pose;
	AntibodyInfoOP ab_info;
	core::scoring::ScoreFunctionOP score;
	utility::vector1<CDRSeqDesignOptionsOP> design_options;
	utility::vector1< CDRSeqDesignOptionsOP > design_options2;
	bool first_run;
	std::string inpath;

	//Outpath for new UTracers.  Can't figure out a better way then manually setting
	// it here or going deep in the compiled code as pwd for test.py is not /source and I don't.
	// Please email me if you know a better way. -JAB.
	std::string first_run_outpath;

public:

	void setUp(){
		core_init();
		first_run = false;
		inpath = "protocols/antibody/design";
		first_run_outpath = "/Users/jadolfbr/Documents/Rosetta/main/source";


		core::import_pose::pose_from_file(pose, "protocols/antibody/2r0l_1_1.pdb", core::import_pose::PDB_file); //AHO renumbered pose
		ab_info = utility::pointer::make_shared< AntibodyInfo >(pose, AHO_Scheme, North);
		score = core::scoring::get_score_function();
		score->score(pose); //Segfault prevention from tenA neighbor graph.  Should be a better way then this...


		///Make sure conservative and profile-based sequences match up.
		///Extremely complicated without the diff.
		// L1 - Profiles
		// L2 - OFF
		// L3 - Profiles
		// L4 - Conservative

		// H1 - Conservative
		// H2 - OFF
		// H3 - Basic Design
		// H4 - OFF

		CDRSeqDesignOptionsOP l1_options( new CDRSeqDesignOptions(l1) );
		CDRSeqDesignOptionsOP l2_options( new CDRSeqDesignOptions(l2) );
		CDRSeqDesignOptionsOP l3_options( new CDRSeqDesignOptions(l3) );
		CDRSeqDesignOptionsOP l4_options( new CDRSeqDesignOptions(l4) );

		CDRSeqDesignOptionsOP h1_options( new CDRSeqDesignOptions(h1) );
		CDRSeqDesignOptionsOP h2_options( new CDRSeqDesignOptions(h2) );
		CDRSeqDesignOptionsOP h3_options( new CDRSeqDesignOptions(h3) );
		CDRSeqDesignOptionsOP h4_options( new CDRSeqDesignOptions(h4) );

		l1_options->design(true);
		l2_options->design(false);
		l3_options->design(true);
		l4_options->design(true);

		h1_options->design(true);
		h2_options->design(false);
		h3_options->design(true);
		h4_options->design(true);

		l1_options->design_strategy(seq_design_profiles);
		l2_options->design_strategy(seq_design_profiles);
		l3_options->design_strategy(seq_design_profiles);
		l4_options->design_strategy(seq_design_basic);

		h1_options->design_strategy(seq_design_conservative);
		h2_options->design_strategy(seq_design_conservative);
		h3_options->design_strategy(seq_design_basic);
		h4_options->design_strategy(seq_design_conservative);

		design_options.clear();
		design_options.resize(8, NULL);

		design_options[ l1 ] = l1_options;
		design_options[ l2 ] = l2_options;
		design_options[ l3 ] = l3_options;
		design_options[ l4 ] = l4_options;

		design_options[ h1 ] = h1_options;
		design_options[ h2 ] = h2_options;
		design_options[ h3 ] = h3_options;
		design_options[ h4 ] = h4_options;


		//ALL Basic
		design_options2.clear();
		design_options2.resize(8, NULL);

		for ( core::Size i = 1; i <= design_options.size(); ++i ) {
			design_options2[ i ] = design_options[ i ]->clone();
			design_options2[ i ]->design_strategy(seq_design_basic);
		}

		assert(design_options[1]->design_strategy() != design_options2[1]->design_strategy()); //Make sure copy worked.

	}

	void test_task_ops() {

		//Manual control of options - move this to setup.  Move each test to its own function.

		AntibodySeqDesignTFCreator creator =  AntibodySeqDesignTFCreator(ab_info, design_options, true);
		creator.set_design_H3_stem(false);
		TaskFactoryOP tf( new TaskFactory() );

		TR << "--CDRs with all of them designing" << std::endl;
		RestrictToLoopsAndNeighborsOP all_cdrs = creator.generate_task_op_all_cdr_design(pose, false);
		tf->push_back(all_cdrs);
		utility::vector1<bool> disabled_cdrs(8, false);
		assert_cdr_design_is_enabled_or_disabled(pose, tf->create_task_and_apply_taskoperations(pose), ab_info, disabled_cdrs);

		TR << "--CDRs with only those from options designing" << std::endl;
		RestrictToLoopsAndNeighborsOP design_cdrs = creator.generate_task_op_cdr_design(pose, false);
		tf->clear();
		tf->push_back(design_cdrs);
		disabled_cdrs[ h2 ] = true;
		disabled_cdrs[ l2 ] = true;

		assert_cdr_design_is_enabled_or_disabled(pose, tf->create_task_and_apply_taskoperations(pose), ab_info, disabled_cdrs);

		TR<< "--CDRs designing based on vector1 bool." << std::endl;
		utility::vector1<bool> designing(8, true);
		designing[ h1 ] = false;
		designing[ l2 ] =false;

		RestrictToLoopsAndNeighborsOP design_vec_cdrs = creator.generate_task_op_cdr_design(pose, designing, false);
		tf->clear();
		tf->push_back(design_vec_cdrs);
		designing.flip();
		assert_cdr_design_is_enabled_or_disabled(pose, tf->create_task_and_apply_taskoperations(pose),ab_info, designing);



	}
	void test_normal_tf_generation() {


		///  What is on and off
		//l1_options->design(true);
		//l2_options->design(false);
		//l3_options->design(true);
		//l4_options->design(true);

		//h1_options->design(true);
		//h2_options->design(false);
		//h3_options->design(true);
		//h4_options->design(true);



		AntibodySeqDesignTFCreator creator =  AntibodySeqDesignTFCreator(ab_info, design_options2, true);
		creator.set_design_H3_stem(true);
		TaskFactoryOP tf( new TaskFactory() );

		TR << "--General TF - Seq Design TF" << std::endl;
		creator.design_antigen(false);
		creator.design_framework(false);

		utility::vector1<bool> disabled_cdrs(8, false);
		disabled_cdrs[ h2 ] = true;
		disabled_cdrs[ l2 ] =true;

		TaskFactoryOP tf_on = creator.generate_tf_seq_design(pose, false /* disable non-designing cdrs */);
		//std::cout << "TF NO DISABLE DESIGN: " << std::endl;
		tf_on->create_task_and_apply_taskoperations(pose);//->show(std::cout);


		tf = creator.generate_tf_seq_design(pose, true /* disable non-designing cdrs */);


		PackerTaskOP task = tf->create_task_and_apply_taskoperations(pose);
		//task->show(std::cout);

		assert_region_design_is_disabled(pose, task, ab_info, antigen_region);
		assert_region_design_is_disabled(pose, task, ab_info, framework_region, false /*cdr4_as_framework*/);
		assert_cdr_design_is_enabled_or_disabled(pose, tf->create_task_and_apply_taskoperations(pose),ab_info, disabled_cdrs); //Must be disabled - as conservative

		///Make sure conservative and profile-based sequences match up.
		///Extremely complicated without the diff.
		// L1 - Profiles
		// L2 - OFF
		// L3 - Profiles
		// L4 - Basic Design

		// H1 - Conservative
		// H2 - OFF
		// H3 - Basic Design
		// H4 - Conservative

		creator =  AntibodySeqDesignTFCreator(ab_info, design_options, true);
		creator.set_design_H3_stem(false);
		tf = creator.generate_tf_seq_design(pose, true /* disable non-designing cdrs */);

		task = tf->create_task_and_apply_taskoperations(pose);

		output_or_test(tf, pose, first_run, "AntibodySeqDesign_general_tf", inpath, first_run_outpath);

		TR << "--TF enable antigen "<< std::endl;
		creator.design_antigen(true);
		tf = creator.generate_tf_seq_design(pose);
		assert_region_design_is_disabled(pose, tf->create_task_and_apply_taskoperations(pose), ab_info, framework_region, false /*cdr4_as_framework */);


		TR << "--TF enable framework " << std::endl;
		creator.design_framework(true);
		creator.design_antigen(false);
		tf = creator.generate_tf_seq_design(pose);
		assert_region_design_is_disabled(pose, tf->create_task_and_apply_taskoperations(pose), ab_info, antigen_region, false /*cdr4_as_framework */);



	}
	void test_graft_tf_generation1() {
		AntibodySeqDesignTFCreator creator =  AntibodySeqDesignTFCreator(ab_info, design_options, true);
		creator.set_design_H3_stem(false);
		TaskFactoryOP tf( new TaskFactory() );

		TR << "--TF used for graft design" << std::endl;
		utility::vector1<bool> l1_neighbors(8, false);
		l1_neighbors[ l3 ] = true;
		l1_neighbors[ l2 ] = true;
		l1_neighbors[ h3 ] = true;

		creator.design_framework(true);
		creator.design_antigen(false);

		tf = creator.generate_tf_seq_design_graft_design(pose, l1, l1_neighbors);
		assert_region_design_is_disabled(pose, tf->create_task_and_apply_taskoperations(pose), ab_info, antigen_region);


	}
	void test_graft_tf_generation2() {
		AntibodySeqDesignTFCreator creator =  AntibodySeqDesignTFCreator(ab_info, design_options, true);
		creator.set_design_H3_stem(false);
		TaskFactoryOP tf( new TaskFactory() );

		TR << "--TF used for graft design" << std::endl;
		utility::vector1<bool> l1_neighbors(8, false);
		l1_neighbors[ l3 ] = true;
		l1_neighbors[ l2 ] = true;
		l1_neighbors[ h3 ] = true;

		creator.design_framework(false);
		creator.design_antigen(false);

		tf = creator.generate_tf_seq_design_graft_design(pose, l1, l1_neighbors);

		utility::vector1<bool> disabled_cdrs(8, false);
		disabled_cdrs[ h2 ] = true; //Goes with options above.
		disabled_cdrs[ l2 ] = true;

		//H1 is set to design in design_options.

		assert_cdr_design_disabled(pose, tf->create_task_and_apply_taskoperations(pose), ab_info, disabled_cdrs);
		//output_or_test(tf, pose, first_run, "AntibodySeqDesign_graft_tf", inpath, first_run_outpath);

	}
	void test_utility_functions() {

		RestrictResidueToRepackingOP disable_antigen = disable_design_region(ab_info, pose, antigen_region);
		RestrictResidueToRepackingOP disable_framework = disable_design_region(ab_info, pose, framework_region);
		RestrictResidueToRepackingOP disable_cdrs = disable_design_region(ab_info, pose, cdr_region);

		assert_region_design_is_disabled_rr(pose, disable_antigen, ab_info, antigen_region);
		assert_region_design_is_disabled_rr(pose, disable_framework, ab_info, framework_region);
		assert_region_design_is_disabled_rr(pose, disable_cdrs, ab_info, cdr_region);

		RestrictResidueToRepackingOP disable_antigen2 = disable_design_antigen(ab_info, pose);
		RestrictResidueToRepackingOP disable_framework2 = disable_design_framework(ab_info, pose);
		RestrictResidueToRepackingOP disable_cdrs2 = disable_design_cdrs(ab_info, pose);

		assert_region_design_is_disabled_rr(pose, disable_antigen2, ab_info, antigen_region);
		assert_region_design_is_disabled_rr(pose, disable_framework2, ab_info, framework_region);
		assert_region_design_is_disabled_rr(pose, disable_cdrs2, ab_info, cdr_region);


	}

};
