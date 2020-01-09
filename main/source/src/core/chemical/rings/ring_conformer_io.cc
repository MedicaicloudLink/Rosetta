// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file    core/chemical/rings/ring_conformer_io.cc
/// @brief   Database input/output function definitions for ring-conformer-specific data.
/// @author  Labonte <JWLabonte@jhu.edu>


// Unit header
#include <core/chemical/rings/ring_conformer_io.hh>
#include <core/chemical/rings/RingConformer.hh>

// Project header
#include <core/types.hh>

// Utility headers
#include <utility/exit.hh>
#include <utility/io/util.hh>

// Basic header
#include <basic/Tracer.hh>
#include <basic/options/option.hh>
#include <basic/options/keys/in.OptionKeys.gen.hh>

// C++ header
#include <sstream>
//#include <string>

// Construct tracer.
static basic::Tracer TR( "core.chemical.rings.ring_conformer_io" );


namespace core {
namespace chemical {
namespace rings {

// Return a list of ring conformers, read from a database file.
utility::vector1< RingConformer >
read_conformers_from_database_file_for_ring_size( std::string const & filename, core::Size ring_size )
{
	using namespace std;
	using namespace utility;

	if ( ring_size < 3 ) {
		utility_exit_with_message( "Cannot load database file: An invalid ring size was provided." );
	}

	vector1< string > const lines( io::get_lines_from_file_data( filename ) );
	vector1< RingConformer > conformers;

	Size const n_lines( lines.size() );

	for ( uint i( 1 ); i <= n_lines; ++i ) {
		istringstream line_word_by_word( lines[ i ] );
		RingConformer conformer;
		//Real junk;  // a place to throw out unneeded angles from the tables in the database

		// We need 3 less than the number of nu angles to define a ring conformer by Cremer-Pople parameters.
		conformer.CP_parameters.resize( ring_size - 3 );

		// We need 1 less than the number of nu angles to define a ring conformer by internal angles;
		// however, all of them are used for the cart_bonded_ring term
		conformer.nu_angles.resize( ring_size );

		// We need all the tau angles.
		conformer.tau_angles.resize( ring_size );

		line_word_by_word >> conformer.specific_name >> conformer.general_name >> conformer.degeneracy;

		for ( uint parameter( 1 ); parameter <= ring_size - 3; ++parameter ) {
			line_word_by_word >> conformer.CP_parameters[ parameter ];
		}

		for ( uint nu( 1 ); nu <= ring_size; ++nu ) {
			line_word_by_word >> conformer.nu_angles[ nu ];
		}

		for ( uint tau( 1 ); tau <= ring_size; ++tau ) {
			line_word_by_word >> conformer.tau_angles[ tau ];
		}
		conformers.push_back( conformer );
	}

	if ( TR.Debug.visible() ) {
		TR.Debug << "Read " << conformers.size() << " " <<
			ring_size << "-membered ring conformers from the database." << endl;
	}

	return conformers;
}

}  // namespace rings
}  // namespace chemical
}  // namespace core
