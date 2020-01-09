// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file   basic/resource_manager/locator/NullResourceLocater.cc
/// @brief
/// @author

//unit headers
#include <basic/resource_manager/locator/NullResourceLocator.hh>
#include <basic/resource_manager/locator/NullResourceLocatorCreator.hh>

// Package headers
#include <basic/resource_manager/locator/locator_schemas.hh>
#include <basic/Tracer.hh>

//utility headers
#include <utility/vector1.hh>
#include <utility/io/izstream.hh>
#include <utility/tag/XMLSchemaGeneration.hh>

//C++ headers
#include <istream>

namespace basic {
namespace resource_manager {
namespace locator {

using utility::tag::TagCOP;
using utility::io::izstream;
using utility::vector1;
using std::string;
using std::endl;
using std::istream;
using basic::Tracer;

static Tracer TR("basic.resource_manager.locator.NullResourceLocator");


///// NullResourceLocatorCreator /////
NullResourceLocatorCreator::NullResourceLocatorCreator() = default;


NullResourceLocatorCreator::~NullResourceLocatorCreator() = default;

ResourceLocatorOP
NullResourceLocatorCreator::create_resource_locator() const {
	return utility::pointer::make_shared< NullResourceLocator >();
}

string
NullResourceLocatorCreator::locator_type() const {
	return NullResourceLocator::classname();
}

void
NullResourceLocatorCreator::provide_xml_schema( utility::tag::XMLSchemaDefinition & xsd ) const {
	return NullResourceLocator::provide_xml_schema( xsd );
}


///// NullStream //////

NullStream::NullStream() = default;

NullStream::~NullStream() = default;

istream &
NullStream::stream() {
	return stream_;
}


///// NullResourceLocator /////

NullResourceLocator::NullResourceLocator() = default;

NullResourceLocator::NullResourceLocator(NullResourceLocator const & /*src*/) = default;

void
NullResourceLocator::show(
	std::ostream & out
) const {
	out << "NullResourceLocator" << endl;
}

std::string
NullResourceLocator::type() const {
	return classname();
}

NullResourceLocator::~NullResourceLocator() = default;

ResourceStreamOP
NullResourceLocator::locate_resource_stream(
	string const & /*input_id*/
) const {
	return utility::pointer::make_shared< NullStream >();
}

void
NullResourceLocator::parse_my_tag(
	TagCOP
)
{}


void
NullResourceLocator::provide_xml_schema(
	utility::tag::XMLSchemaDefinition & xsd
)
{
	using namespace utility::tag;
	AttributeList attrs;

	xsd_type_definition_w_attributes( xsd, classname(),
		"The NullResourceLocator is meant for cases where a resource can"
		" be created without reading from an input file. It goes through the motions"
		" of returning a ResourceStream (an empty NullStream) as is required of all"
		" ResourceLocators, but the stream that it creates will not be used.", attrs );
}

std::string
NullResourceLocator::classname()
{
	return "NullResourceLocator";
}

} // namespace locator
} // namespace resource_manager
} // namespace basic
