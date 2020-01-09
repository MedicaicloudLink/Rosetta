// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file   utility/options/OptionCollection.cc
/// @brief  Program options collection
/// @author Stuart G. Mentzer (Stuart_Mentzer@objexx.com)
/// @author Modified by Sergey Lyskov (Sergey.Lyskov@jhu.edu)
/// @author Modified by Vikram K. Mulligan (vmulligan@flatironinstitute.org) -- added thread safety.

// Unit headers
#include <utility/options/OptionCollection.hh>
// Package headers
#include <utility/options/keys/OptionKeys.hh>
#include <utility/exit.hh>

// ObjexxFCL headers
#include <ObjexxFCL/char.functions.hh>
#include <ObjexxFCL/string.functions.hh>
#include <ObjexxFCL/format.hh>

// Utility headers
#include <utility/vector0.hh>
#include <utility/vector1.hh>
#include <utility/string_util.hh>
#include <utility/io/izstream.hh>
#include <utility/excn/Exceptions.hh>


// C++ headers
#include <algorithm>
#include <iostream>
#include <utility>
//#include <list>


#include <boost/algorithm/string/erase.hpp>

#ifdef    SERIALIZATION
// Utility serialization headers
#include <utility/serialization/serialization.hh>

// Cereal headers
#include <cereal/types/polymorphic.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>
#endif // SERIALIZATION

