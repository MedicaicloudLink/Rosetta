// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file   core/select/residue_selector/SecondaryStructureSelector.cc
/// @brief  The SecondaryStructureSelector selects residues based on secondary structure
/// @author Tom Linsky (tlinsky at uw dot edu)

// Unit headers
#include <core/select/residue_selector/SecondaryStructureSelector.hh>
#include <core/select/residue_selector/ResidueSelectorCreators.hh>

// Package headers
#include <core/select/residue_selector/ResidueRanges.hh>
#include <core/select/residue_selector/ResidueSelectorFactory.hh>
#include <core/select/residue_selector/util.hh>

// Project headers
#include <core/pose/Pose.hh>
#include <core/pose/variant_util.hh>
#include <core/scoring/dssp/Dssp.hh>

// Basic headers
#include <basic/Tracer.hh>

// Utility Headers
#include <utility/tag/Tag.hh>
#include <utility/tag/XMLSchemaGeneration.hh>
#include <utility/string_util.hh>
#include <utility/vector1.hh>

// C++ headers
#include <utility/assert.hh>

static basic::Tracer TR( "core.select.residue_selector.SecondaryStructureSelector" );

#ifdef    SERIALIZATION
// Utility serialization headers
#include <utility/serialization/serialization.hh>

// Cereal headers
#include <cereal/types/polymorphic.hpp>
#include <cereal/types/set.hpp>
#include <cereal/types/string.hpp>
#endif // SERIALIZATION

