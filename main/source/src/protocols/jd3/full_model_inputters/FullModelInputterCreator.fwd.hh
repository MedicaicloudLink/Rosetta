// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file   protocols/jd3/full_model_inputters/FullModelInputterCreator.fwd.hh
/// @brief  Forward declaration of the %FullModelInputterCreator class
/// @author Andrew Leaver-Fay (aleaverfay@gmail.com)


#ifndef INCLUDED_protocols_jd3_full_model_inputters_FullModelInputterCreator_FWD_HH
#define INCLUDED_protocols_jd3_full_model_inputters_FullModelInputterCreator_FWD_HH

// utility headers
#include <utility/pointer/owning_ptr.hh>

// C++ headers
#include <string>

namespace protocols {
namespace jd3 {
namespace full_model_inputters {

class FullModelInputterCreator;

typedef utility::pointer::shared_ptr< FullModelInputterCreator > FullModelInputterCreatorOP;
typedef utility::pointer::shared_ptr< FullModelInputterCreator const > FullModelInputterCreatorCOP;

} // namespace full_model_inputters
} // namespace jd3
} // namespace protocols

#endif //INCLUDED_protocols_jd3_full_model_inputters_FullModelInputter_HH
