// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file IntraDesignTerminusMotifScorer.cc
///
/// @brief
/// @author Frank Teets

//Unit headers
#include <protocols/sewing/scoring/IntraDesignTerminusMotifScorer.hh>
#include <protocols/sewing/scoring/IntraDesignTerminusMotifScorerCreator.hh>
#include <protocols/sewing/scoring/AssemblyScorerFactory.hh>
//Core headers
#include <core/types.hh>

#include <core/chemical/ChemicalManager.hh>

#include <core/pose/motif/reference_frames.hh>

#include <core/scoring/motif/util.hh>

#include <core/scoring/dssp/Dssp.hh>

#include <core/pose/Pose.hh>

#include <core/conformation/Atom.hh>
#include <core/id/AtomID.hh>

#include <numeric/xyzVector.hh>

//Utility headers
#include <basic/Tracer.hh>
#include <numeric/xyzTransform.hh>
#include <utility/io/ozstream.hh>
#include <utility/tag/Tag.hh>
#include <utility/tag/XMLSchemaGeneration.hh>
namespace protocols {
namespace sewing  {
namespace scoring {

static basic::Tracer TR("protocols.sewing.scoring.IntraDesignTerminusMotifScorer");

IntraDesignTerminusMotifScorer::IntraDesignTerminusMotifScorer():
	MotifScorer()
{}


IntraDesignTerminusMotifScorer::IntraDesignTerminusMotifScorer( IntraDesignTerminusMotifScorer const & src ):
	MotifScorer( src ),
	weight_( src.weight_ ),
	optimum_distance_( src.optimum_distance_ ),
	maximum_unpenalized_variance_( src.maximum_unpenalized_variance_ )
{
}

///@details Use the negative normalized motif score
core::Real
IntraDesignTerminusMotifScorer::score(
	data_storage::SmartAssemblyCOP assembly
) {
	this->set_old_last_score( this->get_last_score() );
	this->set_last_score( terminus_motif_score(assembly) );
	return this->get_last_score();
}

///@details use Will's Motif score to calculate the motif score for the entire Assembly
core::Real
IntraDesignTerminusMotifScorer::terminus_motif_score(
	data_storage::SmartAssemblyCOP assembly
) {
	core::Real distance;
	distance = assembly->get_n_terminal_segment()->get_residue(1)->get_atom(2).xyz().distance(assembly->get_c_terminal_segment()->get_residue(assembly->get_c_terminal_segment()->get_length())->get_atom(2).xyz());

	TR << "Intra-Terminus Distance is: " << distance << std::endl;
	if ( distance > (optimum_distance_ + maximum_unpenalized_variance_) ) {
		return (distance - (optimum_distance_ + maximum_unpenalized_variance_)) * (distance - (optimum_distance_ + maximum_unpenalized_variance_));
	} else if ( distance < (optimum_distance_ - maximum_unpenalized_variance_) ) {
		return ((optimum_distance_ - maximum_unpenalized_variance_) - distance) * ((optimum_distance_ - maximum_unpenalized_variance_) - distance);
	} else {
		return 0.0;
	}
}

void
IntraDesignTerminusMotifScorer::set_options_from_tag(
	utility::tag::TagCOP scorer,
	basic::datacache::DataMap& ,
	protocols::filters::Filters_map const & ,
	protocols::moves::Movers_map const & ,
	core::pose::Pose const &)
{
	//No data members to set!

	if ( !scorer->hasOption( "weight" ) ) {
		TR << "No weight provided for scorer " << scorer->getName() << " Using default value" << std::endl;
	}

	weight_ = scorer->getOption<core::Real>( "weight", 1.0 );
	if ( scorer->hasOption("optimum_distance") ) {
		optimum_distance_ = scorer->getOption<core::Real>("optimum_distance");
	}
	if ( scorer->hasOption("maximum_unpenalized_variance") ) {
		maximum_unpenalized_variance_ = scorer->getOption<core::Real>("maximum_unpenalized_variance");
	}


	TR << "Created IntraDesignTerminusMotifScorer from tag!" << std::endl;
}


void
IntraDesignTerminusMotifScorer::provide_xml_schema( utility::tag::XMLSchemaDefinition & xsd ){
	using namespace utility::tag;
	AttributeList attributes;
	attributes
		+ XMLSchemaAttribute::attribute_w_default( "weight", xsct_real, "How heavily will this term be weighted during scoring?", "1.0"  )
		+ XMLSchemaAttribute( "optimum_distance", xsct_real, "How far apart should that residue optimally be from the terminus?")
		+ XMLSchemaAttribute( "maximum_unpenalized_variance", xsct_real, "How far off from that can it be before it should be penalized?");


	XMLSchemaComplexTypeGenerator ct_gen;
	ct_gen.complex_type_naming_func( & AssemblyScorerFactory::assembly_scorer_ct_namer )
		.element_name( IntraDesignTerminusMotifScorer::type_name() )
		.add_optional_name_attribute()
		.description( "Motif score to measure packing of assembly against partner PDB" )
		.add_attributes( attributes )
		.write_complex_type_to_schema( xsd ); //We won't have regular score functions/task operations

}

core::Real
IntraDesignTerminusMotifScorer::get_weight() const {
	return weight_;
}

void
IntraDesignTerminusMotifScorer::set_weight( core::Real weight ){
	weight_ = weight;
}


core::Real
IntraDesignTerminusMotifScorer::get_optimum_distance() const{
	return optimum_distance_;
}

core::Real
IntraDesignTerminusMotifScorer::get_maximum_unpenalized_variance() const{
	return maximum_unpenalized_variance_;
}

void
IntraDesignTerminusMotifScorer::set_optimum_distance( core::Real setting ){
	optimum_distance_ = setting;
}

void
IntraDesignTerminusMotifScorer::set_maximum_unpenalized_variance( core::Real setting ){
	maximum_unpenalized_variance_ = setting;
}

//////Creator methods//////////
void
IntraDesignTerminusMotifScorerCreator::provide_xml_schema( utility::tag::XMLSchemaDefinition & xsd ) const{
	IntraDesignTerminusMotifScorer::provide_xml_schema( xsd );

}



AssemblyScorerOP
IntraDesignTerminusMotifScorerCreator::create_assembly_scorer() const{
	return utility::pointer::make_shared< IntraDesignTerminusMotifScorer >();
}

std::string
IntraDesignTerminusMotifScorer::type_name(){
	return "IntraDesignTerminusMotifScorer";
}


std::string
IntraDesignTerminusMotifScorerCreator::keyname() const{
	return IntraDesignTerminusMotifScorer::type_name();
}

/////End Creator methods///////











} //scoring namespace
} //sewing namespace
} //protocols namespace