namespace core {
namespace select {
namespace residue_selector {

SecondaryStructureSelector::SecondaryStructureSelector() :
	ResidueSelector(),
	pose_secstruct_( "" ),
	overlap_( 0 ),
	minH_( 1 ),
	minE_( 1 ),
	include_terminal_loops_( false ),
	use_dssp_( true ),
	selected_ss_()
{
	selected_ss_.clear();
}

/// @brief Clone operator.
/// @details Copy this object and return an owning pointer to the new object.
ResidueSelectorOP SecondaryStructureSelector::clone() const { return utility::pointer::make_shared< SecondaryStructureSelector >(*this); }

SecondaryStructureSelector::SecondaryStructureSelector( std::string const & selected ) :
	ResidueSelector(),
	pose_secstruct_( "" ),
	overlap_( 0 ),
	minH_( 1 ),
	minE_( 1 ),
	include_terminal_loops_( false ),
	use_dssp_( true ),
	selected_ss_()
{
	if ( !check_ss( selected ) ) {
		throw CREATE_EXCEPTION(utility::excn::BadInput,  "SecondaryStructureSelector: invalid ss to select: " + selected + " -- only H, E and L are allowed.\n" );
	}
	set_selected_ss( selected );
}

SecondaryStructureSelector::~SecondaryStructureSelector() = default;

/// @brief Override pose secondary structure. The secondary structure set by this
///        method will always be used if it is non-empty.
void
SecondaryStructureSelector::set_pose_secstruct( std::string const & ss )
{
	pose_secstruct_ = ss;
}

/// @brief gets the secondary structure to be used by the selector
/// @param[in] pose  Input pose
/// @details Determines secondary structure by the following rules:
///          1. If pose_secstruct_ is user-specified, use that
///          2. If use_dssp_ is true, run DSSP and use DSSP secstruct
///          3. If use_dssp_ is false, return pose.secstruct()
std::string
SecondaryStructureSelector::get_secstruct( core::pose::Pose const & pose ) const
{
	if ( !pose_secstruct_.empty() ) {
		TR << "Using given pose secondary structure: " << pose_secstruct_ << std::endl;
		return pose_secstruct_;
	}

	if ( use_dssp_ ) {
		core::scoring::dssp::Dssp dssp( pose );
		TR << "Using dssp for secondary structure: " << dssp.get_dssp_secstruct() << std::endl;
		return dssp.get_dssp_secstruct();
	}

	TR << "Using secondary structure in pose: " << pose.secstruct() << std::endl;
	return pose.secstruct();
}

ResidueSubset
SecondaryStructureSelector::apply( core::pose::Pose const & pose ) const
{
	// selected secondary structure can't be empty
	if ( selected_ss_.empty() ) {
		std::stringstream err;
		err << "SecondaryStructureSelector: no secondary structure types are selected -- you must specify one or more secondary structure types for selection." << std::endl;
		throw CREATE_EXCEPTION(utility::excn::BadInput,  err.str() );
	}

	// first check pose secstruct, otherwise use dssp
	std::string ss = get_secstruct( pose );
	if ( !check_ss( ss ) ) {
		std::stringstream err;
		err << "SecondaryStructureSelector: the secondary structure string " << ss << " is invalid." << std::endl;
		throw CREATE_EXCEPTION(utility::excn::BadInput,  err.str() );
	}

	fix_secstruct_definition( ss );

	ResidueSubset matching_ss( pose.size(), false );
	for ( core::Size i=1; i<=pose.size(); ++i ) {
		if ( selected_ss_.find( ss[ i - 1 ] ) != selected_ss_.end() ) {
			TR.Debug << "Found ss match at position " << i << std::endl;
			matching_ss[ i ] = true;
		}
	}

	add_overlap( matching_ss, pose, ss );

	return matching_ss;
}

void SecondaryStructureSelector::parse_my_tag(
	utility::tag::TagCOP tag,
	basic::datacache::DataMap & )
{
	set_overlap( tag->getOption< core::Size >( "overlap", overlap_ ) );
	set_minH( tag->getOption< core::Size >( "minH", minH_ ) );
	set_minE( tag->getOption< core::Size >( "minE", minE_ ) );
	set_include_terminal_loops( tag->getOption< bool >( "include_terminal_loops", include_terminal_loops_ ) );
	set_use_dssp( tag->getOption< bool >( "use_dssp", use_dssp_ ) );
	set_pose_secstruct( tag->getOption< std::string >( "pose_secstruct", pose_secstruct_ ) );

	/*
	if ( tag->hasOption( "blueprint" ) ) {
	if ( tag->hasOption( "pose_secstruct" ) ) {
	std::stringstream msg;
	msg << "SecondaryStructureSelector::parse_my_tag(): You cannot specify a blueprint and a pose secondary structure to the same selector."
	<< std::endl;
	throw CREATE_EXCEPTION(utility::excn::RosettaScriptsOptionError,  err.str() );
	}
	std::string const bp_name = tag->getOption< std::string >( "blueprint" );
	protocols::parser::BluePrint bp( bp_name );
	set_pose_secstruct( bp.secstruct() );
	}
	*/

	if ( !tag->hasOption( "ss" ) ) {
		std::stringstream err;
		err << "SecondaryStructureSelector: the ss option is required to specify which secondary structure elements are being selected." << std::endl;
		err << "Usage: ss=\"([HEL])*\" (e.g. ss=\"L\" for loops only, ss=\"HE\" for helices and strands)" << std::endl;
		throw CREATE_EXCEPTION(utility::excn::RosettaScriptsOptionError,  err.str() );
	}
	std::string const selected = tag->getOption< std::string >( "ss" );

	if ( !check_ss( selected ) ) {
		std::stringstream err;
		err << "SecondaryStructureSelector: an invalid character was specified in the ss option -- only H, E and L are allowed: " << selected << std::endl;
		throw CREATE_EXCEPTION(utility::excn::RosettaScriptsOptionError,  err.str() );
	}

	set_selected_ss( selected );
}

std::string
SecondaryStructureSelector::get_name() const
{
	return SecondaryStructureSelector::class_name();
}

std::string
SecondaryStructureSelector::class_name()
{
	return "SecondaryStructure";
}

void
SecondaryStructureSelector::provide_xml_schema( utility::tag::XMLSchemaDefinition & xsd ) {
	using namespace utility::tag;
	AttributeList attributes;
	attributes
		+ XMLSchemaAttribute::attribute_w_default( "overlap", xsct_non_negative_integer ,
		"If specified, the ranges of residues with matching secondary structure are expanded "
		"by this many residues. Each selected range of residues will not be expanded past chain "
		"endings. For example, a pose with secondary structure LHHHHHHHLLLEEEEELLEEEEEL, ss='E', "
		"and overlap 0 would select the two strand residue ranges EEEEE and EEEEE. With overlap 2, "
		"the selected residues would also include up to two residues before and after the strands "
		"(LLEEEEELLEEEEEL).", "0" )
		+ XMLSchemaAttribute::attribute_w_default( "minH", xsct_non_negative_integer ,
		"Defines the minimal number of consecutive residues with helix assignation to be considered "
		"an helix. Smaller assignation patches are understood as loops.", "1" )
		+ XMLSchemaAttribute::attribute_w_default( "minE", xsct_non_negative_integer ,
		"Defines the minimal number of consecutive residues with beta assignation to be considered "
		"a beta. Smaller assignation patches are understood as loops.", "1" )
		+ XMLSchemaAttribute::attribute_w_default( "include_terminal_loops", xsct_rosetta_bool ,
		"If false, one-residue \"loop\" regions at the termini of chains will be "
		"ignored. If true, all residues will be considered for selection.", "false" )
		+ XMLSchemaAttribute::attribute_w_default( "use_dssp", xsct_rosetta_bool,
		"f true, dssp will be used to determine the pose secondary structure every "
		"time the SecondaryStructure residue selector is applied. If false, and a "
		"secondary structure is set in the pose, the secondary structure in the pose "
		"will be used without re-computing DSSP. This option has no effect if pose_secstruct "
		"is set.", "true" )
		+ XMLSchemaAttribute( "pose_secstruct", xs_string , " If set, the given secondary "
		"structure string will be used instead of the pose secondary structure or DSSP. The given "
		"secondary structure must match the length of the pose." )
		+ XMLSchemaAttribute::required_attribute( "ss", xs_string ,
		"The secondary structure types to be selected. This parameter is required. "
		"Valid secondary structure characters are 'E', 'H' and 'L'. To select loops, "
		"for example, use ss=\"L\", and to select both helices and sheets, use ss=\"HE\"" );
	xsd_type_definition_w_attributes( xsd, class_name(),
		"SecondaryStructureSelector selects all residues with given secondary structure. "
		"For example, you might use it to select all loop residues in a pose. "
		"SecondaryStructureSelector uses the following rules to determine the pose secondary "
		"structure: 1. If pose_secstruct is specified, it is used. 2. If use_dssp is true, "
		"DSSP is used on the input pose to determine its secondary structure. 3. If use_dssp "
		"is false, the secondary structure stored in the pose is used.", attributes );
}

bool
SecondaryStructureSelector::check_ss( std::string const & ss ) const
{
	for ( char s : ss ) {
		if ( s == 'L' ) {
			continue;
		}
		if ( s == 'E' ) {
			continue;
		}
		if ( s == 'H' ) {
			continue;
		}
		return false;
	}
	return true;
}

/// @brief fixes the secondary structure according to the expected minimal length of each secondary structure.
/// @param[in] ss string with the secondary structure definition
void
SecondaryStructureSelector::fix_secstruct_definition( std::string & ss ) const
{
	if ( minH_ == 1 and minE_ == 1 ) return;

	utility::vector1< std::pair< core::Size, core::Size > > Hcontent;
	utility::vector1< std::pair< core::Size, core::Size > > Econtent;
	char last = 'L';
	core::Size i = 0;
	for ( std::string::const_iterator c = ss.begin(); c != ss.end(); ++c ) {
		if ( *c == 'E' or *c == 'H' ) {
			if ( *c != last ) {
				std::pair< core::Size, core::Size > newpair( i, 1 );
				if ( *c == 'E' ) Econtent.push_back( newpair );
				if ( *c == 'H' ) Hcontent.push_back( newpair );
			} else {
				if ( *c == 'E' ) ++Econtent.back().second;
				if ( *c == 'H' ) ++Hcontent.back().second;
			}
		}
		last = *c;
		++i;
	}
	utility::vector1< std::pair< core::Size, core::Size > > todel;
	for ( auto const & combo : Hcontent ) {
		if ( combo.second < minH_ ) todel.push_back( combo );
	}
	for ( auto const & combo : Econtent ) {
		if ( combo.second < minE_ ) todel.push_back( combo );
	}
	TR << ss << std::endl;
	TR << todel << std::endl;
	for ( auto const & combo : todel ) {
		for ( core::Size ii = 0; ii < combo.second; ++ii ) {
			ss[ combo.first + ii ] = 'L';
		}
	}
	TR << ss << std::endl;
}

void
SecondaryStructureSelector::add_overlap(
	ResidueSubset & matching_ss,
	pose::Pose const & pose,
	std::string const & ss ) const
{
	ResidueRanges intervals( matching_ss );
	for ( auto & range : intervals ) {
		auto count = Size( 0 );
		while ( (count < overlap_) && !pose::pose_residue_is_terminal( pose, range.start() ) ) {
			range.set_start( range.start() - 1 );
			matching_ss[ range.start() ] = true;
			count += 1;
		}
		count = Size( 0 );
		while ( (count < overlap_) && !pose::pose_residue_is_terminal( pose, range.stop() ) ) {
			range.set_stop( range.stop() + 1 );
			matching_ss[ range.stop() ] = true;
			count += 1;
		}
		TR.Debug << "Interval: " << range.start() << " " << range.stop() << " " << ss[ range.start() - 1 ] << std::endl;

		// get rid of one-residue terminal "loops"
		if ( include_terminal_loops_ ) {
			continue;
		}

		if ( ( range.start() == range.stop() ) && ( ss[ range.start() - 1 ] == 'L' )
				&& ( pose::pose_residue_is_terminal( pose, range.start() ) ) ) {
			matching_ss[ range.start() ] = false;
		}
		if ( ( range.start() + 1 == range.stop() ) && ( ss[ range.start() - 1 ] == 'L' )
				&& ( pose::pose_residue_is_terminal( pose, range.start() ) ) ) {
			matching_ss[ range.start() ] = false;
		}
		if ( ( range.start() + 1 == range.stop() ) && ( ss[ range.stop() - 1 ] == 'L' )
				&& ( pose::pose_residue_is_terminal( pose, range.stop() ) ) ) {
			matching_ss[ range.stop() ] = false;
		}
	}
}


void
set_pose_secstruct( std::string const & ss );

/// @brief If set, dssp will be used to determine secondary structure. Has no effect if pose_secstruct_
///        is set
/// @details Determines secondary structure by the following rules:
///          1. If pose_secstruct_ is user-specified, use that
///          2. If use_dssp_ is true, run DSSP and use DSSP secstruct
///          3. If use_dssp_ is false, return pose.secstruct()
void
SecondaryStructureSelector::set_use_dssp( bool const use_dssp )
{
	use_dssp_ = use_dssp;
}

void
SecondaryStructureSelector::set_overlap( core::Size const overlapval )
{
	overlap_ = overlapval;
}
void
SecondaryStructureSelector::set_minH( core::Size const minHval )
{
	minH_ = minHval;
}

void
SecondaryStructureSelector::set_minE( core::Size const minEval )
{
	minE_ = minEval;
}

void
SecondaryStructureSelector::set_selected_ss( std::string const & selected )
{
	selected_ss_.clear();
	selected_ss_ = std::set< char >( selected.begin(), selected.end() );
}

/// @brief if true, one-residue terminal "loops" will be included (default=false)
void
SecondaryStructureSelector::set_include_terminal_loops( bool const inc_term )
{
	include_terminal_loops_ = inc_term;
}

ResidueSelectorOP
SecondaryStructureSelectorCreator::create_residue_selector() const
{
	return utility::pointer::make_shared< SecondaryStructureSelector >();
}

std::string
SecondaryStructureSelectorCreator::keyname() const
{
	return SecondaryStructureSelector::class_name();
}

void
SecondaryStructureSelectorCreator::provide_xml_schema( utility::tag::XMLSchemaDefinition & xsd ) const {
	SecondaryStructureSelector::provide_xml_schema( xsd );
}

} //namespace residue_selector
} //namespace select
} //namespace core


