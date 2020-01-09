// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file core/simple_metrics/metrics/CompositeEnergyMetric.hh
/// @brief A metric to report/calculate all of the energies of a scorefunction that are not 0 or the delta of each energy between another input pose
/// @author Jared Adolf-Bryfogle (jadolfbr@gmail.com)

#ifndef INCLUDED_core_simple_metrics_composite_metrics_CompositeEnergyMetric_HH
#define INCLUDED_core_simple_metrics_composite_metrics_CompositeEnergyMetric_HH

#include <core/simple_metrics/composite_metrics/CompositeEnergyMetric.fwd.hh>
#include <core/simple_metrics/CompositeRealMetric.hh>

// Core headers
#include <core/types.hh>
#include <core/scoring/ScoreFunction.fwd.hh>
#include <core/select/residue_selector/ResidueSelector.fwd.hh>

// Utility headers
#include <utility/tag/XMLSchemaGeneration.fwd.hh>

// C++ headers
#include <map>

namespace core {
namespace simple_metrics {
namespace composite_metrics {

///@brief A metric to report/calculate all of the energies of a scorefunction that are not 0 or the delta of each energy between another input pose
class CompositeEnergyMetric : public core::simple_metrics::CompositeRealMetric{

public:

	/////////////////////
	/// Constructors  ///
	/////////////////////

	/// @brief Default constructor
	CompositeEnergyMetric();

	CompositeEnergyMetric(
		select::residue_selector::ResidueSelectorCOP selector );

	CompositeEnergyMetric(
		select::residue_selector::ResidueSelectorCOP selector,
		scoring::ScoreFunctionCOP scorefxn);

	/// @brief Copy constructor (not needed unless you need deep copies)
	CompositeEnergyMetric( CompositeEnergyMetric const & src );

	/// @brief Destructor (important for properly forward-declaring smart-pointer members)
	~CompositeEnergyMetric() override;

	/////////////////////
	/// Metric Methods ///
	/////////////////////

	///Defined in RealMetric:
	///
	/// @brief Calculate the metric and add it to the pose as a score.
	///           labeled as prefix+metric+suffix.
	///
	/// @details Score is added through setExtraScorePose and is output
	///            into the score tables/file at pose output.
	//void
	//apply( pose::Pose & pose, prefix="", suffix="" ) override;

	///@brief Calculate the metric.
	std::map< std::string, core::Real >
	calculate( core::pose::Pose const & pose ) const override;

public:

	///@brief Name of the class
	std::string
	name() const override;

	///@brief Name of the class for creator.
	static
	std::string
	name_static();

	///@brief Name of the metric
	std::string
	metric() const override;

	///@brief Get the metric name(s) that this Metric will calculate
	utility::vector1< std::string >
	get_metric_names() const override;

public:

	///@brief Set a scorefunction.  Will use default Rosetta scorefunction if not set.
	void
	set_scorefunction( scoring::ScoreFunctionCOP scorefxn );

	///@brief Set a residue selector to each energy term for the subset of residues.
	void
	set_residue_selector( select::residue_selector::ResidueSelectorCOP residue_selector );

	///@brief Set a pose into to calculate/report delta of total energy.
	/// (apply_pose - comparison_pose)
	///
	void
	set_comparison_pose( core::pose::PoseCOP pose );

public:

	/// @brief called by parse_my_tag -- should not be used directly
	void
	parse_my_tag(
		utility::tag::TagCOP tag,
		basic::datacache::DataMap & data ) override;

	static
	void
	provide_xml_schema( utility::tag::XMLSchemaDefinition & xsd );

	core::simple_metrics::SimpleMetricOP
	clone() const override;

private:

	select::residue_selector::ResidueSelectorCOP residue_selector_ = nullptr;
	scoring::ScoreFunctionCOP scorefxn_ = nullptr;
	pose::PoseCOP ref_pose_ = nullptr;

};

} // metrics
} // simple_metrics
} // core



#endif //protocols_analysis_simple_metrics_CompositeEnergyMetric_HH





