// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file
/// @author Phil Bradley

#ifndef INCLUDED_core_chemical_ResidueTypeSelector_fwd_hh
#define INCLUDED_core_chemical_ResidueTypeSelector_fwd_hh

// Utility headers
#include <utility/pointer/owning_ptr.hh>

namespace core {
namespace chemical {

class ResidueTypeSelectorSingle;
typedef utility::pointer::shared_ptr< ResidueTypeSelectorSingle > ResidueTypeSelectorSingleOP;

class ResidueTypeSelector;
typedef utility::pointer::shared_ptr< ResidueTypeSelector > ResidueTypeSelectorOP;

} // chemical
} // core

#endif
