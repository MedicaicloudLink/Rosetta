// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file core/pack/task/operation/KeepSequenceSymmetry.hh
/// @brief Use this when you give Rosetta a multimer to design and you want the sequences of the chains to be the same but you don't need strict physical symmetry.
/// @details see SequenceSymmetricAnnealer for more info
/// @author Jack Maguire, jackmaguire1444@gmail.com


#ifndef INCLUDED_core_pack_task_operation_KeepSequenceSymmetry_hh
#define INCLUDED_core_pack_task_operation_KeepSequenceSymmetry_hh

#include <core/pack/task/operation/KeepSequenceSymmetry.fwd.hh>

#include <core/pack/task/operation/TaskOperation.hh>

// Utility headers
#include <utility/tag/Tag.fwd.hh>
#include <utility/tag/XMLSchemaGeneration.fwd.hh>

namespace core {
namespace pack {
namespace task {
namespace operation {

///@brief Task Operation for turning on the multi-cool annealer
class KeepSequenceSymmetry: public TaskOperation {
public:

	KeepSequenceSymmetry();

	KeepSequenceSymmetry(KeepSequenceSymmetry const & src);

	~KeepSequenceSymmetry() override;

	TaskOperationOP clone() const override;

	/// @brief Configure from a RosettaScripts XML tag.
	void
	parse_tag(
		utility::tag::TagCOP tag,
		basic::datacache::DataMap & ) override;

	//////////////////////

	void
	apply( core::pose::Pose const & pose, core::pack::task::PackerTask & task ) const override;

	/// @brief Return the name used to construct this TaskOperation from an XML file
	static std::string keyname();

	/// @brief Describe the format of XML file used to initialize this TaskOperation
	static void provide_xml_schema( utility::tag::XMLSchemaDefinition & xsd );

	void
	set_setting( bool const setting ){
		setting_ = setting;
	}

	bool setting() const {
		return setting_;
	}

private:
	bool setting_;

};

} //operation
} //task
} //pack
} //core

#endif
