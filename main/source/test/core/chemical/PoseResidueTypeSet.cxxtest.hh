// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file  core/chemical/PoseResidueTypeSet.cxxtest.hh
/// @brief  Test ResidueTypeSetPose
/// @author Rocco Moretti (rmorettiase@gmail.com)


// Test headers
#include <test/UMoverTest.hh>
#include <test/UTracer.hh>
#include <cxxtest/TestSuite.h>

// Project Headers
#include <core/chemical/GlobalResidueTypeSet.hh>
#include <core/chemical/PoseResidueTypeSet.hh>
#include <core/chemical/ResidueTypeFinder.hh>
#include <core/chemical/ResidueProperties.hh>

// Core Headers
#include <core/pose/Pose.hh>
#include <core/import_pose/import_pose.hh>

// Protocol Headers

#include <basic/database/open.hh>
#include <basic/Tracer.hh>

static basic::Tracer TR("core.chemical.PoseResidueTypeSet.cxxtest");


class PoseResidueTypeSetTests : public CxxTest::TestSuite {
	//Define Variables

public:

	void setUp(){
		core_init();

	}

	void tearDown(){

	}


	void test_addition() {
		using namespace core::chemical;

		GlobalResidueTypeSetOP grs( new GlobalResidueTypeSet( FA_STANDARD, basic::database::full_name( "chemical/residue_type_sets/"+FA_STANDARD+"/" ) ) );

		ResidueType const & serine = grs->name_map( "SER" );

		ResidueTypeOP modser = serine.clone();
		modser->nbr_radius( 15.0);
		modser->name( "bigser" );

		PoseResidueTypeSetOP rs( new PoseResidueTypeSet( grs ) );

		//get some stuff from the residue type set
		core::Size n_base_res_types   = rs->base_residue_types().size();
		core::Size n_unpatchable_res_types = rs->unpatchable_residue_types().size();
		core::Size n_ser_types = ResidueTypeFinder( *rs ).name3( "SER" ).get_all_possible_residue_types().size();
		core::Size n_gln_types = ResidueTypeFinder( *rs ).name3( "GLN" ).get_all_possible_residue_types().size();
		core::Size n_ser_aa = ResidueTypeFinder( *rs ).aa( aa_ser ).get_all_possible_residue_types().size();

		//now change the residue type set
		// const_cast, for testing purposes only.
		rs->add_unpatchable_residue_type( modser );

		//now make sure everything is as should be
		TS_ASSERT( n_base_res_types == rs->base_residue_types().size());
		TS_ASSERT( n_unpatchable_res_types + 1 == rs->unpatchable_residue_types().size());
		TS_ASSERT( n_ser_types + 1 == ResidueTypeFinder( *rs ).name3( "SER" ).get_all_possible_residue_types().size() );
		TS_ASSERT( n_gln_types == ResidueTypeFinder( *rs ).name3( "GLN" ).get_all_possible_residue_types().size() );
		TS_ASSERT( n_ser_aa + 1 == ResidueTypeFinder( *rs ).aa( aa_ser ).get_all_possible_residue_types().size() );
		TS_ASSERT( rs->has_name("bigser") );
	}

