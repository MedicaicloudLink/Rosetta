// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file   core/scoring/etable/etrie/TrieCountPair1BC3.fwd.hh
/// @brief
/// @author Andrew Leaver-Fay (aleaverfay@gmail.com)

#ifndef INCLUDED_core_scoring_etable_etrie_TrieCountPairGeneric_fwd_hh
#define INCLUDED_core_scoring_etable_etrie_TrieCountPairGeneric_fwd_hh

#include <utility/pointer/owning_ptr.hh>

namespace core {
namespace scoring {
namespace etable {
namespace etrie {

class TrieCountPairGeneric;

typedef utility::pointer::shared_ptr< TrieCountPairGeneric > TrieCountPairGenericOP;
typedef utility::pointer::shared_ptr< TrieCountPairGeneric const > TrieCountPairGenericCOP;

} // namespace etrie
} // namespace etable
} // namespace scoring
} // namespace core

#endif