// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file core/simple_metrics/metrics/SecondaryStructureMetric.hh
/// @brief A SimpleMetric to output the secondary structure of the protein or residues selected by a residue selector.
/// @author Jared Adolf-Bryfogle (jadolfbr@gmail.com)

#ifndef INCLUDED_core_simple_metrics_metrics_SecondaryStructureMetric_HH
#define INCLUDED_core_simple_metrics_metrics_SecondaryStructureMetric_HH

#include <core/simple_metrics/metrics/SecondaryStructureMetric.fwd.hh>
#include <core/simple_metrics/StringMetric.hh>

// Core headers
#include <core/types.hh>
#include <core/select/residue_selector/ResidueSelector.fwd.hh>

// Utility headers
#include <utility/tag/XMLSchemaGeneration.fwd.hh>

namespace core {
namespace simple_metrics {
namespace metrics {

///@brief A SimpleMetric to output the secondary structure of the protein or residues selected by a residue selector.
/// Uses DSSP
class SecondaryStructureMetric : public core::simple_metrics::StringMetric{

public:

	/////////////////////
	/// Constructors  ///
	/////////////////////

	/// @brief Default constructor
	SecondaryStructureMetric();

	SecondaryStructureMetric( select::residue_selector::ResidueSelectorCOP selector );

	/// @brief Copy constructor (not needed unless you need deep copies)
	SecondaryStructureMetric( SecondaryStructureMetric const & src );

	/// @brief Destructor (important for properly forward-declaring smart-pointer members)
	~SecondaryStructureMetric() override;

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

	///@brief Calculate the metric.  Return the SS string using DSSP.
	///  Reduced, 3 letter alphabet as the default.
	std::string
	calculate( core::pose::Pose const & pose ) const override;

public:

	void
	set_residue_selector( select::residue_selector::ResidueSelectorCOP selector );

	///@brief Set to use the reduced DSSP alphabet or not.
	/// Default true.
	void
	set_use_dssp_reduced( bool reduced );

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

	select::residue_selector::ResidueSelectorCOP selector_ = nullptr;
	bool dssp_reduced_ = true;

};

} //core
} //simple_metrics
} //metrics



#endif //core_simple_metrics_metrics_SecondaryStructureMetric_HH





