// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file   core/select/residue_selector/ResidueSpanSelector.hh
/// @brief  The ResidueSpanSelector selects residues using start/end defintions
/// @author Rocco Moretti (rmorettiase@gmail.com)

// Unit headers
#include <core/select/residue_selector/ResidueSpanSelector.hh>
#include <core/select/residue_selector/ResidueSelectorCreators.hh>

// Package headers
#include <core/select/residue_selector/util.hh>
#include <core/pose/ResidueIndexDescription.hh>

// Basic Headers
#include <basic/datacache/DataMap.hh>

// Project headers
#include <core/pose/Pose.hh>
#include <core/pose/selection.hh>
#include <core/conformation/Residue.hh>

// Utility Headers
#include <utility>
#include <utility/tag/Tag.hh>
#include <utility/tag/XMLSchemaGeneration.hh>

// C++ headers
#include <utility/assert.hh>
#include <basic/Tracer.hh>

#ifdef    SERIALIZATION
// Utility serialization headers
#include <utility/serialization/serialization.hh>

// Cereal headers
#include <cereal/types/polymorphic.hpp>
#include <cereal/types/string.hpp>
#endif // SERIALIZATION

static basic::Tracer TR("core.select.residue_selector.ResidueSpanSelector");

namespace core {
namespace select {
namespace residue_selector {

ResidueSpanSelector::ResidueSpanSelector() = default;

ResidueSpanSelector::ResidueSpanSelector( ResidueSpanSelector const & ) = default;

ResidueSelectorOP ResidueSpanSelector::clone() const { return utility::pointer::make_shared< ResidueSpanSelector >(*this); }


ResidueSpanSelector::ResidueSpanSelector(
	core::pose::ResidueIndexDescriptionCOP start,
	core::pose::ResidueIndexDescriptionCOP end
):
	start_( start ),
	end_( end)
{}

ResidueSpanSelector::ResidueSpanSelector( std::string const & start_str, std::string const & end_str ):
	start_( core::pose::parse_resnum( start_str ) ),
	end_( core::pose::parse_resnum( end_str ) )
{}

ResidueSpanSelector::ResidueSpanSelector( core::Size start, core::Size end ) :
	start_( core::pose::make_rid_posenum(start) ),
	end_( core::pose::make_rid_posenum(end) )
{}

ResidueSpanSelector::~ResidueSpanSelector() = default;

ResidueSubset
ResidueSpanSelector::apply( core::pose::Pose const & pose ) const
{
	debug_assert( start_ != nullptr );
	debug_assert( end_ != nullptr );

	// For backward-compatability reasons in parse_movemap_factory_legacy,
	// we need to be robust for start/end ranges which are outside the normal pose size.

	core::Size start = start_->resolve_index( pose, true ); // Don't error out for bad designations
	core::Size end = end_->resolve_index( pose, true );

	if ( start > pose.size() ) {
		TR.Warning << "Residue span start designation '" << *start_ << "' is outside the Pose!" << std::endl;
		start = pose.size();
	}
	if ( end > pose.size() ) {
		TR.Warning << "Residue span end designation '" << *end_ << "' is outside the Pose!" << std::endl;
		end = pose.size();
	}

	if ( start == 0 ) {
		// Hard error so we don't inadvertantly expand the range.
		TR.Error << "Residue span start designation '" << *start_ << "' can't be interpreted!" << std::endl;
		utility_exit_with_message("Error reading start designation in Residue span.");
	}
	if ( end == 0 ) {
		// Hard error so we don't inadvertantly expand the range.
		TR.Error << "Residue span start designation '" << *end_ << "' can't be interpreted!" << std::endl;
		utility_exit_with_message("Error reading end designation in Residue span.");
	}

	ResidueSubset subset( pose.size(), false );

	for ( core::Size res(1); res <= subset.size(); ++res ) {
		if ( (start <= res && res <= end ) ||
				(end <= res && res <= start ) ) { // Start/end might be accidentally flipped.
			subset[ res ] = true;
		}
	}

	return subset;
}

void
ResidueSpanSelector::parse_my_tag(
	utility::tag::TagCOP tag,
	basic::datacache::DataMap &)
{
	set_span( tag->getOption< std::string >( "start" ), tag->getOption< std::string >( "end" ) );
}

void
ResidueSpanSelector::set_span( std::string const & start, std::string const & end) {
	start_ = core::pose::parse_resnum( start );
	end_ = core::pose::parse_resnum( end );
}

std::string ResidueSpanSelector::get_name() const {
	return ResidueSpanSelector::class_name();
}


std::string ResidueSpanSelector::class_name() {
	return "Span";
}

void
ResidueSpanSelector::provide_xml_schema( utility::tag::XMLSchemaDefinition & xsd ) {
	using namespace utility::tag;
	AttributeList attributes;
	core::pose::attributes_for_parse_resnum( attributes, "start", "The starting residue of the span." );
	core::pose::attributes_for_parse_resnum( attributes, "end", "The ending residue of the span." );
	xsd_type_definition_w_attributes(
		xsd, class_name(),
		"The ResidueSpanSelector sets the positions corresponding to the residues between the start and end points to true, and all other positions to false. Note that it does not support PDB insertion codes.",
		attributes );
}


ResidueSelectorOP
ResidueSpanSelectorCreator::create_residue_selector() const {
	return utility::pointer::make_shared< ResidueSpanSelector >();
}

std::string
ResidueSpanSelectorCreator::keyname() const {
	return ResidueSpanSelector::class_name();
}

void
ResidueSpanSelectorCreator::provide_xml_schema( utility::tag::XMLSchemaDefinition & xsd ) const {
	ResidueSpanSelector::provide_xml_schema( xsd );
}

} //namespace residue_selector
} //namespace select
} //namespace core

#ifdef    SERIALIZATION

/// @brief Automatically generated serialization method
template< class Archive >
void
core::select::residue_selector::ResidueSpanSelector::save( Archive & arc ) const {
	arc( cereal::base_class< core::select::residue_selector::ResidueSelector >( this ) );
	arc( CEREAL_NVP( start_ ) ); // core::pose::ResidueIndexDescriptionCOP
	arc( CEREAL_NVP( end_ ) ); // core::pose::ResidueIndexDescriptionCOP
}

/// @brief Automatically generated deserialization method
template< class Archive >
void
core::select::residue_selector::ResidueSpanSelector::load( Archive & arc ) {
	arc( cereal::base_class< core::select::residue_selector::ResidueSelector >( this ) );
	core::pose::ResidueIndexDescriptionOP start, end;
	arc( start ); // core::pose::ResidueIndexDescriptionCOP
	start_ = start;
	arc( end ); // core::pose::ResidueIndexDescriptionCOP
	end_ = end;
}

SAVE_AND_LOAD_SERIALIZABLE( core::select::residue_selector::ResidueSpanSelector );
CEREAL_REGISTER_TYPE( core::select::residue_selector::ResidueSpanSelector )

CEREAL_REGISTER_DYNAMIC_INIT( core_pack_task_residue_selector_ResidueSpanSelector )
#endif // SERIALIZATION
