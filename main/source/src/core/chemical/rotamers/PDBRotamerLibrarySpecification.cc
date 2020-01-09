// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file   core/chemical/rotamers/PDBRotamerLibrarySpecification.cc
/// @brief  The PDBRotamerLibrarySpecification class specifies building PDBRotamers.
/// @author Rocco Moretti (rmorettase@gmail.com)

// Unit headers
#include <core/chemical/rotamers/PDBRotamerLibrarySpecification.hh>
#include <core/chemical/rotamers/PDBRotamerLibrarySpecificationCreator.hh>

// Utility headers
#include <utility>
#include <utility/exit.hh>

#ifdef    SERIALIZATION
// Utility serialization headers
#include <utility/serialization/serialization.hh>

// Cereal headers
#include <cereal/types/string.hpp>
#include <cereal/types/polymorphic.hpp>
#endif // SERIALIZATION

namespace core {
namespace chemical {
namespace rotamers {

// Creator Functions

RotamerLibrarySpecificationOP
PDBRotamerLibrarySpecificationCreator::create() const {
	return utility::pointer::make_shared< PDBRotamerLibrarySpecification >();
}

RotamerLibrarySpecificationOP
PDBRotamerLibrarySpecificationCreator::create( std::istream & input ) const {
	return utility::pointer::make_shared< PDBRotamerLibrarySpecification >( input );
}

std::string
PDBRotamerLibrarySpecificationCreator::keyname() const {
	return PDBRotamerLibrarySpecification::library_name();
}

// Specification Functions

PDBRotamerLibrarySpecification::PDBRotamerLibrarySpecification() = default;

PDBRotamerLibrarySpecification::PDBRotamerLibrarySpecification( std::string const & library_file ):
	pdb_rotamers_file_(library_file)
{}

PDBRotamerLibrarySpecification::PDBRotamerLibrarySpecification(std::istream & input) {
	input >> pdb_rotamers_file_;
	if ( ! input ) {
		utility_exit_with_message("Must provide a file name with PDB Rotamers input");
	}
}
PDBRotamerLibrarySpecification::~PDBRotamerLibrarySpecification() = default;

std::string
PDBRotamerLibrarySpecification::keyname() const {
	return library_name();
}

std::string
PDBRotamerLibrarySpecification::library_name() {
	return "PDB";
}

} //namespace rotamers
} //namespace chemical
} //namespace core

#ifdef    SERIALIZATION

/// @brief Automatically generated serialization method
template< class Archive >
void
core::chemical::rotamers::PDBRotamerLibrarySpecification::save( Archive & arc ) const {
	arc( cereal::base_class< core::chemical::rotamers::RotamerLibrarySpecification >( this ) );
	arc( CEREAL_NVP( pdb_rotamers_file_ ) ); // std::string
}

/// @brief Automatically generated deserialization method
template< class Archive >
void
core::chemical::rotamers::PDBRotamerLibrarySpecification::load( Archive & arc ) {
	arc( cereal::base_class< core::chemical::rotamers::RotamerLibrarySpecification >( this ) );
	arc( pdb_rotamers_file_ ); // std::string
}

SAVE_AND_LOAD_SERIALIZABLE( core::chemical::rotamers::PDBRotamerLibrarySpecification );
CEREAL_REGISTER_TYPE( core::chemical::rotamers::PDBRotamerLibrarySpecification )

CEREAL_REGISTER_DYNAMIC_INIT( core_chemical_rotamers_PDBRotamerLibrarySpecification )
#endif // SERIALIZATION
