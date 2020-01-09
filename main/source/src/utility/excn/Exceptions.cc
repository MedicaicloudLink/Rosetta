// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file _Exception
/// @brief  base class for Exception system
/// @details responsibilities:
/// @author Oliver Lange

// Unit Headers
#include <utility/excn/Exceptions.hh>
#include <utility/backtrace.hh>
#include <utility/crash_report.hh>

// Package Headers
#include <string>
#include <ostream>
#include <iostream>

namespace utility {
namespace excn {


Exception::Exception(char const *file, int line, std::string const &msg) :
	msg_ ( msg ),
	file_( file ),
	line_( line ),
	traceback_( backtrace_string() )
{

	//would like to add an option run:no_exceptions
	// * if ( option[ run::no_exceptions ] ) {
	//exit - the hard way:
	//damn can't call virtual from constructor....
	//show( std::cerr );


	//* THE ASSERT MACRO is here that one can find the origin of the EXCEPTION in gdb */
	// debug_assert( false );
	// a better method for this is to issue the command
	// catch throw
	// in the gdb command line... now gdb will stop execution when an Exception
	// http://www.delorie.com/gnu/docs/gdb/gdb_31.html
	/* IN RELEASE MODE THIS HAS CONSTRUCTOR MUST NOT FAIL! --- otherwise the ERROR Msg get's lost! */
}

std::string
Exception::msg() const {
	return std::string("\n\nFile: ") + file_ + ':' + std::to_string(line_) + "\n" + msg_;
}

void
Exception::display() const {
	std::cerr << utility::CSI_Magenta(); // set color of cerr to magenta
	std::cerr << "\n[ ERROR ]: Caught exception:\n";
	std::cerr << msg();
	std::cerr << utility::CSI_Reset() << '\n';
	crash_log();
}

void
Exception::crash_log() const {
	save_crash_report( msg_, file_, line_, traceback_ );
}


void Exception::show( std::ostream& os ) const {
	os << msg() << std::endl;
}

void UserCorrectableIssue::display() const {
	std::cerr << utility::CSI_Magenta(); // set color of cerr to magenta
	std::cerr << "\n---------------------------------------------------------------\n";
	std::cerr << "[ ERROR ]: An issue with your Rosetta run was detected\n";
	std::cerr << "Please correct the following issue and retry:\n\n";
	std::cerr << raw_msg() << "\n";
	std::cerr << "---------------------------------------------------------------\n";
	std::cerr << utility::CSI_Reset() << '\n';
}

void UserCorrectableIssue::crash_log() const {}


}
}
