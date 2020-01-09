// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file /protocols/minimization_packing/TaskAwareMinMover.hh
/// @brief MinMover which updates MoveMap to mirror PackerTask
/// @author Steven Lewis

#ifndef INCLUDED_protocols_minimization_packing_TaskAwareMinMover_hh
#define INCLUDED_protocols_minimization_packing_TaskAwareMinMover_hh

// Unit Headers
#include <protocols/minimization_packing/TaskAwareMinMover.fwd.hh>

// Project Headers
#include <core/pose/Pose.fwd.hh>
#include <protocols/moves/Mover.hh>
#include <protocols/minimization_packing/MinMover.fwd.hh>
#include <core/kinematics/MoveMap.fwd.hh>
#include <protocols/filters/Filter.fwd.hh>

#include <core/pack/task/PackerTask.fwd.hh>
#include <core/pack/task/TaskFactory.fwd.hh>

#include <utility/vector1.hh>


// Utility Headers

namespace protocols {
namespace minimization_packing {

/// @details this class wraps MinMover, but ensures that its MoveMap always contains up-to-date information about sidechain mobility.  It takes its base movemap, allows sidechain freedom at any position mobile in a Factory-generated PackerTask, and passes the new movemap to MinMover.  The MinMover's MoveMap does not accumulate state over many calls to apply().
class TaskAwareMinMover : public protocols::moves::Mover {

public:

	TaskAwareMinMover();

	/// @brief constructor with TaskFactory
	TaskAwareMinMover(
		protocols::minimization_packing::MinMoverOP minmover_in,
		core::pack::task::TaskFactoryCOP factory_in
	);

	/// @brief Constructor with PackerTask.  The input PackerTask is cloned.
	/// @author Vikram K. Mulligan (vmulligan@flatironinstitute.org).
	TaskAwareMinMover(
		protocols::minimization_packing::MinMoverOP minmover_in,
		core::pack::task::PackerTaskCOP const & task_in
	);

	~TaskAwareMinMover() override;

	void apply( core::pose::Pose & pose ) override;

	protocols::moves::MoverOP fresh_instance() const override;
	protocols::moves::MoverOP clone() const override;

	/// @brief parse XML (specifically in the context of the parser/scripting scheme)
	void parse_my_tag(
		TagCOP,
		basic::datacache::DataMap &,
		Filters_map const &,
		protocols::moves::Movers_map const &,
		Pose const & ) override;

	/// @brief parse "task_operations" XML option
	virtual void parse_task_operations(
		TagCOP,
		basic::datacache::DataMap const &,
		Filters_map const &,
		protocols::moves::Movers_map const &,
		Pose const & );

	void chi( bool val ) { chi_ = val; }
	void bb( bool val ) { bb_ = val; }
	void jump( bool const j ){ jump_ = j; }
	bool chi() const { return chi_; }
	bool bb() const { return bb_; }
	bool jump() const{ return jump_;}

	std::string
	get_name() const override;

	static
	std::string
	mover_name();

	static
	void
	provide_xml_schema( utility::tag::XMLSchemaDefinition & xsd );


private:
	/// @brief OP for MinMover
	protocols::minimization_packing::MinMoverOP minmover_;
	/// @brief Default movemap to use (Will be altered by the task info).
	core::kinematics::MoveMapCOP base_movemap_;
	/// @brief OP for constant task factory for nonconstant tasks, if present
	core::pack::task::TaskFactoryCOP factory_;
	bool chi_, bb_, jump_;

	/// @brief Optional: from code, a packer task may be provided directly, but it must match the size of the pose
	/// on which we will be calling apply().
	/// @author Vikram K. Mulligan (vmulligan@flatironinstitute.org).
	core::pack::task::PackerTaskOP task_;

};//end TaskAwareMinMover

}//namespace moves
}//namespace protocols

#endif // INCLUDED_protocols_minimization_packing_TaskAwareMinMover_HH
