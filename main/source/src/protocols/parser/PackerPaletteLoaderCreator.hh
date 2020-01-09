// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file   protocols/parser/PackerPaletteLoaderCreator.hh
/// @brief  Creator classes for PackerPaletteLoader class.
/// @author Vikram K. Mulligan (vmullig@uw.edu).

#ifndef INCLUDED_protocols_parser_PackerPaletteLoaderCreator_hh
#define INCLUDED_protocols_parser_PackerPaletteLoaderCreator_hh

// Package headers
#include <protocols/parser/DataLoaderCreator.hh>

// Utility Headers
#include <utility/pointer/ReferenceCount.hh>

namespace protocols {
namespace parser {

class PackerPaletteLoaderCreator : public DataLoaderCreator
{
public:
	/// @brief Destructor.
	///
	~PackerPaletteLoaderCreator() override {}

	DataLoaderOP create_loader() const override;
	std::string keyname() const override;
	DerivedNameFunction schema_ct_naming_function() const override;
	void provide_xml_schema( utility::tag::XMLSchemaDefinition & xsd ) const override;

	static std::string packerpalette_loader_ct_namer( std::string const & element_name );
};

} //namespace parser
} //namespace protocols

#endif
