// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file   core/chemical/AA.cc
/// @brief  translation between amino acid enum and string name/one letter char codes
/// @author Phil Bradley

// Rosetta headers
#include <core/chemical/AA.hh>

// Utility headers
#include <utility/exit.hh>

// C++ headers
#include <map>
#include <string>

#include <utility/vector1.hh>
#include <utility/thread/backwards_thread_local.hh>
#include <sstream>


namespace core {
namespace chemical {


/* BEGIN: the following functions are local to this file */


/// @brief setup the map that converts string name to AA enum
std::map< std::string, AA > setup_name2aa() {

	std::map< std::string, AA > n2aa;

	// L-amino acid types
	n2aa[ "ALA" ] = aa_ala;
	n2aa[ "CYS" ] = aa_cys;
	n2aa[ "ASP" ] = aa_asp;
	n2aa[ "GLU" ] = aa_glu;
	n2aa[ "PHE" ] = aa_phe;
	n2aa[ "GLY" ] = aa_gly;
	n2aa[ "HIS" ] = aa_his;
	n2aa[ "ILE" ] = aa_ile;
	n2aa[ "LYS" ] = aa_lys;
	n2aa[ "LEU" ] = aa_leu;
	n2aa[ "MET" ] = aa_met;
	n2aa[ "ASN" ] = aa_asn;
	n2aa[ "PRO" ] = aa_pro;
	n2aa[ "GLN" ] = aa_gln;
	n2aa[ "ARG" ] = aa_arg;
	n2aa[ "SER" ] = aa_ser;
	n2aa[ "THR" ] = aa_thr;
	n2aa[ "VAL" ] = aa_val;
	n2aa[ "TRP" ] = aa_trp;
	n2aa[ "TYR" ] = aa_tyr;

	// D-amino acids
	n2aa[ "DAL" ] = aa_dal;
	n2aa[ "DCS" ] = aa_dcs;
	n2aa[ "DAS" ] = aa_das;
	n2aa[ "DGU" ] = aa_dgu;
	n2aa[ "DPH" ] = aa_dph;
	n2aa[ "DHI" ] = aa_dhi;
	n2aa[ "DIL" ] = aa_dil;
	n2aa[ "DLY" ] = aa_dly;
	n2aa[ "DLE" ] = aa_dle;
	n2aa[ "DME" ] = aa_dme;
	n2aa[ "DAN" ] = aa_dan;
	n2aa[ "DPR" ] = aa_dpr;
	n2aa[ "DGN" ] = aa_dgn;
	n2aa[ "DAR" ] = aa_dar;
	n2aa[ "DSE" ] = aa_dse;
	n2aa[ "DTH" ] = aa_dth;
	n2aa[ "DVA" ] = aa_dva;
	n2aa[ "DTR" ] = aa_dtr;
	n2aa[ "DTY" ] = aa_dty;

	// beta-3-L amino acids
	n2aa[ "B3A" ] = aa_b3a;
	n2aa[ "B3C" ] = aa_b3c;
	n2aa[ "B3D" ] = aa_b3d;
	n2aa[ "B3E" ] = aa_b3e;
	n2aa[ "B3F" ] = aa_b3f;
	n2aa[ "B3G" ] = aa_b3g;
	n2aa[ "B3H" ] = aa_b3h;
	n2aa[ "B3I" ] = aa_b3i;
	n2aa[ "B3K" ] = aa_b3k;
	n2aa[ "B3L" ] = aa_b3l;
	n2aa[ "B3M" ] = aa_b3m;
	n2aa[ "B3N" ] = aa_b3n;
	n2aa[ "B3P" ] = aa_b3p;
	n2aa[ "B3Q" ] = aa_b3q;
	n2aa[ "B3R" ] = aa_b3r;
	n2aa[ "B3S" ] = aa_b3s;
	n2aa[ "B3T" ] = aa_b3t;
	n2aa[ "B3V" ] = aa_b3v;
	n2aa[ "B3W" ] = aa_b3w;
	n2aa[ "B3Y" ] = aa_b3y;
	// 3 common cyclic beta-3 amino acids
	n2aa[ "CAC" ] = aa_b3cisACPC;
	n2aa[ "TAC" ] = aa_b3transACPC;
	n2aa[ "CAH" ] = aa_b3cisACHC;

	// oligoureas
	n2aa[ "OU3_ALA" ] = ou3_ala;
	n2aa[ "OU3_CYS" ] = ou3_cys;
	n2aa[ "OU3_ASP" ] = ou3_asp;
	n2aa[ "OU3_GLU" ] = ou3_glu;
	n2aa[ "OU3_PHE" ] = ou3_phe;
	n2aa[ "OU3_GLY" ] = ou3_gly;
	n2aa[ "OU3_HIS" ] = ou3_his;
	n2aa[ "OU3_ILE" ] = ou3_ile;
	n2aa[ "OU3_LYS" ] = ou3_lys;
	n2aa[ "OU3_LEU" ] = ou3_leu;
	n2aa[ "OU3_MET" ] = ou3_met;
	n2aa[ "OU3_ASN" ] = ou3_asn;
	n2aa[ "OU3_PRO" ] = ou3_pro;
	n2aa[ "OU3_GLN" ] = ou3_gln;
	n2aa[ "OU3_ARG" ] = ou3_arg;
	n2aa[ "OU3_SER" ] = ou3_ser;
	n2aa[ "OU3_THR" ] = ou3_thr;
	n2aa[ "OU3_VAL" ] = ou3_val;
	n2aa[ "OU3_TRP" ] = ou3_trp;
	n2aa[ "OU3_TYR" ] = ou3_tyr;
	n2aa[ "OU3_AIB" ] = ou3_aib;

	// Nucleic acids
	n2aa[ "ADE" ] = na_ade;
	n2aa[ "CYT" ] = na_cyt;
	n2aa[ "GUA" ] = na_gua;
	n2aa[ "THY" ] = na_thy;

	// Support for name3 too for DNA -- important
	// for some packertask/motif issues that have
	// legacy support for EITHER name3 or aa spec
	// in command line options
	n2aa[ " DA" ] = na_ade;
	n2aa[ " DC" ] = na_cyt;
	n2aa[ " DG" ] = na_gua;
	n2aa[ " DT" ] = na_thy;

	n2aa[ "RAD" ] = na_rad;
	n2aa[ "RCY" ] = na_rcy;
	n2aa[ "RGU" ] = na_rgu;
	n2aa[ "URA" ] = na_ura;

	n2aa[ "H2O" ] = aa_h2o;
	n2aa[ "HOH" ] = aa_h2o; //ok to have redundant?

	// Virtual residues
	n2aa[ "VRT" ] = aa_vrt;

	n2aa[ "UNP" ] = aa_unp;
	n2aa[ "UNK" ] = aa_unk;

	return n2aa;
}


/// @brief setup the map the converts one letter char to AA enum
std::map< char, AA > setup_oneletter2aa() {

	std::map< char, AA > l2aa;

	l2aa[ 'A' ] = aa_ala;
	l2aa[ 'C' ] = aa_cys;
	l2aa[ 'D' ] = aa_asp;
	l2aa[ 'E' ] = aa_glu;
	l2aa[ 'F' ] = aa_phe;
	l2aa[ 'G' ] = aa_gly;
	l2aa[ 'H' ] = aa_his;
	l2aa[ 'I' ] = aa_ile;
	l2aa[ 'K' ] = aa_lys;
	l2aa[ 'L' ] = aa_leu;
	l2aa[ 'M' ] = aa_met;
	l2aa[ 'N' ] = aa_asn;
	l2aa[ 'P' ] = aa_pro;
	l2aa[ 'Q' ] = aa_gln;
	l2aa[ 'R' ] = aa_arg;
	l2aa[ 'S' ] = aa_ser;
	l2aa[ 'T' ] = aa_thr;
	l2aa[ 'V' ] = aa_val;
	l2aa[ 'W' ] = aa_trp;
	l2aa[ 'Y' ] = aa_tyr;
	l2aa[ 'a' ] = na_ade;
	l2aa[ 'c' ] = na_cyt;
	l2aa[ 'g' ] = na_gua;
	l2aa[ 't' ] = na_thy;

	//rhiju RNA. Hmmm. The conflict with DNA seems like it could be a serious issue.
	l2aa[ 'a' ] = na_rad;
	l2aa[ 'c' ] = na_rcy;
	l2aa[ 'g' ] = na_rgu;
	l2aa[ 'u' ] = na_ura;

	l2aa[ 'w' ] = aa_h2o;
	l2aa[ 'z' ] = aa_unp;
	l2aa[ 'Z' ] = aa_unk;
	l2aa[ 'X' ] = aa_vrt;

	//vmullig -- The conflict for the D-amino acids and the beta-amino acids is also a problem:
	/*l2aa[ "A" ] = aa_dal;
	l2aa[ "C" ] = aa_dcs;
	l2aa[ "D" ] = aa_das;
	l2aa[ "E" ] = aa_dgu;
	l2aa[ "F" ] = aa_dph;
	l2aa[ "H" ] = aa_dhi;
	l2aa[ "I" ] = aa_dil;
	l2aa[ "K" ] = aa_dly;
	l2aa[ "L" ] = aa_dle;
	l2aa[ "M" ] = aa_dme;
	l2aa[ "N" ] = aa_dan;
	l2aa[ "P" ] = aa_dpr;
	l2aa[ "Q" ] = aa_dgn;
	l2aa[ "R" ] = aa_dar;
	l2aa[ "S" ] = aa_dse;
	l2aa[ "T" ] = aa_dth;
	l2aa[ "V" ] = aa_dva;
	l2aa[ "W" ] = aa_dtr;
	l2aa[ "Y" ] = aa_dty;*/
	return l2aa;
}


/// @brief map that converts string name to AA enum
inline
std::map< std::string, AA > const & name2aa() {
	// static initialization only happens once
	static const std::map< std::string, AA > name2aa_( setup_name2aa() );
	return name2aa_;
}


/// @brief map that converts one letter char to AA enum
inline
std::map< char, AA > const & oneletter2aa() {
	// static initialization only happens once
	static const std::map< char, AA > oneletter2aa_( setup_oneletter2aa() );
	return oneletter2aa_;
}


/// @brief setup the vector that maps AA enum to string name
utility::vector1< std::string > setup_aa2name() {

	utility::vector1< std::string > aa2n( num_aa_types );

	for ( auto const & iter : name2aa() ) {
		aa2n[ iter.second ] = iter.first;
	}

	return aa2n;
}


/// @brief vector that maps AA enum to string name
inline
utility::vector1< std::string > const & aa2name() {
	// static initialization only happens once
	static const utility::vector1< std::string > aa2name_( setup_aa2name() );
	return aa2name_;
}


/// @brief setup the vector that maps AA enum to one letter char
utility::vector1< char > setup_aa2oneletter() {

	utility::vector1< char > aa2l( num_aa_types );

	for ( auto iter : oneletter2aa() ) {
		aa2l[ iter.second ] = iter.first;
	}

	return aa2l;
}


/// @brief vector that maps AA enum to one letter char
inline
utility::vector1< char > const & aa2oneletter() {
	// static initialization only happens once
	static const utility::vector1< char > aa2oneletter_( setup_aa2oneletter() );
	return aa2oneletter_;
}


/* END: the following functions are local to this file */


AA
aa_from_name( std::string const & name )
{
	std::map< std::string, AA >::const_iterator iter = name2aa().find( name );
	if ( iter == name2aa().end() ) {
		utility_exit_with_message( "unrecognized aa type " + name );
	}
	return iter->second;
}

AA
aa_from_one_or_three( std::string const & name ){
	if ( name.size() == 1 ) {
		return aa_from_oneletter_code( name.at(0) );
	} else if ( name.size() == 3 ) {
		return aa_from_name( name );
	} else {
		utility_exit_with_message(" String does not specify a 3 or 1 letter amino acid code. :"+name+":");
	}
}



/// @brief Give an enum type, return true if and only if
/// it is a D-amino acid that is the mirror image of a
/// canonical alpha-L-amino acid.
/// @author Vikram K. Mulligan (vmullig@uw.edu)
bool
is_canonical_D_aa( AA const aa )
{
	if ( aa>=first_D_aa && aa<=last_D_aa ) return true;
	return false;
}

//////////////////////////////////////////////////////////
/// @brief Given an enum type, return true if and only if
/// it is a canonical L-amino acid. Rocco approves.
/// @author Steven Cobms (steven.combs1@gmail.com)
//////////////////////////////////////////////////////////
bool
is_canonical_L_aa_or_gly( AA const aa){
	if ( aa>=first_l_aa && aa<=num_canonical_aas ) return true;
	return false;
}

//////////////////////////////////////////////////////////
/// @brief Given an enum type, return true if and only if
/// it is a canonical beta-amino acid.
/// @author Vikram K. Mulligan (vmullig@uw.edu).
//////////////////////////////////////////////////////////
bool
is_canonical_beta3_aa( AA const aa) {
	if ( aa >= first_beta3_aa && aa <= last_beta3_aa ) return true;
	return false;
}

//////////////////////////////////////////////////////////
/// @brief Given aa three letter string, return true if
/// it is an unknown aa.
/// @author Steven Cobms (steven.combs1@gmail.com)
bool
is_aa_name_unknown( std::string const & name){
	std::map< std::string, AA >::const_iterator iter = name2aa().find( name );
	if ( iter == name2aa().end() ) {
		return true;
	}
	return false;
}


AA
get_L_equivalent( AA const aa ) {
	switch( aa ) {
	case aa_dal : return aa_ala;
	case aa_dcs : return aa_cys;
	case aa_das : return aa_asp;
	case aa_dgu : return aa_glu;
	case aa_dph : return aa_phe;
	case aa_dhi : return aa_his;
	case aa_dil : return aa_ile;
	case aa_dly : return aa_lys;
	case aa_dle : return aa_leu;
	case aa_dme : return aa_met;
	case aa_dan : return aa_asn;
	case aa_dpr : return aa_pro;
	case aa_dgn : return aa_gln;
	case aa_dar : return aa_arg;
	case aa_dse : return aa_ser;
	case aa_dth : return aa_thr;
	case aa_dva : return aa_val;
	case aa_dtr : return aa_trp;
	case aa_dty : return aa_tyr;
		// RNA
	case na_rad : return na_lra;
	case na_rcy : return na_lrc;
	case na_rgu : return na_lrg;
	case na_ura : return na_lur;

	default : return aa_unk;
	}
}

AA
get_D_equivalent( AA const aa ) {
	switch( aa ) {
	case aa_ala : return aa_dal;
	case aa_cys : return aa_dcs;
	case aa_asp : return aa_das;
	case aa_glu : return aa_dgu;
	case aa_phe : return aa_dph;
	case aa_his : return aa_dhi;
	case aa_ile : return aa_dil;
	case aa_lys : return aa_dly;
	case aa_leu : return aa_dle;
	case aa_met : return aa_dme;
	case aa_asn : return aa_dan;
	case aa_pro : return aa_dpr;
	case aa_gln : return aa_dgn;
	case aa_arg : return aa_dar;
	case aa_ser : return aa_dse;
	case aa_thr : return aa_dth;
	case aa_val : return aa_dva;
	case aa_trp : return aa_dtr;
	case aa_tyr : return aa_dty;
		// RNA
	case na_lra : return na_rad;
	case na_lrc : return na_rcy;
	case na_lrg : return na_rgu;
	case na_lur : return na_ura;

	default : return aa_unk;
	}
}

///////////////////////////////////////////////////////////////////////////////
/// @brief input operator for AA enum type
///
/// @details read in a string name from a file or std::cin and directly convert
/// it to an AA enum type, for example, std::cin >> AA. This will first check
/// if the lookup map has been set up already. If the string name cannot be
/// converted properly, it will flag the input stream as failure
/// (e.g., istream.fail() is true) and set AA enum type to aa_unk.
////////////////////////////////////////////////////////////////////////////////
std::istream &
operator >>(
	std::istream & is,
	AA & aa
)
{
	std::string name;
	is >> name;
	std::map< std::string, AA >::const_iterator iter = name2aa().find( name );
	if ( iter == name2aa().end() ) {
		//std::cout << "aaextract failed: " << name << std::endl;
		aa = aa_unk;
		is.setstate( std::ios_base::failbit );
	} else {
		//std::cout << "aaextract succeeded " << name << std::endl;
		aa = iter->second;
	}
	return is;
}


//////////////////////////////////////////////////////////////////////////////
/// @brief output operator for AA enum type
///
/// @details example usage: std::cout << aa_gly << std::endl;
//////////////////////////////////////////////////////////////////////////////
std::ostream &
operator <<(
	std::ostream & os,
	AA const & aa
)
{
	os << name_from_aa( aa );
	return os;
}


std::string
name_from_aa( AA aa ) {
	if ( aa > num_aa_types || aa == 0 ) return "AAOutOfRange";
	return aa2name()[ aa ];
}


char
oneletter_code_from_aa( AA aa ) {
	debug_assert( aa <= chemical::num_canonical_aas );
	return aa2oneletter()[ aa ];
}


AA
aa_from_oneletter_code( char onelettercode )
{
	auto item( oneletter2aa().find( onelettercode ) );
	if ( item == oneletter2aa().end() ) {
		return aa_unk; // Not ideal, but better than a random value or a crash later on.
	} else {
		return item->second;
	}
}


bool
oneletter_code_specifies_aa( char onelettercode ) {
	return oneletter2aa().find( onelettercode ) != oneletter2aa().end();
}


} // namespace chemical
} // namespace core
