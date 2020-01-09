// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file basic/resource_manager/ResourceOptionsFactory.cxxtest.hh
/// @brief test suite for basic::resource_manager::ResourceOptionsFactory
/// @author Andrew Leaver-Fay (aleaverfay@gmail.com)

// Test headers
#include <cxxtest/TestSuite.h>
#include <test/protocols/init_util.hh>

// Project headers
#include <basic/resource_manager/ResourceLoader.hh>
#include <basic/resource_manager/ResourceLoaderCreator.hh>
#include <basic/resource_manager/ResourceLoaderFactory.hh>

// Utility headers
#include <utility/tag/Tag.hh>
#include <utility/excn/Exceptions.hh>

// C++ headers
#include <string>

//#include <basic/resource_manager/;

using namespace basic::resource_manager;

class DummyResource : public utility::pointer::ReferenceCount
{
private:
	int myint_;
public:
	DummyResource() { myint_ = 5; }
	int intval() const { return myint_; }
};

class DummyResourceLoader : public ResourceLoader {
public:
	DummyResourceLoader() {}

	basic::resource_manager::ResourceCOP
	create_resource(
		basic::resource_manager::ResourceManager & ,
		utility::tag::TagCOP ,
		std::string const &,
		std::istream &
	) const override
	{
		return std::make_shared< DummyResource >();
	}

};

class DummyResourceLoaderCreator : public ResourceLoaderCreator {
public:
	basic::resource_manager::ResourceLoaderOP
	create_resource_loader() const override {
		return std::make_shared< DummyResourceLoader >();
	}


	std::string loader_type() const override { return "DummyResource"; }

	void
	provide_xml_schema( utility::tag::XMLSchemaDefinition & ) const override
	{}

};


class ResourceLoaderFactoryTests : public CxxTest::TestSuite {

public:

	void setUp() {
	}

	// @brief test default ctor
	void test_register_one_creator_with_ResourceLoaderFactory() {

		ResourceLoaderFactory * factory = ResourceLoaderFactory::get_instance();
		factory->set_throw_on_double_registration();
		factory->factory_register( utility::pointer::make_shared< DummyResourceLoaderCreator >() );
		ResourceLoaderOP loader = factory->create_resource_loader( "DummyResource" );
		TS_ASSERT( loader.get() ); // make sure we got back a non-null pointer
		DummyResourceLoader * dloader = dynamic_cast< DummyResourceLoader * > ( loader.get() );
		TS_ASSERT( dloader ); // make sure we got back the right resource loader kind
	}

	void test_register_one_creator_twice_with_ResourceLoaderFactory() {
		try {
			ResourceLoaderFactory * factory = ResourceLoaderFactory::get_instance();
			factory->set_throw_on_double_registration();
			factory->factory_register( utility::pointer::make_shared< DummyResourceLoaderCreator >() );
			TS_ASSERT( false );
		} catch (utility::excn::Exception & e ) {
			std::string expected_error_message = "Double registration of a ResourceLoaderCreator in the ResourceLoaderFactory, named DummyResource. Are there two registrators for this ResourceLoader object, or have you chosen a previously assigned name to a new resource option?";
			TS_ASSERT( e.msg().find(expected_error_message) != std::string::npos );

		}

	}

};
