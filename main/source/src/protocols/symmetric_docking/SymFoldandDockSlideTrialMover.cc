// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file MinMover.cc
/// @brief
/// @author Ingemar Andre

// Unit headers
#include <protocols/symmetric_docking/SymFoldandDockSlideTrialMover.hh>

// Package headers
#include <core/conformation/symmetry/SymmetricConformation.hh>
#include <core/conformation/symmetry/SymmetryInfo.hh>
#include <core/pose/symmetry/util.hh>

#include <protocols/symmetry/SetupForSymmetryMover.hh>
#include <protocols/symmetry/SymDockingInitialPerturbation.hh>

#include <protocols/symmetric_docking/SymFoldandDockCreators.hh>

#include <utility/tag/Tag.hh>
#include <basic/datacache/DataMap.hh>
#include <protocols/rosetta_scripts/util.hh>

// options
#include <basic/options/option.hh>
#include <basic/options/keys/fold_and_dock.OptionKeys.gen.hh>

// Utility Headers
#include <basic/Tracer.hh>

#include <core/conformation/symmetry/SymDof.hh>
#include <core/pose/Pose.hh>
#include <utility/vector1.hh>
// XSD XRW Includes
#include <utility/tag/XMLSchemaGeneration.hh>
#include <protocols/moves/mover_schemas.hh>


namespace protocols {
namespace symmetric_docking {

static basic::Tracer TR( "protocols.moves.symmetry.SymFoldandDockSlideTrialMover" );

SymFoldandDockSlideTrialMover::SymFoldandDockSlideTrialMover()
: Mover("SymFoldandDockSlideTrialMover") {
	using namespace basic::options;
	rotate_anchor_to_x_ = option[ OptionKeys::fold_and_dock::rotate_anchor_to_x ]();
}

SymFoldandDockSlideTrialMover::~SymFoldandDockSlideTrialMover()= default;

void
SymFoldandDockSlideTrialMover::apply( core::pose::Pose & pose ) {
	protocols::symmetry::SetupForSymmetryMover setup;
	setup.apply( pose );

	using namespace core::conformation::symmetry;
	using namespace protocols::symmetric_docking;
	using namespace basic::options;

	debug_assert( core::pose::symmetry::is_symmetric( pose ));
	auto & symm_conf (
		dynamic_cast<SymmetricConformation & > ( pose.conformation()) );

	SymSlideInfo const & slide_info( symm_conf.Symmetry_Info()->get_slide_info() );

	TR.Debug << "Slide into contact mover..." << std::endl;
	if ( rotate_anchor_to_x_ ) {
		TR.Debug << "Rotate anchor to x axis.." << std::endl;
		core::pose::symmetry::rotate_anchor_to_x_axis( pose );
	}

	//fpd -- make these parsable
	if ( slide_info.get_slide_type() == SEQUENTIAL ) {
		symmetry::SequentialSymmetrySlider symm_slider
			= symmetry::SequentialSymmetrySlider(
			pose, slide_info.get_SlideCriteriaType(), slide_info.get_SlideCriteriaVal() );
		symm_slider.apply( pose );
	}
	if ( slide_info.get_slide_type() == ORDERED_SEQUENTIAL ) {
		symmetry::OrderedSequentialSymmetrySlider symm_slider
			= symmetry::OrderedSequentialSymmetrySlider(
			pose, slide_info.get_SlideCriteriaType(),
			slide_info.get_SlideCriteriaVal(), slide_info.get_slide_order() );
		symm_slider.apply( pose );
	}
	if ( slide_info.get_slide_type() == RANDOM ) {
		symmetry::RandomSymmetrySlider symm_slider
			= symmetry::RandomSymmetrySlider(
			pose, slide_info.get_SlideCriteriaType(), slide_info.get_SlideCriteriaVal() );
		symm_slider.apply( pose );
	}

}

// XRW TEMP std::string
// XRW TEMP SymFoldandDockSlideTrialMover::get_name() const {
// XRW TEMP  return "SymFoldandDockSlideTrialMover";
// XRW TEMP }

void
SymFoldandDockSlideTrialMover::parse_my_tag(
	utility::tag::TagCOP tag,
	basic::datacache::DataMap & ,
	protocols::filters::Filters_map const &,
	protocols::moves::Movers_map const &,
	core::pose::Pose const & )
{
	using namespace core::scoring;

	if ( tag->hasOption( "rotate_anchor_to_x" ) ) {
		rotate_anchor_to_x_ = tag->getOption<bool>( "rotate_anchor_to_x" );
	}
}


// XRW TEMP std::string
// XRW TEMP SymFoldandDockSlideTrialMoverCreator::keyname() const {
// XRW TEMP  return SymFoldandDockSlideTrialMover::mover_name();
// XRW TEMP }

// XRW TEMP protocols::moves::MoverOP
// XRW TEMP SymFoldandDockSlideTrialMoverCreator::create_mover() const {
// XRW TEMP  return utility::pointer::make_shared< SymFoldandDockSlideTrialMover >();
// XRW TEMP }

// XRW TEMP std::string
// XRW TEMP SymFoldandDockSlideTrialMover::mover_name() {
// XRW TEMP  return "SymFoldandDockSlideTrialMover";
// XRW TEMP }

std::string SymFoldandDockSlideTrialMover::get_name() const {
	return mover_name();
}

std::string SymFoldandDockSlideTrialMover::mover_name() {
	return "SymFoldandDockSlideTrialMover";
}

void SymFoldandDockSlideTrialMover::provide_xml_schema( utility::tag::XMLSchemaDefinition & xsd )
{
	using namespace utility::tag;
	AttributeList attlist;
	attlist + XMLSchemaAttribute( "rotate_anchor_to_x", xsct_rosetta_bool, "Rotate the anchor residue to the x-axis before applying rigid body transformations." ) ;

	protocols::moves::xsd_type_definition_w_attributes( xsd, mover_name(), "Setting up what types of slides to make during symmetric fold and dock?", attlist );
}

std::string SymFoldandDockSlideTrialMoverCreator::keyname() const {
	return SymFoldandDockSlideTrialMover::mover_name();
}

protocols::moves::MoverOP
SymFoldandDockSlideTrialMoverCreator::create_mover() const {
	return utility::pointer::make_shared< SymFoldandDockSlideTrialMover >();
}

void SymFoldandDockSlideTrialMoverCreator::provide_xml_schema( utility::tag::XMLSchemaDefinition & xsd ) const
{
	SymFoldandDockSlideTrialMover::provide_xml_schema( xsd );
}


} // symmetric_docking
} // protocols