#ifdef    SERIALIZATION

/// @brief Automatically generated serialization method
template< class Archive >
void
core::select::residue_selector::SecondaryStructureSelector::save( Archive & arc ) const {
	arc( cereal::base_class< core::select::residue_selector::ResidueSelector >( this ) );
	arc( CEREAL_NVP( pose_secstruct_ ) ); // std::string
	arc( CEREAL_NVP( overlap_ ) ); // core::Size
	arc( CEREAL_NVP( minH_ ) ); // core::Size
	arc( CEREAL_NVP( minE_ ) ); // core::Size
	arc( CEREAL_NVP( include_terminal_loops_ ) ); // _Bool
	arc( CEREAL_NVP( use_dssp_ ) ); // _Bool
	arc( CEREAL_NVP( selected_ss_ ) ); // std::set<char>
}

/// @brief Automatically generated deserialization method
template< class Archive >
void
core::select::residue_selector::SecondaryStructureSelector::load( Archive & arc ) {
	arc( cereal::base_class< core::select::residue_selector::ResidueSelector >( this ) );
	arc( pose_secstruct_ ); // std::string
	arc( overlap_ ); // core::Size
	arc( minH_ ); // core::Size
	arc( minE_ ); // core::Size
	arc( include_terminal_loops_ ); // _Bool
	arc( use_dssp_ ); // _Bool
	arc( selected_ss_ ); // std::set<char>
}

SAVE_AND_LOAD_SERIALIZABLE( core::select::residue_selector::SecondaryStructureSelector );
CEREAL_REGISTER_TYPE( core::select::residue_selector::SecondaryStructureSelector )

CEREAL_REGISTER_DYNAMIC_INIT( core_pack_task_residue_selector_SecondaryStructureSelector )
#endif // SERIALIZATION
