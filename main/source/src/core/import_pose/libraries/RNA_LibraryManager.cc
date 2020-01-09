// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file core/import_pose/libraries/RNA_LibraryManager.cc
/// @brief
/// @details
/// @author Rhiju Das, rhiju@stanford.edu


#include <core/import_pose/libraries/RNA_LibraryManager.hh>
#include <core/fragment/rna/FullAtomRNA_Fragments.hh>
#include <core/import_pose/libraries/RNA_JumpLibrary.hh>
#include <core/import_pose/libraries/BasePairStepLibrary.hh>
#include <core/import_pose/options/RNA_FragmentMonteCarloOptions.hh>
#include <basic/Tracer.hh>
#include <basic/options/option.hh>

static basic::Tracer TR( "core.import_pose.libraries.RNA_LibraryManager" );

using namespace core::import_pose::options;
using namespace core::import_pose;
using namespace core::fragment::rna;

namespace core {
namespace import_pose {
namespace libraries {

RNA_Fragments const &
RNA_LibraryManager::rna_fragment_library( std::string const & tag ) {
	if ( rna_fragment_libraries_.find( tag ) == rna_fragment_libraries_.end() ) {
		rna_fragment_libraries_[ tag ] = utility::pointer::make_shared< FullAtomRNA_Fragments >( tag );
	}
	return *rna_fragment_libraries_[ tag ];
}

RNA_JumpLibrary const &
RNA_LibraryManager::rna_jump_library( std::string const & tag ) {
	return *( rna_jump_library_cop( tag ) );
}

RNA_JumpLibraryCOP const &
RNA_LibraryManager::rna_jump_library_cop( std::string const & tag ) {
	if ( rna_jump_libraries_.find( tag ) == rna_jump_libraries_.end() ) {
		rna_jump_libraries_[ tag ] = utility::pointer::make_shared< RNA_JumpLibrary >( tag );
	}
	return rna_jump_libraries_[ tag ];
}

RNA_JumpLibraryCOP const &
RNA_LibraryManager::rna_jump_library_cop() {
	RNA_FragmentMonteCarloOptions options; // default value is stored here.
	// ... nope, we need to initialize_from_options
	// since we don't have an OptionsCollection here, let's just say...
	options.initialize_from_options( basic::options::option );
	return rna_jump_library_cop( options.jump_library_file() );
}

BasePairStepLibrary const &
RNA_LibraryManager::canonical_base_pair_step_library() {
	if ( canonical_base_pair_step_library_ == nullptr )  canonical_base_pair_step_library_ = utility::pointer::make_shared< BasePairStepLibrary >( true );
	return *canonical_base_pair_step_library_;
}

BasePairStepLibrary const &
RNA_LibraryManager::general_base_pair_step_library() {
	if ( general_base_pair_step_library_ == nullptr )  general_base_pair_step_library_ = utility::pointer::make_shared< BasePairStepLibrary >( false );
	return *general_base_pair_step_library_;
}

} //libraries
} //denovo
} //protocols
