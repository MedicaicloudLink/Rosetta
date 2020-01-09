// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file   core/fragment/picking_old/vall/gen/LengthGen.cc
/// @brief  default constant length fragment VallExtentGenerator
/// @author Yih-En Andrew Ban (yab@u.washington.edu)

// unit headers
#include <core/fragment/picking_old/vall/gen/LengthGen.hh>

#include <utility/vector1.hh>


namespace core {
namespace fragment {
namespace picking_old {
namespace vall {
namespace gen {


/// @brief default constructor
LengthGen::LengthGen() :
	Super()
{}


/// @brief fragment size constructor
/// @param[in] frag_size the desired length of the fragment
LengthGen::LengthGen( Size const frag_size ) :
	Super(),
	frag_size_( frag_size )
{}


/// @brief copy constructor
LengthGen::LengthGen( LengthGen const & /*rval*/ ) = default;


/// @brief default destructor
LengthGen::~LengthGen() = default;


/// @brief copy assignment
LengthGen & LengthGen::operator =( LengthGen const & rval ) {
	if ( this != &rval ) {
		Super::operator =( rval );
		frag_size_ = rval.frag_size_;
	}
	return *this;
}


/// @brief clone this object
VallFragmentGenOP LengthGen::clone() const {
	return utility::pointer::make_shared< LengthGen >( *this );
}


/// @brief return the desired fragment extent w/requested fragment size
/// @return valid (true) extent if the end of the extent does not go past the
///  section_end, invalid (false) extent otherwise
/// @remarks we assume VallResidueIterator is a type of RandomAccessIterator, such as
///  those used in std::vector
LengthGen::Extent LengthGen::operator ()( VallResidueIterator extent_begin, VallResidueIterator section_end ) const {
	Extent extent;
	extent.begin = extent_begin;

	// OK, we can't even REFER to this iterator if it goes beyond section_end
	if ( frag_size_ > static_cast< core::Size >( section_end - extent_begin ) ) {
		extent.valid = false;
		extent.end = section_end;
	} else {
		extent.end = extent_begin + frag_size_;
		extent.valid = ( extent.end <= section_end );
	}

	return extent;
}


} // namespace gen
} // namespace vall
} // namespace picking_old
} // namespace fragment
} // namespace core
