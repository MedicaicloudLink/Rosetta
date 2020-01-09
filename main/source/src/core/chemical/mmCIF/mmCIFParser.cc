// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// comments in header file!
/// @file   src/core/chemical/sdf/mmCIFParser.cc
/// @author Steven Combs


#include <core/chemical/mmCIF/mmCIFParser.hh>
#include <core/types.hh>
#include <string>
#include <core/chemical/ResidueType.hh>
#include <core/chemical/ChemicalManager.hh>
#include <basic/database/open.hh>
#include <core/chemical/sdf/MolFileIOData.hh>

#include <basic/Tracer.hh>
#include <utility/string_util.hh>
#include <utility/file/FileName.hh>
#include <utility/io/izstream.hh>

//Utility functions
#include <utility/string_util.hh>


//external CIF includes
#include <cifparse/CifFile.h>
#include <cifparse/CifParserBase.h>

#include <ctime>


namespace core {
namespace chemical {
namespace mmCIF {

//Load up the tracer for this class
static basic::Tracer TR( "core.io.mmCIF.mmCIFParser" );


mmCIFParser::mmCIFParser() {
	bond_string_to_sdf_size_[ "SING"] = 1;
	bond_string_to_sdf_size_[ "DOUB"] = 2;
	bond_string_to_sdf_size_[ "TRIP"] = 3;
}

sdf::MolFileIOMoleculeOP
mmCIFParser::parse(
	std::string const & lines,
	std::string const & pdb_id
) {
	std::string diagnostics; //output from the parser about errors, etc.
	sdf::MolFileIOMoleculeOP molecule( new sdf::MolFileIOMolecule());
	CifFileOP cifFile( new CifFile);

	CifParserOP cifParser( new CifParser(cifFile.get()) );
	cifParser->ParseString( lines, diagnostics);
	if ( !diagnostics.empty() ) {
		TR.Error << diagnostics << std::endl;
	} else {
		std::vector< std::string > blocks;
		cifFile->GetBlockNames( blocks );
		//You have to add the "#" string to the pdb_id. This is because of how
		//the cifFile parser interpets the lines. For whatever reason, it adds
		//a # to the data_??? block, where ??? is the 3 letter code for the
		//file
		if ( !cifFile->IsBlockPresent( pdb_id + "#" ) ) {
			TR.Error << pdb_id << " not found in components.cif" << std::endl;
		} else {
			Block& block = cifFile->GetBlock( pdb_id + "#" );
			molecule = get_molfile_molecule( block );
			molecule->name( pdb_id );
			return molecule;
		}
	}

	return molecule;
}

utility::vector1< sdf::MolFileIOMoleculeOP>
mmCIFParser::parse(std::string const &filename){
	utility::vector1< sdf::MolFileIOMoleculeOP> molecules;
	sdf::MolFileIOMoleculeOP molecule( new sdf::MolFileIOMolecule());
	std::string diagnostics; //output from the parser about errors, etc.
	CifFileOP cifFile( new CifFile );

	CifParserOP cifParser( new CifParser(cifFile.get()) );
	cifParser->Parse( filename, diagnostics);
	//this assumes that the very first block being passed is the one being used.
	std::vector< std::string > block_names;
	cifFile->GetBlockNames( block_names );
	for ( std::string const & block_name : block_names ) {
		Block & block = cifFile->GetBlock( block_name );
		molecule = get_molfile_molecule( block );
		molecule->name( block_name );
		molecules.push_back( molecule );
	}

	return molecules;
}

sdf::MolFileIOMoleculeOP
mmCIFParser::get_molfile_molecule( Block & block ) {
	sdf::MolFileIOMoleculeOP molecule( new sdf::MolFileIOMolecule() );
	//only proceed if the tables for bonds and atoms are present
	if ( !block.IsTablePresent("chem_comp_atom") ) {
		TR.Error << "Cannot parse CIF file. No atom block (chem_comp_atom) found for " << block.GetName() << std::endl;
		return molecule;
	}


	// There's another possible issue. to pre-pick about. We absolutely NEED N,
	// because we need to be very specific about adding and deleting atoms.
	// also... residue types without N are very likely to be a poor representative
	// of "L-PEPTIDE LINKING"
	ISTable & atom_comp = block.GetTable("chem_comp_atom");

	//store the atom_id_type we will be using. atom id should be the atom name
	std::string atom_name_type( atom_comp.IsColumnPresent( "atom_id" ) ? "atom_id" : "pdbx_component_atom_id" );

	// A map of atom names to their chemical symbols
	std::map< std::string, std::string > name_to_element_map;

	bool N_found = false;
	bool P_found = false;
	bool is_peptide_linking = true;
	bool is_nucleic_linking = true;
	for ( Size ii = 0; ii < atom_comp.GetNumRows(); ++ii ) {
		//set atom name
		std::string atom_name( atom_comp( ii, atom_name_type ) );
		name_to_element_map[ atom_name ] = atom_comp( ii, "type_symbol" );

		if ( atom_name == "N" ) N_found = true;
		if ( atom_name == "P" ) P_found = true;
	}
	if ( ! N_found ) is_peptide_linking = false;
	if ( ! P_found ) is_nucleic_linking = false;

	// It's not nucleic linking if we can't establish that the phosphate P is bonded to
	// O5'. There are some 'reversed' types -- that plausibly we might choose later to
	// read in (AMW TODO) as patched base types -- like T3P that we should USUALLY treat
	// as ligands instead.
	if ( is_nucleic_linking ) {
		bool P_O5P_bond_found = false;
		if ( block.IsTablePresent( "chem_comp_bond" ) ) {
			ISTable& bond_comp = block.GetTable("chem_comp_bond");

			//start bond block
			for ( Size ii = 0; ii < bond_comp.GetNumRows(); ++ii ) {
				sdf::MolFileIOBondOP bond( new sdf::MolFileIOBond() );

				std::string source( bond_comp( ii, "atom_id_1" ) ); //atom 1
				std::string target( bond_comp( ii, "atom_id_2" ) ); //atom 2 - I guess thats self explanatory

				// Could imagine getting 'all Hs' by finding, instead, the
				// names that match H[number] -- but why not wait, for now.
				if ( ( source == "P" && target == "O5'" ) || ( source == "O5'" && target == "P" ) ) {
					P_O5P_bond_found = true;
				}
			}
		}
		if ( !P_O5P_bond_found ) {
			is_nucleic_linking = false;
		}
	}


	// It's possible (NA8 is one example) that a peptide linking residue will have an
	// atom bonded to OXT that is not named C. Catch this.
	std::string rename_to_C = "C";
	if ( is_peptide_linking ) {
		if ( block.IsTablePresent( "chem_comp_bond" ) ) {
			ISTable& bond_comp = block.GetTable("chem_comp_bond");

			//start bond block
			for ( Size ii = 0; ii < bond_comp.GetNumRows(); ++ii ) {
				sdf::MolFileIOBondOP bond( new sdf::MolFileIOBond() );

				std::string source( bond_comp( ii, "atom_id_1" ) ); //atom 1
				std::string target( bond_comp( ii, "atom_id_2" ) ); //atom 2 - I guess thats self explanatory

				// If we already have a "C", then don't rename something else to it (even if our 'C' isn't bonded to OXT)
				if ( source == "C" || target == "C" ) {
					rename_to_C = "C";
					break;
				}

				// We only want to rename carbons. Ignore any hydrogens attached, or any non-carbon atoms
				if ( source == "OXT" && name_to_element_map[ target ] == "C" ) {
					rename_to_C = target;
				} else if ( target == "OXT" && name_to_element_map[ source ] == "C" ) {
					rename_to_C = source;
				}
			}
		}
	}


	// Get the chem_comp table first, because this will help us
	// look out for extraneous atoms common in CIF entries -- extra nitrogen H
	// and OH terminus on C
	if ( block.IsTablePresent( "chem_comp" ) ) {
		ISTable & chem_comp = block.GetTable( "chem_comp" );
		std::string type( chem_comp( 0, "type" ) );
		if ( type == "L-PEPTIDE LINKING" && is_peptide_linking ) {
			TR.Debug << "Found L-peptide RT" << std::endl;// named " << molecule->name() << std::endl;
			molecule->add_str_str_data( "Rosetta Properties", "PROTEIN POLYMER L_AA" );
			is_peptide_linking = true;
			is_nucleic_linking = false;
		} else if ( type == "D-PEPTIDE LINKING" && is_peptide_linking ) {
			TR.Debug << "Found D-peptide RT" << std::endl;//named " << molecule->name() << std::endl;
			molecule->add_str_str_data( "Rosetta Properties", "PROTEIN POLYMER D_AA" );
			is_peptide_linking = true;
			is_nucleic_linking = false;
		} else if ( type == "RNA LINKING" && is_nucleic_linking ) {
			TR.Debug << "Found D-RNA RT" << std::endl;//named " << molecule->name() << std::endl;
			molecule->add_str_str_data( "Rosetta Properties", "RNA POLYMER D_RNA" );
			is_peptide_linking = false;
			is_nucleic_linking = true;
		} else if ( type == "L-RNA LINKING" && is_nucleic_linking ) {
			TR.Debug << "Found L-RNA RT" << std::endl;//named " << molecule->name() << std::endl;
			molecule->add_str_str_data( "Rosetta Properties", "RNA POLYMER L_RNA" );
			is_peptide_linking = false;
			is_nucleic_linking = true;
		}  else if ( type == "DNA LINKING" && is_nucleic_linking ) {
			TR.Debug << "Found D-DNA RT" << std::endl;//named " << molecule->name() << std::endl;
			molecule->add_str_str_data( "Rosetta Properties", "DNA POLYMER" );
			is_peptide_linking = false;
			is_nucleic_linking = true;
		} else if ( type == "L-DNA LINKING" && is_nucleic_linking ) {
			TR.Debug << "Found L-DNA RT" << std::endl;//named " << molecule->name() << std::endl;
			molecule->add_str_str_data( "Rosetta Properties", "DNA POLYMER" );
			is_peptide_linking = false;
			is_nucleic_linking = true;
		} else {
			is_peptide_linking = false;
			is_nucleic_linking = false;
		}
	}

	// Sometimes OP3/O3P is used for non-term-deletable phosphate oxygens. (THX)
	//bool interesting_upper_behavior = false;

	utility::vector1< std::string > O3P_connected;

	// Before we actually LOOK at the atoms (or bonds) for real, we need to know
	// what atoms are bonded to the polymeric termini or to to-be-deleted
	// atoms -- so we can ignore them too as appropriate
	utility::vector1< std::string > possible_atoms_to_skip;
	if ( is_peptide_linking ) {
		if ( block.IsTablePresent( "chem_comp_bond" ) ) {
			ISTable& bond_comp = block.GetTable("chem_comp_bond");

			//start bond block
			for ( Size ii = 0; ii < bond_comp.GetNumRows(); ++ii ) {
				sdf::MolFileIOBondOP bond( new sdf::MolFileIOBond() );

				std::string source( bond_comp( ii, "atom_id_1" ) ); //atom 1
				std::string target( bond_comp( ii, "atom_id_2" ) ); //atom 2 - I guess thats self explanatory

				if ( source == rename_to_C ) source = "C";
				if ( target == rename_to_C ) target = "C";

				// Could imagine getting 'all Hs' by finding, instead, the
				// names that match H[number] -- but why not wait, for now.
				if ( source == "OXT" ) {
					TR.Trace << "It may be appropriate to skip the maybe-hydrogen " << target << " due to its bond to OXT " << std::endl;
					possible_atoms_to_skip.push_back( target );
				}
				if ( target == "OXT" ) {
					TR.Trace << "It may be appropriate to skip the maybe-hydrogen " << source << " due to its bond to OXT " << std::endl;
					possible_atoms_to_skip.push_back( source );
				}
				if ( source == "N" ) {
					TR.Trace << "It may be appropriate to skip the maybe-hydrogen " << target << " due to its bond to N " << std::endl;
					possible_atoms_to_skip.push_back( target );
				}
				if ( target == "N" ) {
					TR.Trace << "It may be appropriate to skip the maybe-hydrogen " << source << " due to its bond to N " << std::endl;
					possible_atoms_to_skip.push_back( source );
				}
			}
		}
	} else if ( is_nucleic_linking ) {

		if ( block.IsTablePresent( "chem_comp_bond" ) ) {
			ISTable& bond_comp = block.GetTable("chem_comp_bond");

			//start bond block
			for ( Size ii = 0; ii < bond_comp.GetNumRows(); ++ii ) {
				sdf::MolFileIOBondOP bond( new sdf::MolFileIOBond() );

				std::string const & source( bond_comp( ii, "atom_id_1" ) ); //atom 1
				std::string const & target( bond_comp( ii, "atom_id_2" ) ); //atom 2 - I guess thats self explanatory

				if ( source == "P" ) {
					TR.Trace << "It may be appropriate to skip the maybe-hydrogen " << target << " due to its bond to  P" << std::endl;
					possible_atoms_to_skip.push_back( target );
				}
				if ( target == "P" ) {
					TR.Trace << "It may be appropriate to skip the maybe-hydrogen " << source << " due to its bond to  P" << std::endl;
					possible_atoms_to_skip.push_back( source );
				}

				if ( source == "O3'" ) {
					TR.Trace << "It may be appropriate to skip the maybe-hydrogen " << target << " due to its bond to  O3'" << std::endl;
					possible_atoms_to_skip.push_back( target );
				}
				if ( target == "O3'" ) {
					TR.Trace << "It may be appropriate to skip the maybe-hydrogen " << source << " due to its bond to  O3'" << std::endl;
					possible_atoms_to_skip.push_back( source );
				}

				if ( source == "OP3" ) {
					TR.Trace << "It may be appropriate to skip the maybe-hydrogen " << target << " due to its bond to  OP3" << std::endl;
					possible_atoms_to_skip.push_back( target );
					O3P_connected.push_back( target );
				}
				if ( target == "OP3" ) {
					TR.Trace << "It may be appropriate to skip the maybe-hydrogen " << source << " due to its bond to  OP3" << std::endl;
					possible_atoms_to_skip.push_back( source );
					O3P_connected.push_back( source );
				}
				if ( source == "O3P" ) {
					TR.Trace << "It may be appropriate to skip the maybe-hydrogen " << target << " due to its bond to  O3P" << std::endl;
					possible_atoms_to_skip.push_back( target );
					O3P_connected.push_back( target );
				}
				if ( target == "O3P" ) {
					TR.Trace << "It may be appropriate to skip the maybe-hydrogen " << source << " due to its bond to  O3P" << std::endl;
					possible_atoms_to_skip.push_back( source );
					O3P_connected.push_back( source );
				}
				if ( source == "OP2" ) {
					TR.Trace << "It may be appropriate to skip the maybe-hydrogen " << target << " due to its bond to  OP2" << std::endl;
					possible_atoms_to_skip.push_back( target );
				}
				if ( target == "OP2" ) {
					TR.Trace << "It may be appropriate to skip the maybe-hydrogen " << source << " due to its bond to  OP2" << std::endl;
					possible_atoms_to_skip.push_back( source );
				}
				if ( target == "O2P" ) {
					TR.Trace << "It may be appropriate to skip the maybe-hydrogen " << source << " due to its bond to  O2P" << std::endl;
					possible_atoms_to_skip.push_back( source );
				}
				if ( source == "O2P" ) {
					TR.Trace << "It may be appropriate to skip the maybe-hydrogen " << target << " due to its bond to  O2P" << std::endl;
					possible_atoms_to_skip.push_back( target );
				}
				if ( source == "OP1" ) {
					TR.Trace << "It may be appropriate to skip the maybe-hydrogen " << target << " due to its bond to  OP1" << std::endl;
					possible_atoms_to_skip.push_back( target );
				}
				if ( target == "OP1" ) {
					TR.Trace << "It may be appropriate to skip the maybe-hydrogen " << source << " due to its bond to  OP1" << std::endl;
					possible_atoms_to_skip.push_back( source );
				}
				if ( source == "O1P" ) {
					TR.Trace << "It may be appropriate to skip the maybe-hydrogen " << target << " due to its bond to  O1P" << std::endl;
					possible_atoms_to_skip.push_back( target );
				}
				if ( target == "O1P" ) {
					TR.Trace << "It may be appropriate to skip the maybe-hydrogen " << source << " due to its bond to  O1P" << std::endl;
					possible_atoms_to_skip.push_back( source );
				}
			}
		}
	}

	//get the atom and bond composition table
	//ISTable & atom_comp = block.GetTable("chem_comp_atom");

	//this is to map the atom names to a core::Size value (id). This is added when adding
	//bonds to the MolIO object.
	std::map< std::string, core::Size > atom_name_to_id;

	//prefer the ideal coordinates, but if not found, use cartesian coordinates
	std::string xyz_start_type, xyz_end_type;
	if ( atom_comp.IsColumnPresent( "pdbx_model_Cartn_x_ideal") ) {
		xyz_start_type = "pdbx_model_Cartn_x_ideal";
		xyz_end_type = "pdbx_model_Cartn_z_ideal";
		if ( atom_comp( 0, xyz_start_type ) == "?" ) {
			// for some entries they're present but undefined - try the model coordinates instead
			xyz_start_type = "model_Cartn_x";
			xyz_end_type = "model_Cartn_z";
		}
	} else {
		xyz_start_type = "model_Cartn_x";
		xyz_end_type = "model_Cartn_z";
	}
	if ( atom_comp( 0, xyz_end_type ) == "?" ) {
		utility_exit_with_message( "No usable coordinates for mmCIF file for " + block.GetName() );
	}

	// Loop over atom block and check to see if any heavyatoms are bound to OP3/O3P
	bool interesting_pendant = false;
	for ( Size ii = 1; ii < atom_comp.GetNumRows(); ++ii ) {
		std::string const & atom_name = atom_comp( ii, atom_name_type );
		std::string const & element = atom_comp( ii, "type_symbol" );
		if ( O3P_connected.contains( atom_name ) ) {
			if ( element != "H" && atom_name != "P" ) {
				TR.Trace << "There is an OP3-bonded heavyatom: " << atom_name << std::endl;
				interesting_pendant = true;
				break;
			}
		}
	}

	utility::vector1< std::string > actual_atoms_to_skip;
	//start atom block
	Size index = 1;
	TR.Trace << "possible_atoms_to_skip: " << possible_atoms_to_skip << std::endl;
	for ( Size ii = 0; ii < atom_comp.GetNumRows(); ++ii ) {
		sdf::MolFileIOAtomOP atom( new sdf::MolFileIOAtom());
		//atom id is whatever the number we are on +1, because we dont do 0 based index
		// AMW: meaning... that we don't need the below, nor would we actually want it
		// because needing to skip (due to non-patched polymers) screws it up.
		// ditto our index, ii... so use another.
		//utility::string2int( atom_comp( ii, "pdbx_ordinal" ) ) );
		atom->index( index );

		//set atom name
		std::string atom_name = atom_comp( ii, atom_name_type );
		TR.Trace << "Examining atom entry " << atom_name << std::endl;
		if ( is_peptide_linking && atom_name == "OXT" ) continue;
		if ( is_nucleic_linking && !interesting_pendant && atom_name == "OP3" ) continue;
		if ( is_nucleic_linking && !interesting_pendant && atom_name == "O3P" ) continue;

		if ( atom_name == rename_to_C ) atom_name = "C";

		atom->name( atom_name );

		//set map to index
		atom_name_to_id[ atom_name ] = index;
		//set element name
		atom->element( atom_comp( ii, "type_symbol" ) );

		TR.Trace << "Type symbol for atom  " << atom_name << " is " <<  atom_comp( ii, "type_symbol" )  << std::endl;
		if ( possible_atoms_to_skip.contains( atom_name ) && atom->element() == "H" ) {
			actual_atoms_to_skip.push_back( atom_name );
			continue;
		}

		TR.Trace << "Keeping atom entry " << atom_name << std::endl;


		//get the xyz cordinates
		std::vector< std::string > atom_coords;
		atom_comp.GetRow( atom_coords, ii, xyz_start_type, xyz_end_type );
		core::Real x = utility::string2float( atom_coords[ 0 ] );
		core::Real y = utility::string2float( atom_coords[ 1 ] );
		core::Real z = utility::string2float( atom_coords[ 2 ] );
		//set xyz coordinates
		atom->position( core::Vector( x, y, z ) );

		std::string charge( atom_comp(ii, "charge"));
		if ( charge == "?" ) {
			atom->formal_charge( 0 );
		} else {
			atom->formal_charge( utility::string2int( charge ) );
		}

		molecule->add_atom( atom );
		// only increment if we actually get here.
		index++;
	}

	TR.Trace << "actual_atoms_to_skip: " << actual_atoms_to_skip << std::endl;

	// pdb_BVP messes with this logic because it names H3' "H1" and HO3' H3'.
	// So we need to place an additional requirement for H skipping.
	// This is actually a very hard problem.


	if ( block.IsTablePresent( "chem_comp_bond" ) ) {
		ISTable& bond_comp = block.GetTable("chem_comp_bond");

		//start bond block
		for ( Size ii = 0; ii < bond_comp.GetNumRows(); ++ii ) {
			sdf::MolFileIOBondOP bond( new sdf::MolFileIOBond() );

			//std::string const & source( bond_comp( ii, "atom_id_1" ) ); //atom 1
			//std::string const & target( bond_comp( ii, "atom_id_2" ) ); //atom 2 - I guess thats self explanatory
			std::string source( bond_comp( ii, "atom_id_1" ) ); //atom 1
			std::string target( bond_comp( ii, "atom_id_2" ) ); //atom 2 - I guess thats self explanatory

			if ( source == rename_to_C ) source = "C";
			if ( target == rename_to_C ) target = "C";

			TR.Trace << "Examining bond entry " << source << " " << target << std::endl;

			if ( is_peptide_linking && source == "OXT" ) continue;
			if ( is_peptide_linking && target == "OXT" ) continue;
			if ( is_nucleic_linking && !interesting_pendant && ( source == "OP3" || source == "O3P" ) ) continue;
			if ( is_nucleic_linking && !interesting_pendant && ( target == "OP3" || target == "O3P" ) ) continue;
			if ( actual_atoms_to_skip.contains( source ) ) continue;
			if ( actual_atoms_to_skip.contains( target ) ) continue;

			TR.Trace << "Keeping bond entry " << source << " " << target << std::endl;

			bond->atom1( atom_name_to_id[ source ] );
			bond->atom2( atom_name_to_id[ target ] );

			core::Size bond_type( bond_string_to_sdf_size_[ bond_comp( ii, "value_order" ) ] ) ;
			bond->sdf_type( bond_type); //bond order

			molecule->add_bond( bond);
		}
	} else if ( atom_comp.GetNumRows() > 1 ) {
		TR.Error << "Cannot parse CIF file. No bond block (chem_comp_bond) found for multi-atom entry " << block.GetName() << std::endl;
	} // else one atom entry without bond block

	// Note, for polymer generalization we may want to find the block here that
	// describes polymeric connections, polymer type, et cetera -- and use it
	// to add UPPER and LOWER entries. Otherwise, we may need/want to do so
	// using molfile comments records... let's see, when we look at the parser,
	// how possible that will be.

	//ICOOR_INTERNAL    H   -180.000000   60.849998    1.010000   N     CA  LOWER
	// Since we deleted ALL hydrogens attached to N, we should add back in an ideal
	// one. We can't add its coordinates yet, though, since only the residue type
	// knows where LOWER is. Hey, actually, LOWER needs to be assigned in the
	// list too!
	if ( is_peptide_linking ) {
		sdf::MolFileIOAtomOP H_atom( new sdf::MolFileIOAtom());
		//atom id is whatever the number we are on +1, because we dont do 0 based index
		H_atom->index( index );
		H_atom->name( "H" );
		atom_name_to_id[ "H" ] = index;
		H_atom->element( "H" );
		// faked xyz cordinates
		H_atom->position( core::Vector( 0, 0, 0 ) );
		H_atom->formal_charge( 0 );
		molecule->add_atom( H_atom );

		sdf::MolFileIOBondOP H_bond( new sdf::MolFileIOBond() );
		H_bond->atom1( atom_name_to_id[ "H" ] );
		H_bond->atom2( atom_name_to_id[ "N" ] );
		H_bond->sdf_type( 1 ); //bond order
		molecule->add_bond( H_bond );
	}

	return molecule;
}


}
}
}
