// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file --path--/--class--.hh
/// @brief --brief--
/// @author --name-- (--email--)

#ifndef INCLUDED_--path_underscore--_--class--_hh
#define INCLUDED_--path_underscore--_--class--_hh

// Unit headers
#include <--path--/--class--.fwd.hh>
#include <protocols/constraint_generator/ConstraintGenerator.hh>

// Core headers
#include <core/pose/Pose.fwd.hh>
#include <core/scoring/constraints/Constraint.fwd.hh>

// Utility headers
#include <utility/pointer/owning_ptr.hh>
#include <utility/pointer/ReferenceCount.hh>
#include <utility/tag/XMLSchemaGeneration.fwd.hh>

--namespace--

///@brief --brief--
class --class-- : public protocols::constraint_generator::ConstraintGenerator {
public:
	--class--();

	virtual ~--class--();

	static std::string
	class_name();

	protocols::constraint_generator::ConstraintGeneratorOP
	clone() const;

	virtual core::scoring::constraints::ConstraintCOPs
	apply( core::pose::Pose const & pose ) const;

	static
	void
	provide_xml_schema( utility::tag::XMLSchemaDefinition & xsd );

protected:
	virtual void
	parse_tag( utility::tag::TagCOP tag, basic::datacache::DataMap & data );

private:

};

--end_namespace--

#endif //INCLUDED_--path_underscore--_--class--_hh
