// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file protocols/simple_filters/ResidueCountFilter.hh
/// @brief Filter on the total number of residues in the structure
/// @author Matthew O'Meara (mattjomeara@gmail.com)

#ifndef INCLUDED_protocols_simple_filters_ResidueCountFilter_hh
#define INCLUDED_protocols_simple_filters_ResidueCountFilter_hh

//unit headers
#include <protocols/simple_filters/ResidueCountFilter.fwd.hh>

// Project Headers
#include <core/scoring/ScoreFunction.hh>
#include <core/types.hh>
#include <protocols/filters/Filter.hh>
#include <core/pose/Pose.fwd.hh>
#include <basic/datacache/DataMap.fwd.hh>
#include <protocols/moves/Mover.fwd.hh>
#include <core/chemical/ResidueTypeSet.fwd.hh>
#include <core/chemical/ResidueProperty.hh>
#include <core/pack/task/TaskFactory.fwd.hh>
#include <core/select/residue_selector/ResidueSelector.fwd.hh>

//C library
#include <cmath> // for round, floor, ceil, trunc, sqrt

namespace protocols {
namespace simple_filters {

class ResidueCountFilter : public filters::Filter
{
public:
	//default ctor
	ResidueCountFilter();

	~ResidueCountFilter() override;

	bool
	apply(
		core::pose::Pose const & pose
	) const override;

	filters::FilterOP
	clone() const override;

	filters::FilterOP
	fresh_instance() const override;

	void
	report(
		std::ostream & out,
		core::pose::Pose const & pose
	) const override;

	core::Real
	report_sm(
		core::pose::Pose const & pose
	) const override;

	/// @details The logic here ensures that any residue that is selected by type OR by properties is counted once and only once.
	/// So, for example, if I say, "count THR and beta-branched and polar", each threonine residue is still only counted once.
	/// @author Original author unknown.
	/// @author Updated by Parisa Hosseinzadeh (parisah@uw.edu) to add property counting.
	/// @author Logic updated by Vikram K. Mulligan (vmullig@uw.edu) to avoid double-counting if names and multiple properties are specified.
	core::Real
	compute(
		core::pose::Pose const & pose
	) const;

	core::Real
	round_to_Real(
		core::Real x) const;

	void
	parse_my_tag(
		utility::tag::TagCOP tag,
		basic::datacache::DataMap &,
		filters::Filters_map const &,
		moves::Movers_map const &,
		core::pose::Pose const &
	) override;

	core::Size
	max_residue_count() const;

	void
	max_residue_count(
		core::Size value
	);

	bool
	enable_max_residue_count() const;

	void
	enable_max_residue_count(
		bool value
	);

	core::Size
	min_residue_count() const;

	void
	min_residue_count(
		core::Size value
	);

	utility::vector1< std::string >
	res_types() const;

	void
	res_types( utility::vector1< std::string > const & res_type );

	utility::vector1< core::chemical::ResidueProperty > const &
	res_props() const;

	void
	res_props( utility::vector1< core::chemical::ResidueProperty > const & res_prop );

	bool
	enable_min_residue_count() const;

	void
	enable_min_residue_count(
		bool value
	);

	core::pack::task::TaskFactoryOP
	task_factory() const;

	void
	task_factory(
		core::pack::task::TaskFactoryOP task_factory
	);

	void
	residue_selector( core::select::residue_selector::ResidueSelectorCOP selector );

	bool packable() const;

	void
	packable(
		bool const pack
	);

	/// @brief Checks whether a residue type is present in the provided residue type set, and if so, adds it to res_types_
	bool
	add_residue_type_by_name(
		core::chemical::ResidueTypeSet const & res_type_set,
		std::string const & res_type_input
	);

	/// @brief add proeprties to peroperty vector
	/// @detail given user specified properties, adds them to the property vector to count. I still need to add a way to check the sanity
	/// @input res_type_set, the residue type set of the input structure
	/// @input res_type_input, the user specified residue type name
	/// @return false if res_type_input doesn't match any residue type names, true otherwise
	/// @author Parisa Hosseinzadeh (parisah@uw.edu), Baker laboratory.
	bool
	add_residue_property_by_name(
		//core::chemical::ResidueTypeSet const & res_type_set,
		std::string const & prop_input
	);

	std::string
	name() const override;

	static
	std::string
	class_name();

	static
	void
	provide_xml_schema( utility::tag::XMLSchemaDefinition & xsd );



private:
	core::Size max_residue_count_;
	bool enable_max_residue_count_;
	core::Size min_residue_count_;
	bool enable_min_residue_count_;
	bool count_as_percentage_;
	utility::vector1< std::string > res_types_;
	utility::vector1< core::chemical::ResidueProperty > res_props_;
	bool packable_;
	core::pack::task::TaskFactoryOP task_factory_;
	core::select::residue_selector::ResidueSelectorCOP selector_;
};

}
}

#endif
