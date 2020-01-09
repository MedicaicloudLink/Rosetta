// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file protocols/minimization_packing/DisulfideOptimizationMoverCreator.hh
/// @brief A Mover to jointly optimize the geometry of a pair of disulfide-bonded residues.
/// @author Andy Watkins (andy.watkins2@gmail.com)

#ifndef INCLUDED_protocols_minimization_packing_DisulfideOptimizationMoverCreator_HH
#define INCLUDED_protocols_minimization_packing_DisulfideOptimizationMoverCreator_HH

#include <protocols/moves/MoverCreator.hh>

namespace protocols {
namespace minimization_packing {

class DisulfideOptimizationMoverCreator : public protocols::moves::MoverCreator {
public:
	protocols::moves::MoverOP create_mover() const override;
	std::string keyname() const override;
	void provide_xml_schema( utility::tag::XMLSchemaDefinition & xsd ) const override;
};

} //protocols
} //minimization_packing

#endif //INCLUDED_protocols_minimization_packing_DisulfideOptimizationMoverCreator_HH
