// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file src/protocols/denovo_design/filters/SSShapeComplementarityFilter.hh
/// @brief Tom's Denovo Protocol. This is freely mutable and used for playing around with stuff
/// @details
/// @author Tom Linsky (tlinsky@gmail.com)


#ifndef INCLUDED_protocols_denovo_design_filters_SSShapeComplementarityFilter_hh
#define INCLUDED_protocols_denovo_design_filters_SSShapeComplementarityFilter_hh

// Unit headers
#include <protocols/denovo_design/filters/SSShapeComplementarityFilter.fwd.hh>

// Project headers
#include <protocols/filters/Filter.hh>
#include <protocols/fldsgn/topology/HelixPairing.fwd.hh>
#include <protocols/fldsgn/topology/HSSTriplet.fwd.hh>
#include <protocols/fldsgn/topology/SS_Info2.fwd.hh>
#include <protocols/parser/BluePrint.fwd.hh>

// Core headers
#include <core/kinematics/MoveMap.fwd.hh>
#include <core/pose/Pose.fwd.hh>
#include <core/scoring/ScoreFunction.fwd.hh>
#include <core/scoring/constraints/ConstraintSet.hh>
#include <core/scoring/sc/MolecularSurfaceCalculator.hh>
#include <core/scoring/sc/ShapeComplementarityCalculator.fwd.hh>
#include <core/select/residue_selector/ResidueSelector.fwd.hh>

// C++ headers
#include <string>

#include <core/io/silent/silent.fwd.hh>
#include <utility/vector1.hh>


namespace protocols {
namespace denovo_design {
namespace filters {

class SSShapeComplementarityFilter : public protocols::filters::Filter {
public:

	/// @brief Initialize SSShapeComplementarityFilter
	SSShapeComplementarityFilter();

	/// @brief virtual constructor to allow derivation
	virtual ~SSShapeComplementarityFilter();

	/// @brief Parses the SSShapeComplementarityFilter tags
	void parse_my_tag(
		utility::tag::TagCOP tag,
		basic::datacache::DataMap & data,
		protocols::filters::Filters_map const &,
		protocols::moves::Movers_map const &,
		core::pose::Pose const & ) override;

	/// @brief Return the name of this mover.
	virtual std::string get_name() const;

	/// @brief return a fresh instance of this class in an owning pointer
	protocols::filters::FilterOP clone() const override;

	/// @brief Apply the SSShapeComplementarityFilter. Overloaded apply function from filter base class.
	protocols::filters::FilterOP fresh_instance() const override;
	void report( std::ostream & out, core::pose::Pose const & pose ) const override;
	core::Real report_sm( core::pose::Pose const & pose ) const override;
	bool apply( core::pose::Pose const & pose ) const override;

	core::Real compute( core::pose::Pose const & pose ) const;

	/// @brief Set the threshold below which structures are rejected.
	///
	void set_rejection_thresh( core::Real const &val ) {rejection_thresh_ = val; return;}

	/// @brief Get the threshold below which structures are rejected.
	///
	core::Real rejection_thresh() const {return rejection_thresh_;}

	void
	set_residue_selector( core::select::residue_selector::ResidueSelector const & selector );

	std::string
	name() const override;

	static
	std::string
	class_name();

	static
	void
	provide_xml_schema( utility::tag::XMLSchemaDefinition & xsd );


private:   // private functions
	/// @brief sets up the underlying filter to work based on a helix
	void
	setup_sc( core::pose::Pose const & pose,
		protocols::fldsgn::topology::SS_BaseCOP const ss ) const;

	/// @brief sets up the underlying shapecomplementarity filter to work based on secondary structure elements
	void
	setup_sc_hss( core::pose::Pose const & pose,
		protocols::fldsgn::topology::SS_Info2 const & ss_info,
		protocols::fldsgn::topology::HSSTripletCOP hss_triplet ) const;
	void
	setup_sc_hh( core::pose::Pose const & pose,
		protocols::fldsgn::topology::SS_Info2 const & ss_info,
		protocols::fldsgn::topology::HelixPairingCOP helix_pair ) const;

	core::Real
	compute_from_selector( core::pose::Pose const & pose ) const;

	core::Real
	compute_from_ss_info( core::pose::Pose const & pose ) const;

	/// @brief Runs the SC calculator to obtain an SC score and an interaction area. Returns a result in the format core::scoring::sc::RESULTS.  Assumes the SC calculator has been initialized and has the correct residues added.
	core::scoring::sc::RESULTS const &
	get_sc_and_area() const;

private:   // options
	/// @brief controls outputtting verbose information about SC
	bool verbose_;
	/// @brief should we calculate SC from each loop to the rest of the protein?
	bool calc_loops_;
	/// @brief should we calculate SC from each helix to the rest of the protein?
	bool calc_helices_;
	/// @brief Threshold below which structures are rejected.  Default 0.0 (no filtration).
	/// @author Vikram K. Mulligan (vmullig@uw.edu)
	core::Real rejection_thresh_;
	/// @brief If set, this will be used as the secondary structure for the pose, instead of DSSP
	std::string secstruct_;

private:   // other data
	/// @brief residue selector
	core::select::residue_selector::ResidueSelectorCOP selector_;
	/// @brief the shape complementarity calculator
	core::scoring::sc::ShapeComplementarityCalculatorOP scc_;

};


} // filters
} // denovo_design
} // protocols

#endif
