// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file   protocols/task_operations/ImportUnboundRotamersOperation.cc
/// @brief  Import unbound (or native) rotamers into Rosetta!
/// @author Dave La ( davela@uw.edu )


// Unit Headers
#include <protocols/task_operations/ImportUnboundRotamersOperation.hh>
#include <protocols/task_operations/ImportUnboundRotamersOperationCreator.hh>

#include <core/chemical/AA.hh>
#include <core/conformation/Residue.hh>
#include <core/pack/rotamer_set/RotamerSetOperation.hh>
#include <core/pack/task/PackerTask.hh>
#include <core/pack/task/operation/TaskOperation.hh>
#include <utility/tag/Tag.hh>


#include <core/pack/rotamer_set/RotamerSet.hh>
#include <core/pack/rotamer_set/UnboundRotamersOperation.hh>
#include <utility/vector0.hh>
#include <utility/vector1.hh>
#include <utility/tag/XMLSchemaGeneration.hh>
#include <core/pack/task/operation/task_op_schemas.hh>

#ifdef WIN32
#include <utility/graph/Graph.hh>
#endif

namespace protocols {
namespace task_operations {

using namespace core::pack::task::operation;
using namespace utility::tag;

///////////////////////////////////////////////////////////////////////////////////////////
TaskOperationOP
ImportUnboundRotamersOperationCreator::create_task_operation() const
{
	return utility::pointer::make_shared< ImportUnboundRotamersOperation >();
}

void ImportUnboundRotamersOperationCreator::provide_xml_schema( utility::tag::XMLSchemaDefinition & xsd ) const
{
	ImportUnboundRotamersOperation::provide_xml_schema( xsd );
}

std::string ImportUnboundRotamersOperationCreator::keyname() const
{
	return ImportUnboundRotamersOperation::keyname();
}

/// @brief default constructor
ImportUnboundRotamersOperation::ImportUnboundRotamersOperation():
	TaskOperation()
{}

/// @brief destructor
ImportUnboundRotamersOperation::~ImportUnboundRotamersOperation()= default;

/// @brief clone
TaskOperationOP
ImportUnboundRotamersOperation::clone() const {
	return utility::pointer::make_shared< ImportUnboundRotamersOperation >( *this );
}

//mjo commenting out 'pose' because it is unused and causes a warning
/// @brief
void
ImportUnboundRotamersOperation::apply( Pose const & /*pose*/, PackerTask & task ) const
{
	core::pack::rotamer_set::UnboundRotamersOperationOP unboundrot( utility::pointer::make_shared< core::pack::rotamer_set::UnboundRotamersOperation >() );
	unboundrot->initialize_from_command_line();
	task.append_rotamerset_operation( unboundrot );

}

void
ImportUnboundRotamersOperation::parse_tag( TagCOP /*tag*/ , DataMap & )
{
}

void ImportUnboundRotamersOperation::provide_xml_schema( utility::tag::XMLSchemaDefinition & xsd )
{
	task_op_schema_empty(
		xsd, keyname(),
		"Import unbound rotamers from a given PDB file. "
		"Specify the unbound/native PDB file using the flag: -packing::unboundrot");
}

} // TaskOperations
} // protocols

