// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file protocols/ncbb/ResidueReplacementRebuildMoverCreator.hh
/// @brief A simple method to mutate a pose fairly destructively, and barely recover
/// @author Andy Watkins (andy.watkins2@gmail.com)

#ifndef INCLUDED_protocols_ncbb_ResidueReplacementRebuildMoverCreator_HH
#define INCLUDED_protocols_ncbb_ResidueReplacementRebuildMoverCreator_HH

#include <protocols/moves/MoverCreator.hh>

namespace protocols {
namespace ncbb {

class ResidueReplacementRebuildMoverCreator : public protocols::moves::MoverCreator {
public:
	protocols::moves::MoverOP create_mover() const override;
	std::string keyname() const override;
	void provide_xml_schema( utility::tag::XMLSchemaDefinition & xsd ) const override;
};

} //protocols
} //ncbb

#endif //INCLUDED_protocols_ncbb_ResidueReplacementRebuildMoverCreator_HH
