// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file   src/protocols/init/init.cc
/// @brief  Declare WidgetRegistrators as static (global) variables in this .cc file
///         so that at load time, they will be initialized, and the Creator classes
///         they register will be handed to the appropriate WidgetFactory.
/// @author Andrew Leaver-Fay (aleaverfay@gmail.com)

#include <protocols/init/init.hh>
#include <core/init/init.hh>

#include <protocols/init/register1.hh>
#include <protocols/init/register2.hh>
#include <protocols/init/register3.hh>
#include <protocols/init/register4.hh>

namespace protocols {
namespace init {

void register_creators() {
	register1(); // Absolutely needed to make sure the creaters get registered.
	register2(); // Absolutely needed to make sure the creaters get registered.
	register3(); // Absolutely needed to make sure the creaters get registered.
	register4(); // Absolutely needed to make sure the creaters get registered.
}

void init( int argc, char * argv [] )
{
	register_creators();
	core::init::init( argc, argv );
}

void init( utility::vector1< std::string > const & args )
{
	register_creators();
	core::init::init( args );
}

} //namespace init
} //namespace protocols


