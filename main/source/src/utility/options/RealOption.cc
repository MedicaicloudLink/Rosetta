// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW CoMotion, email: license@uw.edu.

/// @file   utility/options/RealOption.cc
/// @brief  Auto-generated serialization template functions
/// @author Andrew Leaver-Fay (aleavefay@gmail.com)

// Unit headers
#include <utility/options/RealOption.hh>


#ifdef    SERIALIZATION
// Utility serialization headers
#include <utility/serialization/serialization.hh>

// Cereal headers
#include <cereal/types/polymorphic.hpp>

/// @brief Automatically generated serialization method
template< class Archive >
void
utility::options::RealOption::save( Archive & arc ) const {
	arc( cereal::base_class< utility::options::ScalarOption_T_<class utility::options::RealOptionKey, double> >( this ) );
}

/// @brief Automatically generated deserialization method
template< class Archive >
void
utility::options::RealOption::load( Archive & arc ) {
	arc( cereal::base_class< utility::options::ScalarOption_T_<class utility::options::RealOptionKey, double> >( this ) );
}

SAVE_AND_LOAD_SERIALIZABLE( utility::options::RealOption );
CEREAL_REGISTER_TYPE( utility::options::RealOption )

CEREAL_REGISTER_DYNAMIC_INIT( utility_options_RealOption )

#endif // SERIALIZATION
