// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file protocols/peptide_deriver/PeptideDeriverMarkdownStreamOutputter.fwd.hh
/// @brief outputs a Markdown formatted Peptiderive report to a stream
/// @author orlypolo (orlymarcu@gmail.com)

#ifndef INCLUDED_protocols_peptide_deriver_PeptideDeriverMarkdownStreamOutputter_fwd_hh
#define INCLUDED_protocols_peptide_deriver_PeptideDeriverMarkdownStreamOutputter_fwd_hh

// Utility headers
#include <utility/pointer/owning_ptr.hh>


// Forward
namespace protocols {
namespace peptide_deriver {

class PeptideDeriverMarkdownStreamOutputter;

typedef utility::pointer::shared_ptr< PeptideDeriverMarkdownStreamOutputter > PeptideDeriverMarkdownStreamOutputterOP;
typedef utility::pointer::shared_ptr< PeptideDeriverMarkdownStreamOutputter const > PeptideDeriverMarkdownStreamOutputterCOP;

} //protocols
} //peptide_deriver

#endif //INCLUDED_protocols_peptide_deriver_PeptideDeriverMarkdownStreamOutputter_fwd_hh