namespace utility {
namespace options {


void std_exit_wrapper( const int error_code ){
	// Don't raise an exception if we're exiting successfully.
	if ( error_code == EXIT_SUCCESS ) {
		std::exit( error_code );
	}
	throw CREATE_EXCEPTION(utility::excn::Exception, "std::exit() was called" );
}

/// @brief Copy constructor.
/// @details Needed because the mutex can't be copied.
OptionCollection::OptionCollection(
	OptionCollection const & src
) :
	utility::pointer::ReferenceCount( src )
{
	(*this) = src;
}

/// @brief Destructor.
OptionCollection::~OptionCollection()
{
#ifdef MULTI_THREADED
	bool show_accessed_options_val, show_unused_options_val;
	{
		//Lock mutex and copy from globals.
		utility::thread::ReadLockGuard readlock( relevant_accessed_unused_mutex_ );
		show_accessed_options_val = show_accessed_options_;
		show_unused_options_val = show_unused_options_;
	}
	if ( show_accessed_options_val ) show_accessed_options(std::cout);
	if ( show_unused_options_val ) show_unused_options(std::cout);
#else
	if ( show_accessed_options_ ) show_accessed_options(std::cout);
	if ( show_unused_options_ ) show_unused_options(std::cout);
#endif
}

/// @brief Copy assignment operator.
/// @details Needed because the mutext can't be copied.
OptionCollection &
OptionCollection::operator=(
	OptionCollection const & src
) {
	if ( this == &src ) return *this;
	{
#ifdef MULTI_THREADED
		utility::thread::PairedReadLockWriteLockGuard( src.mutex_ /*Gets read-lock.*/, mutex_ /*Gets write-lock.*/ );
#endif
		booleans_ = src.booleans_;
		integers_ = src.integers_;
		reals_ = src.reals_;
		strings_ = src.strings_;
		files_ = src.files_;
		paths_ = src.paths_;
		anys_ = src.anys_;
		boolean_vectors_ = src.boolean_vectors_;
		integer_vectors_ = src.integer_vectors_;
		real_vectors_ = src.real_vectors_;
		residue_chain_vectors_ = src.residue_chain_vectors_;
		string_vectors_ = src.string_vectors_;
		file_vectors_ = src.file_vectors_;
		path_vectors_ = src.path_vectors_;
		any_vectors_ = src.any_vectors_;
		all_ = src.all_;
		argv_copy_ = src.argv_copy_;
	} {
#ifdef MULTI_THREADED
		utility::thread::PairedReadLockWriteLockGuard( src.relevant_accessed_unused_mutex_ /*Gets read-lock.*/, relevant_accessed_unused_mutex_ /*Gets write-lock.*/ );
#endif
		relevant_ = src.relevant_;
		show_accessed_options_ = src.show_accessed_options_;
		show_unused_options_ = src.show_unused_options_;
	}
	return *this;
}


/// @brief Add the built-in options.
/// @note This function calls only threadsafe functions (that lock appropriate mutexes), so it doesn't
/// need to lock any mutexes itself.
void
OptionCollection::add_built_in_options()
{
	using namespace options::OptionKeys;

	{ // Help options
		add( help, "Generate help message and exit" );
	}

	{ // Option display options
		add( options::options, "Option display option group" ).legal( true ).def( true );
		add( options::user, "Show user-specified options" );
		add( options::all, "Show all options" );
		add( options::table::table, "Option table display option group" );
		add( options::table::text, "Generate the option definitions table in text format" );
		add( options::table::Wiki, "Generate the option definitions table in Wiki format" );
		add( options::exit, "Exit after displaying the options" );
	}

	// Check for problems in the option specifications
	check_specs();
}


/// @brief Checks is option has been registered as relevant.
/*static*/ bool
OptionCollection::is_relevant( OptionKey const & key ){
#ifdef MULTI_THREADED
	utility::thread::ReadLockGuard readlock( relevant_accessed_unused_mutex_ );
#endif
	for ( auto & i : relevant_ ) {
		if ( i->id() == key.id() ) return true;
	}
	return false;
}


// /// @brief Specify mutually exclusive options
// void
// OptionCollection::exclusive(
//  OptionKey const & key1,
//  OptionKey const & key2
// )
// {
//  if ( ( (*this)[ key1 ].user() ) && ( (*this)[ key2 ].user() ) ) {
//   std::cerr << "ERROR: Mutually exclusive options specified: " << key1.id() << " and " << key2.id() << std::endl;
//   std_exit_wrapper( EXIT_FAILURE );
//  }
// }


/// @brief Check for problems in the option specifications
/// @note This function calls only threadsafe functions (that lock appropriate mutexes), so it doesn't
/// need to lock any mutexes itself.
void
OptionCollection::check_specs() const
{
	// Initializations
	bool error( false );

	// Check specs
	for ( auto i = OptionKeys::begin(), e = OptionKeys::end(); i != e; ++i ) {
		OptionKey const & key( *i );
		if ( has( key ) ) { // Active option: Check it
			if ( ! option( key ).legal_specs_report() ) {
				error = true;
			}
		}
	}

	// Exit if an error was detected
	if ( error ) std_exit_wrapper( EXIT_FAILURE );
}

/// @brief Load the user-specified option values
void
OptionCollection::load(
	const std::vector<std::string> & args,
	bool const free_args // Support free argument (without - prefix)?
)
{
	using std::string;
	if ( args.size() == 0 ) return;
	// Put the arguments strings in a list
	ValueStrings arg_strings;
	{ //Scope for possible mutex lock
#ifdef MULTI_THREADED
		utility::thread::WriteLockGuard writelock( mutex_ );
#endif
		for ( auto const & arg : args ) {
			arg_strings.push_back( arg );
			argv_copy_ += " " + arg;
		}
	} //Mutex lock scope

	load( "", arg_strings, free_args);
} // load


/// @brief Load the user-specified option values
void
OptionCollection::load(
	int const argc,
	char * const argv[],
	bool const free_args // Support free argument (without - prefix)?
)
{
	using std::string;

	// Put the arguments strings in a list
	ValueStrings arg_strings;
	{ //Mutex scope
#ifdef MULTI_THREADED
		utility::thread::WriteLockGuard writelock( mutex_ );
#endif
		for ( int iarg = 1; iarg < argc; ++iarg ) {
			arg_strings.emplace_back(argv[ iarg ] );
			std::string temp( argv[ iarg ] );
			argv_copy_ += " " + temp;
		}
	}

	load( std::string(argv[0]), arg_strings, free_args);
} // load


/// @note This function calls only threadsafe functions (that lock appropriate mutexes), so it doesn't
/// need to lock any mutexes itself.
void
OptionCollection::load(
	std::string executable_name, // usually argv[ 0 ]
	ValueStrings& arg_strings,
	bool const free_args // Support free argument (without - prefix)?
){
	using std::string;
	using size_type = std::string::size_type;

	// Load the options
	string cid; // Context option id
	while ( ! arg_strings.empty() ) { // Process the next option
		string arg_string( arg_strings.front() ); // Lead argument string
		arg_strings.pop_front(); // Remove lead argument

		char const arg_first( arg_string[ 0 ] );
		if ( ( arg_first == '-' ) && ( ! ObjexxFCL::is_double( arg_string ) ) ) { // - prefix: Treat as an option

			// Load the option
			//std::cout << "load_option_c: arg_string=" << arg_string << "\n";
			//for(ValueStrings::iterator it=arg_strings.begin(); it!=arg_strings.end(); it++)
			// std::cout << "arg_strings[] =" << *it << "\n";

			// Taking care of '-@MyOption' case
			if ( arg_string.size() > 1 ) {
				if ( arg_string[1] == '@' ) {
					load_option_from_file(arg_string, arg_strings, cid); //this function still has std_exit_wrapper for failures
					continue;
				} else if ( arg_string[1] == '-' ) {
					arg_string = arg_string.substr(1);
				}
			}
			load_option_cl( arg_string, arg_strings, cid ); //might throw exception
		} else if ( arg_first == '@' ) { // @ prefix: Treat as an option file
			// Parse argument to get file specification string
			size_type const fb( arg_string.find_first_not_of( "@\"" ) );
			string file_string;
			if ( fb == string::npos ) { // -...-
				// This should be the "@ ../filename" 'tab-completion' case
				file_string = arg_strings.front();
				arg_strings.pop_front(); // remove next argument
			} else {
				size_type const fe( arg_string.find_last_not_of( '"' ) );
				file_string = ( fb <= fe ? arg_string.substr( fb, fe - fb + 1 ) : string() );
			}

			load_options_from_file(ObjexxFCL::trim(file_string), cid);

		} else if ( ! free_args ) { // Warn about free argument
			throw( CREATE_EXCEPTION(excn::Exception,  "ERROR: Unused \"free\" argument specified: " + arg_string ));
		}
	}

	// Check for problems in the option values
	check_values();

	{ // Generate any requested option outputs
		using namespace utility::options::OptionKeys;

		// Help
		if ( option( help ) ) { // Display help and exit
			std::cout << "\nUsage:\n\n" << executable_name << " [options]\n";
			show_help_hier( std::cout );
			std_exit_wrapper( EXIT_SUCCESS );
		}

		// Options displays
		bool showed( false );
		if ( option( options::user ) ) { // Show the specified options
			show_user( std::cout );
			showed = true;
		}
		if ( option( options::all ) ) { // Show all the options
			show_all_hier( std::cout );
			showed = true;
		}
		if ( option( options::table::text ) ) { // Show the options definitions table in text format
			show_table_text( std::cout );
			showed = true;
		}
		if ( option( options::table::Wiki ) ) { // Show the options definitions table in Wiki format
			show_table_Wiki( std::cout );
			showed = true;
		}
		if ( ( option( options::table::table ).user() ) &&
				( ! option( options::table::text ) ) &&
				( ! option( options::table::Wiki ) ) ) { // User specified -options:table : Default to -options:table:text
			show_table_text( std::cout );
			showed = true;
		}
		if ( ( ! showed ) && ( option( options::options ).user() ) ) { // User specified -options : Default to -options:user
			show_user( std::cout );
			showed = true;
		}
		if ( ( showed ) && ( option( options::exit ) ) ) { // Show all the options
			std_exit_wrapper( EXIT_SUCCESS );
		}

	}


}


/// @brief Load all options in a flags file
/// @note This function calls only threadsafe functions (that lock appropriate mutexes), so it doesn't
/// need to lock any mutexes itself.
void OptionCollection::load_options_from_stream(
	std::istream& stream,
	std::string const & file_string,
	std::string const & cid,
	bool print_lines /* false */
) {
	using std::string;
	using size_type = std::string::size_type;

	// Read and load the options from the file
	using std::make_pair;
	string key_id( cid ); // Local context id
	string line;
	char const SPACE( ' ' );
	char const TAB( '\t' );
	string const QUOTES( "\"'" );
	string const WHITESPACE( " \t" );
	string const FIRST_SEP( "= \t#" );
	string const FIRST_SEP_NC( "= \t" );
	string const AFTER_SEP( " \t#" );
	enum { UNKNOWN_INDENT, SPACE_INDENT, TAB_INDENT } indent_type( UNKNOWN_INDENT );
	typedef  std::pair< int, std::string >  IndentContext;
	using ContextStack = utility::vector0<IndentContext>;
	ContextStack context_stack( 1, make_pair( 0, cid ) );
	while ( stream ) {
		using ObjexxFCL::has;
		using ObjexxFCL::is_any_of;
		using ObjexxFCL::len_trim_whitespace;
		using ObjexxFCL::strip_whitespace;
		using ObjexxFCL::stripped_whitespace;

		//std::getline( stream, line );
		string ln;  //< Logic for '\' character in options file
		line = "";
		bool f = true;
		while ( f ) {
			std::getline( stream, ln );//stream.getline( ln );
			f = false;

			if ( ln.size() > 0 ) {
				if ( ln[ ln.size()-1 ] == '\\' ) {
					ln.resize( ln.size()-1 );
					line = line + ln;
					f = true;
				}
			}
		}
		line = line + ln;

		line = replace_environment_variables(line);

		if ( ( stream ) && ( len_trim_whitespace( line ) > 0 ) &&
				( stripped_whitespace( line )[ 0 ] == '-' ) ) { // Process option on line
			string const indent( line.substr( 0, line.find_first_not_of( WHITESPACE ) ) ); // Line indentation
			strip_whitespace( line );
			size_type const line_length( line.length() );

			if ( print_lines ) {
				std::cout << line << std::endl;
			}
			// Tokenize the line into option identifier and values
			size_type vb( line.find_first_of( FIRST_SEP ) ), ve(0); // Begin/end token indexes
			string const opt_string( vb == string::npos ? line : line.substr( 0, vb ) );
			if ( vb != string::npos ) vb = line.find_first_not_of( FIRST_SEP_NC, vb );
			if ( ( vb != string::npos ) && ( line[ vb ] == '#' ) ) { // Rest is comment
				vb = string::npos;
			}
			ValueStrings val_strings;
			while ( ( vb != string::npos ) && ( line[ vb ] != '#' ) ) { // Tokenize the values
				if ( is_any_of( line[ vb ], QUOTES ) ) { // Quoted value
					char const quote( line[ vb ] ); // Quote character being used
					vb = line.find_first_not_of( QUOTES, vb ); // Skip all the quotes
					if ( vb != string::npos ) {
						ve = line.find( quote, vb ); // Find matching quote
						if ( ve == string::npos ) ve = line_length; // Take rest of line if unclosed quote
						if ( vb < ve ) { // Non-empty value string
							val_strings.push_back( line.substr( vb, ve - vb ) );
						}
						if ( ve < line_length ) {
							ve = line.find_first_not_of( QUOTES, ve ); // Skip past quotes
							if ( ve == string::npos ) ve = line_length; // Make sure we stop looking
						}
					}
				} else { // Non-quoted value
					ve = line.find_first_of( AFTER_SEP, vb );
					if ( ve == string::npos ) ve = line_length; // Take rest of line if no more whitespace
					if ( vb < ve ) { // Non-empty value string
						val_strings.push_back( line.substr( vb, ve - vb ) );
					}
				}
				vb = ( ( ve < line_length ) && ( line[ ve ] != '#' ) ?
					line.find_first_not_of( WHITESPACE, ve ) :
					string::npos ); // Start of next token
			}

			// Load the option
			if ( indent.empty() ) { // Indent level == 0: Use command line context
				key_id = cid;
				load_option_file( opt_string, val_strings, key_id, true );
				context_stack.resize( 1 );
				context_stack.push_back( make_pair( 0, key_id ) );
			} else { // Find the indent level and load the option
				if ( indent_type == UNKNOWN_INDENT ) { // Set the indent type
					if ( has( indent, SPACE ) ) {
						indent_type = SPACE_INDENT;
					} else {
						indent_type = TAB_INDENT;
					}
				}
				if ( ( ( indent_type == SPACE_INDENT ) && ( has( indent, TAB ) ) ) ||
						( ( indent_type == TAB_INDENT ) && ( has( indent, SPACE ) ) ) ) {
					throw( CREATE_EXCEPTION(excn::Exception,  "Option file has mixed space and tab indent characters: "+file_string ) );
					//     std::cerr << "ERROR: Option file has mixed space and tab indent characters: "
					//      << file_string << std::endl;
					//     std_exit_wrapper( EXIT_FAILURE );
				}
				int const n_indent( indent.size() ); // Number of indent characters
				ContextStack::size_type j( context_stack.size() );
				while ( ( j > 0 ) && ( context_stack[ j - 1 ].first >= n_indent ) ) {
					--j;
				}
				if ( j == 0 ) { // Use command line context
					key_id = cid;
					load_option_file( opt_string, val_strings, key_id, true );
				} else { // Use indented @file context
					key_id = context_stack[ j - 1 ].second;
					load_option_file( opt_string, val_strings, key_id );
				}
				if ( j < context_stack.size() ) context_stack.resize( j ); // Remove obs. part of context
				context_stack.push_back( make_pair( n_indent, key_id ) );
			}
		} else if ( ( stream ) && ( len_trim_whitespace( line ) > 0 ) &&
				( stripped_whitespace( line )[ 0 ] == '@' ) ) { // Process other option file
			// Parse argument to get file specification string
			size_type const fb( line.find_first_not_of( "@\"" ) );
			if ( fb == string::npos ) { // -...-
				throw( CREATE_EXCEPTION(excn::Exception,  "ERROR: Unsupported option file specification: " + line ));
			}
			size_type const fe( line.find_last_not_of( "\" " ) );
			string file_string2( fb <= fe ? line.substr( fb, fe - fb + 1 ) : string() );

			load_options_from_file(ObjexxFCL::trim(file_string2), cid);
		} else if ( (stream ) && len_trim_whitespace(line) > 0 && stripped_whitespace(line)[0] != '#' ) {
			throw(CREATE_EXCEPTION(excn::Exception,  "Comments in an option file must begin with '#', options must begin with '-' the line:\n"
				+stripped_whitespace(line)+"\n is incorrectly formatted"));
		}
	}
}

/// @brief Load all options in a flags file
/// @note This function calls only threadsafe functions (that lock appropriate mutexes), so it doesn't
/// need to lock any mutexes itself.
void OptionCollection::load_options_from_file(std::string const & file_string, std::string const & cid){
	load_options_from_file_exception( file_string, cid );
}

/// @brief Load all options in a flags file
/// @note This function calls only threadsafe functions (that lock appropriate mutexes), so it doesn't
/// need to lock any mutexes itself.
void OptionCollection::load_options_from_file_exception(std::string const & file_string, std::string const & cid){

	utility::io::izstream stream( file_string.c_str() );
	if ( ! stream ) {
		throw( CREATE_EXCEPTION(excn::Exception,  "Option file open failed for: '"+file_string+"'" ) );
	}
	load_options_from_stream( stream, file_string, cid );
}


/// @brief Load one option from user specified file
/// @note This function calls only threadsafe functions (that lock appropriate mutexes), so it doesn't
/// need to lock any mutexes itself.
void OptionCollection::load_option_from_file(
	std::string const & arg_string_, // Lead argument string
	ValueStrings & arg_strings, // Argument strings: Value string(s) in front
	std::string const & cid // Previous option id
) {
	using std::string;

	string arg_string(arg_string_);
	if ( arg_string.size() > 1 ) arg_string.erase(1, 1); // erase '@' sign from option name

	// Now next argument *should* be a file name
	if ( arg_strings.size() < 1 ) {
		throw( CREATE_EXCEPTION(excn::Exception,  "ERROR: No file name supplied for option: "+ arg_string));
	}

	// Open the option file
	string const file_name( arg_strings.front() ); // Lead argument string
	throw( CREATE_EXCEPTION(excn::Exception,  "load_option from file:"+file_name));

	arg_strings.pop_front(); // Remove lead argument
	utility::io::izstream stream( file_name.c_str() );
	if ( ! stream ) {
		throw( CREATE_EXCEPTION(excn::Exception,  "ERROR: Option file open failed for: "+file_name));
	}
	string res;
	while ( stream ) {
		string ln;
		getline( stream, ln ); //.getline( ln );
		res += ln + " ";
	}
	stream.close();

	ValueStrings f_args( split_to_list(res) );

	//std::cout << "F:load_option_c: arg_string=" << arg_string << "\n";
	//for(ValueStrings::iterator it=f_args.begin(); it!=f_args.end(); it++)
	// std::cout << "F:arg_strings[] =" << *it << "\n";
	load_option_cl( arg_string, f_args, cid );
}




/// @brief Check for problems in the option values
/// @note This function calls only threadsafe functions (that lock appropriate mutexes), so it doesn't
/// need to lock any mutexes itself.
void
OptionCollection::check_values() const
{
	// Initializations
	bool error( false );

	// Check values are legal
	for ( auto i = OptionKeys::begin(), e = OptionKeys::end(); i != e; ++i ) {
		OptionKey const & key( *i );
		if ( has( key ) ) { // Active option: Check it
			if ( ! option( key ).legal_report() ) error = true;
		}
	}

	// Exit if an error was detected
	if ( error ) std_exit_wrapper( EXIT_FAILURE );
	//  only called in ::load --- no exception, keep the hard - exit
}


/// @note This function calls only threadsafe functions (that lock appropriate mutexes), so it doesn't
/// need to lock any mutexes itself.
void OptionCollection::show_option_help(OptionKey const &key, std::string &group, std::ostream & stream )
{
	using std::string;
	using size_type = std::string::size_type;

	if ( has( key ) ) { // Active option
		Option const & opt( option( key ) );
		std::string const opt_group( prefix( opt.id() ) );
		if ( opt_group != group ) { // New group
			stream << '\n'; // Spacer line between groups
			group = opt_group;
		}
		size_type const d( 2 + opt.id().length() + 3 ); // Description indent spaces
		stream
			<< wrapped(
			" -" + opt.id() + "   " +
			opt.description() +
			space_prefixed( opt.legal_string(), 2 ) +
			space_prefixed( opt.default_string(), 2 ),
			std::min( d, size_type( 20 ) ) )
			<< '\n';
	}
}

/// @brief Show all the options and their descriptions
void
OptionCollection::show_help( std::ostream & stream )
{
	using std::string;

	stream << "\nOptions:   [Specify on command line or in @file]\n";
	string group; // Previous option group name

#ifdef MULTI_THREADED
	utility::thread::ReadLockGuard readlock( relevant_accessed_unused_mutex_ );
#endif
	if ( relevant_.size() > 0 ) {
		stream << "\nShowing only relevant options...";
		for ( auto & i : relevant_ ) {
			OptionKey const & key( *i );
			show_option_help(key, group, stream);
		}
	} else {
		for ( auto i = OptionKeys::begin(), e = OptionKeys::end(); i != e; ++i ) {
			OptionKey const & key( *i );
			show_option_help(key, group, stream);
		}
	}
}

#define COL1 30
#define COL2 25
#define COL3 30

/// @note This function calls only threadsafe functions (that lock appropriate mutexes), so it doesn't
/// need to lock any mutexes itself.
void OptionCollection::show_option_help_hier(OptionKey const &key, std::string &group, std::ostream & stream )
{
	using std::string;

	if ( has( key ) ) { // Active option
		Option const & opt( option( key ) );
		string const opt_group( prefix( opt.id() , n_part( opt.id() )-1) );
		string const opt_str( suffix( opt.id() ) );
		bool const bShowGrp ( opt_group != group ); // New group
		group = opt_group;

		using namespace ObjexxFCL::format;

		std::ostringstream empty_separator;
		empty_separator << RJ( COL1-3, "" )<<" " << RJ( 3, "" ) << "| " << A( COL2, "" ) << " | " << A(3,"") << "|";
		if ( bShowGrp ) {
			stream << empty_separator.str() << "\n";
			if ( opt_group.size() > 0 ) {
				stream << RJ( COL1-3, opt_group )<<":" << RJ( 3, "" ) << "| " << A( COL2, "" ) << " | " << A(3,"") << "| \n";
			}
		};
		if ( opt_str.size() <= COL1 ) {
			stream << RJ( COL1, opt_str ) << " | "  << A( COL2, opt.value_string() ) //<< A( COL2/2, opt.default_string() )
				<< " |" + A(4,opt.type_string())+ "| "
				<<  wrapped( opt.description(), 2, COL3, empty_separator.str() ) << "\n";
		} else {
			platform::Size col2_adjusted = COL2 - ( opt_str.size() - COL1 );
			stream << RJ( COL1, opt_str ) << " | "  << A( col2_adjusted, opt.value_string() ) //<< A( COL2/2, opt.default_string() )
				<< " |" + A(4,opt.type_string())+ "| "
				<<  wrapped( opt.description(), 2, COL3, empty_separator.str() ) << "\n";
		}
	}
}

/// @brief Show all the options and their descriptions in a hierarchy format
void
OptionCollection::show_help_hier( std::ostream & stream )
{
	using std::string;
	using namespace ObjexxFCL::format;
	stream << "\nOptions:   [Specify on command line or in @file]\n";
	string group; // Previous option group name

#ifdef MULTI_THREADED
	utility::thread::ReadLockGuard readlock( relevant_accessed_unused_mutex_ );
#endif

	if ( relevant_.size() > 0 ) {
		stream << "\nShowing only relevant options...\n\n\n";
		stream << RJ( COL1, "Option" ) << " | " << A( COL2, " Setting " ) << " |" << A(4,"Type") << "| "  << LJ( COL3, " Description" ) << "\n";
		stream << "--------------------------------------------------------------------------------------\n";
		for ( auto & i : relevant_ ) {
			OptionKey const & key( *i );
			show_option_help_hier( key, group, stream );
		}
	} else {
		for ( auto i = OptionKeys::begin(), e = OptionKeys::end(); i != e; ++i ) {
			OptionKey const & key( *i );
			show_option_help_hier( key, group, stream );
		}
	}
}


/// @brief Show the user-specified options and their values
void
OptionCollection::show_user( std::ostream & stream ) const
{
	using std::string;

	stream << "\n# User Specified Options:\n";
	string group; // Previous option group name
	for ( auto i = OptionKeys::begin(), e = OptionKeys::end(); i != e; ++i ) {
		OptionKey const & key( *i );
		if ( has( key ) ) { // Active option
			Option const & opt( option( key ) );
			bool const save_access_status( opt.is_been_accessed() );
			if ( opt.user() ) { //user changes access status ... here clearly not wanted.
				string const opt_group( prefix( opt.id() ) );
				if ( opt_group != group ) { // New group
					stream << '\n'; // Spacer line between groups
					group = opt_group;
				}
				stream << "-" << opt.id() << " " << opt.value_string() << '\n';
			}
			opt.set_accessed( save_access_status );
		}
	}
	stream << '\n';
}


/// @brief Show the user-specified options and their values
void
OptionCollection::show_inaccessed_user_options( std::ostream & stream ) const
{
	using std::string;

	for ( auto i = OptionKeys::begin(), e = OptionKeys::end(); i != e; ++i ) {
		OptionKey const & key( *i );
		if ( has( key ) ) { // Active option
			Option const& opt( option( key ) );
			if ( !opt.is_been_accessed() && opt.user() ) { //user accesses this option ... we have to set it back...
				opt.set_accessed( false );
				stream << "-" << opt.id() << " " << opt.value_string() << '\n';
			}
		}
	}
}


/// @brief Show all the options and their values
void
OptionCollection::show_all( std::ostream & stream ) const
{
	using std::string;

	stream << "\nOptions:   [Specify on command line or in @file]\n";
	string group; // Previous option group name
	for ( auto i = OptionKeys::begin(), e = OptionKeys::end(); i != e; ++i ) {
		OptionKey const & key( *i );
		if ( has( key ) ) { // Active option
			Option const & opt( option( key ) );
			string const opt_group( prefix( opt.id() ) );
			if ( opt_group != group ) { // New group
				stream << '\n'; // Spacer line between groups
				group = opt_group;
			}
			stream << " -" << opt.id() << opt.equals_string() << '\n';
		}
	}
	stream << '\n';
}


/// @brief Show all the options and their values in a hierarchy format
void
OptionCollection::show_all_hier( std::ostream & stream ) const
{
	using std::string;
	using size_type = std::string::size_type;

	stream << "\nOptions:   [Specify on command line or in @file]\n";
	string group; // Previous option group name
	for ( auto i = OptionKeys::begin(), e = OptionKeys::end(); i != e; ++i ) {
		OptionKey const & key( *i );
		if ( has( key ) ) { // Active option
			Option const & opt( option( key ) );
			string const opt_group( prefix( opt.id() ) );
			if ( opt_group != group ) { // New group
				stream << '\n'; // Spacer line between groups
				stream << " -" << opt.id() << opt.equals_string() << '\n';
				group = opt_group;
			} else { // Indent and remove prefix in common with previous id
				size_type const l( n_part( opt.id() ) - 1 ); // Indent level
				stream << string( l, ' ' ) << " -" << suffix( opt.id() ) << opt.equals_string() << '\n';
			}
		}
	}
	stream << '\n';
}


/// @brief Show the options definitions table in text format
void
OptionCollection::show_table_text( std::ostream & stream ) const
{
	using std::string;
	using size_type = std::string::size_type;

	stream << "\nOption Definitions Table\n";
	string group; // Previous option group name
	for ( auto i = OptionKeys::begin(), e = OptionKeys::end(); i != e; ++i ) {
		OptionKey const & key( *i );
		if ( has( key ) ) { // Active option
			Option const & opt( option( key ) );
			string const opt_group( prefix( opt.id() ) );
			if ( opt_group != group ) { // New group
				stream << '\n'; // Spacer line between groups
				stream << " -" << opt.id() << "   " << opt.description() << '\n';
				group = opt_group;
			} else { // Indent and remove prefix in common with previous id
				size_type const l( n_part( opt.id() ) - 1 ); // Indent level
				stream << string( l, ' ' ) << " -" << suffix( opt.id() ) << '\t' << opt.description() << '\t'
					<< opt.type_string() << '\t'
					<< opt.legal_string() << '\t'
					<< opt.default_string()
					<< '\n';
			}
		}
	}
	stream << '\n';
}


/// @brief Show the options definitions table in Wiki format
/// @note  Based on Sergey Lyskov's Python Wiki table generator code
void
OptionCollection::show_table_Wiki( std::ostream & stream ) const
{
	using std::string;

	string group; // Previous option group name
	for ( auto i = OptionKeys::begin(), e = OptionKeys::end(); i != e; ++i ) {
		OptionKey const & key( *i );
		if ( has( key ) ) { // Active option
			Option const & opt( option( key ) );
			string const opt_group( prefix( opt.id() ) );
			if ( opt_group != group ) { // New group
				if ( ! group.empty() ) stream << " |}\n"; // Closing previous table if any
				stream << "{| border=\"1\" cellpadding=\"10\" width=\"100%\"\n |+ '''" << prefix( opt.id() ) << " Option Group'''\n";
				stream << " ! Option name\n";
				stream << " ! Description\n";
				stream << " ! Range\n";
				stream << " ! Default\n";
				stream << " ! Old name\n";
				stream << " |-\n";
				group = opt_group;
			}
			stream << " |-\n";
			stream << " | -" << suffix( opt.id() ) << " <" << opt.type_string() << ">\n";
			stream << " | " << opt.description() << '\n';
			stream << " | " << opt.legal_string() << '\n';
			stream << " | " << opt.default_string() << '\n';
			stream << " | \n"; // Don't have the oldName in the C++ option
			stream << " |-\n";
		}
	}
	stream << " |}\n";
}


/// Local helper function for implementing show_accessed_options
template< class T >
void show_accessed_options_T(T i, T e, std::vector< std::string > &sv) {
	for ( ; i != e; ++i ) {
		Option const & opt( *i );
		if ( opt.is_been_accessed() ) {
			std::ostringstream buf;
			buf << opt.id();
			sv.push_back( buf.str() );
		}
	}
}

void OptionCollection::show_accessed_options(std::ostream & stream) const {
	using std::string;

	stream << "OptionCollection::show_accessed_options flag has been set, listing all accessed options..." << std::endl;

#ifdef MULTI_THREADED
	utility::thread::ReadLockGuard readlock( mutex_ );
#endif

	std::vector< std::string > sv;
	show_accessed_options_T(booleans_.begin(),        booleans_.end(), sv);
	show_accessed_options_T(integers_.begin(),        integers_.end(), sv);
	show_accessed_options_T(reals_.begin(),           reals_.end(), sv);
	show_accessed_options_T(strings_.begin(),         strings_.end(), sv);
	show_accessed_options_T(files_.begin(),           files_.end(), sv);
	show_accessed_options_T(paths_.begin(),           paths_.end(), sv);
	show_accessed_options_T(boolean_vectors_.begin(), boolean_vectors_.end(), sv);
	show_accessed_options_T(integer_vectors_.begin(), integer_vectors_.end(), sv);
	show_accessed_options_T(real_vectors_.begin(),    real_vectors_.end(), sv);
	show_accessed_options_T(string_vectors_.begin(),  string_vectors_.end(), sv);
	show_accessed_options_T(file_vectors_.begin(),    file_vectors_.end(), sv);
	show_accessed_options_T(path_vectors_.begin(),    path_vectors_.end(), sv);

	std::sort(sv.begin(), sv.end());

	for ( auto & i : sv ) stream << i << '\n';

	stream << std::endl;
}


/// Local helper function for implementing show_unused_options
template< class T >
void show_unused_options_T(T i, T e, std::vector< std::string > &sv) {
	for ( ; i != e; ++i ) {
		Option const & opt( *i );
		// We store the accessed status because user() will alter it.
		bool accessed = opt.is_been_accessed();
		// Don't count option groups, which are set by the nested
		// syntax of flags files, but are never used.
		if ( !accessed && opt.user() && ! opt.is_group() ) {
			std::ostringstream buf;
			buf << "-" << opt.id() << " " << opt.value_string();
			sv.push_back( buf.str() );
		}
		opt.set_accessed( accessed ); // Reset accessed to original state.
	}
}

void OptionCollection::show_unused_options(std::ostream & stream) const {
	using std::string;

#ifdef MULTI_THREADED
	utility::thread::ReadLockGuard readlock( mutex_ );
#endif

	std::vector< std::string > sv;
	show_unused_options_T(booleans_.begin(),        booleans_.end(), sv);
	show_unused_options_T(integers_.begin(),        integers_.end(), sv);
	show_unused_options_T(reals_.begin(),           reals_.end(), sv);
	show_unused_options_T(strings_.begin(),         strings_.end(), sv);
	show_unused_options_T(files_.begin(),           files_.end(), sv);
	show_unused_options_T(paths_.begin(),           paths_.end(), sv);
	show_unused_options_T(boolean_vectors_.begin(), boolean_vectors_.end(), sv);
	show_unused_options_T(integer_vectors_.begin(), integer_vectors_.end(), sv);
	show_unused_options_T(real_vectors_.begin(),    real_vectors_.end(), sv);
	show_unused_options_T(string_vectors_.begin(),  string_vectors_.end(), sv);
	show_unused_options_T(file_vectors_.begin(),    file_vectors_.end(), sv);
	show_unused_options_T(path_vectors_.begin(),    path_vectors_.end(), sv);

	if ( sv.size() > 0 ) {
		std::sort(sv.begin(), sv.end());

		// '\n' instead of std::endl so that the options won't get prefixed if stream is a tracer.
		stream << "The following options have been set, but have not yet been used:\n";
		for ( auto & i : sv ) {
			stream << "\t" << i << '\n';
		}
		stream << std::endl; // Now we flush
	}
}

std::string
OptionCollection::get_argv() const {
#ifdef MULTI_THREADED
	utility::thread::ReadLockGuard readlock( mutex_ );
#endif
	return argv_copy_;
}

/// @brief Output to stream
std::ostream &
operator <<( std::ostream & stream, OptionCollection const & options )
{
	for ( auto i = OptionKeys::begin(), e = OptionKeys::end(); i != e; ++i ) {
		OptionKey const & key( *i );
		if ( options.has( key ) ) { // Active option
			Option const & opt( options.option( key ) );
			bool const save_access_status( opt.is_been_accessed() );
			if ( opt.user() ) { // Will change access status -- probably not what we want for this sort of general printing
				stream << " -" << opt.id() << opt.equals_string();
			}
			opt.set_accessed( save_access_status );
		}
	}
	return stream;
}


/// @brief Load a user-specified option argument from a command line
void
OptionCollection::load_option_cl(
	std::string const & arg_string, // Lead argument string
	ValueStrings & arg_strings, // Argument strings: Value string(s) in front
	std::string const & cid // Context option id
)
{
	using std::string;
	using ObjexxFCL::stripped_whitespace;
	using size_type = std::string::size_type;

	// Parse argument into key and value strings
	bool top( false ); // Top-level context?
	size_type kb( arg_string.find_first_not_of( '-' ) );
	char const COLON( ':' );
	if ( ( kb != string::npos ) && ( arg_string[ kb ] == COLON ) ) { // Top level context
		kb = arg_string.find_first_not_of( COLON, kb );
		top = true;
	}
	if ( kb == string::npos ) { // -...-
		throw ( CREATE_EXCEPTION(excn::Exception,  "Unsupported option specified: " + arg_string ) );
		//std::cerr << "ERROR: Unsupported option specified: " << arg_string << std::endl;
		//std_exit_wrapper( EXIT_FAILURE );
	}
	size_type const ke( arg_string.find_first_of( "= \t" ) );
	bool const ends( ke != string::npos );
	string const key_string( cleaned( ends ? arg_string.substr( kb, ke - kb ) : arg_string.substr( kb ) ) );
	string const val_string( ( ends ) && ( ke + 1 < arg_string.length() ) ?
		stripped_whitespace( arg_string.substr( ke + 1 ) ) : string() );
	if ( ! val_string.empty() ) arg_strings.push_front( val_string ); // Put in front of value strings

	// Find the option and set its value
	string const key_id( find_key_cl( key_string, cid, top ) );
	set_option_value_cl( key_id, arg_strings );

	// Update the context id
	// Actually, this is very annoying in practice.
	// Require cmd line arg names to either be (globally) unique or fully-qualified.
	//cid = key_id;
}


/// @brief Load a user-specified option argument from an @file
void
OptionCollection::load_option_file(
	std::string const & arg_string, // Argument string
	ValueStrings & val_strings, // Value strings
	std::string & cid, // Context option id
	bool const cl_context // Use command line key context?
)
{
	using std::string;
	using ObjexxFCL::stripped_whitespace;
	using size_type = std::string::size_type;

	// std::cout << "load_option_file: arg_string=" << arg_string << "\n";
	// for(ValueStrings::iterator it=val_strings.begin(); it!=val_strings.end(); it++)
	// std::cout << "val_strings[] =" << *it << "\n";

	// Taking care of '-@MyOption' case
	if ( arg_string.size() > 1 ) {
		if ( arg_string[1] == '@' ) {
			load_option_from_file(arg_string, val_strings, cid);
			return;
		}
	}


	// Parse argument into key and value strings
	bool top( false ); // Top-level context?
	size_type kb( arg_string.find_first_not_of( '-' ) );
	char const COLON( ':' );
	if ( ( kb != string::npos ) && ( arg_string[ kb ] == COLON ) ) { // Top level context
		kb = arg_string.find_first_not_of( COLON, kb );
		top = true;
	}
	if ( kb == string::npos ) { // -...-
		throw( CREATE_EXCEPTION(excn::Exception,  "Unsupported option specified: "+arg_string ) );
	}
	size_type const ke( arg_string.find_first_of( "= \t" ) );
	bool const ends( ke != string::npos );
	string const key_string( cleaned( ends ? arg_string.substr( kb, ke - kb ) : arg_string.substr( kb ) ) );
	string const val_string( ( ends ) && ( ke + 1 < arg_string.length() ) ?
		stripped_whitespace( arg_string.substr( ke + 1 ) ) : string() );
	if ( ! val_string.empty() ) val_strings.push_front( val_string ); // Put in front of value strings

	// Find the option and set its value
	string const key_id( cl_context ? find_key_cl( key_string, cid, top ) : find_key_file( key_string, cid, top ) );
	set_option_value_file( key_id, val_strings );

	// Update the context id
	cid = key_id;
}


/// @brief Set a user-specified option value from a command line
/// note Decides whether to use value strings and erases the ones used
void
OptionCollection::set_option_value_cl(
	std::string const & key_id, // Option key id
	ValueStrings & arg_strings // Argument strings: Value string(s) in front
)
{
	using std::string;
	using ObjexxFCL::is_any_of;

	// Set the option value
	runtime_assert( OptionKeys::has( key_id ) ); // Precondition
	OptionKey const & key( OptionKeys::key( key_id ) );
	if ( ! has( key ) ) {
		throw( CREATE_EXCEPTION(excn::Exception,  "No option exists for the valid option key -" + key.id() ) );
	}
	Option & opt( option( key ) );
	if ( key.scalar() ) { // Scalar option key
		if ( ( arg_strings.empty() ) || ( ! opt.is_cl_value( arg_strings.front() ) ) ) {
			// Pass empty string: Valid for some options and others will give error
			opt.cl_value( string() );
		} else {
			opt.cl_value( arg_strings.front() ); // Use the argument
			arg_strings.pop_front(); // Remove the argument
		}
	} else { // Vector option key
		runtime_assert( key.vector() );
		if ( arg_strings.empty() ) { // No values
			auto & vopt( option< VectorOption >( key ) );
			if ( ( vopt.n() > 0 ) || ( vopt.n_lower() > 0 ) ) {
				throw ( CREATE_EXCEPTION(excn::Exception,  "No values specified for multi-valued option -"+ key.id() ) );
			}
		} else if ( ! opt.is_cl_value( arg_strings.front() ) ) { // No values of correct type
			throw ( CREATE_EXCEPTION(excn::Exception,  "No values of the appropriate type specified for multi-valued option -"+key.id() ) );
		} else { // Take value(s)
			// This takes the first value even if the vector is full to trigger an error
			opt.cl_value( arg_strings.front() ); // Use the first argument
			arg_strings.pop_front(); // Remove the first argument
			while ( ( ! arg_strings.empty() ) &&  ( opt.can_hold_another() ) &&
					( opt.is_cl_value( arg_strings.front() ) ) ) { // Take another value
				opt.cl_value( arg_strings.front() ); // Use the argument
				arg_strings.pop_front(); // Remove the argument
			}
		}
	}
}


/// @brief Set a user-specified option value from an @file
/// note Uses all value strings and doesn't erase them
void
OptionCollection::set_option_value_file(
	std::string const & key_id, // Option key id
	ValueStrings & val_strings // Value strings
)
{
	using std::string;

	// Set the option value
	runtime_assert( OptionKeys::has( key_id ) ); // Precondition
	OptionKey const & key( OptionKeys::key( key_id ) );
	if ( ! has( key ) ) {
		throw( CREATE_EXCEPTION(excn::Exception,  " No option exists for the valid option key - " + key.id() ) );
	}
	if ( key.scalar() ) { // Scalar option key
		if ( val_strings.size() > 1 ) { // Multiple values for a scalar option
			throw( CREATE_EXCEPTION(excn::Exception,  " Multiple values specified for option -" + key.id() ) );
		}
		option( key ).cl_value( val_strings.empty() ? string() : *val_strings.begin() );
	} else { // Vector option key
		runtime_assert( key.vector() );
		if ( val_strings.empty() ) { // No values for a vector option
			auto & vopt( option< VectorOption >( key ) );
			if ( ( vopt.n() > 0 ) || ( vopt.n_lower() > 0 ) ) {
				throw( CREATE_EXCEPTION(excn::Exception,  "ERROR: Multiple values specified for multi-valued option -"
					+ key.id() + " requiring one or more values" ) );
			}
		}
		for ( ValueStrings::const_iterator i = val_strings.begin(), e = val_strings.end(); i != e; ++i ) {
			option( key ).cl_value( *i );
		}
	}
}


/// @brief Check that a key's identifiers are legal
void
OptionCollection::check_key( OptionKey const & key )
{
	using ObjexxFCL::is_double;

	if ( is_double( suffix( key.id() ) ) ) {
		throw( CREATE_EXCEPTION(excn::Exception,  "ERROR: Options with numeric identifiers are not allowed: -" + key.id() ));
	}
	if ( is_double( suffix( key.identifier() ) ) ) {
		throw( CREATE_EXCEPTION(excn::Exception,  "ERROR: Options with numeric identifiers are not allowed: -" + key.id() ));
	}
	if ( is_double( suffix( key.code() ) ) ) {
		throw( CREATE_EXCEPTION(excn::Exception,  "ERROR: Options with numeric identifiers are not allowed: -" + key.id() ));
	}
	//not called during option parsing --- keep hard exit
}


/// @brief Check that an option's identifiers are legal
void
OptionCollection::check_key( Option const & option )
{
	using ObjexxFCL::is_double;

	if ( is_double( suffix( option.id() ) ) ) {
		throw( CREATE_EXCEPTION(excn::Exception,  "ERROR: Options with numeric identifiers are not allowed: -" + option.id() ));
	}
	if ( is_double( suffix( option.identifier() ) ) ) {
		throw( CREATE_EXCEPTION(excn::Exception,  "ERROR: Options with numeric identifiers are not allowed: -" + option.id() ));
	}
	if ( is_double( suffix( option.code() ) ) ) {
		throw( CREATE_EXCEPTION(excn::Exception,  "ERROR: Options with numeric identifiers are not allowed: -" + option.id() ));
	}
}

std::string
OptionCollection::lower_no_under( std::string const & instring ) {
	return boost::algorithm::erase_all_copy( utility::lowercased(instring), "_" );
}

void
OptionCollection::add_edits( std::set<std::string> & items ) {
	std::set< std::string > new_edits;
	std::string const charset( "abcdefghijklmnopqrstuvwxyz1234567890" ); // all lowercase, no underscores
	for ( auto const & instring : items ) {
		// keep the unmodified item
		new_edits.insert( instring );
		//delete a single charachter
		for ( platform::Size ii(0); ii < instring.size(); ++ii ) {
			//delete a single charachter (ii)
			new_edits.insert( instring.substr(0,ii) + instring.substr(ii+1) );
			//transpose the charachter with the one before it
			if ( ii != 0 ) {
				std::string transposed( instring );
				transposed[ii] = instring[ii-1];
				transposed[ii-1] = instring[ii];
				new_edits.insert( transposed );
			}
			for ( char jj : charset ) {
				// add a single charachter before ii
				new_edits.insert( instring.substr(0,ii) + jj + instring.substr(ii) );
				// replace a charachter at ii
				std::string replaced( instring );
				replaced[ii] = jj;
				new_edits.insert( replaced );
			}
		}
		// add a single charachter at the end of the string
		for ( char jj : charset ) {
			new_edits.insert( instring + jj );
		}
	}
	items = new_edits;
}


/// @brief Find a user-specified option key in a command line context
/// @note  Searches up the context to find a match
std::string
OptionCollection::find_key_cl(
	std::string const & key_string, // Option key string entered
	std::string const & cid, // Context option id
	bool const top // Top-level context?
)
{
	using std::string;
	using size_type = std::string::size_type;
	// Find the option key
	string kid; // Matched key name
	if ( ( cid.empty() ) || ( top ) ) { // No context: Search for key with specified key string
		if ( OptionKeys::has( key_string ) ) {
			kid = key_string;
		}
	} else { // Search upwards in context
		string pid( cid ); // Context id prefix being tried
		bool done( false );
		while ( ! done ) {
			string const tid( merged( pid, key_string ) );
			//std::cout << _LINE_ << " tid: " << tid << std::endl;
			if ( OptionKeys::has( tid ) ) { // Valid option identifier
				if ( ! kid.empty() ) { // Already found a match
					if ( tid != kid ) { // Different id match
						throw( CREATE_EXCEPTION(excn::Exception,  "WARNING: Specified option -" + key_string + " resolved to option -" + kid + " not option -" + tid + " in command line context -" + cid ) );
					}
				} else { // Assign the matched id
					kid = tid;
				}
			}
			if ( pid.empty() ) {
				done = true;
			} else {
				trim( pid ); // Remove last part of prefix for next pass
			}
		}
	}


	if ( kid.empty() ) { // Look for unique best suffix match wrt the context
		size_type const k_part( n_part( key_string ) ); // Number of parts in key string entered
		size_type m_part( 0 ); // Number of prefix parts matching context
		string bid; // Best id match so far
		std::vector< std::string > possible_matches; //what matches could it be; for error reporting
		int n_best( 0 );
		bool found_relevant( false );
		for ( auto i = OptionKeys::begin(), e = OptionKeys::end(); i != e; ++i ) {
			OptionKey const & key( *i );
			string sid;
			if ( suffix( key.id(), k_part ) == key_string ) {
				sid = key.id();
			} else if ( suffix( key.identifier(), k_part ) == key_string ) {
				sid = key.identifier();
			} else if ( suffix( key.code(), k_part ) == key_string ) {
				sid = key.code();
			}
			if ( ! sid.empty() ) {
				size_type p_part( n_part_prefix_match( cid, sid ) );
				bool this_is_relevant = is_relevant( key );
				if ( !found_relevant && ( p_part > m_part || this_is_relevant ) ) { // New best prefix match level
					m_part = p_part;
					n_best = 0;
					possible_matches.clear();
					bid = sid;
				}
				if ( this_is_relevant ) found_relevant = true;
				if ( (!found_relevant && p_part == m_part) || this_is_relevant ) { // Another match at this prefix match level
					++n_best;
					possible_matches.push_back(sid);
					if ( bid.empty() ) bid = sid; // First match at zero level
				}
			}
		}
		if ( n_best == 1 ) { // Unique best match found
			kid = bid;
		} else if ( n_best > 1 ) { // Nonunique matches found
			std::string too_many_choices_error("Unique best command line context option match not found for -" + key_string + ".\nPossible matches include:\n");
			for ( auto & possible_matche : possible_matches ) {
				too_many_choices_error += ("\t -" + possible_matche) += "\n";
			}
			too_many_choices_error += "Either specify namespace from command line; or in code, use add_relevant() to register option.\n";
			throw ( CREATE_EXCEPTION(excn::Exception,  too_many_choices_error ) );
		}
	}

	if ( kid.empty() ) { // No such option
		std::string not_found_error( "Option matching -" + key_string + " not found" );
		if ( cid.empty() ) {
			not_found_error += " in command line top-level context";
		} else {
			not_found_error += " in command line context -" + cid;
		}

		std::set< std::string > all_edits;
		all_edits.insert( lower_no_under( suffix( key_string, 1 ) ) );
		// Everything with edit distance of 2
		add_edits( all_edits );
		add_edits( all_edits );

		std::set< std::string > possible_fixes; // The set will sort the entries

		for ( auto i = OptionKeys::begin(), e = OptionKeys::end(); i != e; ++i ) {
			OptionKey const & key( *i );
			if ( all_edits.count( lower_no_under( suffix( key.id() ) ) ) == 1 ) {
				possible_fixes.insert( key.id() );
			}
		}

		if ( ! possible_fixes.empty() ) {
			not_found_error += "\nDid you mean:\n";
			for ( auto const & possible_fixe : possible_fixes ) {
				not_found_error += "\t -" + possible_fixe + "\n";
			}
		}
		throw ( CREATE_EXCEPTION(excn::Exception,  not_found_error ) );
	}

	return kid;
}

/// @brief Find a user-specified option key in an indented @file context
/// @note  Looks in the context to find a match
std::string
OptionCollection::find_key_file(
	std::string const & key_string, // Option key string entered
	std::string const & cid, // Context option id
	bool const top // Top-level context?
)
{
	using std::string;

	// Find the option key
	string kid; // Matched key name
	if ( ( cid.empty() ) || ( top ) ) { // No context: Search for key with specified key string
		if ( OptionKeys::has( key_string ) ) {
			kid = key_string;
		}
	} else { // Look for key in context
		string const tid( merged( cid, key_string ) );
		if ( OptionKeys::has( tid ) ) { // Valid option identifier
			kid = tid;
		}
	}

	// Check if key matched
	if ( kid.empty() ) { // No such option
		throw( CREATE_EXCEPTION(excn::Exception,  "Option matching -" + key_string
			+ " not found in indented @file context -" + cid ) );
	}

	return kid;
}


/// @brief Number of parts in an option id
std::string::size_type
OptionCollection::n_part( std::string const & s )
{
	if ( s.empty() ) {
		return 1u;
	} else { // Scan the string: Count each transition to : so we can accept -a::b as well as -a:b
		using size_type = std::string::size_type;
		char const COLON( ':' );
		size_type n_part_( s[ 0 ] == COLON ? 2u : 1u );
		for ( size_type i = 1, e = s.size(); i < e; ++i ) {
			if ( ( s[ i ] == COLON ) && ( s[ i - 1 ] != COLON ) ) ++n_part_;
		}
		return n_part_;
	}
}


/// @brief Number of prefix parts of two ids that match
std::string::size_type
OptionCollection::n_part_prefix_match(
	std::string const & s,
	std::string const & t
)
{
	if ( ( s.empty() ) || ( t.empty() ) ) {
		return 0u;
	} else {
		using size_type = std::string::size_type;
		for ( size_type n = std::min( n_part( s ), n_part( t ) ); n > 0; --n ) {
			if ( prefix( s, n ) == prefix( t, n ) ) return n;
		}
		return 0u;
	}
}


/// @brief Prefix of an option id with a specified number of parts
std::string
OptionCollection::prefix(
	std::string const & s, // String
	int const n // Number of prefix parts desired
)
{
	using std::string;
	using size_type = std::string::size_type;

	size_type const n_s( n_part( s ) );
	if ( ( n <= 0 ) || ( s.empty() ) ) { // Nothing
		return string();
	} else if ( size_type( n ) >= n_s ) { // Whole string
		return s;
	} else {
		char const COLON( ':' );
		size_type idx( 0 );
		for ( int i = 1; i <= n; ++i ) {
			idx = s.find( COLON, idx );
			runtime_assert( idx != string::npos );
			if ( i < n ) idx = s.find_first_not_of( COLON, idx );
			runtime_assert( idx != string::npos );
		}
		return s.substr( 0, idx );
	}
}


/// @brief Suffix of an option id with a specified number of parts
std::string
OptionCollection::suffix(
	std::string const & s, // String
	int const n // Number of suffix parts desired
)
{
	using std::string;
	using size_type = std::string::size_type;

	size_type const n_s( n_part( s ) );

	if ( ( n <= 0 ) || ( s.empty() ) ) { // Nothing
		return string();
	} else if ( size_type( n ) >= n_s ) { // Whole string
		return s;
	} else {
		char const COLON( ':' );
		size_type idx( s.length() - 1 );
		for ( int i = 1; i <= n; ++i ) {
			idx = s.find_last_of( COLON, idx );
			runtime_assert( idx != string::npos );
			if ( i < n ) idx = s.find_last_not_of( COLON, idx );
			runtime_assert( idx != string::npos );
		}
		return s.substr( idx + 1 );
	}
}


/// @brief Trim a specified number of parts from the suffix of an option id
std::string &
OptionCollection::trim(
	std::string & s, // String
	int const n // Number of suffix parts to trim
)
{
	using std::string;
	using size_type = std::string::size_type;

	size_type const n_s( n_part( s ) );

	if ( ( n <= 0 ) || ( s.empty() ) ) { // Nothing
		// Do nothing
	} else if ( size_type( n ) >= n_s ) { // Trim whole string
		s.clear();
	} else {
		char const COLON( ':' );
		size_type idx( s.length() - 1 );
		for ( int i = 1; i <= n; ++i ) {
			runtime_assert( idx != string::npos );
			idx = s.find_last_of( COLON, idx );
			runtime_assert( idx != string::npos );
			idx = s.find_last_not_of( COLON, idx ); // None is OK since npos+1==0
		}
		return s.erase( idx + 1 );
	}
	return s;
}


/// @brief Prefix of an option id with a specified number of suffix parts removed
std::string
OptionCollection::trimmed(
	std::string const & s, // String
	int const n // Number of suffix parts to trim
)
{
	using std::string;
	using size_type = std::string::size_type;

	size_type const n_s( n_part( s ) );

	if ( ( n <= 0 ) || ( s.empty() ) ) { // Nothing
		return s;
	} else if ( size_type( n ) >= n_s ) { // Trim whole string
		return string();
	} else {
		char const COLON( ':' );
		size_type idx( s.length() - 1 );
		for ( int i = 1; i <= n; ++i ) {
			runtime_assert( idx != string::npos );
			idx = s.find_last_of( COLON, idx );
			runtime_assert( idx != string::npos );
			idx = s.find_last_not_of( COLON, idx ); // None is OK since npos+1==0
		}
		return s.substr( 0, idx + 1 );
	}
}


/// @brief Cleaned option id with repeat colons condensed
std::string
OptionCollection::cleaned( std::string const & s )
{
	using std::string;
	using size_type = std::string::size_type;

	char const COLON( ':' );
	string t;
	for ( size_type i = 0, e = s.size(); i < e; ++i ) {
		if ( i+1 == e ) {
			t += s[i];
		} else if ( s[i] != COLON ) {
			t += s[i];
		} else if ( s[i] == COLON && s[i+1] != COLON ) {
			t += s[i];
		}
	}
	for ( char & i : t ) {
		if ( i == '.' ) i = ':';
	}
	return t;
}


/// @brief Merged option ids with the minimal suffix-prefix overlap, if any, removed
std::string
OptionCollection::merged(
	std::string const & s, // Lead id
	std::string const & t // Tail id
)
{
	using size_type = std::string::size_type;

	// Handle either id empty
	if ( ( s.empty() ) || ( t.empty() ) ) return t;

	// Part counts
	size_type const n_s( n_part( s ) );
	size_type const n_t( n_part( t ) );
	size_type const max_overlap( std::min( n_s, n_t ) );

	// Look for an overlap match
	size_type o( 1 ); // Overlap being tried
	while ( o <= max_overlap ) {
		if ( cleaned( suffix( s, o ) ) == cleaned( prefix( t, o ) ) ) { // Match found
			if ( o == n_s ) {
				return t;
			} else {
				return cleaned( trimmed( s, o ) ) + ':' += t;
			}
		}
		++o;
	}

	// No overlap match found
	return cleaned( s ) + ':' += cleaned( t ); // No overlap match found: Concatenate
}


/// @brief String wrapped and indented
std::string
OptionCollection::wrapped(
	std::string const & s, // String to wrap
	std::string::size_type indent, // Width to indent continuation lines
	std::string::size_type width, // Column width to wrap at [80]
	std::string const & header_for_extra_lines // put this before each new line (default is empty)
)
{
	using std::string;
	using ObjexxFCL::is_any_of;
	using size_type = std::string::size_type;

	// check arguments
	runtime_assert( indent + 1 < width );

	// Handle empty or 1-line string
	if ( ( s.empty() ) || ( s.length() <= width ) ) return s;

	// Create the wrapped string
	string w;
	size_type l( 0 ); // Line length
	char const nl( '\n' ); // Newline
	string const ws( " \t" ); // Whitespace
	for ( size_type i = 0, e = s.length(); i < e; ++i ) {
		if ( l + 1 >= width ) { // Wrap
			while ( ( i < e ) && ( is_any_of( s[ i ], ws ) ) ) ++i;
			if ( i < e ) { // Indent and add next non-whitespace character
				w += nl + header_for_extra_lines + string( indent, ' ' ) + s[ i ];
				l = indent + 1;
			} else { // Nothing left
				l = 0;
			}
		} else if ( s[ i ] == nl ) { // Embedded line terminator: Wrap
			if ( i + 1 >= e ) { // Last character: Add the newline
				w += nl;
				l = 0;
			} else if ( s[ i + 1 ] == nl ) { // Next character a newline: Don't indent
				w += nl;
				l = 0;
			} else { // Next character not a newline: Indent
				while ( ( ++i < e ) && ( is_any_of( s[ i ], ws ) ) ) ++i;
				if ( i < e ) { // Indent and add next non-whitespace character
					w += nl + string( indent, ' ' ) + s[ i ];
					l = indent + 1;
				} else { // Nothing left: Add the newline
					w += nl;
					l = 0;
				}
			}
		} else { // Add next character to line
			size_type const b( std::min( s.find_first_of( ws, i ), s.length() ) ); // Next whitespace break
			if ( ( l + 1 + b - i >= width ) && ( b - i < width - indent ) ) { // Token won't fit: Wrap
				w += nl + header_for_extra_lines + string( indent, ' ' ) + s[ i ];
				l = indent + 1;
			} else { // Add the character
				w += s[ i ];
				++l;
			}
		}
	}
	return w;
}


} // namespace options
} // namespace utility