	void test_local_vs_default() {
		using namespace core::chemical;
		// The local PoseResidueTypeSet should pull out the ResidueType from the default RTS, unless overridden.

		GlobalResidueTypeSetOP grts( new GlobalResidueTypeSet( FA_STANDARD, basic::database::full_name( "chemical/residue_type_sets/"+FA_STANDARD+"/" ) ) );
		PoseResidueTypeSetOP prts( new PoseResidueTypeSet( grts ) );

		ResidueTypeOP new_cys( new ResidueType( grts->name_map("CYS") ) );
		std::string const & unique_tag( "UNIQUE_PROPERTY_TAG_13243214" );
		new_cys->add_string_property(unique_tag,"VALUE");
		prts->add_base_residue_type( new_cys );

		utility::vector1< std::string > variants{ "UPPER_TERMINUS_VARIANT" };

		// Non-CYS should get the same types as the Global RTS

		TS_ASSERT( prts->name_mapOP("GLU").get() == grts->name_mapOP("GLU").get() ); // Yes, we want to compare object addresses
		TS_ASSERT( prts->name_mapOP("GLU:NtermProteinFull").get() == grts->name_mapOP("GLU:NtermProteinFull").get() ); // Check patched

		TS_ASSERT( prts->get_representative_type_aa(aa_trp).get() == grts->get_representative_type_aa(aa_trp).get() );
		TS_ASSERT( prts->get_representative_type_aa(aa_trp,variants).get() == grts->get_representative_type_aa(aa_trp,variants).get() );

		TS_ASSERT( prts->get_representative_type_name1('R').get() == grts->get_representative_type_name1('R').get() );
		TS_ASSERT( prts->get_representative_type_name1('R',variants).get() == grts->get_representative_type_name1('R',variants).get() );

		TS_ASSERT( prts->get_representative_type_name3("LYS").get() == grts->get_representative_type_name3("LYS").get() );
		TS_ASSERT( prts->get_representative_type_name3("LYS",variants).get() == grts->get_representative_type_name3("LYS",variants).get() );

		TS_ASSERT( prts->get_mirrored_type( prts->name_mapOP("ALA") ).get() == grts->get_mirrored_type( grts->name_mapOP("ALA") ).get() );

		// Make sure that even if we cache things, we still get the same types

		TS_ASSERT( prts->name_mapOP("GLU").get() == grts->name_mapOP("GLU").get() ); // Yes, we want to compare object addresses
		TS_ASSERT( prts->name_mapOP("GLU:NtermProteinFull").get() == grts->name_mapOP("GLU:NtermProteinFull").get() ); // Check patched

		// Now we *shouldn't* get the same type as the global for anything involving CYS

		TS_ASSERT( prts->name_mapOP("CYS").get() != grts->name_mapOP("CYS").get() );
		TS_ASSERT( prts->name_mapOP("CYS")->properties().string_properties().count(unique_tag) == 1 );
		TS_ASSERT( prts->name_mapOP("CYS:NtermProteinFull").get() != grts->name_mapOP("CYS:NtermProteinFull").get() );
		TS_ASSERT( prts->name_mapOP("CYS:NtermProteinFull")->properties().string_properties().count(unique_tag) == 1 );

		TS_ASSERT( prts->get_representative_type_aa(aa_cys).get() != grts->get_representative_type_aa(aa_cys).get() );
		TS_ASSERT( prts->get_representative_type_aa(aa_cys,variants).get() != grts->get_representative_type_aa(aa_cys,variants).get() );

		TS_ASSERT( prts->get_representative_type_name1('C').get() != grts->get_representative_type_name1('C').get() );
		TS_ASSERT( prts->get_representative_type_name1('C',variants).get() != grts->get_representative_type_name1('C',variants).get() );

		TS_ASSERT( prts->get_representative_type_name3("CYS").get() != grts->get_representative_type_name3("CYS").get() );
		TS_ASSERT( prts->get_representative_type_name3("CYS",variants).get() != grts->get_representative_type_name3("CYS",variants).get() );

		// D/L patching in PoseResidueTypeSets doesn't work at the moment.
		//// The D-version is also a patch, so it should also be different
		//TS_ASSERT( prts->get_mirrored_type( prts->name_mapOP("CYS") ).get() != grts->get_mirrored_type( grts->name_mapOP("CYS") ).get() );
		//TS_ASSERT( prts->get_mirrored_type( prts->name_mapOP("CYS") )->properties().string_properties().count(unique_tag) == 1 );

		// Now if we add a new patch, we should grab that instead of the global one.
		utility::vector1< std::string > new_patch{"core/chemical/conflict_patch.txt"};
		utility::vector1< std::string > new_metapatch;
		prts->add_patches( new_patch, new_metapatch );

		TS_ASSERT( prts->name_mapOP("TYR").get() == grts->name_mapOP("TYR").get() ); // The non-patched versions should be the same
		TS_ASSERT( prts->name_mapOP("TYR:NtermProteinFull").get() == grts->name_mapOP("TYR:NtermProteinFull").get() ); // As should non-involved patches
		TS_ASSERT( prts->name_mapOP("TYR:sulfated").get() != grts->name_mapOP("TYR:sulfated").get() ); // But the patch in question shouldn't
		TS_ASSERT_EQUALS( prts->name_mapOP("TYR:sulfated")->interchangeability_group(), "FOO" );
		TS_ASSERT_EQUALS( grts->name_mapOP("TYR:sulfated")->interchangeability_group(), "TYS" );
	}



};



