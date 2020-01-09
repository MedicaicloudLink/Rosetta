// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

/// @file   core/chemical/mainchain_potential/MainchainScoreTable.cc
/// @brief  A general class for storing a torsional potential for mainchain resiudes.
/// @details Can be used by terms like rama, rama_prepro, p_aa_pp.
/// @author Vikram K. Mulligan (vmullig@uw.edu)

// Unit Headers
#include <core/chemical/mainchain_potential/MainchainScoreTable.hh>

// Core Headers
#include <core/chemical/ResidueType.hh>

// Utility Headers
#include <utility/string_util.hh>
#include <utility/vector1.hh>
#include <utility/fixedsizearray1.hh>
#include <utility/pointer/memory.hh>

// Basic Headers
#include <basic/Tracer.hh>
#include <basic/database/open.hh>
#include <basic/options/option.hh>
#include <basic/options/keys/score.OptionKeys.gen.hh>

// Numeric Headers
#include <numeric/angle.functions.hh>
#include <numeric/MathNTensorBase.hh>
#include <numeric/MathNTensor.hh>
#include <numeric/util.hh>
#include <numeric/random/random.hh>
#include <numeric/interpolation/spline/PolycubicSplineBase.hh>
#include <numeric/interpolation/spline/PolycubicSpline.tmpl.hh>
#include <numeric/interpolation/spline/PolycubicSpline.hh>
#include <numeric/interpolation/spline/BicubicSpline.hh>
#include <numeric/interpolation/spline/CubicSpline.hh>