#ifdef    SERIALIZATION

/// @brief Automatically generated serialization method
template< class Archive >
void
utility::options::OptionCollection::save( Archive & arc ) const {
#ifdef MULTI_THREADED
	utility::thread::ReadLockGuard readlock( mutex_ );
#endif
	arc( CEREAL_NVP( booleans_ ) ); // Booleans
	arc( CEREAL_NVP( integers_ ) ); // Integers
	arc( CEREAL_NVP( reals_ ) ); // Reals
	arc( CEREAL_NVP( strings_ ) ); // Strings
	arc( CEREAL_NVP( files_ ) ); // Files
	arc( CEREAL_NVP( paths_ ) ); // Paths
	//arc( CEREAL_NVP( anys_ ) ); // Anys
	// EXEMPT anys_
	arc( CEREAL_NVP( boolean_vectors_ ) ); // BooleanVectors
	arc( CEREAL_NVP( integer_vectors_ ) ); // IntegerVectors
	arc( CEREAL_NVP( real_vectors_ ) ); // RealVectors
	arc( CEREAL_NVP( residue_chain_vectors_ ) ); // ResidueChainVectors
	arc( CEREAL_NVP( string_vectors_ ) ); // StringVectors
	arc( CEREAL_NVP( file_vectors_ ) ); // FileVectors
	arc( CEREAL_NVP( path_vectors_ ) ); // PathVectors
	//arc( CEREAL_NVP( any_vectors_ ) ); // AnyVectors
	// EXEMPT any_vectors_
	arc( CEREAL_NVP( all_ ) ); // All
	// EXEMPT relevant_
	//arc( CEREAL_NVP( relevant_ ) ); //CANNOT ARCHIVE relevant_; nor should we.
	arc( CEREAL_NVP( argv_copy_ ) ); // std::string
	arc( CEREAL_NVP( show_accessed_options_ ) );
	arc( CEREAL_NVP( show_unused_options_ ) );
}

