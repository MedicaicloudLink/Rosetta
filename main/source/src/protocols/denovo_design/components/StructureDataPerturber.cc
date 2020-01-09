// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file  protocols/denovo_design/components/StructureDataPerturber.cc
/// @brief Classes for altering StructureData objects on the fly
/// @author Tom Linsky (tlinsky@uw.edu)

// Unit header or inline function header
#include <protocols/denovo_design/components/StructureDataPerturber.hh>

// Protocol headers
#include <protocols/denovo_design/architects/HelixArchitect.hh>
#include <protocols/denovo_design/components/Segment.hh>
#include <protocols/denovo_design/components/StructureData.hh>
#include <protocols/denovo_design/components/VectorSelector.hh>
#include <protocols/denovo_design/connection/ConnectionArchitect.hh>
#include <protocols/denovo_design/util.hh>

// Basic headers
#include <basic/Tracer.hh>
#include <basic/datacache/DataMap.hh>
#include <numeric/random/random.hh>
#include <numeric/random/random_permutation.hh>
#include <utility/tag/Tag.hh>

// Boost headers
#include <boost/assign.hpp>

static basic::Tracer TR( "protocols.denovo_design.components.StructureDataPerturber" );

namespace protocols {
namespace denovo_design {
namespace components {

StructureDataPerturber::StructureDataPerturber():
	ignore_()
{
}

StructureDataPerturber::~StructureDataPerturber() = default;

void
StructureDataPerturber::set_ignore_segments( SegmentNameSet const & ignore )
{
	ignore_ = ignore;
}

bool
StructureDataPerturber::ignored( SegmentName const & seg_name ) const
{
	return ( ignore_.find( seg_name ) != ignore_.end() );
}

void
StructureDataPerturber::apply( StructureData & sd )
{
	// if we don't have any options waiting in the queue, reset the list
	if ( finished() ) {
		Permutations const all_motifs = enumerate( sd );
		TR << "Computed " << all_motifs.size() << " permutations." << std::endl;
		permutations_.assign( all_motifs.begin(), all_motifs.end() );
		numeric::random::random_permutation( permutations_, numeric::random::rg() );
	}

	// if there aren't any options, don't do anything
	if ( permutations_.empty() ) {
		TR << "No options to choose from for CompoundPerturber! "
			<< " -- not doing anything" << std::endl;
		return;
	}

	replace_segments( sd, permutations_.select() );
}

/// @brief creates a structuredata perturber from a tag
StructureDataPerturberOP
StructureDataPerturber::create( utility::tag::Tag const & tag, basic::datacache::DataMap & data )
{
	StructureDataPerturberOP perturber;
	if ( tag.getName() == ConnectionPerturber::class_name() ) {
		perturber = utility::pointer::make_shared< ConnectionPerturber >();
		perturber->parse_my_tag( tag, data );
	} else if ( tag.getName() == HelixPerturber::class_name() ) {
		perturber = utility::pointer::make_shared< HelixPerturber >();
		perturber->parse_my_tag( tag, data );
	} else if ( tag.getName() == CompoundPerturber::class_name() ) {
		perturber = utility::pointer::make_shared< CompoundPerturber >();
		perturber->parse_my_tag( tag, data );
	} else {
		std::stringstream msg;
		msg << "StructureDataPerturber::create(): Invalid perturber name " << tag.getName() << std::endl
			<< "Valid names are: "
			<< ConnectionPerturber::class_name() << ", "
			<< HelixPerturber::class_name() << ", "
			<< CompoundPerturber::class_name() << std::endl;
		utility_exit_with_message( msg.str() );
	}
	return perturber;
}

bool
StructureDataPerturber::finished() const
{
	return permutations_.empty();
}

void
StructureDataPerturber::replace_segments( StructureData & sd, Permutation const & perm ) const
{
	for ( auto const & s : perm ) {
		std::string const & segname = s->id();
		TR.Debug << "Looking at segment " << segname << std::endl;

		// Don't change this -- ensures that all Perturbers are working correctly
		if ( ignored( segname ) ) {
			utility_exit_with_message( "Segment " + segname + " is ignored, but was selected somehow.\n" );
		}
		if ( !sd.has_segment( segname ) ) {
			utility_exit_with_message( "Segment " + segname + " is not present in the StructureData, but was selected somehow.\n" );
		}

		if ( ignored( segname ) ) continue;
		if ( !sd.has_segment( segname ) ) continue;

		Segment const & cur_seg = sd.segment( segname );
		Segment newseg = *s;

		if ( cur_seg.lower_padding() == 0 ) newseg.delete_lower_padding();
		if ( cur_seg.upper_padding() == 0 ) newseg.delete_upper_padding();
		newseg.set_movable_group( cur_seg.movable_group() );
		newseg.set_lower_segment( cur_seg.lower_segment() );
		newseg.set_upper_segment( cur_seg.upper_segment() );
		TR << "Segment: " << segname << " SS: " << newseg.ss()
			<< " ABEGO: " << newseg.abego() << std::endl;
		TR.Debug << "Replacing " << cur_seg << " with " << newseg << std::endl;
		sd.replace_segment( segname, newseg );
	}
}

////////////////////////////////////////////////////////////////////////////
/// NullPerturber
////////////////////////////////////////////////////////////////////////////

/// @brief Default constructor
NullPerturber::NullPerturber():
	StructureDataPerturber()
{}

/// @brief Destructor
NullPerturber::~NullPerturber() = default;

StructureDataPerturberOP
NullPerturber::clone() const
{
	return utility::pointer::make_shared< NullPerturber >( *this );
}

StructureDataPerturber::Permutations
NullPerturber::enumerate( StructureData const & ) const
{
	return Permutations();
}

void
NullPerturber::parse_my_tag( utility::tag::Tag const & , basic::datacache::DataMap & )
{}

////////////////////////////////////////////////////////////////////////////
/// ConnectionPerturber
////////////////////////////////////////////////////////////////////////////

ConnectionPerturber::ConnectionPerturber():
	architect_()
{}

ConnectionPerturber::~ConnectionPerturber() = default;

StructureDataPerturberOP
ConnectionPerturber::clone() const
{
	return utility::pointer::make_shared< ConnectionPerturber >( *this );
}

void
ConnectionPerturber::parse_my_tag( utility::tag::Tag const & tag, basic::datacache::DataMap & data )
{
	std::string const arch_name = tag.getOption< std::string >( "architect" );
	if ( !arch_name.empty() ) retrieve_connection_architect( arch_name, data );
}

StructureDataPerturber::Permutations
ConnectionPerturber::enumerate( StructureData const & sd ) const
{
	if ( !architect_ ) {
		std::stringstream msg;
		msg << class_name() << "::enumerate(): No connection architect is set!" << std::endl;
		utility_exit_with_message( msg.str() );
	}

	Permutations all_motifs;
	if ( ignored( architect_->id() ) ) return all_motifs;
	if ( !sd.has_segment( architect_->id() ) ) return all_motifs;

	// find segment to perturb
	Segment const & target_motif = sd.segment( architect_->id() );

	// delete segment from sd for computation of candidates
	StructureData temp_sd = sd;
	temp_sd.delete_segment( architect_->id() );

	SegmentOPs const all_arch_motifs = architect_->compute_connection_candidates( temp_sd );

	// filter connection candidates with different connection points
	SegmentCOPs motifs;
	for ( auto const & all_arch_motif : all_arch_motifs ) {
		if ( all_arch_motif->lower_segment() != target_motif.lower_segment() ) continue;
		if ( all_arch_motif->upper_segment() != target_motif.upper_segment() ) continue;
		all_motifs.push_back( boost::assign::list_of (all_arch_motif) );
	}
	return all_motifs;
}

void
ConnectionPerturber::retrieve_connection_architect( std::string const & arch_name, basic::datacache::DataMap & data )
{
	connection::ConnectionArchitectCOP connection =
		data.get_ptr< connection::ConnectionArchitect >( "ConnectionArchitects", arch_name );

	architect_ = connection;
}

HelixPerturber::HelixPerturber() = default;

HelixPerturber::~HelixPerturber() = default;

StructureDataPerturberOP
HelixPerturber::clone() const
{
	return utility::pointer::make_shared< HelixPerturber >( *this );
}

void
HelixPerturber::parse_my_tag( utility::tag::Tag const & tag, basic::datacache::DataMap & data )
{
	std::string const arch_name = tag.getOption< std::string >( "architect" );
	retrieve_helix_architect( arch_name, data );
}

StructureDataPerturber::Permutations
HelixPerturber::enumerate( StructureData const & sd ) const
{
	Permutations motifs;
	for ( auto m=architect_->motifs_begin(); m!=architect_->motifs_end(); ++m ) {
		if ( ignored( (*m)->id() ) ) continue;
		if ( !sd.has_segment( (*m)->id() ) ) continue;
		debug_assert( *m );
		motifs.push_back( boost::assign::list_of(*m) );
	}
	return motifs;
}

void
HelixPerturber::retrieve_helix_architect( std::string const & arch_name, basic::datacache::DataMap & data )
{
	architects::DeNovoArchitectCOP architect_base =
		data.get_ptr< architects::DeNovoArchitect >( "DeNovoArchitects", arch_name );

	if ( !architect_base ) {
		std::stringstream msg;
		msg << class_name() << ": No architect named " << arch_name
			<< " was found in the datamap!" << std::endl;
		utility_exit_with_message( msg.str() );
	}

	// use dynamic pointer cast directly so that an appropriate error can be thrown
	// speed isn't super important at parse-time, so I think this should be fine
	// as opposed to a static cast
	HelixArchitectCOP architect = utility::pointer::dynamic_pointer_cast< HelixArchitect const >( architect_base );
	if ( !architect ) {
		std::stringstream msg;
		msg << class_name() << ": The architect named " << arch_name
			<< " was found in the datamap, but is not a HelixArchitect!" << std::endl;
		utility_exit_with_message( msg.str() );
	}

	architect_ = architect;
}

CompoundPerturber::CompoundPerturber():
	StructureDataPerturber(),
	mode_( OR ),
	perturbers_()
{}

/// @brief copy constructor -- necessary because we want to deep copy perturbers_
CompoundPerturber::CompoundPerturber( CompoundPerturber const & other ):
	StructureDataPerturber( other ),
	mode_( other.mode_ ),
	perturbers_()
{
	for ( auto const & perturber : other.perturbers_ ) {
		perturbers_.push_back( perturber->clone() );
	}
}

CompoundPerturber::~CompoundPerturber() = default;

StructureDataPerturberOP
CompoundPerturber::clone() const
{
	return utility::pointer::make_shared< CompoundPerturber >( *this );
}

void
CompoundPerturber::parse_my_tag( utility::tag::Tag const & tag, basic::datacache::DataMap & data )
{
	using utility::tag::Tag;

	std::string const mode = tag.getOption< std::string >( "mode", "AND" );
	if ( mode == "AND" ) mode_ = AND;
	else if ( mode == "OR" ) mode_ = OR;
	else {
		std::stringstream msg;
		msg << class_name() << "::parse_my_tag(): Invalid mode specified ("
			<< mode << ") -- valid modes are [ AND, OR ]" << std::endl;
		throw CREATE_EXCEPTION(utility::excn::RosettaScriptsOptionError,  msg.str() );
	}

	perturbers_.clear();
	for ( auto const & subtag : tag.getTags() ) {
		perturbers_.push_back( StructureDataPerturber::create( *subtag, data ) );
	}
}

StructureDataPerturber::Permutations
CompoundPerturber::enumerate( StructureData const & sd ) const
{
	PermutationsByPerturber all_motifs;
	for ( auto const & perturber : perturbers_ ) {
		all_motifs.push_back( perturber->enumerate( sd ) );
	}
	return all_combinations( all_motifs );
}

StructureDataPerturber::Permutations
CompoundPerturber::all_combinations( PermutationsByPerturber const & all_motifs ) const
{
	Permutations retval;

	if ( all_motifs.empty() ) return retval;

	Permutations const remaining = all_combinations( PermutationsByPerturber( all_motifs.begin()+1, all_motifs.end() ) );

	// work on first perturber in the list
	Permutations const & first = *all_motifs.begin();
	if ( remaining.empty() ) {
		// we are at the lowest level of recursion
		for ( auto const & s : first ) {
			retval.push_back( s );
		}
	} else {
		for ( auto const & p : remaining ) {
			if ( first.empty() ) {
				Permutation new_perm( p.begin(), p.end() );
				retval.push_back( new_perm );
			} else {
				for ( auto new_perm : first ) {
					new_perm.insert( new_perm.end(), p.begin(), p.end() );
					retval.push_back( new_perm );
				}
			}
		}
	}
	return retval;
}

void
CompoundPerturber::set_ignore_segments( SegmentNameSet const & ignore_set )
{
	StructureDataPerturber::set_ignore_segments( ignore_set );
	for ( StructureDataPerturberOPs::const_iterator ptb=perturbers_.begin(); ptb!=perturbers_.end(); ++ptb ) {
		(*ptb)->set_ignore_segments( ignore_set );
	}
}

} //protocols
} //denovo_design
} //components

