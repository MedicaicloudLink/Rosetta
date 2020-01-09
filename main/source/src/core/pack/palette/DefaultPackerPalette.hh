// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file   core/pack/palette/DefaultPackerPalette.hh
/// @brief  DefaultPackerPalette: a PackerPalette that just sets up
/// default Packer behaviour (design with the canonical 20 amino acids and whatever
/// is present at a position in a pose).  This PackerPalette has no user-configurable
/// options.\nThis was implemented as part of the 2016 Chemical XRW (eXtreme Rosetta
/// Workshop).
// @author Vikram K. Mulligan, Baker laboratory (vmullig@uw.edu).

#ifndef INCLUDED_core_pack_palette_DefaultPackerPalette_hh
#define INCLUDED_core_pack_palette_DefaultPackerPalette_hh

// Unit Headers
#include <core/pack/palette/DefaultPackerPalette.fwd.hh>

// Package Headers

// Project Headers
#include <core/chemical/ResidueTypeSet.fwd.hh>
#include <core/pack/palette/PackerPalette.hh>
#include <core/pose/Pose.fwd.hh>
#include <basic/datacache/DataMap.fwd.hh>

// Utility Headers
#include <utility/tag/Tag.fwd.hh>
#include <utility/pointer/ReferenceCount.hh>
#include <utility/vector1.fwd.hh>

// STL Headers
#include <utility/vector1.hh>


namespace core {
namespace pack {
namespace palette {

/// @brief  The DefaultPackerPalette class gives instructions to the packer about
/// the set of ResidueTypes and VariantTypes to use by default, in the
/// absence of any TaskOperations that prune the list.  Specifically, the DefaultPackerPalette
/// says, "By default, use the twenty canonical amino acids and whatever is present at a given position -- and
/// nothing else."
class DefaultPackerPalette : public PackerPalette
{

	/// @brief The parent class (PackerPalette).
	///
	typedef PackerPalette parent;

public:
	/// @brief Constructor with ResidueTypeSet.
	///
	DefaultPackerPalette( core::chemical::ResidueTypeSetCOP restypeset);

	/// @brief Default constructor.
	///
	DefaultPackerPalette();

	/// @brief Copy constructor.
	///
	DefaultPackerPalette( DefaultPackerPalette const &src );

	/// @brief Destructor.
	///
	~DefaultPackerPalette();

	/// @brief Clone operator: make a copy and return an owning pointer to the copy.
	/// @details Derived classes MUST implement this.
	virtual PackerPaletteOP clone() const override;

	/// @brief Function to parse XML tags, implemented by derived classes.
	/// @brief Failure to implement this results in a warning message, but does not prevent compilation or
	/// program execution.
	virtual void
	parse_my_tag(
		utility::tag::TagCOP const &tag,
		basic::datacache::DataMap const &datamap
	) override;

	/// @brief Provide information about the XML options available for this PackerPalette.
	static void provide_xml_schema( utility::tag::XMLSchemaDefinition & xsd );

	/// @brief Function to allow a different ResidueTypeSet to be set.
	/// @details Each PackerPalette derived class must implement this.  After setting the new ResidueTypeSet, things need to happen.
	virtual void
	set_residue_type_set( core::chemical::ResidueTypeSetCOP new_type_set ) override;

	/// @brief Get the name of this object ("DefaultPackerPalette").
	std::string const & name() const override;

private: //Private setup functions:

	/// @brief Set up the DefaultPackerPalette with the standard residues.
	///
	void set_up_base_types();

	/// @brief Set up the DefaultPackerPalette with the default set of position-specific behaviours.
	///
	void set_up_behaviours();

private: //Private member variables:



};


} //namespace palette
} //namespace pack
} //namespace core

#endif