/// @brief Automatically generated deserialization method
template< class Archive >
void
utility::options::OptionCollection::load( Archive & arc ) {
#ifdef MULTI_THREADED
	utility::thread::WriteLockGuard writelock( mutex_ );
#endif
	arc( booleans_ ); // Booleans
	arc( integers_ ); // Integers
	arc( reals_ ); // Reals
	arc( strings_ ); // Strings
	arc( files_ ); // Files
	arc( paths_ ); // Paths
	// EXEMPT anys_
	//arc( anys_ ); // Anys
	arc( boolean_vectors_ ); // BooleanVectors
	arc( integer_vectors_ ); // IntegerVectors
	arc( real_vectors_ ); // RealVectors
	arc( residue_chain_vectors_ ); // ResidueChainVectors
	arc( string_vectors_ ); // StringVectors
	arc( file_vectors_ ); // FileVectors
	arc( path_vectors_ ); // PathVectors
	// EXEMPT any_vectors_
	//arc( any_vectors_ ); // AnyVectors
	arc( all_ ); // All
	// EXEMPT relevant_
	//arc( relevant_ ); //CANNOT ARCHIVE relevant_; nor should we.
	arc( argv_copy_ ); // std::string
	arc( show_accessed_options_ );
	arc( show_unused_options_ );
}

SAVE_AND_LOAD_SERIALIZABLE( utility::options::OptionCollection );
CEREAL_REGISTER_TYPE( utility::options::OptionCollection )

CEREAL_REGISTER_DYNAMIC_INIT( utility_options_OptionCollection )
#endif // SERIALIZATION
