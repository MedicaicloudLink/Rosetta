// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file   core/conformation/parametric/SizeValuedParameter.fwd.hh
/// @brief  Owning pointers and whatnot for the class for holding a core::Size-valued parameter for parametric backbone generation.
/// @author Vikram K. Mulligan (vmullig@uw.edu)


#ifndef INCLUDED_core_conformation_parametric_SizeValuedParameter_fwd_hh
#define INCLUDED_core_conformation_parametric_SizeValuedParameter_fwd_hh

// Utility headers
#include <utility/vector1.fwd.hh>
#include <utility/pointer/access_ptr.fwd.hh>
#include <utility/pointer/owning_ptr.fwd.hh>

// C++ headers

namespace core {
namespace conformation {
namespace parametric {

class SizeValuedParameter;

typedef  utility::pointer::weak_ptr< SizeValuedParameter >  SizeValuedParameterAP;
typedef  utility::pointer::weak_ptr< SizeValuedParameter const >  SizeValuedParameterCAP;
typedef  utility::pointer::shared_ptr< SizeValuedParameter >  SizeValuedParameterOP;
typedef  utility::pointer::shared_ptr< SizeValuedParameter const >  SizeValuedParameterCOP;

typedef  utility::vector1< SizeValuedParameterOP >  SizeValuedParameterOPs;
typedef  utility::vector1< SizeValuedParameterCOP >  SizeValuedParameterCOPs;
typedef  utility::vector1< SizeValuedParameterCAP >  SizeValuedParameterCAPs;

} // namespace parametric
} // namespace conformation
} // namespace core

#endif // INCLUDED_core_conformation_parametric_SizeValuedParameter_fwd_hh