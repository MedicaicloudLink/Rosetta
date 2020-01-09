// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file   utility/backtrace.hh
/// @brief  Programmatic backtrace whenever you want it.
/// @author Rhiju Das

#ifndef INCLUDED_utility_backtrace_hh
#define INCLUDED_utility_backtrace_hh

//////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////
// Note to devs -- if your build is failing to compile because
// of unrecognized functions backtrace() or backtrace_symbols(),
// then just expand the
//
//   #ifdef _WIN32
//
// to include some tag that goes with your build.
//////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////

#include <cassert>

// Provide workaround for modern compiler feature, if missing
#ifdef __has_include
#define MY__has_include( x ) __has_include( x )
#else
#define MY__has_include( x ) 1
#endif

// Shared with utility/exit.hh
#ifndef NORETURN
#ifdef __GNUC__
#  define NORETURN __attribute__ ((noreturn))
#elif __clang__
#  define NORETURN __attribute__ ((noreturn))
#else
#  define NORETURN
#endif
#endif

/// @brief Function for unit testing only -- if an assertion failure is hit, throw an exception
/// instead of exiting.  Don't let me catch you calling this function from anywhere besides a
/// unit test.  Punishment will be swift.
void set_throw_on_next_assertion_failure();

/// @brief Throw an exception if set_throw_on_next_assertion_failure was called since the
/// last time this function was called.
bool maybe_throw_on_next_assertion_failure( char const * condition );


// C++ headers
#if defined(__GNUC__)  &&  !defined(WIN32)  &&  !defined(__CYGWIN__) && MY__has_include( <cxxabi.h> ) && !defined(ANDROID)

#include <execinfo.h>
#include <cxxabi.h>
#include <string>
#include <cstdio>
#include <stdlib.h>
#include <iostream>

#include <utility/CSI_Sequence.hh>


//////////////////////////////////////////////////////////////////////
//
// See note above if this is causing your build to not compile.
//
// from user 'john' in stackoverflow, but adapted to work on a Mac.
//  and then also re-adapted to work on linux.
//  -- rhiju, 2014
//////////////////////////////////////////////////////////////////////
inline
std::string
demangle( std::string trace ) {

	std::string::size_type begin, end;

	// find the beginning and the end of the useful part of the trace

	// On a Mac, part to be demangled starts with underscore, and then is followed by "+" increment,
	//  separated by spaces from surrounding.
	begin = trace.find(" _") + 1;
	end   = trace.find(" +",begin);

	//How it looks for Linux, with parentheses around part to be demangled.
	// /home/rhiju/src/rosetta/main/source/cmake/build_release/libutility.so(_Z15print_backtracev+0x23) [0x7fb75e08c1a3]
	if ( begin == std::string::npos || end == std::string::npos ) {
		begin = trace.find("(_") + 1;
		end   = trace.find("+",begin);
	}

	// if begina and end were found, we'll go ahead and demangle
	if ( begin != std::string::npos && end != std::string::npos ) {
		std::string mangled_trace = trace.substr(begin, end - begin);
		size_t maxName = 1024;
		int demangleStatus;

		// If output_buffer to __cxa_demangle() is null, memory will be allocated and the pointer returned.
		char* demangledName; // = (char*) malloc(maxName);
		if ( (demangledName = abi::__cxa_demangle(mangled_trace.c_str(), 0, &maxName,
				&demangleStatus)) && demangleStatus == 0 ) {
			trace = trace.substr(0,begin) + demangledName + trace.substr(end ); // the demangled name is now in our trace string
		}
		free(demangledName); // Will handle null pointers gracefully.
	}
	return trace;
}

////////////////////////////////////////////////////////////////////
//
// See note above if this is causing your build to not compile.
//
// stolen directly from
//
// https://developer.apple.com/library/mac/documentation/Darwin/Reference/ManPages/man3/backtrace.3.html
//
// see also:
//
// http://man7.org/linux/man-pages/man3/backtrace.3.html#NOTES
//
//   -- rhiju, 2014.
////////////////////////////////////////////////////////////////////

inline
std::string
backtrace_string(int skip=0) {
	std::string bt_str;

	size_t const callstack_size = 128;
	void* callstack[callstack_size];
	int frames = backtrace(callstack, callstack_size);
	char** strs = backtrace_symbols(callstack, frames);
	for ( int i = skip; i < frames; ++i ) {
		bt_str += demangle(strs[i]);
		bt_str += '\n';
	}
	free(strs);
	return bt_str;
}

inline
bool
print_backtrace( char const * /*unused*/ ) {
	std::cerr << utility::CSI_Magenta(); // set color of cerr to magenta
	std::cerr << "BACKTRACE:\n";
	std::cerr <<  backtrace_string();
	std::cerr << utility::CSI_Reset();
	return false; // allows use in debug_assert
}

#else
// _WIN32, etc.
#include <assert.h>
#include <string>

inline
std::string
backtrace_string(int skip=0) {
	return "";
}

inline
bool
print_backtrace( char const * /*unused*/ ) {
	// no op
	// if someone cares, should be possible to code up a backtrace for Windows!
	// function signature needs to match windows and *nix builds.
	return false; // allows use in debug_assert
}

#endif

#ifdef __clang_analyzer__
/// @details NORETURN to tell the Clang Static Analyzer that we don't continue on if we hit this point.
bool handle_assert_failure( char const * condition, std::string const & file, int const line ) NORETURN;
#else
bool handle_assert_failure( char const * condition, std::string const & file, int const line );
#endif

// When used, this macro must be followed by a semi-colon to be beautified properly.
#define debug_assert(condition) assert( ( condition ) || handle_assert_failure( #condition, __FILE__, __LINE__ ) )


#endif // INCLUDED_utility_backtrace_HH
