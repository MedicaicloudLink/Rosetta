// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file utility/tag/util.hh
/// @brief utility functions for schema generation.
/// @author Jared Adolf-Bryfogle (jadolfbr@gmail.com)

#ifndef INCLUDED_utility_tag_util_hh
#define INCLUDED_utility_tag_util_hh

#include <utility/tag/XMLSchemaGeneration.fwd.hh>
#include <utility/vector1.hh>

namespace utility {
namespace tag {

///@brief Add a SchemaRestriction for a set of strings.  Useful for enums.
///  The type name is then used as the type when adding an attribute.
///
///@details
///  Example: attlist+ XMLScemaAttribute(tag_name, type_name, description);
void
add_schema_restrictions_for_strings(
	XMLSchemaDefinition & xsd,
	std::string type_name,
	vector1< std::string > const & restrictions );



} //utility
} //tag


#endif //utility/tag_util_hh