namespace core {
namespace chemical {
namespace mainchain_potential {

static basic::Tracer TR("core.chemical.mainchain_potential.MainchainScoreTable");

/// @brief Default constructor.
///
MainchainScoreTable::MainchainScoreTable():
	initialized_(false), //Will be set to true when tables are read.
	dimension_(0), //Zero indicates uninitialized.
	energies_(), //Initializes to null pointer.
	probabilities_(),  //Initializes to null pointer.
	cdf_(), //Initializes to null pointer.
	use_polycubic_interpolation_(true),
	energies_spline_1D_(), //NULL by default
	energies_spline_2D_(), //NULL by default
	energies_spline_ND_(), //NULL by default
	n_mainchain_torsions_total_(2),
	mainchain_torsions_covered_(),
	symmetrize_gly_(false)
{
	symmetrize_gly_ = basic::options::option[ basic::options::OptionKeys::score::symmetric_gly_tables ](); //Read from option system.
}

/// @brief Clone function: make a copy of this object and return
/// an owning pointer to the copy.
MainchainScoreTableOP
MainchainScoreTable::clone() const {
	return utility::pointer::make_shared< MainchainScoreTable >(*this);
}

/// @brief Parse a Shapovalov-style rama database file and set up this MainchainScoreTable.
/// @details Sets initialized_ to true.
/// @param[in] filename The name of the file that was read.  (Just used for output messages -- this function does not file read).
/// @param[in] file_contents The slurped contents of the file to parse.
/// @param[in] res_type_name The name of the ResidueType for which we're reading data.  Data lines for other residue types will be
/// ignored.
/// @param[in] use_polycubic_interpolation If true, uses polycubic interpolation; if false, uses polylinear interpolation.
void
MainchainScoreTable::parse_rama_map_file_shapovalov(
	std::string const &filename, //TODO -- make this function take a slurped string of file contents instead of the file itself.  Put a file read function in a utility file, and allow it to set up many MainchainScoreTables (for multiple amino acids contained in a file) from a single file read.
	std::string const &file_contents,
	std::string const &res_type_name,
	bool const use_polycubic_interpolation
) {
	runtime_assert_string_msg( !initialized(), "Error in core::chemical::mainchain_potential::MainchainScoreTable::parse_rama_map_file_shapovalov(): The MainchainScoreTable has already been initialized." );

	//Store whether we're using polycubic interpolatoin:
	use_polycubic_interpolation_ = use_polycubic_interpolation;

	// Set defaults that can be overridden in the file:
	core::Size n_mainchain_torsions(2); //Number of mainchain torsions
	utility::vector1 < core::Size > dimensions; //Number of gridpoints for each mainchain torsion (defaults to 36x36).
	dimensions.resize(2, 36);
	utility::vector1 < core::Real > offsets; //The offset of each gridpoint, as a fraction of grid cell width (0.0 to 1.0).
	offsets.resize(2, 0.0);
	initialize_tensors(dimensions);
	bool data_encountered(false); //Have we read any data lines yet?

	std::string line; //Buffer for line read

	utility::vector1< core::Size > scoring_grid_indices; //This is a vector of grid indicies: a 2-vector for a 2D grid (e.g. phi/psi table), 3-vector for a 3D grid (e.g. phi/mu/psi for beta-amino acids), etc.
	scoring_grid_indices.resize(2, 0); //Initialize to a 2-vector (default).
	std::string aa_name; //Amino acid name.
	utility::vector1 < core::Real > scoring_grid_torsion_values; //This is a vector of torsion values of dimensionality corresponding to the grid.  (Could store phi=60, psi=40, for example).
	scoring_grid_torsion_values.resize(2, 0.0); //Initialize to a 2-vector (default).
	core::Real prob(0.0), minusLogProb(0.0); //The probability and -k_B*T*ln(prob) for this scoring grid point.
	core::Real entropy(0.0);  //The entropic correction factor.

	//Have certain setup lines been read?
	bool n_mainchain_torsions_read(false);
	bool n_mainchain_torsions_total_read(false);
	bool mainchain_torsions_provided_read(false);
	bool dimensions_read(false);
	bool offsets_read(false);

	std::istringstream iunit( file_contents );

	//Parse the file contents:
	do {
		std::getline( iunit, line );
		if ( iunit.eof() ) break;

		utility::trim( line, " \n\t" ); //Strip terminal whitespace.
		line = line.substr( 0, line.find('#') ); //Strip anything following the number sign (comments).
		if ( line.empty() || line[0] == '\n' || line[0] == '\r' ) continue;  //Skip blank lines.

		std::istringstream linestream(line); //Put the line into a stringstream for easy parsing.

		if ( line[0] == '@' ) {
			//Parse setup lines here:

			runtime_assert_string_msg(!data_encountered, "Error in core::chemical::mainchain_potential::MainchainScoreTable::parse_rama_map_file_shapovalov(): configuration line (starting with \"@\" found after data lines."); //The lines starting with '@' must all precede the regular data lines.

			std::string linehead;
			linestream >> linehead;
			check_linestream(linestream, filename);

			//The line starts with an '@', which precedes a command indicating setup information.  Figure out what we're setting up and set it up:
			if ( !linehead.compare("@N_MAINCHAIN_TORSIONS") ) { //The number of mainchain torsions provided in this file.
				runtime_assert_string_msg(!n_mainchain_torsions_read, "Error in core::chemical::mainchain_potential::MainchainScoreTable::parse_rama_map_file_shapovalov(): The \"" +filename + "\" file contains more than one \"@N_MAINCHAIN_TORSIONS\" line." );
				runtime_assert_string_msg(!dimensions_read, "Error in core::chemical::mainchain_potential::MainchainScoreTable::parse_rama_map_file_shapovalov(): The \"" +filename + "\" file contains an \"@N_MAINCHAIN_TORSIONS\" line after a \"@DIMENSIONS\" line." );
				runtime_assert_string_msg(!offsets_read, "Error in core::chemical::mainchain_potential::MainchainScoreTable::parse_rama_map_file_shapovalov(): The \"" +filename + "\" file contains an \"@N_MAINCHAIN_TORSIONS\" line after a \"@OFFSETS\" line." );
				linestream >> n_mainchain_torsions;
				check_linestream(linestream, filename, false);
				dimensions.resize(n_mainchain_torsions, 36);
				offsets.resize(n_mainchain_torsions, 0.5);
				scoring_grid_indices.resize(n_mainchain_torsions, 0);
				scoring_grid_torsion_values.resize(n_mainchain_torsions, 0.0);
				n_mainchain_torsions_read=true;
			} else if ( !linehead.compare("@N_MAINCHAIN_TORSIONS_TOTAL") ) { //The actual number of mainchain torsions in the residue type.  Excludes inter-residue torsions (omega).
				runtime_assert_string_msg( n_mainchain_torsions_read, "Error in core::chemical::mainchain_potential::MainchainScoreTable::parse_rama_map_file_shapovalov(): The \"" +filename + "\" file contains an \"@N_MAINCHAIN_TORSIONS_TOTAL\" line that occurs before any \"@N_MAINCHAIN_TORSIONS\" line. " );
				runtime_assert_string_msg( !n_mainchain_torsions_total_read, "Error in core::chemical::mainchain_potential::MainchainScoreTable::parse_rama_map_file_shapovalov(): The \"" +filename + "\" file contains more than one \"@N_MAINCHAIN_TORSIONS_TOTAL\" line." );
				linestream >> n_mainchain_torsions_total_;
				check_linestream(linestream, filename, false);
				n_mainchain_torsions_total_read = true;
				runtime_assert_string_msg( n_mainchain_torsions_total_ >= n_mainchain_torsions, "Error in core::chemical::mainchain_potential::MainchainScoreTable::parse_rama_map_file_shapovalov(): The \"" +filename + "\" file specified that there are more torsions in this file than there are actual mainchain torsions." );
			} else if ( !linehead.compare("@MAINCHAIN_TORSIONS_PROVIDED") ) { //If only a subset of mainchain torsions are covered by the table, which ones are provided?
				runtime_assert_string_msg( n_mainchain_torsions_total_read, "Error in core::chemical::mainchain_potential::MainchainScoreTable::parse_rama_map_file_shapovalov(): The \"" +filename + "\" file contains a \"@MAINCHAIN_TORSIONS_PROVIDED\" line that precedes any \"@N_MAINCHAIN_TORSIONS_TOTAL\" line." );
				runtime_assert_string_msg( !mainchain_torsions_provided_read, "Error in core::chemical::mainchain_potential::MainchainScoreTable::parse_rama_map_file_shapovalov(): The \"" +filename + "\" file contains more than one \"@MAINCHAIN_TORSIONS_PROVIDED\" line." );
				mainchain_torsions_covered_.resize( n_mainchain_torsions );
				for ( core::Size i(1); i<=n_mainchain_torsions; ++i ) {
					linestream >> mainchain_torsions_covered_[i];
					check_linestream( linestream, filename, i != n_mainchain_torsions );
				}
				mainchain_torsions_provided_read = true;
				runtime_assert_string_msg( linestream.eof(), "Error in core::chemical::mainchain_potential::MainchainScoreTable::parse_rama_map_file_shapovalov(): Too many dimensions were specified in a \"@MAINCHAIN_TORSIONS_PROVIDED\" line.  The number should match the @N_MAINCHAIN_TORSIONS line." );
			} else if ( !linehead.compare("@DIMENSIONS") ) {
				runtime_assert_string_msg(!dimensions_read, "Error in core::chemical::mainchain_potential::MainchainScoreTable::parse_rama_map_file_shapovalov(): The \"" +filename + "\" file contains more than one \"@DIMENSIONS\" line." );
				runtime_assert( n_mainchain_torsions == dimensions.size() ); //Should be guaranteed true.
				for ( core::Size i=1; i<=n_mainchain_torsions; ++i ) {
					linestream >> dimensions[i];
					check_linestream(linestream, filename, i<n_mainchain_torsions);
				}
				initialize_tensors(dimensions);
				runtime_assert_string_msg( linestream.eof(), "Error in core::chemical::mainchain_potential::MainchainScoreTable::parse_rama_map_file_shapovalov(): Too many dimensions were specified in a \"@DIMENSIONS\" line." );
				dimensions_read=true;
			} else if ( !linehead.compare("@OFFSETS") ) {
				runtime_assert_string_msg( !offsets_read, "Error in core::chemical::mainchain_potential::MainchainScoreTable::parse_rama_map_file_shapovalov(): The \"" +filename + "\" file contains more than one \"@OFFSETS\" line." );
				runtime_assert( n_mainchain_torsions == offsets.size() ) ;  //Should be guaranteed true.
				for ( core::Size i=1; i<=n_mainchain_torsions; ++i ) {
					linestream >> offsets[i];
					check_linestream(linestream, filename, i<n_mainchain_torsions);
				}
				runtime_assert_string_msg( linestream.eof(), "Error in core::chemical::mainchain_potential::MainchainScoreTable::parse_rama_map_file_shapovalov(): Too many dimensions were specified in an \"@OFFSETS\" line." );
				offsets_read=true;
			}

			//TODO -- Add parsing of any additional setup stuff here.
		} else {
			//Parse a data line here:

			linestream >> aa_name; //Get amino acid name.
			check_linestream(linestream, filename);
			if ( aa_name.compare( res_type_name ) ) continue; //If the aa name doesn't match the res type that we're setting up, go to the next line.

			//Read the mainchain torsion values for this line:
			for ( core::Size i=1; i<=n_mainchain_torsions; ++i ) {
				linestream >> scoring_grid_torsion_values[i]; //Get phi/psi values (or as many torisons as we have).
				scoring_grid_torsion_values[i] = numeric::nonnegative_principal_angle_degrees( scoring_grid_torsion_values[i] ); //Set angles to [0,360)
				check_linestream(linestream, filename);
			}

			linestream >> prob; //Get the probability.
			check_linestream(linestream, filename);
			linestream >> minusLogProb; //Get -k_B*T*ln(P).
			runtime_assert_string_msg( linestream.eof(), "Error in core::chemical::mainchain_potential::MainchainScoreTable::parse_rama_map_file_shapovalov(): Extra columns found in a data line in file " + filename + ".  Line was \"" + linestream.str() + "\"." );
			runtime_assert_string_msg( minusLogProb != 0.0 && !linestream.fail() && !linestream.bad(), "Error in core::chemical::mainchain_potential::MainchainScoreTable::parse_rama_map_file_shapovalov(): Could not parse line " + line + " in file " + filename + ".  Line was \"" + linestream.str() + "\"." );

			//All the data for the line have been parsed.  Now, it's time to set up the tensors.
			runtime_assert( scoring_grid_indices.size() /*The scoring_grid_indices vector is used to specify a position in the tensor*/ == n_mainchain_torsions); //Should be guaranteed true.
			//Figure out the coordinates:
			for ( core::Size i=1; i<=n_mainchain_torsions; ++i ) {
				scoring_grid_indices[i] = static_cast< core::Size >( std::round( scoring_grid_torsion_values[i] / 360.0 * static_cast<core::Real>(dimensions[i] ) ) /*+ 1*/ ); //Note that MathNTensors are 0-based.
			}

			numeric::access_Real_MathNTensor( energies_, scoring_grid_indices ) = prob;
			numeric::access_Real_MathNTensor( probabilities_, scoring_grid_indices ) = prob;
			entropy -= prob * minusLogProb;

			data_encountered = true;
		}
	} while(true);

	// If the total number of mainchain torsions wasn't specified, assume that it matches the number in the file.
	if ( !n_mainchain_torsions_total_read ) {
		n_mainchain_torsions_total_ = n_mainchain_torsions;
	}

	// What to do if the mainchain torsions that are covered weren't specified:
	if ( !mainchain_torsions_provided_read ) {
		runtime_assert_string_msg( n_mainchain_torsions_total_ == n_mainchain_torsions, "Error in core::chemical::mainchain_potential::MainchainScoreTable::parse_rama_map_file_shapovalov(): The number of mainchain torsions covered by the data in " + filename + " is less than the number in the residue, but no \"@MAINCHAIN_TORSIONS_PROVIDED\" line was found." );
		mainchain_torsions_covered_.resize( n_mainchain_torsions_total_ );
		for ( core::Size i(1); i<=n_mainchain_torsions_total_; ++i ) {
			mainchain_torsions_covered_[i] = i;
		}
	}

	// Symmetrize the gly table, if we should:
	if ( symmetrize_gly_ && res_type_name == "GLY" ) {
		symmetrize_tensor( probabilities_ );
		energies_from_probs( energies_, probabilities_, 1.0 );
		for ( core::Size i=1, imax=offsets.size(); i<=imax; ++i ) {
			offsets[i] = 180.0/static_cast<core::Real>(dimensions[i]); //Need 5 degree offsets for symmetric gly scoring.
		}
	} else {
		//Entropy correction and conversion to energy units:
		iteratively_correct_energy_tensor( entropy );
	}

	if ( use_polycubic_interpolation_ ) {
		set_up_polycubic_interpolation( offsets, dimensions );
	} else {
		//TODO -- write a linear interpolation setup function.
	}

	//Calculate the cumulative distribution function used for drawing random phi/psi values (or random mainchain torsion values):
	set_up_cumulative_distribution_function( probabilities_, cdf_ );

	//OK, we're now fully initialized.
	set_initialized();
}

/// @brief Reset this object completely.
void
MainchainScoreTable::clear() {
	initialized_ = false;
	dimension_ = 0;
	energies_ = nullptr;
	probabilities_ = nullptr;
	cdf_ = nullptr;
	use_polycubic_interpolation_ = true;
	energies_spline_1D_ = nullptr;
	energies_spline_2D_ = nullptr;
	energies_spline_ND_ = nullptr;
	n_mainchain_torsions_total_ = 2;
	mainchain_torsions_covered_.clear();
	symmetrize_gly_ = false;
}

/// @brief Initialize this object for on-the-fly generation of a mainchain scoretable.
/// @details Calls clear() first.
void
MainchainScoreTable::initialize_for_de_novo_generation(
	core::chemical::ResidueType const & restype,
	utility::vector1< core::Size > const & dimensions,
	utility::vector1< core::Size > const & mainchain_torsions_covered,
	bool const symmetrize
) {
	clear();
	set_dimension_and_ntorsions_for_restype( restype, dimensions, mainchain_torsions_covered );
	set_symmetrize_gly( symmetrize );
}

/// @brief Set an entry in the energies tensor.
/// @note Bounds checking only in debug build!
void
MainchainScoreTable::set_energy(
	utility::vector1< core::Size > const & coords,
	core::Real const & energy_in
) {
	debug_assert( energies_ != nullptr );
	debug_assert( energies_->is_valid_position(coords) );
	energies_->set_value( coords, energy_in );
}

/// @brief Given coordinates in the energy tensor, increment the coordinates.
/// @returns Returns "true" if increment was successful, "false" if the end of the tensor has been reached.
bool
MainchainScoreTable::increment_energy_coords(
	utility::vector1< core::Size > & coords
) const {
	debug_assert( energies_ != nullptr );
	debug_assert( energies_->is_valid_position(coords) );
	return increment_coords( coords, energies_ );
}

/// @brief After the energies tensor has been populated, compute all derived data.
/// @details If "normalize" is true, the probabilities are normalized to 1, and the energies adjusted accordingly.  (This
/// has the effect of raising or lowering all of the energies by some constant.)  If "symmetrize_gly_" is true, the tensors are
/// made symmetric.
/// @note Sets initalized_ to true.
void
MainchainScoreTable::finalize_de_novo_scoretable_from_energies(
	core::Real const &kbt,
	bool const normalize
) {
	//TODO:
	//Symmetrize tensors (if symmetrize option is used).
	//Generate probabilities from energies.
	//Normalize probabilities.
	//Regenerate energies from normalized probabilities.
	TR << TR.Red << "\nDANGER DANGER DANGER\nTHE MainchainScoreTable::finalize_de_novo_scoretable_from_energies() function must be written!\nDANGER DANGER DANGER" << TR.Reset << std::endl;
	TR.flush();

	// Find lowest energy and shift up if negative:
	core::Real lowest( energies_->min() );
	if ( lowest < 0.0 ) lowest -= 1e-10;

	// Generate probabilities:
	TR << "Generating probabilities from energies." << std::endl;
	utility::vector1< core::Size > coords( energies_->dimensionality(), 0 );
	debug_assert( probabilities_->dimensionality() == energies_->dimensionality() );
	core::Real accumulator( 0.0 );
	for ( core::Size i(1), imax(coords.size()); i<=imax; ++i ) {
		coords[i] = 0;
	}
	do {
		debug_assert( energies_->is_valid_position( coords ) );
		debug_assert( probabilities_->is_valid_position( coords ) );
		if ( lowest < 0.0 ) energies_->set_value( coords, energies_->value(coords) - lowest ); //Shift energies up for probability calculation.

		core::Real prob( std::exp( -energies_->value(coords) / kbt ) );
		//if( std::isnan( prob ) ) prob = 1.0e-10; //TEMPORARY HACK -- fix later.
		accumulator += prob;
		probabilities_->set_value( coords, prob );
	} while ( increment_energy_coords(coords) );

	// Loop through again to normalize probabilities and to regenerate energies from probabilities:
	if ( normalize ) {
		TR << "Normalizing probabilities and regenerating energies." << std::endl;
		TR.flush();
		for ( core::Size i(1), imax(coords.size()); i<=imax; ++i ) {
			coords[i] = 0;
		}
		do {
			debug_assert( energies_->is_valid_position( coords ) );
			debug_assert( probabilities_->is_valid_position( coords ) );
			core::Real const normedprob( probabilities_->value(coords) / accumulator );
			probabilities_->set_value(coords, normedprob);
			if ( !symmetrize_gly_ ) energies_->set_value( coords, -kbt*std::log( normedprob ) ); //If the tensors have to be symmetrized anyways, don't bother calculating energies here.
		} while( increment_energy_coords(coords) );
	}

	// Symmetrize tensors if we're supposed to do so:
	if ( symmetrize_gly_ ) {
		TR << "Symmetrizing probability tensor." << std::endl;
		TR.flush();
		symmetrize_tensor( probabilities_ );
		TR << "Regenerating energies tensor." << std::endl;
		TR.flush();
		energies_from_probs( energies_, probabilities_, kbt );
	}

	initialized_ = true;
}

/// @brief After the mainchain scoretable has been finalized, write it to a scorefile.
void
MainchainScoreTable::write_mainchain_scoretable_to_stream( std::stringstream & outstream ) const {
	outstream << "@N_MAINCHAIN_TORSIONS " << mainchain_torsions_covered_.size() << "\n";
	outstream << "@N_MAINCHAIN_TORSIONS_TOTAL " << n_mainchain_torsions_total_ << "\n";
	outstream << "@MAINCHAIN_TORSIONS_PROVIDED ";
	for ( core::Size i(1), imax(mainchain_torsions_covered_.size()); i<=imax; ++i ) {
		outstream << mainchain_torsions_covered_[i];
		if ( i<imax ) outstream << " ";
	}
	outstream << "\n";
	outstream << "@DIMENSIONS ";
	for ( core::Size i(1), imax(energies_->dimensionality()); i<=imax; ++i ) {
		outstream << energies_->n_bins(i);
		if ( i<imax ) outstream << " ";
	}
	outstream << "\n";
	outstream << "@OFFSETS ";
	for ( core::Size i(1), imax(energies_->dimensionality()); i<=imax; ++i ) {
		outstream << (symmetrize_gly_ ? static_cast<core::Size>(std::round(180.0/static_cast<core::Real>(energies_->n_bins(i)))) : 0 );
		if ( i<imax ) outstream << " ";
	}

	outstream << "\n#Resname\t";
	if ( dimension_ == 2 && ( mainchain_torsions_covered_.empty() || ( mainchain_torsions_covered_[1] == 1 && mainchain_torsions_covered_[2] == 2 ) ) ) {
		outstream << "phi\tpsi\t";
	} else {
		for ( core::Size i(1); i<=dimension_; ++i ) {
			outstream << "tors" << ( mainchain_torsions_covered_.empty() ? i : mainchain_torsions_covered_[i] ) << "\t";
		}
	}
	outstream << "probablity\t-log(prob)\n";

	utility::vector1< core::Size > coords( energies_->dimensionality(), 0 );
	do {
		utility::vector1< core::Real > torsions;
		get_mainchain_torsions_from_coords( coords, torsions, false );

		outstream << "NCAA_NAME\t"; //TODO replace with NCAA name.
		for ( core::Size i(1), imax(torsions.size()); i<=imax; ++i ) {
			outstream << numeric::principal_angle_degrees( torsions[i] ) << "\t";
		}
		outstream << probabilities_->value( coords ) << "\t" << energies_->value( coords ) << "\n";
	} while( increment_energy_coords(coords) );
}

/// @brief Given a vector of coordinates, get the corresponding vector of mainchain torsions.
/// @details Bounds-checking only in debug mode!
/// @note Offset is 0 if symmetrize_gly_ is false, 1/2 well if symmetrize_gly_ is true.  Offset is only added if offset_if_symmetric
/// is set to true.
void
MainchainScoreTable::get_mainchain_torsions_from_coords(
	utility::vector1< core::Size > const & coords_in,
	utility::vector1< core::Real > & torsions_out,
	bool const offset_if_symmetric /*= true*/
) const {
	debug_assert( energies_ != nullptr );
	debug_assert( energies_->is_valid_position(coords_in) );
	torsions_out.resize(coords_in.size());
	for ( core::Size i(1), imax(coords_in.size()); i<=imax; ++i ) {
		core::Size const nbins( energies_->n_bins(i) );
		torsions_out[i] = 360.0 / static_cast< core::Real >(nbins) * coords_in[i] + ( symmetrize_gly_ && offset_if_symmetric ? 180.0/static_cast<core::Real>( energies_->n_bins(i) ) : 0 );
	}
}

/// @brief Access an entry in the energy tensor.
/// @details Note that bounds checking only occurs in debug mode!
/// @note Intended for unit testing.
core::Real const &
MainchainScoreTable::energy_tensor(
	utility::vector1< core::Size > const & coords
) const {
	runtime_assert(initialized());
	debug_assert( energies_->is_valid_position(coords) );
	return energies_->value(coords);
}

/// @brief Access values in this MainchainScoreTable.
/// @details Note that the vector is deliberately not passed by reference.  The function copies the vector and ensures
/// that all coordinates are in the range [0, 360).
/// @note The full vector of mainchain torsions should be passed in.  If the potential is a function of fewer degrees of freedom,
/// this function will disregard the appropraite entries in the coords vector.
core::Real
MainchainScoreTable::energy(
	utility::vector1 < core::Real > coords
) const {
	using namespace numeric;
	using namespace numeric::interpolation::spline;

	runtime_assert_string_msg( initialized(), "Error in core::chemical::mainchain_potential::MainchainScoreTable::energy(): The MainchainScoreTable has not yet been initialized!" );

	runtime_assert_string_msg( coords.size() == n_mainchain_torsions_total_, "Error in core::chemical::mainchain_potential::MainchainScoreTable::energy(): The dimensionality of the MainchainScoreTable and the coordinates vector must match." );
	runtime_assert_string_msg( coords.size() > 0, "Error in core::chemical::mainchain_potential::MainchainScoreTable::energy(): The dimensionality of the coordinates vector must be greater than 0." );

	//If this mainchain potential is a function of only a subset of mainchain degrees of freedom, update the coords vector to reflect that.
	if ( dimension_ < n_mainchain_torsions_total_ ) {
		utility::vector1< core::Real > coords_copy(dimension_);
		debug_assert( dimension_ == mainchain_torsions_covered_.size());
		for ( core::Size i(1); i<=dimension_; ++i ) {
			coords_copy[i] = coords[ mainchain_torsions_covered_[i] ];
		}
		coords = coords_copy;
	}

	for ( core::Size i=1, imax=coords.size(); i<=imax; ++i ) coords[i] = numeric::nonnegative_principal_angle_degrees( coords[i] );

	switch( coords.size() ) {
	case 1 :
		runtime_assert(energies_spline_1D_);
		return energies_spline_1D_->F( coords[1] );
	case 2 :
		runtime_assert(energies_spline_2D_);
		return energies_spline_2D_->F( coords[1], coords[2] );
	case 3 : //Cases 3 through 9 all call the same code (numeric::interpolation::spline::get_PolycubicSpline_F).
	case 4:
	case 5:
	case 6:
	case 7:
	case 8:
	case 9 :
		return get_PolycubicSpline_F( energies_spline_ND_, coords );
	default :
		utility_exit_with_message( "Error in core::chemical::mainchain_potential::MainchainScoreTable::energy(): The dimensionality of the coordinates vector must be less than 9." );
	}

	return 0.0; //Should never be called.
}

/// @brief Get the gradient with respect to x1,x2,x3,...xn for this MainchainScoreTable.
/// @param[in] coords_in The coordinates at which to evaluate the gradient.
/// @param[out] gradient_out The resulting gradient.
/// @note The full vector of mainchain torsions should be passed in.  If the potential is a function of fewer degrees of freedom,
/// this function will disregard the appropraite entries in the coords vector.
void
MainchainScoreTable::gradient(
	utility::vector1 < core::Real > coords_in,
	utility::vector1 < core::Real > & gradient_out
) const {
	using namespace numeric;
	using namespace numeric::interpolation::spline;

	runtime_assert_string_msg( initialized(), "Error in core::chemical::mainchain_potential::MainchainScoreTable::gradient(): The MainchainScoreTable has not yet been initialized!" );

	runtime_assert_string_msg( coords_in.size() == n_mainchain_torsions_total_, "Error in core::chemical::mainchain_potential::MainchainScoreTable::gradient(): The dimensionality of the MainchainScoreTable and the coordinates vector must match." );

	//If this mainchain potential is a function of only a subset of mainchain degrees of freedom, update the coords vector to reflect that.
	if ( dimension_ < n_mainchain_torsions_total_ ) {
		utility::vector1< core::Real > coords_copy(dimension_);
		debug_assert( dimension_ == mainchain_torsions_covered_.size());
		for ( core::Size i(1); i<=dimension_; ++i ) {
			coords_copy[i] = coords_in[ mainchain_torsions_covered_[i] ];
		}
		coords_in = coords_copy;
	}

	for ( core::Size i=1, imax=coords_in.size(); i<=imax; ++i ) coords_in[i] = numeric::nonnegative_principal_angle_degrees( coords_in[i] );

	switch( coords_in.size() ) {
	case 1 :
		runtime_assert(energies_spline_1D_);
		gradient_out.resize(1);
		gradient_out[1] = energies_spline_1D_->dF( coords_in[1] );
		break;
	case 2 :
		runtime_assert(energies_spline_2D_);
		gradient_out.resize(2);
		gradient_out[1] = energies_spline_2D_->dFdx( coords_in[1], coords_in[2] );
		gradient_out[2] = energies_spline_2D_->dFdy( coords_in[1], coords_in[2] );
		break;
	case 3 : //Cases 3 through 9 all call the same code (numeric::interpolation::spline::get_PolycubicSpline_F).
	case 4:
	case 5:
	case 6:
	case 7:
	case 8:
	case 9 :
		get_PolycubicSpline_gradient( energies_spline_ND_, coords_in, gradient_out );
		break;
	default :
		utility_exit_with_message( "Error in core::chemical::mainchain_potential::MainchainScoreTable::gradient(): The dimensionality of the coordinates vector must be less than 9." );
	}

	//Need to adjust the size of the gradient vector (padding with zero) if the number of degrees of freedom covered by the
	//mainchain potential is less than the total number of mainchain degrees of freedom.
	if ( dimension_ < n_mainchain_torsions_total_ ) {
		utility::vector1< core::Real > gradient_out_copy( n_mainchain_torsions_total_, 0.0 );
		debug_assert( dimension_ == mainchain_torsions_covered_.size());
		for ( core::Size i(1); i<=dimension_; ++i ) {
			gradient_out_copy[mainchain_torsions_covered_[i]] = gradient_out[i];
		}
		gradient_out = gradient_out_copy;
	}
}

/// @brief Set whether we should symmetrize tables for glycine.
///
void
MainchainScoreTable::set_symmetrize_gly(
	bool const setting_in
) {
	symmetrize_gly_ = setting_in;
}

/// @brief Given the cumulative distribution function (pre-calculated), draw a random set of mainchain torsion values
/// biased by the probability distribution.
/// @details output is in the range (-180, 180].
/// @note The dimensionality of the torsions vector will match the degrees of freedom covered by the mainchain potential, NOT the
/// total mainchain degrees of freedom.  Use the get_mainchain_torsions_covered() function to get the vector of mainchiain torsion
/// indices that are covered.
void
MainchainScoreTable::draw_random_mainchain_torsion_values(
	utility::vector1 < core::Real > &torsions
) const {
	runtime_assert_string_msg( initialized(), "Error in core::chemical::mainchain_potential::MainchainScoreTable::draw_random_mainchain_torsion_values(): The MainchainScoreTable is not initialized."  );

	//Draw a random number, uniformly-distributed on the interval [0,1]
	core::Real const randval( numeric::random::rg().uniform() );

	//Loop through the cumulative distribution function to find the bin greater than that value.
	//Note that the CDF is set up so that each bin stores the probability of being in that bin or an earlier bin.
	//So the algorithm is to draw a random number, then loop through to find the bin where the value is equal or greater.
	core::Size const dims( cdf_->dimensionality() );
	utility::vector1 < core::Size > coords( dims, 0 );
	do {
		if ( numeric::const_access_Real_MathNTensor( cdf_, coords ) >= randval ) break;
	} while( increment_coords(coords, cdf_) );

	//Now we've found coordinates for the bin from which we want to draw random mainchain torsions.  We need to figure out to which torsion
	//value ranges this bin corresponds, then draw uniform random numbers in those ranges.
	core::Size dim_index(0);
	torsions.resize( n_mainchain_torsions_total_ );
	for ( core::Size i=1; i<=n_mainchain_torsions_total_; ++i ) { //Loop through the coordinates
		bool is_covered( false );
		for ( core::Size j(1), jmax(mainchain_torsions_covered_.size()); j<=jmax; ++j ) {
			if ( i == mainchain_torsions_covered_[j] ) {
				is_covered = true;
				++dim_index;
				break;
			}
		}
		if ( !is_covered ) {
			torsions[i] = 0.0;
			continue;
		}
		core::Size const nbins( numeric::get_Real_MathNTensor_dimension_size( cdf_, dim_index ) ); //Get the number of bins in the ith dimension.
		core::Real const binwidth( 360.0 / static_cast< core::Real >(nbins) ); //Calculate the width of a bin.
		torsions[i] = numeric::principal_angle_degrees( numeric::random::rg().uniform() * binwidth + binwidth * static_cast<core::Real>(coords[dim_index]) );
	}
}


/********************
PRIVATE FUNCTIONS
********************/

/// @brief Sets the state of this MainchainScoreTable object to "initialized".
/// @details Double-checks that it's not already initialized, and throws an error if it is.
void
MainchainScoreTable::set_initialized() {
	runtime_assert_string_msg( !initialized(), "Error in core::chemical::mainchain_potential::MainchainScoreTable::set_initialized(): The MainchainScoreTable is already initialized." );
	initialized_ = true;
}

/// @brief Given a residue type, set dimension_ and n_mainchain_torsions_total_, and resize all tensors appropriately.
/// @details Assumes that all data have already been cleared.  Call clear() before invoking this method.
/// @note The mainchain_torsions_covered vector is deliberately passed by copy.
void
MainchainScoreTable::set_dimension_and_ntorsions_for_restype(
	core::chemical::ResidueType const & restype,
	utility::vector1< core::Size > const & dimensions,
	utility::vector1< core::Size > mainchain_torsions_covered
) {
	static const std::string errmsg("Error in MainchainScoreTable::set_dimension_and_ntorsions_for_restype(): ");

	dimension_ = dimensions.size();

	// By default (if not user-specified), we generate the mainchain potential for all mainchain dihedrals:
	if ( mainchain_torsions_covered.empty() ) {
		mainchain_torsions_covered.resize(dimension_);
		for ( core::Size i(1); i<=dimension_; ++i ) {
			mainchain_torsions_covered[i] = i;
		}
	}

	runtime_assert_string_msg( mainchain_torsions_covered.size() <= dimension_, errmsg + "The number of mainchain torsions specified exceeds the number for the " + restype.base_name() + " residue type." );

	n_mainchain_torsions_total_ = mainchain_torsions_covered.size();
	runtime_assert_string_msg( dimensions.size() == n_mainchain_torsions_total_, errmsg + "The number of entries in the mainchain scoretable dimensions vector does not match the number of rotatable mainchain bonds for residue type " + restype.name() + "." );

	mainchain_torsions_covered_.clear();
	mainchain_torsions_covered_.reserve( n_mainchain_torsions_total_ );
	for ( core::Size i(1); i<=n_mainchain_torsions_total_; ++i ) {
		runtime_assert_string_msg( !mainchain_torsions_covered_.has( mainchain_torsions_covered[i] ), errmsg + "A mainchain torsion index was given twice." );
		runtime_assert_string_msg( mainchain_torsions_covered[i] <= dimension_, errmsg + "A mainchain torsion index was given that exceeds the number of mainchain torsions in the " + restype.base_name() + " residue type." );
		mainchain_torsions_covered_.push_back( mainchain_torsions_covered[i] );
	}

	initialize_tensors(dimensions);
}

/// @brief Set up the cumulative distribution function.
/// @details The CDF is used for drawing random mainchain torsion values biased by the relative probabilities of mainchain torsion values.
/// @note Each bin stores the probability of being in the current bin or an earlier bin (so the final bin should store a probability of 1).
/// This differs from the convention used in Ramachandran.cc, but allows for a simpler drawing algorithm: I pick a uniformly-distributed random
/// number from 0 to 1, loop through my bins, and stop when I get to a bin with a value greater than the value that I have drawn.
/// @param[in] probs Tensor of probabilities.  Need not be normalized (sum to 1).
/// @param[out] cdf Tensor for the cumulative distribution function.
void
MainchainScoreTable::set_up_cumulative_distribution_function(
	numeric::MathNTensorBaseCOP< core::Real > probs,
	numeric::MathNTensorBaseOP< core::Real > cdf
) const {
	core::Size const dims( probs->dimensionality() );
	runtime_assert_string_msg( cdf->dimensionality() == dims, "Error in core::chemical::mainchain_potential::MainchainScoreTable::set_up_cumulative_distribution_function():  The cdf tensor has different dimensionality than the probabilities tensor." );

	utility::vector1< core::Size > coords( dims, 0 );

	core::Real accumulator(0.0);

	do {
		accumulator += numeric::const_access_Real_MathNTensor( probs, coords );
		numeric::access_Real_MathNTensor( cdf, coords ) = accumulator;
	} while ( increment_coords(coords, probs) );

	runtime_assert( accumulator > 1e-15 ); //Should be true.

	//Now, we need to normalize:
	utility::vector1< core::Size > coords2( dims, 0 );
	do {
		numeric::access_Real_MathNTensor( cdf, coords2 ) /= accumulator;
	} while( increment_coords(coords2, cdf) );
}

/// @brief Given a probabilities tensor, calculate the energies.
/// @details Tensors must be the same size.  Contents of the probabilities tensor are overwritten.
/// @param[out] energies Tensor of energies.
/// @param[in] probs Tensor of probabilities.
/// @param[in] kbt Boltzmann temperature (k_B*T), in Rosetta energy units.
void
MainchainScoreTable::energies_from_probs(
	numeric::MathNTensorBaseOP< core::Real > energies,
	numeric::MathNTensorBaseCOP< core::Real > probs,
	core::Real const &kbt
) const {
	core::Size const dims( probs->dimensionality() );
	runtime_assert_string_msg( energies->dimensionality() == dims, "Error in core::chemical::mainchain_potential::MainchainScoreTable::energies_from_probs(): The energies and probabilities tensors have diffent dimensionality.");

	utility::vector1< core::Size > coords( dims, 0 );

	do {
		numeric::access_Real_MathNTensor( energies, coords ) = -1.0*kbt*log( numeric::const_access_Real_MathNTensor( probs, coords ) );
	} while ( increment_coords(coords, energies) );

}

void
MainchainScoreTable::check_linestream(
	std::istringstream const &linestream,
	std::string const &filename,
	bool const fail_on_eof
) const {
	runtime_assert_string_msg( !linestream.bad() && ( !fail_on_eof || !linestream.eof()), "Error core::chemical::mainchain_potential::MainchainScoreTable::check_linestream():  Error reading file " + filename + ".  Bad data line encountered.  [Line was \"" + linestream.str() + "\".]");
}

/// @brief Initialize the energies_ and probabilities_ tensors to 0-containing N-tensors, of the
/// dimensions given by the dimensions vector.
void
MainchainScoreTable::initialize_tensors(
	utility::vector1 < core::Size > dimensions_vector
) {
	using namespace numeric;
	using namespace numeric::interpolation::spline;

	dimension_ = dimensions_vector.size();

	//Ugh -- because of the template classes which require dimensionality to be known at compile time, this has to be one giant switch statement:
	switch( dimension_ ) {
	case 1 :
		{
		utility::fixedsizearray1< core::Size, 1 > dimensions;
		dimensions[1] = dimensions_vector[1];
		for ( core::Size i=1; i<=dimension_; ++i ) dimensions[i] = dimensions_vector[i];
		energies_ = MathNTensorBaseOP< core::Real >( new MathNTensor< core::Real, 1 >(dimensions, 0.0) );
		probabilities_ = MathNTensorBaseOP< core::Real >( new MathNTensor< core::Real, 1 >(dimensions, 0.0) );
		cdf_ = MathNTensorBaseOP< core::Real >( new MathNTensor< core::Real, 1 >(dimensions, 0.0) );
		energies_spline_1D_ = utility::pointer::make_shared< CubicSpline >();
	}
		break;
	case 2 :
		{
		utility::fixedsizearray1< core::Size, 2 > dimensions;
		dimensions[1] = dimensions_vector[1]; dimensions[2] = dimensions_vector[2];
		energies_ = MathNTensorBaseOP< core::Real >( new MathNTensor< core::Real, 2 >(dimensions, 0.0) );
		probabilities_ = MathNTensorBaseOP< core::Real >( new MathNTensor< core::Real, 2 >(dimensions, 0.0) );
		cdf_ = MathNTensorBaseOP< core::Real >( new MathNTensor< core::Real, 2 >(dimensions, 0.0) );
		energies_spline_2D_ = utility::pointer::make_shared< BicubicSpline >();
	}
		break;
	case 3 :
		{
		utility::fixedsizearray1< core::Size, 3 > dimensions;
		for ( core::Size i=1; i<=dimension_; ++i ) dimensions[i] = dimensions_vector[i];
		energies_ = MathNTensorBaseOP< core::Real >( new MathNTensor< core::Real, 3 >(dimensions, 0.0) );
		probabilities_ = MathNTensorBaseOP< core::Real >( new MathNTensor< core::Real, 3 >(dimensions, 0.0) );
		cdf_ = MathNTensorBaseOP< core::Real >( new MathNTensor< core::Real, 3 >(dimensions, 0.0) );
		energies_spline_ND_ = utility::pointer::make_shared< PolycubicSpline<3> >();
	}
		break;
	case 4 :
		{
		utility::fixedsizearray1< core::Size, 4 > dimensions;
		for ( core::Size i=1; i<=dimension_; ++i ) dimensions[i] = dimensions_vector[i];
		energies_ = MathNTensorBaseOP< core::Real >( new MathNTensor< core::Real, 4 >(dimensions, 0.0) );
		probabilities_ = MathNTensorBaseOP< core::Real >( new MathNTensor< core::Real, 4 >(dimensions, 0.0) );
		cdf_ = MathNTensorBaseOP< core::Real >( new MathNTensor< core::Real, 4 >(dimensions, 0.0) );
		energies_spline_ND_ = utility::pointer::make_shared< PolycubicSpline<4> >();
	}
		break;
	case 5 :
		{
		utility::fixedsizearray1< core::Size, 5 > dimensions;
		for ( core::Size i=1; i<=dimension_; ++i ) dimensions[i] = dimensions_vector[i];
		energies_ = MathNTensorBaseOP< core::Real >( new MathNTensor< core::Real, 5 >(dimensions, 0.0) );
		probabilities_ = MathNTensorBaseOP< core::Real >( new MathNTensor< core::Real, 5 >(dimensions, 0.0) );
		cdf_ = MathNTensorBaseOP< core::Real >( new MathNTensor< core::Real, 5 >(dimensions, 0.0) );
		energies_spline_ND_ = utility::pointer::make_shared< PolycubicSpline<5> >();
	}
		break;
	case 6 :
		{
		utility::fixedsizearray1< core::Size, 6 > dimensions;
		for ( core::Size i=1; i<=dimension_; ++i ) dimensions[i] = dimensions_vector[i];
		energies_ = MathNTensorBaseOP< core::Real >( new MathNTensor< core::Real, 6 >(dimensions, 0.0) );
		probabilities_ = MathNTensorBaseOP< core::Real >( new MathNTensor< core::Real, 6 >(dimensions, 0.0) );
		cdf_ = MathNTensorBaseOP< core::Real >( new MathNTensor< core::Real, 6 >(dimensions, 0.0) );
		energies_spline_ND_ = utility::pointer::make_shared< PolycubicSpline<6> >();
	}
		break;
	case 7 :
		{
		utility::fixedsizearray1< core::Size, 7 > dimensions;
		for ( core::Size i=1; i<=dimension_; ++i ) dimensions[i] = dimensions_vector[i];
		energies_ = MathNTensorBaseOP< core::Real >( new MathNTensor< core::Real, 7 >(dimensions, 0.0) );
		probabilities_ = MathNTensorBaseOP< core::Real >( new MathNTensor< core::Real, 7 >(dimensions, 0.0) );
		cdf_ = MathNTensorBaseOP< core::Real >( new MathNTensor< core::Real, 7 >(dimensions, 0.0) );
		energies_spline_ND_ = utility::pointer::make_shared< PolycubicSpline<7> >();
	}
		break;
	case 8 :
		{
		utility::fixedsizearray1< core::Size, 8 > dimensions;
		for ( core::Size i=1; i<=dimension_; ++i ) dimensions[i] = dimensions_vector[i];
		energies_ = MathNTensorBaseOP< core::Real >( new MathNTensor< core::Real, 8 >(dimensions, 0.0) );
		probabilities_ = MathNTensorBaseOP< core::Real >( new MathNTensor< core::Real, 8 >(dimensions, 0.0) );
		cdf_ = MathNTensorBaseOP< core::Real >( new MathNTensor< core::Real, 8 >(dimensions, 0.0) );
		energies_spline_ND_ = utility::pointer::make_shared< PolycubicSpline<8> >();
	}
		break;
	case 9 :
		{
		utility::fixedsizearray1< core::Size, 9 > dimensions;
		for ( core::Size i=1; i<=dimension_; ++i ) dimensions[i] = dimensions_vector[i];
		energies_ = MathNTensorBaseOP< core::Real >( new MathNTensor< core::Real, 9 >(dimensions, 0.0) );
		probabilities_ = MathNTensorBaseOP< core::Real >( new MathNTensor< core::Real, 9 >(dimensions, 0.0) );
		cdf_ = MathNTensorBaseOP< core::Real >( new MathNTensor< core::Real, 9 >(dimensions, 0.0) );
		energies_spline_ND_ = utility::pointer::make_shared< PolycubicSpline<9> >();
	}
		break;
	default :
		utility_exit_with_message( "Error in core:::chemical::mainchain_potential::MainchainScoreTable::initialize_tensors.  A MainchainScoreTable must be at least 1-dimensional, and at most 9-dimensional." );
		break;
	}
}


/// @brief Convert the energies from probabilities to Rosetta energy units, and add the
/// entropic correction factor.
void
MainchainScoreTable::iteratively_correct_energy_tensor(
	core::Real const &entropy
) {
	utility::vector1 < core::Size > coords;
	coords.resize( energies_->dimensionality(), 0); //Note that MathNTensors are 0-based.
	do {
		numeric::access_Real_MathNTensor(energies_, coords) = -std::log( numeric::const_access_Real_MathNTensor(energies_, coords) ) + entropy;
	} while ( increment_coords(coords, energies_) );
}

/// @brief Given coordinates in the energy tensor, go to the next bin.
/// @details As row ends are reached, the row resets to 1 and the next column is selected (and so forth down the dimensions).
/// Returns "true" if increment was successful, "false" if the end of the tensor has been reached.
bool
MainchainScoreTable::increment_coords(
	utility::vector1 < core::Size > &coords,
	numeric::MathNTensorBaseCOP< core::Real > tensor
) const {
	debug_assert( coords.size() == tensor->dimensionality() ); //Should always be true

	core::Size coord_index(1);
	do {
		++coords[coord_index];
		if ( coords[coord_index] >= numeric::get_Real_MathNTensor_dimension_size( tensor, coord_index ) ) {
			coords[coord_index] = 0; //MathNTensors are 0-based.
			++coord_index;
			if ( coord_index > coords.size() ) return false;
		} else {
			break;
		}
	} while( true );

	return true;
}

/// @brief Given a set of coordinates in a MathNTensor, get the opposite coordinates.
/// @details For example, in a 2D 5x5 tensor, (1, 3) would yield an opposite of (3, 1).
void
MainchainScoreTable::get_opposite_coord(
	utility::vector1 < core::Size > const &coord,
	numeric::MathNTensorBaseCOP< core::Real > tensor,
	utility::vector1 <core::Size> &opposite_coord
) const {
	opposite_coord.resize(coord.size());
	debug_assert( coord.size() == tensor->dimensionality() );
	for ( core::Size i=1, imax=coord.size(); i<=imax; ++i ) {
		opposite_coord[i] = numeric::get_Real_MathNTensor_dimension_size( tensor, i ) - 1 - coord[i];
	}
}


/// @brief Once the internal MathNTensor has been set up, set up polycubic interpolation.
/// @details This function includes special-case logic for setting up cubic interpolation in the 1D case and
/// bicubic interpolation in the 2D case, since these are not handled by the PolycubicSpline class.
/// @param[in] offsets Vector of offset values, from 0 to 1 -- where centres are, as fraction of bin width.
/// @param[in] dimensions Vector of number of entries in each dimension.  Bin widths are inferred from this: 36 entries would correspond to 10-degree bins.
void
MainchainScoreTable::set_up_polycubic_interpolation(
	utility::vector1 < core::Real > const &offsets,
	utility::vector1 < core::Size > const &dimensions
) {
	std::string const errormsg( "Error in core::chemical::mainchain_potential::set_up_polycubic_interpolation(): " );

	runtime_assert_string_msg( dimension_ > 0, errormsg + "The MainchainScoreTable has dimensionality zero.  Has it been initialized?" );
	runtime_assert_string_msg( dimension_ == energies_->dimensionality(), errormsg + "The MainchainScoreTable has different dimensionality than the energies_ tensor." );
	runtime_assert_string_msg( dimension_ == probabilities_->dimensionality(), errormsg + "The MainchainScoreTable has different dimensionality than the probabilities_ tensor." );
	runtime_assert_string_msg( dimension_ == offsets.size(), errormsg + "The MainchainScoreTable has different dimensionality than the number of entries in the offsets vector." );
	runtime_assert_string_msg( dimension_ == dimensions.size(), errormsg + "The MainchainScoreTable has different dimensionality than the number of entries in the dimensions vector." );

	using namespace numeric;
	using namespace numeric::interpolation::spline;

	switch( dimension_ ) {
	case 1 :
		//TODO
		break;
	case 2 :
		{ //Scope for variable declaration
		MathNTensorOP< core::Real, 2 > energies_ptr( utility::pointer::dynamic_pointer_cast< MathNTensor< core::Real, 2 > >( energies_ ) );
		runtime_assert( energies_ptr );
		MathMatrix < core::Real > const energies_matrix( energies_ptr->get_mathmatrix() );
		BorderFlag borderflags_array[2] = {e_Periodic, e_Periodic};
		core::Real offset_array[2] = {offsets[1], offsets[2]};
		bool const lincont_array[2] = {false,false}; //meaningless argument for a bicubic spline with periodic boundary conditions
		core::Real deltas_array[2]; //Initialized below.
		std::pair< core::Real, core::Real > unused_array[2]; //Initialized below.
		for ( core::Size i=0; i<2; ++i ) {
			runtime_assert_string_msg( dimensions[i+1] > 0, errormsg + "The dimensions vector has zero as an entry.  This should not be possible." );
			deltas_array[i] = 360.0 / static_cast< core::Real >( dimensions[i+1] );
			unused_array[i] = std::pair<core::Real, core::Real>( 0.0, 0.0 );
		}
		energies_spline_2D_ = utility::pointer::make_shared< BicubicSpline >();
		debug_assert( energies_spline_2D_ );
		energies_spline_2D_->train( borderflags_array, offset_array, deltas_array, energies_matrix, lincont_array, unused_array );
	}
		break;
	case 3 :
		{ //Scope for variable declaration
		utility::fixedsizearray1< BorderFlag, 3 > borderflags; //Periodic boundaries.
		utility::fixedsizearray1< core::Real, 3 > offsets_array; //Bin widths.
		utility::fixedsizearray1< core::Real, 3 > deltas; //Bin widths.
		utility::fixedsizearray1< bool, 3 > lincont; //Meaningless argument for a polycubic spline with periodic boundary conditions.
		utility::fixedsizearray1< std::pair< core::Real, core::Real >, 3 > unused( std::pair<core::Real,core::Real>(0.0, 0.0) ); //Another meaningless argument for a polycubic spline with periodic boundary conditions.
		MathNTensorOP< core::Real, 3 > energies_ptr( utility::pointer::dynamic_pointer_cast< MathNTensor< core::Real, 3 > >( energies_ ) );
		runtime_assert( energies_ptr );
		for ( core::Size i=1; i<=dimension_; ++i ) {
			runtime_assert_string_msg( dimensions[i] > 0, errormsg + "The dimensions vector has zero as an entry.  This should not be possible." );
			borderflags[i] = e_Periodic;
			offsets_array[i] = offsets[i];
			deltas[i] = 360.0 / static_cast< core::Real >( dimensions[i] );
			lincont[i] = false;
		}
		PolycubicSplineOP<3> spline( new PolycubicSpline<3> );
		energies_spline_ND_ = PolycubicSplineBaseOP(spline);
		debug_assert( energies_spline_ND_ );
		spline->train( borderflags, offsets_array, deltas, *energies_ptr, lincont, unused );
		break;
	}
		break;
	case 4 :
		{ //Scope for variable declaration
		utility::fixedsizearray1< BorderFlag, 4 > borderflags; //Periodic boundaries.
		utility::fixedsizearray1< core::Real, 4 > offsets_array; //Bin widths.
		utility::fixedsizearray1< core::Real, 4 > deltas; //Bin widths.
		utility::fixedsizearray1< bool, 4 > lincont; //Meaningless argument for a polycubic spline with periodic boundary conditions.
		utility::fixedsizearray1< std::pair< core::Real, core::Real >, 4 > unused( std::pair<core::Real,core::Real>(0.0, 0.0) ); //Another meaningless argument for a polycubic spline with periodic boundary conditions.
		MathNTensorOP< core::Real, 4 > energies_ptr( utility::pointer::dynamic_pointer_cast< MathNTensor< core::Real, 4 > >( energies_ ) );
		runtime_assert( energies_ptr );
		for ( core::Size i=1; i<=dimension_; ++i ) {
			runtime_assert_string_msg( dimensions[i] > 0, errormsg + "The dimensions vector has zero as an entry.  This should not be possible." );
			borderflags[i] = e_Periodic;
			offsets_array[i] = offsets[i];
			deltas[i] = 360.0 / static_cast< core::Real >( dimensions[i] );
			lincont[i] = false;
		}
		PolycubicSplineOP<4> spline( new PolycubicSpline<4> );
		energies_spline_ND_ = PolycubicSplineBaseOP(spline);
		debug_assert( energies_spline_ND_ );
		spline->train( borderflags, offsets_array, deltas, *energies_ptr, lincont, unused );
		break;
	}
		break;
	case 5 :
		{ //Scope for variable declaration
		utility::fixedsizearray1< BorderFlag, 5 > borderflags; //Periodic boundaries.
		utility::fixedsizearray1< core::Real, 5 > offsets_array; //Bin widths.
		utility::fixedsizearray1< core::Real, 5 > deltas; //Bin widths.
		utility::fixedsizearray1< bool, 5 > lincont; //Meaningless argument for a polycubic spline with periodic boundary conditions.
		utility::fixedsizearray1< std::pair< core::Real, core::Real >, 5 > unused( std::pair<core::Real,core::Real>(0.0, 0.0) ); //Another meaningless argument for a polycubic spline with periodic boundary conditions.
		MathNTensorOP< core::Real, 5 > energies_ptr( utility::pointer::dynamic_pointer_cast< MathNTensor< core::Real, 5 > >( energies_ ) );
		runtime_assert( energies_ptr );
		for ( core::Size i=1; i<=dimension_; ++i ) {
			runtime_assert_string_msg( dimensions[i] > 0, errormsg + "The dimensions vector has zero as an entry.  This should not be possible." );
			borderflags[i] = e_Periodic;
			offsets_array[i] = offsets[i];
			deltas[i] = 360.0 / static_cast< core::Real >( dimensions[i] );
			lincont[i] = false;
		}
		PolycubicSplineOP<5> spline( new PolycubicSpline<5> );
		energies_spline_ND_ = PolycubicSplineBaseOP(spline);
		debug_assert( energies_spline_ND_ );
		spline->train( borderflags, offsets_array, deltas, *energies_ptr, lincont, unused );
		break;
	}
		break;
	case 6 :
		{ //Scope for variable declaration
		utility::fixedsizearray1< BorderFlag, 6 > borderflags; //Periodic boundaries.
		utility::fixedsizearray1< core::Real, 6 > offsets_array; //Bin widths.
		utility::fixedsizearray1< core::Real, 6 > deltas; //Bin widths.
		utility::fixedsizearray1< bool, 6 > lincont; //Meaningless argument for a polycubic spline with periodic boundary conditions.
		utility::fixedsizearray1< std::pair< core::Real, core::Real >, 6 > unused( std::pair<core::Real,core::Real>(0.0, 0.0) ); //Another meaningless argument for a polycubic spline with periodic boundary conditions.
		MathNTensorOP< core::Real, 6 > energies_ptr( utility::pointer::dynamic_pointer_cast< MathNTensor< core::Real, 6 > >( energies_ ) );
		runtime_assert( energies_ptr );
		for ( core::Size i=1; i<=dimension_; ++i ) {
			runtime_assert_string_msg( dimensions[i] > 0, errormsg + "The dimensions vector has zero as an entry.  This should not be possible." );
			borderflags[i] = e_Periodic;
			offsets_array[i] = offsets[i];
			deltas[i] = 360.0 / static_cast< core::Real >( dimensions[i] );
			lincont[i] = false;
		}
		PolycubicSplineOP<6> spline( new PolycubicSpline<6> );
		energies_spline_ND_ = PolycubicSplineBaseOP(spline);
		debug_assert( energies_spline_ND_ );
		spline->train( borderflags, offsets_array, deltas, *energies_ptr, lincont, unused );
		break;
	}
		break;
	case 7 :
		{ //Scope for variable declaration
		utility::fixedsizearray1< BorderFlag, 7 > borderflags; //Periodic boundaries.
		utility::fixedsizearray1< core::Real, 7 > offsets_array; //Bin widths.
		utility::fixedsizearray1< core::Real, 7 > deltas; //Bin widths.
		utility::fixedsizearray1< bool, 7 > lincont; //Meaningless argument for a polycubic spline with periodic boundary conditions.
		utility::fixedsizearray1< std::pair< core::Real, core::Real >, 7 > unused( std::pair<core::Real,core::Real>(0.0, 0.0) ); //Another meaningless argument for a polycubic spline with periodic boundary conditions.
		MathNTensorOP< core::Real, 7 > energies_ptr( utility::pointer::dynamic_pointer_cast< MathNTensor< core::Real, 7 > >( energies_ ) );
		runtime_assert( energies_ptr );
		for ( core::Size i=1; i<=dimension_; ++i ) {
			runtime_assert_string_msg( dimensions[i] > 0, errormsg + "The dimensions vector has zero as an entry.  This should not be possible." );
			borderflags[i] = e_Periodic;
			offsets_array[i] = offsets[i];
			deltas[i] = 360.0 / static_cast< core::Real >( dimensions[i] );
			lincont[i] = false;
		}
		PolycubicSplineOP<7> spline( new PolycubicSpline<7> );
		energies_spline_ND_ = PolycubicSplineBaseOP(spline);
		debug_assert( energies_spline_ND_ );
		spline->train( borderflags, offsets_array, deltas, *energies_ptr, lincont, unused );
		break;
	}
		break;
	case 8 :
		{ //Scope for variable declaration
		utility::fixedsizearray1< BorderFlag, 8 > borderflags; //Periodic boundaries.
		utility::fixedsizearray1< core::Real, 8 > offsets_array; //Bin widths.
		utility::fixedsizearray1< core::Real, 8 > deltas; //Bin widths.
		utility::fixedsizearray1< bool, 8 > lincont; //Meaningless argument for a polycubic spline with periodic boundary conditions.
		utility::fixedsizearray1< std::pair< core::Real, core::Real >, 8 > unused( std::pair<core::Real,core::Real>(0.0, 0.0) ); //Another meaningless argument for a polycubic spline with periodic boundary conditions.
		MathNTensorOP< core::Real, 8 > energies_ptr( utility::pointer::dynamic_pointer_cast< MathNTensor< core::Real, 8 > >( energies_ ) );
		runtime_assert( energies_ptr );
		for ( core::Size i=1; i<=dimension_; ++i ) {
			runtime_assert_string_msg( dimensions[i] > 0, errormsg + "The dimensions vector has zero as an entry.  This should not be possible." );
			borderflags[i] = e_Periodic;
			offsets_array[i] = offsets[i];
			deltas[i] = 360.0 / static_cast< core::Real >( dimensions[i] );
			lincont[i] = false;
		}
		PolycubicSplineOP<8> spline( new PolycubicSpline<8> );
		energies_spline_ND_ = PolycubicSplineBaseOP(spline);
		debug_assert( energies_spline_ND_ );
		spline->train( borderflags, offsets_array, deltas, *energies_ptr, lincont, unused );
		break;
	}
		break;
	case 9 :
		{ //Scope for variable declaration
		utility::fixedsizearray1< BorderFlag, 9 > borderflags; //Periodic boundaries.
		utility::fixedsizearray1< core::Real, 9 > offsets_array; //Bin widths.
		utility::fixedsizearray1< core::Real, 9 > deltas; //Bin widths.
		utility::fixedsizearray1< bool, 9 > lincont; //Meaningless argument for a polycubic spline with periodic boundary conditions.
		utility::fixedsizearray1< std::pair< core::Real, core::Real >, 9 > unused( std::pair<core::Real,core::Real>(0.0, 0.0) ); //Another meaningless argument for a polycubic spline with periodic boundary conditions.
		MathNTensorOP< core::Real, 9 > energies_ptr( utility::pointer::dynamic_pointer_cast< MathNTensor< core::Real, 9 > >( energies_ ) );
		runtime_assert( energies_ptr );
		for ( core::Size i=1; i<=dimension_; ++i ) {
			runtime_assert_string_msg( dimensions[i] > 0, errormsg + "The dimensions vector has zero as an entry.  This should not be possible." );
			borderflags[i] = e_Periodic;
			offsets_array[i] = offsets[i];
			deltas[i] = 360.0 / static_cast< core::Real >( dimensions[i] );
			lincont[i] = false;
		}
		PolycubicSplineOP<9> spline( new PolycubicSpline<9> );
		energies_spline_ND_ = PolycubicSplineBaseOP(spline);
		debug_assert( energies_spline_ND_ );
		spline->train( borderflags, offsets_array, deltas, *energies_ptr, lincont, unused );
		break;
	}
		break;
	default :
		utility_exit_with_message( errormsg + "The number of dimensions must be greater than 0 and less than or equal to 9." );
	}

}

/// @brief Given a tensor, symmetrize it.
/// @details Assumes that tensor stores probabilities; normalizes tensor in the process.
void
MainchainScoreTable::symmetrize_tensor(
	numeric::MathNTensorBaseOP< core::Real > tensor
) const {
	TR << "Symmetrizing tensor for glycine." << std::endl;
	core::Real accumulator( 0.0 );
	utility::vector1 < core::Size > coord( tensor->dimensionality(), 0 );
	utility::vector1 < core::Size > opposite_coord( tensor->dimensionality(), 0 );

	do {
		get_opposite_coord( coord, tensor, opposite_coord );
		core::Real const val ( ( numeric::access_Real_MathNTensor( tensor, coord ) + numeric::access_Real_MathNTensor( tensor, opposite_coord ) ) / 2.0 );
		numeric::access_Real_MathNTensor( tensor, coord ) = val;
		numeric::access_Real_MathNTensor( tensor, opposite_coord ) = val;
		accumulator += val;
	} while( increment_coords( coord, tensor ) );

	//Normalize:
	for ( core::Size i=1, imax=coord.size(); i<=imax; ++i ) { coord[i] = 0; }
	do {
		numeric::access_Real_MathNTensor( tensor, coord ) /= accumulator;
	} while( increment_coords( coord, tensor ) );

}

} //mainchain_potential
} //chemical
} //core
