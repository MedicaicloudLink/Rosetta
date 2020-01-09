// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file core/pack/task/xml_util.cc
/// @brief Task operation/factory utilities useful in RosettaScripts.
/// @author Sarel Fleishman (sarelf@u.washington.edu)
///         Jacob Corn (jecorn@u.washington.edu)
///         Rocco Moretti (rmoretti@u.washington.edu)
///         Eva-Maria Strauch (evas01@uw.edu)

// Unit headers
#include <core/pack/task/xml_util.hh>

// Core headers
#include <core/types.hh>
#include <core/pack/task/TaskFactory.hh>
#include <core/pack/task/operation/TaskOperation.hh>
#include <core/pack/task/operation/TaskOperationFactory.hh>
#include <core/pack/task/operation/ResLvlTaskOperationFactory.hh>
#include <core/pack/palette/PackerPalette.hh>

// Utility headers
#include <utility/tag/Tag.hh>
#include <utility/tag/XMLSchemaGeneration.hh>
#include <utility/string_util.hh>
#include <utility/vector1.hh>

// Basic headers
#include <basic/datacache/DataMap.hh>
#include <basic/Tracer.hh>

// C++ headers
#include <string>

static basic::Tracer TR( "core.pack.task.xml_util" );

namespace core {
namespace pack {
namespace task {

using namespace std;
using namespace core::pack::task::operation;
using utility::tag::TagCOP;
using utility::tag::AttributeList;
using utility::vector1;
using basic::datacache::DataMap;

vector1<TaskOperationOP> get_task_operations(
	TagCOP tag, DataMap const & data) {

	vector1< TaskOperationOP > task_operations;
	string const t_o_val( tag->getOption<string>(TASK_OPERATIONS_TAG, "" ) );
	if ( t_o_val != "" ) {
		vector1<string> const t_o_keys( utility::string_split( t_o_val, ',' ) );
		for ( auto const & t_o_key : t_o_keys ) {
			if ( data.has( TASK_OPERATIONS_TAG, t_o_key ) ) {
				task_operations.push_back( data.get_ptr<TaskOperation>( TASK_OPERATIONS_TAG, t_o_key ) );
			} else {
				throw CREATE_EXCEPTION(utility::excn::RosettaScriptsOptionError, "TaskOperation " + t_o_key + " not found in basic::datacache::DataMap.");
			}
		}
	}
	return task_operations;
}

TaskFactoryOP parse_task_operations(
	TagCOP tag, DataMap const & data ) {

	if ( ! tag->hasOption(TASK_OPERATIONS_TAG) ) {
		TaskFactoryOP new_task_factory( utility::pointer::make_shared< TaskFactory >() );

		//VKM, Chemical XRW 2016: set up packer palettes:
		set_up_packer_palette( tag, data, new_task_factory );

		return new_task_factory;
	}

	TR << "Object " << tag->getOption<string>("name", "???") << " reading the following task_operations: ";
	TaskFactoryOP new_task_factory( parse_task_operations( tag->getOption<string>(TASK_OPERATIONS_TAG), data ) );
	set_up_packer_palette(tag, data, new_task_factory);
	return new_task_factory;
}

TaskFactoryOP parse_task_operations(
	string const & task_list, DataMap const & data ) {

	TaskFactoryOP new_task_factory( utility::pointer::make_shared< TaskFactory >() );

	string const t_o_val( task_list );
	typedef utility::vector1<string> StringVec;
	StringVec const t_o_keys( utility::string_split( t_o_val, ',' ) );

	TR << "Adding the following task operations\n";
	for ( auto const & t_o_key : t_o_keys ) {
		if ( data.has( TASK_OPERATIONS_TAG, t_o_key ) ) {
			new_task_factory->push_back( data.get_ptr< TaskOperation >( TASK_OPERATIONS_TAG, t_o_key ) );
			TR << t_o_key << ' ';
		} else {
			throw CREATE_EXCEPTION(utility::excn::RosettaScriptsOptionError, "TaskOperation " + t_o_key + " not found in basic::datacache::DataMap.");
		}
	}
	TR << std::endl;
	return new_task_factory;
}

// option to add or refer to a Taskfactory through the datamap, similar to how to add/refer to movemap OPs (EMS)
TaskFactoryOP parse_task_operations(
	TagCOP tag, DataMap & data, TaskFactoryOP & task_factory) {

	if ( tag->hasOption(TASK_FACTORY_TAG) ) {
		string const name( tag->getOption<string>(TASK_FACTORY_TAG) );
		TR << "taskfactory name: " << name << std::endl;

		if ( data.has( "TaskFactory", name ) ) {
			task_factory = data.get_ptr<TaskFactory>( "TaskFactory", name );
			TR << "found helper task_factory: " << name <<" for mover: " << tag->getName() << std::endl;
		} else {
			string tf_string = "TaskFactory";
			task_factory = utility::pointer::make_shared< TaskFactory >();
			data.add( tf_string , name , task_factory );
			TR << "adding new TaskFactory to the datamap: " << name << std::endl;
		}
	}

	//VKM, Chemical XRW 2016: set up packer palettes:
	set_up_packer_palette( tag, data, task_factory );

	if ( ! tag->hasOption(TASK_OPERATIONS_TAG) ) return task_factory;

	string const t_o_val( tag->getOption<string>(TASK_OPERATIONS_TAG) );
	typedef utility::vector1<string> StringVec;
	StringVec const t_o_keys( utility::string_split( t_o_val, ',' ) );
	TR << "Adding the following task operations to mover " << tag->getName() << " called " << tag->getOption<string>( "name", "no_name" ) << ":\n";

	for ( auto const & t_o_key : t_o_keys ) {

		if ( data.has( TASK_OPERATIONS_TAG, t_o_key ) ) {
			task_factory->push_back( data.get_ptr< TaskOperation >( TASK_OPERATIONS_TAG, t_o_key ) );
			TR << t_o_key << ' ';
		} else {
			throw CREATE_EXCEPTION(utility::excn::RosettaScriptsOptionError, "TaskOperation " + t_o_key + " not found in basic::datacache::DataMap.");
		}
	}
	TR << std::endl;
	return task_factory;
}

void attributes_for_parse_task_operations(
	AttributeList & attributes, string const & description, string const & packerpalette_description) {

	attributes + utility::tag::XMLSchemaAttribute(
		TASK_OPERATIONS_TAG,
		utility::tag::xsct_task_operation_comma_separated_list,
		(description.empty() ? "A comma-separated list of TaskOperations to use." : description) );

	attributes + utility::tag::XMLSchemaAttribute( PACKER_PALETTE_TAG, utility::tag::xsct_packer_palette,
		( packerpalette_description.empty() ? "A previously-defined PackerPalette to use, which specifies the set of residue types with which to design (to be pruned with TaskOperations)." : packerpalette_description ) );
}

void attributes_for_parse_task_operations_w_factory(
	AttributeList & attributes, string const & used_for_descr ) {

	string to_descrip = "A comma-separated list of TaskOperations to use";
	string tf_descrip = "A TaskFactory specification to use";
	string pp_descrip = "A previously-defined PackerPalette to use";
	if ( ! used_for_descr.empty() ) {
		to_descrip += " for " + used_for_descr;
		tf_descrip += " for " + used_for_descr;
		pp_descrip += " for " + used_for_descr;
	} else {
		to_descrip += ".";
		tf_descrip += ".";
		pp_descrip += ", which specifies the set of residue types with which to design (to be pruned with TaskOperations).";
	}

	attributes
		+ utility::tag::XMLSchemaAttribute( TASK_OPERATIONS_TAG, utility::tag::xsct_task_operation_comma_separated_list, to_descrip )
		+ utility::tag::XMLSchemaAttribute( TASK_FACTORY_TAG, utility::tag::xs_string , tf_descrip )
		+ utility::tag::XMLSchemaAttribute( PACKER_PALETTE_TAG, utility::tag::xsct_packer_palette , pp_descrip );
}


/// @brief Parse a "packer_palette" tag from XML, and add a PackerPalette to a TaskFactory.
/// @details Added as part of the 2016 Chemical XRW (eXtreme Rosetta Workshop), New Brunswick, NJ,
/// 25 January 2016 to 1 February 2016.
/// @param[in] tag Const owning pointer to an XML tag that may or may not contain a single "packer_palette=xxx" string.
/// @param[in] data Const reference to the data map, from which defined PackerPalettes will be sought.
/// @param[out] task_factory Non-const owning pointer to the task_factory that will have a PackerPalette added to it.
/// @author Vikram K. Mulligan (vmullig@uw.edu)
/// @note Moved to core on 7 Nov. 2018 by VKM, vmulligan@flatironinstitute.org.
void
set_up_packer_palette(
	utility::tag::TagCOP tag,
	basic::datacache::DataMap const & data,
	core::pack::task::TaskFactoryOP & task_factory
) {
	if ( !tag->hasOption("packer_palette") ) return;

	std::string const pp_string( tag->getOption<std::string>("packer_palette") );
	if ( data.has( "packer_palettes", pp_string ) ) {
		TR << "Adding PackerPalette named \"" << pp_string << "\"." << std::endl;
		task_factory->set_packer_palette( data.get_ptr< core::pack::palette::PackerPalette >("packer_palettes", pp_string ) );
	} else {
		throw CREATE_EXCEPTION(utility::excn::RosettaScriptsOptionError, "PackerPalette " + pp_string + " not found in basic::datacache::DataMap.");
	}
}

}
}
}
