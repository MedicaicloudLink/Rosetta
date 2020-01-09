// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file protocols/recces/sampler/rna/MC_RNA_Sugar.fwd.hh
/// @brief Markov chain sampler for sugar pucker.
/// @author Fang-Chieh Chou


#ifndef INCLUDED_protocols_sampler_rna_MC_RNA_Sugar_fwd_HH
#define INCLUDED_protocols_sampler_rna_MC_RNA_Sugar_fwd_HH

#include <utility/pointer/owning_ptr.hh>
#include <utility/pointer/access_ptr.hh>

namespace protocols {
namespace recces {
namespace sampler {
namespace rna {

class MC_RNA_Sugar;
typedef utility::pointer::shared_ptr< MC_RNA_Sugar > MC_RNA_SugarOP;
typedef utility::pointer::weak_ptr< MC_RNA_Sugar > MC_RNA_SugarAP;

} //rna
} //sampler
} //recces
} //protocols

#endif
