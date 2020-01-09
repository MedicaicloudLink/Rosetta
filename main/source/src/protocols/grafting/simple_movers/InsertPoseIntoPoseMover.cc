// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file    protocols/grafting/simple_movers/InsertPoseIntoPoseMover.hh
/// @brief
/// @author  Jared Adolf-Bryfogle

#include <protocols/grafting/simple_movers/InsertPoseIntoPoseMover.hh>
#include <protocols/grafting/simple_movers/InsertPoseIntoPoseMoverCreator.hh>

#include <core/pose/selection.hh>

#include <protocols/moves/Mover.hh>
#include <protocols/rosetta_scripts/util.hh>
#include <protocols/grafting/util.hh>

#include <basic/datacache/DataCache.hh>
#include <utility>
#include <utility/py/PyAssert.hh>
#include <utility/tag/Tag.hh>
// XSD XRW Includes
#include <utility/tag/XMLSchemaGeneration.hh>
#include <protocols/moves/mover_schemas.hh>

namespace protocols {
namespace grafting {
namespace simple_movers {

InsertPoseIntoPoseMover::InsertPoseIntoPoseMover(bool copy_pdbinfo /*false*/):
	protocols::moves::Mover("InsertPoseIntoPoseMover"),
	copy_pdbinfo_(copy_pdbinfo),
	src_pose_(/* NULL */)
{

}

InsertPoseIntoPoseMover::InsertPoseIntoPoseMover(
	const core::pose::Pose& src_pose,
	std::string const & res_start,
	std::string const & res_end,
	bool copy_pdbinfo /*false*/) :
	protocols::moves::Mover("InsertPoseIntoPoseMover"),
	start_(res_start),
	end_(res_end),
	copy_pdbinfo_(copy_pdbinfo)
{
	src_pose_ = utility::pointer::make_shared< core::pose::Pose >(src_pose);
}


InsertPoseIntoPoseMover::InsertPoseIntoPoseMover(const InsertPoseIntoPoseMover& )  = default;


InsertPoseIntoPoseMover::~InsertPoseIntoPoseMover()= default;

void
InsertPoseIntoPoseMover::src_pose(const core::pose::Pose& src_pose){
	src_pose_ = utility::pointer::make_shared< core::pose::Pose >(src_pose);
}

void
InsertPoseIntoPoseMover::start(std::string const & res_start){
	start_ = res_start;
}

void
InsertPoseIntoPoseMover::end(std::string const & res_end){
	end_ = res_end;
}

std::string const &
InsertPoseIntoPoseMover::start() const{
	return start_;
}

std::string const &
InsertPoseIntoPoseMover::end() const {
	return end_;
}

// XRW TEMP std::string
// XRW TEMP InsertPoseIntoPoseMover::get_name() const {
// XRW TEMP  return "InsertPoseIntoPoseMover";
// XRW TEMP }

protocols::moves::MoverOP
InsertPoseIntoPoseMover::clone() const {
	return utility::pointer::make_shared< InsertPoseIntoPoseMover >(*this);
}

void
InsertPoseIntoPoseMover::parse_my_tag(
	TagCOP tag,
	basic::datacache::DataMap& data_map,
	const Filters_map& ,
	const moves::Movers_map& ,
	const Pose& )
{
	start_ = tag->getOption<std::string>("start");
	end_ = tag->getOption<std::string>("end");

	src_pose_ = protocols::rosetta_scripts::saved_reference_pose(tag, data_map, "spm_reference_name");
	copy_pdbinfo_ = tag->getOption<bool>("copy_pdbinfo", false);

}

// XRW TEMP protocols::moves::MoverOP
// XRW TEMP InsertPoseIntoPoseMoverCreator::create_mover() const {
// XRW TEMP  return utility::pointer::make_shared< InsertPoseIntoPoseMover >();
// XRW TEMP }

// XRW TEMP std::string
// XRW TEMP InsertPoseIntoPoseMoverCreator::keyname() const {
// XRW TEMP  return InsertPoseIntoPoseMover::mover_name();
// XRW TEMP }

// XRW TEMP std::string
// XRW TEMP InsertPoseIntoPoseMover::mover_name(){
// XRW TEMP  return "InsertPoseIntoPoseMover";
// XRW TEMP }

void
InsertPoseIntoPoseMover::apply(core::pose::Pose& pose) {

	core::Size start = core::pose::parse_resnum( start_, pose );
	core::Size end = core::pose::parse_resnum( end_, pose );

	PyAssert(start != 0, "Cannot insert region starting with 0 - make sure region is set for InsertPoseIntoPoseMover");
	PyAssert(end !=0, "Cannot insert region ending with 0 - make sure region is set for InsertPoseIntoPoseMover");
	PyAssert(end > start, "Cannot insert into a region where end < start");
	PyAssert(end <= pose.size(), "Cannot insert a region where end is > pose.sizes of the scaffold");

	pose = protocols::grafting::insert_pose_into_pose(pose, *src_pose_, start, end);

}

std::string InsertPoseIntoPoseMover::get_name() const {
	return mover_name();
}

std::string InsertPoseIntoPoseMover::mover_name() {
	return "InsertPoseIntoPoseMover";
}

void InsertPoseIntoPoseMover::provide_xml_schema( utility::tag::XMLSchemaDefinition & xsd )
{
	using namespace utility::tag;
	AttributeList attlist;
	attlist
		+ XMLSchemaAttribute::required_attribute( "start", xs_string, "Insert AFTER this residue" )
		+ XMLSchemaAttribute::required_attribute( "end", xs_string, "Insert BEFORE this residue" )
		+ XMLSchemaAttribute::attribute_w_default( "copy_pdbinfo", xsct_rosetta_bool, "Copy PDB info to new pose?", "false");

	rosetta_scripts::attributes_for_saved_reference_pose( attlist, "spm_reference_name" );

	protocols::moves::xsd_type_definition_w_attributes( xsd, mover_name(),
		"Authors: Jared Adolf-Bryfogle (jadolfbr@gmail.com) and Steven Lewis\n"
		"Inserts a pose between the specified residues of a second pose", attlist );
}

std::string InsertPoseIntoPoseMoverCreator::keyname() const {
	return InsertPoseIntoPoseMover::mover_name();
}

protocols::moves::MoverOP
InsertPoseIntoPoseMoverCreator::create_mover() const {
	return utility::pointer::make_shared< InsertPoseIntoPoseMover >();
}

void InsertPoseIntoPoseMoverCreator::provide_xml_schema( utility::tag::XMLSchemaDefinition & xsd ) const
{
	InsertPoseIntoPoseMover::provide_xml_schema( xsd );
}



}
}
}
