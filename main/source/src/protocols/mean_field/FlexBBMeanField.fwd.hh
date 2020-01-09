// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is protocolsoped by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file    FlexBBMeanField.hh

/// @brief   Forward declarations for FlexBBMeanField.
/// @author  arubenstein

#ifndef INCLUDED_protocols_mean_field_FlexBBMeanField_FWD_HH
#define INCLUDED_protocols_mean_field_FlexBBMeanField_FWD_HH

// Utility header
#include <utility/pointer/owning_ptr.hh>

namespace protocols {
namespace mean_field {

/// @brief calculator which conducts mean-field algorithm calculations on many, non-designable poses
class FlexBBMeanField;

typedef utility::pointer::shared_ptr<FlexBBMeanField> FlexBBMeanFieldOP;
typedef utility::pointer::shared_ptr<FlexBBMeanField const> FlexBBMeanFieldCOP;

}  // namespace mean_field
}  // namespace protocols

#endif  // INCLUDED_protocols_mean_field_FlexBBMeanField_FWD_HH
