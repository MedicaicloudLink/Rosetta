// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

// Test headers
#include <cxxtest/TestSuite.h>
#include <test/core/init_util.hh>
#include <test/UTracer.hh>

#include <core/chemical/automorphism.hh>
#include <core/chemical/ChemicalManager.hh>
#include <core/chemical/ResidueType.hh>
#include <core/chemical/ResidueTypeSet.hh>

//Auto Headers
#include <utility/vector1.hh>


class AutomorphismTest : public CxxTest::TestSuite {

public:

	void setUp() {
		core_init_with_additional_options(
			"-extra_res_fa core/chemical/1pqc.params core/chemical/uk002.params"
		);
		/*
		// Residue definitions can't be supplied on the command line b/c
		// the ResidueTypeSet is already initialized.
		using namespace core::chemical;
		utility::vector1< std::string > params_files;
		ResidueTypeSetCOP const_residue_set = ChemicalManager::get_instance()->residue_type_set( FA_STANDARD );
		ResidueTypeSet & residue_set = const_cast< ResidueTypeSet & >(*const_residue_set);
		if ( !residue_set.has_name("QC1") )   params_files.push_back("core/chemical/1pqc.params");
		if ( !residue_set.has_name("UK002") ) params_files.push_back("core/chemical/uk002.params");
		residue_set.read_files_for_custom_residue_types(params_files);
		*/
	}

	void tearDown() {}

	void test_automorphism_counts() {
		using namespace core::chemical;
		ResidueTypeSetCOP residue_set = ChemicalManager::get_instance()->residue_type_set( FA_STANDARD );
		TS_ASSERT_EQUALS(3456, count_automorphisms( residue_set->name_mapOP("QC1") ) );
		TS_ASSERT_EQUALS(8, count_automorphisms( residue_set->name_mapOP("UK002") ) );
	}

	core::Size count_automorphisms(core::chemical::ResidueTypeCOP rsdtype)
	{
		core::chemical::AutomorphismIterator itr(*rsdtype, false /*exclude H*/);
		core::Size cnt = 0;
		while ( !itr.next().empty() ) cnt++;
		return cnt;
	}
};

