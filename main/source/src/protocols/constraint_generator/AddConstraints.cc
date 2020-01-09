// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file protocols/constraint_generator/AddConstraints.cc
/// @brief Adds constraints generated by ConstraintGenerators to a pose
/// @author Tom Linsky (tlinsky@uw.edu)

#include <protocols/constraint_generator/AddConstraints.hh>
#include <protocols/constraint_generator/AddConstraintsCreator.hh>

// Protocol headers
#include <protocols/constraint_generator/ConstraintGenerator.hh>
#include <protocols/constraint_generator/ConstraintGeneratorFactory.hh>
#include <protocols/constraint_generator/ConstraintsManager.hh>

// Core headers
#include <core/pose/Pose.hh>

// Basic/Utility headers
#include <basic/datacache/DataMap.hh>
#include <basic/Tracer.hh>
#include <utility>
#include <utility/tag/Tag.hh>
#include <utility/string_util.hh>
// XSD XRW Includes
#include <utility/tag/XMLSchemaGeneration.hh>
#include <protocols/moves/mover_schemas.hh>

static basic::Tracer TR( "protocols.constraint_generator.AddConstraints" );

namespace protocols {
namespace constraint_generator {

AddConstraints::AddConstraints():
	protocols::moves::Mover( AddConstraints::mover_name() ),
	generators_()
{
}

AddConstraints::AddConstraints( ConstraintGeneratorCOPs const & generators ):
	protocols::moves::Mover( AddConstraints::mover_name() ),
	generators_( generators )
{
}

AddConstraints::~AddConstraints()= default;

void
AddConstraints::parse_my_tag(
	utility::tag::TagCOP tag,
	basic::datacache::DataMap & data,
	protocols::filters::Filters_map const & ,
	protocols::moves::Movers_map const & ,
	core::pose::Pose const & )
{
	//We should also accept a comma-separated list of previously defined constraint generators
	if ( tag->hasOption( "constraint_generators" ) ) {
		std::string cst_gen_cslist = tag->getOption< std::string >( "constraint_generators" );
		utility::vector1< std::string > cst_gen_vector = utility::string_split( cst_gen_cslist, ',' );
		for ( std::string gen: cst_gen_vector ) {
			//Retrieve from the data map
			if ( data.has( "ConstraintGenerators", gen ) ) {
				ConstraintGeneratorCOP new_cg( data.get_ptr< ConstraintGenerator >( "ConstraintGenerators", gen ) );
				add_generator( new_cg );
				TR << "Added constraint generator " << new_cg->id() << "." << std::endl;
				//No need to add generator to data map
			} else {
				throw CREATE_EXCEPTION(utility::excn::RosettaScriptsOptionError, "ConstraintGenerator " + gen + " not found in basic::datacache::DataMap.");
			}
		}
	}
	//Note that this mover is a little unusual in that it adds any constraint generators defined as subtags to the DataMap.
	for ( auto subtag=tag->getTags().begin(); subtag!=tag->getTags().end(); ++subtag ) {
		ConstraintGeneratorOP new_cg = ConstraintGeneratorFactory::get_instance()->new_constraint_generator( (*subtag)->getName(), *subtag, data );
		add_generator( new_cg );
		TR << "Added constraint generator " << new_cg->id() << "." << std::endl;
		data.add( "ConstraintGenerators", new_cg->id(), new_cg );
	}
}

protocols::moves::MoverOP
AddConstraints::clone() const
{
	return utility::pointer::make_shared< AddConstraints >( *this );
}

protocols::moves::MoverOP
AddConstraints::fresh_instance() const
{
	return utility::pointer::make_shared< AddConstraints >();
}

// XRW TEMP std::string
// XRW TEMP AddConstraints::get_name() const
// XRW TEMP {
// XRW TEMP  return AddConstraints::mover_name();
// XRW TEMP }

void
AddConstraints::apply( core::pose::Pose & pose )
{
	using core::scoring::constraints::ConstraintCOPs;

	for ( ConstraintGeneratorCOPs::const_iterator cg=generators_.begin(); cg!=generators_.end(); ++cg ) {
		debug_assert( *cg );

		// Generate constraints
		ConstraintCOPs const csts = (*cg)->apply( pose );

		// Store them in pose datacache
		ConstraintsManager::get_instance()->store_constraints( pose, (*cg)->id(), csts );

		if ( csts.size() == 0 ) {
			TR << "ConstraintGenerator named " << (*cg)->id() << " did not generate any constraints. Not adding anything." << std::endl;
			continue;
		}

		TR << "Adding " << csts.size() << " constraints generated by ConstraintGenerator named " << (*cg)->id() << std::endl;
		pose.add_constraints( csts );
	}
}

void
AddConstraints::add_generator( ConstraintGeneratorCOP cst_generator )
{
	generators_.push_back( cst_generator );
}

/////////////// Creator ///////////////

// XRW TEMP protocols::moves::MoverOP
// XRW TEMP AddConstraintsCreator::create_mover() const
// XRW TEMP {
// XRW TEMP  return utility::pointer::make_shared< AddConstraints >();
// XRW TEMP }

// XRW TEMP std::string
// XRW TEMP AddConstraintsCreator::keyname() const
// XRW TEMP {
// XRW TEMP  return AddConstraints::mover_name();
// XRW TEMP }

std::string AddConstraints::get_name() const {
	return mover_name();
}

std::string AddConstraints::mover_name() {
	return "AddConstraints";
}

void AddConstraints::provide_xml_schema( utility::tag::XMLSchemaDefinition & xsd )
{
	using namespace utility::tag;
	ConstraintGeneratorFactory::get_instance()->define_constraint_generator_xml_schema_group( xsd );
	AttributeList attlist;
	attlist
		+ XMLSchemaAttribute( "constraint_generators", xs_string, "Comma-separated list of previously defined constraint generators to be added." );
	XMLSchemaSimpleSubelementList subelements;
	subelements.add_group_subelement( & ConstraintGeneratorFactory::constraint_generator_xml_schema_group_name );

	protocols::moves::xsd_type_definition_w_attributes_and_repeatable_subelements( xsd, mover_name(), "Add constraints from the specified constraint generators to the pose", attlist, subelements );
}

std::string AddConstraintsCreator::keyname() const {
	return AddConstraints::mover_name();
}

protocols::moves::MoverOP
AddConstraintsCreator::create_mover() const {
	return utility::pointer::make_shared< AddConstraints >();
}

void AddConstraintsCreator::provide_xml_schema( utility::tag::XMLSchemaDefinition & xsd ) const
{
	AddConstraints::provide_xml_schema( xsd );
}


} //protocols
} //constraint_generator

