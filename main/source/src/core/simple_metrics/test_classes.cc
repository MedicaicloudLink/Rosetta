// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file core/simple_metrics/util.cc
/// @brief Util files for SimpleMetrics.
/// @author Jared Adolf-Bryfogle (jadolfbr@gmail.com)

#include <core/simple_metrics/test_classes.hh>
#include <core/simple_metrics/test_classes_creators.hh>

#include <core/simple_metrics/util.hh>

#include <utility/tag/XMLSchemaGeneration.hh>
#include <basic/Tracer.hh>

#include <core/pose/Pose.hh>
#include <core/select/residue_selector/ResidueSelector.hh>
#include <core/simple_metrics/SimpleMetric.hh>
#include <core/simple_metrics/SimpleMetricFactory.hh>
#include <core/select/util.hh>

#include <utility/excn/Exceptions.hh>
#include <utility/vector1.hh>
#include <utility/tag/Tag.hh>
#include <utility/tag/XMLSchemaGeneration.hh>
#include <utility/tag/util.hh>
#include <utility/string_util.hh>
#include <basic/datacache/DataMap.hh>

static basic::Tracer TR( "core.simple_metrics.util" );


namespace core {
namespace simple_metrics {

using namespace core::select;
using namespace core::select::residue_selector;

std::string
TestStringMetric::name_static(){
	return "TestStringMetric";
}

SimpleMetricOP
TestStringMetric::clone() const {
	return utility::pointer::make_shared< TestStringMetric >( *this );
}

void
TestStringMetric::parse_my_tag(
	utility::tag::TagCOP tag,
	basic::datacache::DataMap & )
{
	SimpleMetric::parse_base_tag( tag );
}

void
TestStringMetric::provide_xml_schema( utility::tag::XMLSchemaDefinition & xsd ) {
	using namespace utility::tag;
	using namespace core::select::residue_selector;

	AttributeList attlist;
	xsd_simple_metric_type_definition_w_attributes(xsd, name_static(),
		"Test class for StringMetrics", attlist);
}

void
TestStringMetricCreator::provide_xml_schema( utility::tag::XMLSchemaDefinition & xsd ) const {
	TestStringMetric::provide_xml_schema( xsd );
}

std::string
TestStringMetricCreator::keyname() const {
	return TestStringMetric::name_static();
}

SimpleMetricOP
TestStringMetricCreator::create_simple_metric() const {
	return utility::pointer::make_shared< TestStringMetric >();

}

/////////////////////
std::string
TestRealMetric::name_static(){
	return "TestRealMetric";
}

SimpleMetricOP
TestRealMetric::clone() const {
	return utility::pointer::make_shared< TestRealMetric >( *this );
}

void
TestRealMetric::parse_my_tag(
	utility::tag::TagCOP tag,
	basic::datacache::DataMap & )
{
	SimpleMetric::parse_base_tag( tag );
}

void
TestRealMetric::provide_xml_schema( utility::tag::XMLSchemaDefinition & xsd ) {
	using namespace utility::tag;
	using namespace core::select::residue_selector;

	AttributeList attlist;
	xsd_simple_metric_type_definition_w_attributes(xsd, name_static(),
		"Test class for RealMetrics", attlist);
}

void
TestRealMetricCreator::provide_xml_schema( utility::tag::XMLSchemaDefinition & xsd ) const {
	TestRealMetric::provide_xml_schema( xsd );
}

std::string
TestRealMetricCreator::keyname() const {
	return TestRealMetric::name_static();
}

SimpleMetricOP
TestRealMetricCreator::create_simple_metric() const {
	return utility::pointer::make_shared< TestRealMetric >();

}

/////////////////////
std::string
TestCompositeStringMetric::name_static(){
	return "TestCompositeStringMetric";
}

SimpleMetricOP
TestCompositeStringMetric::clone() const {
	return utility::pointer::make_shared< TestCompositeStringMetric >( *this );
}

void
TestCompositeStringMetric::parse_my_tag(
	utility::tag::TagCOP tag,
	basic::datacache::DataMap & )
{
	SimpleMetric::parse_base_tag( tag );
}

void
TestCompositeStringMetric::provide_xml_schema( utility::tag::XMLSchemaDefinition & xsd ) {
	using namespace utility::tag;
	using namespace core::select::residue_selector;

	AttributeList attlist;
	xsd_simple_metric_type_definition_w_attributes(xsd, name_static(),
		"Test class for CompositeStringMetrics", attlist);
}

void
TestCompositeStringMetricCreator::provide_xml_schema( utility::tag::XMLSchemaDefinition & xsd ) const {
	TestCompositeStringMetric::provide_xml_schema( xsd );
}

std::string
TestCompositeStringMetricCreator::keyname() const {
	return TestCompositeStringMetric::name_static();
}

SimpleMetricOP
TestCompositeStringMetricCreator::create_simple_metric() const {
	return utility::pointer::make_shared< TestCompositeStringMetric >();

}

/////////////////////
std::string
TestCompositeRealMetric::name_static(){
	return "TestCompositeRealMetric";
}

SimpleMetricOP
TestCompositeRealMetric::clone() const {
	return utility::pointer::make_shared< TestCompositeRealMetric >( *this );
}

void
TestCompositeRealMetric::parse_my_tag(
	utility::tag::TagCOP tag,
	basic::datacache::DataMap & )
{
	SimpleMetric::parse_base_tag( tag );
}

void
TestCompositeRealMetric::provide_xml_schema( utility::tag::XMLSchemaDefinition & xsd ) {
	using namespace utility::tag;
	using namespace core::select::residue_selector;

	AttributeList attlist;
	xsd_simple_metric_type_definition_w_attributes(xsd, name_static(),
		"Test class for RealMetrics", attlist);
}

void
TestCompositeRealMetricCreator::provide_xml_schema( utility::tag::XMLSchemaDefinition & xsd ) const {
	TestCompositeRealMetric::provide_xml_schema( xsd );
}

std::string
TestCompositeRealMetricCreator::keyname() const {
	return TestCompositeRealMetric::name_static();
}

SimpleMetricOP
TestCompositeRealMetricCreator::create_simple_metric() const {
	return utility::pointer::make_shared< TestCompositeRealMetric >();

}

/////////////////////
std::string
TestPerResidueRealMetric::name_static(){
	return "TestPerResidueRealMetric";
}

SimpleMetricOP
TestPerResidueRealMetric::clone() const {
	return utility::pointer::make_shared< TestPerResidueRealMetric >( *this );
}

void
TestPerResidueRealMetric::parse_my_tag(
	utility::tag::TagCOP tag,
	basic::datacache::DataMap & )
{
	SimpleMetric::parse_base_tag( tag );
}

void
TestPerResidueRealMetric::provide_xml_schema( utility::tag::XMLSchemaDefinition & xsd ) {
	using namespace utility::tag;
	using namespace core::select::residue_selector;

	AttributeList attlist;
	xsd_simple_metric_type_definition_w_attributes(xsd, name_static(),
		"Test class for PerResidueRealMetrics", attlist);
}

void
TestPerResidueRealMetricCreator::provide_xml_schema( utility::tag::XMLSchemaDefinition & xsd ) const {
	TestPerResidueRealMetric::provide_xml_schema( xsd );
}

std::string
TestPerResidueRealMetricCreator::keyname() const {
	return TestPerResidueRealMetric::name_static();
}

SimpleMetricOP
TestPerResidueRealMetricCreator::create_simple_metric() const {
	return utility::pointer::make_shared< TestPerResidueRealMetric >();

}


/////////////////////
std::string
TestPerResidueStringMetric::name_static(){
	return "TestPerResidueStringMetric";
}

SimpleMetricOP
TestPerResidueStringMetric::clone() const {
	return utility::pointer::make_shared< TestPerResidueStringMetric >( *this );
}

void
TestPerResidueStringMetric::parse_my_tag(
	utility::tag::TagCOP tag,
	basic::datacache::DataMap & )
{
	SimpleMetric::parse_base_tag( tag );
}

void
TestPerResidueStringMetric::provide_xml_schema( utility::tag::XMLSchemaDefinition & xsd ) {
	using namespace utility::tag;
	using namespace core::select::residue_selector;

	AttributeList attlist;
	xsd_simple_metric_type_definition_w_attributes(xsd, name_static(),
		"Test class for PerResidueStringMetrics", attlist);
}

void
TestPerResidueStringMetricCreator::provide_xml_schema( utility::tag::XMLSchemaDefinition & xsd ) const {
	TestPerResidueStringMetric::provide_xml_schema( xsd );
}

std::string
TestPerResidueStringMetricCreator::keyname() const {
	return TestPerResidueStringMetric::name_static();
}

SimpleMetricOP
TestPerResidueStringMetricCreator::create_simple_metric() const {
	return utility::pointer::make_shared< TestPerResidueStringMetric >();

}

} //core
} //simple_metrics


