// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file   core/select/residue_selector/ResidueIndexSelector.hh
/// @brief  The ResidueIndexSelector selects residues using a string containing pose indices
/// @author Robert Lindner (rlindner@mpimf-heidelberg.mpg.de)

// Unit headers
#include <core/select/residue_selector/ResidueIndexSelector.hh>
#include <core/select/residue_selector/ResidueSelectorCreators.hh>

// Package headers
#include <core/select/residue_selector/util.hh>

// Basic Headers
#include <basic/datacache/DataMap.hh>

// Project headers
#include <core/pose/Pose.hh>
#include <core/pose/selection.hh>
#include <core/conformation/Residue.hh>
#include <core/pose/symmetry/util.hh>
#include <core/conformation/symmetry/SymmetryInfo.hh>

// Utility Headers
#include <utility>
#include <utility/tag/Tag.hh>
#include <utility/tag/XMLSchemaGeneration.hh>

// C++ headers
#include <utility/assert.hh>


#ifdef    SERIALIZATION
// Utility serialization headers
#include <utility/serialization/serialization.hh>
#include <utility/string_util.hh>

// Cereal headers
#include <cereal/types/polymorphic.hpp>
#include <cereal/types/string.hpp>
#endif // SERIALIZATION

static basic::Tracer TR( "core.select.residue_selector.ResidueIndexSelector" );

namespace core {
namespace select {
namespace residue_selector {

ResidueIndexSelector::ResidueIndexSelector():
	index_str_(),
	error_on_out_of_bounds_index_(true),
	reverse_(false)
{}

ResidueIndexSelector::ResidueIndexSelector( std::string const & index_str ):
	index_str_( index_str ),
	error_on_out_of_bounds_index_(true),
	reverse_(false)
{}

/// @brief Convenience constructor for a single residue index
ResidueIndexSelector::ResidueIndexSelector( core::Size index_in ):
	reverse_(false)
{
	append_index( index_in );
}

/// @brief Convenience constructor for a vector of indexes
ResidueIndexSelector::ResidueIndexSelector( utility::vector1< core::Size > const & index_in ):
	reverse_(false)
{
	append_index( index_in );
}


/// @brief Copy constructor
///
ResidueIndexSelector::ResidueIndexSelector( ResidueIndexSelector const & ) = default;

/// @brief Clone operator.
/// @details Copy this object and return an owning pointer to the new object.
ResidueSelectorOP ResidueIndexSelector::clone() const { return utility::pointer::make_shared< ResidueIndexSelector >(*this); }



ResidueIndexSelector::~ResidueIndexSelector() = default;

ResidueSubset
ResidueIndexSelector::apply( core::pose::Pose const & pose ) const
{
	runtime_assert_string_msg( !index_str_.empty(), "A residue index string must be supplied to the ResidueIndexSelector." );
	ResidueSubset subset( pose.size(), false );
	std::set< Size > const res_set( get_resnum_list( index_str_, pose ) );

	//check for symmetry
	bool sym_check = core::pose::symmetry::is_symmetric( pose );
	core::Size asu_size=0;
	if ( sym_check ) {
		if ( reverse_ ) {
			TR << "Pose is symmetric. Using asymmetric pose size for reverse index." << std::endl;
		}
		asu_size = core::pose::symmetry::symmetry_info(pose)->num_independent_residues();
	}

	for ( Size const res : res_set ) {
		if ( res == 0 || res > subset.size() ) {
			if ( error_on_out_of_bounds_index() ) {
				std::stringstream err_msg;
				err_msg << "Residue " << res << " not found in pose!\n";
				throw CREATE_EXCEPTION(utility::excn::Exception,  err_msg.str() );
			} else {
				TR.Warning << "Residue " << res << " was not found in the pose.  Ignoring and continuing." << std::endl;
				continue;
			}
		}
		if ( !reverse_ ) {
			subset[ res ] = true;
		} else {
			if ( sym_check ) {
				//Pose is symmetric, but using asymmetric pose size for reverse index
				subset[asu_size-res+1]=true;
			} else subset[subset.size()-res+1]=true;
		}
	}
	return subset;
}

void
ResidueIndexSelector::parse_my_tag(
	utility::tag::TagCOP tag,
	basic::datacache::DataMap &)
{
	reverse_=tag->getOption<bool>("reverse", false);
	if ( reverse_ ) {
		TR << "WARNING: Reverse = true" << std::endl;
	}

	try {
		set_index( tag->getOption< std::string >( "resnums" ) );
	} catch ( utility::excn::Exception & e ) {
		std::stringstream err_msg;
		err_msg << "Failed to access required option 'resnums' from ResidueIndexSelector::parse_my_tag.\n";
		err_msg << e.msg();
		throw CREATE_EXCEPTION(utility::excn::Exception,  err_msg.str() );
	}
	set_error_on_out_of_bounds_index( tag->getOption< bool >( "error_on_out_of_bounds_index", error_on_out_of_bounds_index() ) );
}

void
ResidueIndexSelector::set_index( std::string const &index_str )
{
	index_str_ = index_str;
}

void
ResidueIndexSelector::set_index( core::Size index ){
	set_index( utility::to_string(index));
}

void
ResidueIndexSelector::set_index_range(core::Size start, core::Size end){
	std::string range = utility::to_string(start)+"-"+utility::to_string(end);
	set_index(range);
}

std::string ResidueIndexSelector::get_name() const {
	return ResidueIndexSelector::class_name();
}

/// @brief Append an additional index (in Rosetta numbering) to the list of indices.
/// @author Vikram K. Mulligan (vmullig@uw.edu)
void
ResidueIndexSelector::append_index(
	core::Size const index_in
) {
	runtime_assert_string_msg( index_in > 0, "Error in core::select::residue_selector::ResidueIndexSelector::append_index(): The index must be greater than zero." );
	std::stringstream this_str;
	this_str << index_in;
	if ( index_str_.empty() ) {
		index_str_ = this_str.str();
	} else {
		index_str_ += "," + this_str.str();
	}
}

/// @brief Append additional indexes (in Rosetta numbering) to the list of indices.
void
ResidueIndexSelector::append_index( utility::vector1< core::Size > const & index_in ) {
	std::stringstream this_str;
	this_str << index_str_;
	bool comma = !index_str_.empty();
	for ( core::Size ii: index_in ) {
		runtime_assert_string_msg( ii > 0, "Error in core::select::residue_selector::ResidueIndexSelector::append_index(): The index must be greater than zero." );
		if ( comma ) {
			this_str << ",";
		}
		this_str << ii;
		comma = true;
	}
	index_str_ = this_str.str();
}


std::string ResidueIndexSelector::class_name() {
	return "Index";
}

void
ResidueIndexSelector::provide_xml_schema( utility::tag::XMLSchemaDefinition & xsd ) {
	using namespace utility::tag;
	AttributeList attributes;
	/* attributes + XMLSchemaAttribute::required_attribute(
	"resnums", xsct_int_cslist,
	"an integer , so that the Pose numbering can be used; "
	"two integers separated by a dash, designating a range of Pose-numbered residues; "
	"an integer followed by a single character,  e.g. 12A, referring to the PDB numbering for residue 12 on chain A; "
	"an integer followed by a single character, followed by a dash, followed by an integer followed by a single character, "
	"e.g. 12A-47A, referring to residues 12 through 47 on chain A in PDB numbering. "
	"(Note, residues that contain insertion codes cannot be properly identified by these PDB numbered schemes).");
	*/
	core::pose::attributes_for_get_resnum_selector( attributes, xsd, "resnums" );
	attributes + utility::tag::XMLSchemaAttribute::attribute_w_default( "error_on_out_of_bounds_index", xsct_rosetta_bool, "If true (the default), this selector throws an error if an index is selected that is not in the pose (e.g. residue 56 of a 55-residue structure).  If false, indices that don't exist in the pose are silently ignored.", "true" )
		+ utility::tag::XMLSchemaAttribute::attribute_w_default( "reverse", xsct_rosetta_bool, "If true(default false), this selector reverses the index. So 1-50  selects the last 50 residues of a protein", "false" );
	xsd_type_definition_w_attributes(
		xsd, class_name(),
		"The ResidueIndexSelector sets the positions corresponding to the residues given in the resnums string to true, and all other positions to false. Note that it does not support PDB insertion codes.",
		attributes );
}


ResidueSelectorOP
ResidueIndexSelectorCreator::create_residue_selector() const {
	return utility::pointer::make_shared< ResidueIndexSelector >();
}

std::string
ResidueIndexSelectorCreator::keyname() const {
	return ResidueIndexSelector::class_name();
}

void
ResidueIndexSelectorCreator::provide_xml_schema( utility::tag::XMLSchemaDefinition & xsd ) const {
	ResidueIndexSelector::provide_xml_schema( xsd );
}

} //namespace residue_selector
} //namespace select
} //namespace core

#ifdef    SERIALIZATION

/// @brief Automatically generated serialization method
template< class Archive >
void
core::select::residue_selector::ResidueIndexSelector::save( Archive & arc ) const {
	arc( cereal::base_class< core::select::residue_selector::ResidueSelector >( this ) );
	arc( CEREAL_NVP( index_str_ ) ); // std::string
	arc( CEREAL_NVP( reverse_ ) );
	arc( CEREAL_NVP( error_on_out_of_bounds_index_ ) ); //bool
}

/// @brief Automatically generated deserialization method
template< class Archive >
void
core::select::residue_selector::ResidueIndexSelector::load( Archive & arc ) {
	arc( cereal::base_class< core::select::residue_selector::ResidueSelector >( this ) );
	arc( index_str_ ); // std::string
	arc( reverse_ );
	arc( error_on_out_of_bounds_index_ ); //bool
}

SAVE_AND_LOAD_SERIALIZABLE( core::select::residue_selector::ResidueIndexSelector );
CEREAL_REGISTER_TYPE( core::select::residue_selector::ResidueIndexSelector )

CEREAL_REGISTER_DYNAMIC_INIT( core_pack_task_residue_selector_ResidueIndexSelector )
#endif // SERIALIZATION
