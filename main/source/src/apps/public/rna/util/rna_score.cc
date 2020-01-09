// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file
/// @brief


// libRosetta headers
#include <core/types.hh>
#include <core/chemical/ChemicalManager.hh>
#include <core/io/silent/BinarySilentStruct.hh>
#include <core/io/silent/SilentFileOptions.hh>
#include <core/io/silent/util.hh>
#include <core/scoring/ScoreFunction.hh>
#include <core/scoring/ScoreFunctionFactory.hh>
#include <core/scoring/rms_util.hh>
#include <core/scoring/rna/RNA_ScoringInfo.hh>
#include <core/scoring/rna/data/RNA_ChemicalMappingEnergy.hh>
#include <core/pose/rna/RNA_DataInfo.hh>
#include <basic/options/option.hh>
#include <basic/options/option_macros.hh>
#include <basic/database/open.hh>
#include <protocols/viewer/viewers.hh>
#include <core/pose/Pose.hh>
#include <core/pose/rna/util.hh>
#include <core/pose/full_model_info/FullModelInfo.hh>
#include <core/pose/datacache/CacheableDataType.hh>
#include <basic/datacache/BasicDataCache.hh>
#include <core/pose/extra_pose_info_util.hh>
#include <devel/init.hh>
#include <core/import_pose/import_pose.hh>
#include <core/import_pose/FullModelPoseBuilder.hh>
#include <core/import_pose/pose_stream/PoseInputStream.hh>
#include <core/import_pose/pose_stream/PDBPoseInputStream.hh>
#include <core/import_pose/pose_stream/SilentFilePoseInputStream.hh>
#include <utility/vector1.hh>
#include <ObjexxFCL/string.functions.hh>
#include <protocols/stepwise/modeler/util.hh>
#include <protocols/stepwise/modeler/rna/util.hh>
#include <protocols/stepwise/modeler/align/util.hh>
#include <protocols/rna/denovo/RNA_DeNovoPoseInitializer.hh>
#include <core/io/rna/RNA_DataReader.hh>
#include <core/pose/PDBInfo.hh>

#include <core/scoring/ScoreType.hh>
#include <core/scoring/etable/EtableEnergy.hh>
#include <core/scoring/etable/EtableEnergyCreator.hh>
#include <core/scoring/ScoringManager.hh>
#include <core/scoring/Energies.hh>
#include <core/scoring/etable/BaseEtableEnergy.hh>
#include <core/scoring/etable/BaseEtableEnergy.tmpl.hh>
#include <core/scoring/etable/count_pair/CountPairFunction.hh>
#include <core/scoring/etable/count_pair/CountPairGeneric.hh>
#include <core/scoring/etable/count_pair/CountPairFactory.hh>
#include <core/scoring/etable/atom_pair_energy_inline.hh>
#include <core/util/SwitchResidueTypeSet.hh>

#include <protocols/moves/Mover.hh>

// C++ headers
#include <iostream>
#include <string>

#include <numeric/conversions.hh>

// option key includes
#include <basic/Tracer.hh>
#include <basic/options/option.hh>
#include <basic/options/keys/in.OptionKeys.gen.hh>
#include <basic/options/keys/chemical.OptionKeys.gen.hh>
#include <basic/options/keys/full_model.OptionKeys.gen.hh>
#include <basic/options/keys/out.OptionKeys.gen.hh>
#include <basic/options/keys/rna.OptionKeys.gen.hh>
#include <basic/options/keys/score.OptionKeys.gen.hh>
#include <basic/options/keys/stepwise.OptionKeys.gen.hh>

#include <protocols/jd2/util.hh>
#include <protocols/jd2/internal_util.hh>
#include <protocols/jd2/JobDistributor.hh>

#include <utility/excn/Exceptions.hh>

static basic::Tracer TR( "apps.public.rna.util.rna_score" );


using namespace core;
using namespace protocols;
using namespace basic::options::OptionKeys;
using utility::vector1;

OPT_KEY( String,  params_file )
OPT_KEY( StringVector, original_input )
OPT_KEY( Boolean, virtualize_free )
OPT_KEY( Boolean, color_by_score )
OPT_KEY( Boolean, soft_rep )
OPT_KEY( Boolean, rmsd_nosuper )
OPT_KEY( Boolean, convert_protein_centroid )
OPT_KEY( Boolean, dihedral_rms  )
OPT_KEY( Boolean, gdt_ha )
OPT_KEY( IntegerVector, rmsd_residues )

// Move this out to protocols/toolbox/ before checkin.
//
// Could pretty easily get this to read in an actual scorefunction, rather than some
// hard wired combination of fa_atr/fa_rep. Problem is that most scorefunctions
// do not provide functions that compute energies at atom level. Even ones that do
// (geometric_solvation, etable energies, hbond) do not have functions with stereotyped
// input/output. Can fill in by hand, one by one.
//
void
do_color_by_score( core::pose::Pose & pose ) {

	using namespace core::scoring;
	using namespace core::scoring::methods;
	using namespace core::scoring::etable;
	using namespace core::scoring::etable::count_pair;
	using namespace core::conformation;
	using namespace core::id;

	ScoreFunction scorefxn;
	EnergyMethodOptions options( scorefxn.energy_method_options() );
	options.etable_options().no_lk_polar_desolvation = true;
	if ( basic::options::option[ soft_rep ]() ) options.etable_type( "FA_STANDARD_SOFT" );
	scorefxn.set_energy_method_options( options );
	scorefxn.set_weight( fa_atr, 0.21 );
	scorefxn.set_weight( fa_rep, 0.20 );
	scorefxn.set_weight( fa_sol, 0.25 );
	//scorefxn.set_weight( fa_rep, 1.0);

	core::scoring::etable::EtableCOP etable(core::scoring::ScoringManager::get_instance()->etable( options ) );
	core::scoring::etable::AnalyticEtableEvaluator eval( *etable );
	eval.set_scoretypes( fa_atr, fa_rep, fa_sol );

	EnergyMap emap, emap_total;

	//////////////////////////////////////////////////////////////////
	// stolen from core/scoring/etable/atom_pair_energy_inline.hh
	//////////////////////////////////////////////////////////////////
	DistanceSquared dsq;
	Real weight;
	Size path_dist;
	using vect = const utility::vector1<Size> &;

	for ( Size m = 1; m <= pose.size(); m++ ) {

		Residue const & res1 = pose.residue( m );

		// get hydrogen interaction cutoff
		Real const Hydrogen_interaction_cutoff2
			( eval.hydrogen_interaction_cutoff2() );

		for ( Size i = 1; i <= res1.natoms(); i++ ) {
			pose.pdb_info()->temperature( m, i, 0.0 );
		}

		Size const res1_start( 1 ), res1_end( res1.nheavyatoms() );
		vect r1hbegin( res1.attached_H_begin() );
		vect r1hend(   res1.attached_H_end()   );

		// Atom pairs
		for ( int i = res1_start, i_end = res1_end; i <= i_end; ++i ) {
			Atom const & atom1( res1.atom(i) );
			//get virtual information
			bool atom1_virtual(res1.atom_type(i).is_virtual());

			emap.zero();

			for ( Size n = 1; n <= pose.size(); n++ ) {

				if ( m == n ) continue; // later could be smart about holding info in fa_intra terms of emap
				Residue const & res2 = pose.residue( n );

				// costly, but I'm having problems with CountPairFactory output.
				// TR << m << " " << n << std::endl;
				CountPairGeneric count_pair( res1, res2 );
				count_pair.set_crossover( 4 );
				Size const res2_start( 1 ), res2_end( res2.nheavyatoms() );
				vect r2hbegin( res2.attached_H_begin() );
				vect r2hend(   res2.attached_H_end()   );

				for ( int j=res2_start, j_end = res2_end; j <= j_end; ++j ) {

					Atom const & atom2( res2.atom(j) );

					//     if ( ! count_pair( i, j, weight, path_dist ) ) continue;
					//     eval.atom_pair_energy( atom1, atom2, weight, emap, dsq );

					//     if ( dsq < 16.0 ) TR << dsq << " " << weight << " " << emap[ fa_atr ] << std::endl;
					//check if virtual
					bool atom2_virtual(res2.atom_type(j).is_virtual());
					weight = 1.0;
					path_dist = 0;
					if ( atom1_virtual || atom2_virtual ) {
						// NOOP! etable_energy.virtual_atom_pair_energy(emap);
					} else {
						if ( count_pair( i, j, weight, path_dist ) ) {
							eval.atom_pair_energy( atom1, atom2, weight, emap, dsq );
						} else {
							dsq = atom1.xyz().distance_squared( atom2.xyz() );
						}
						if ( dsq < Hydrogen_interaction_cutoff2 ) {
							residue_fast_pair_energy_attached_H(
								res1, i, res2, j,
								r1hbegin[ i ], r1hend[ i ],
								r2hbegin[ j ], r2hend[ j ],
								count_pair, eval , emap);

						}
					} // virtual
				} // j
			} // n

			emap *= 0.5; /* double counting */
			emap_total += emap;

			Real score = emap.dot(  scorefxn.weights() );
			pose.pdb_info()->temperature( m, i, score );


		} // i
	} // m


	( scorefxn )( pose );
	for ( Size n = 1; n <= n_score_types; n++ ) {
		auto st( static_cast< ScoreType >( n ) );
		if ( scorefxn.has_nonzero_weight( st ) ) {
			TR << st << ":   conventional score function " << pose.energies().total_energies()[ st ] << "   atomwise " << emap_total[ st ] << std::endl;
		}
	}

}

Real
calc_dihedral_rms( core::pose::Pose const & pose, core::pose::Pose const & native_pose, bool verbose = false ) {
	// Go residue by residue. These need to be the same length.

	if ( pose.size() != native_pose.size() ) {
		utility_exit_with_message( "Pose and native are different lengths. If you want, pass -dihedral_rms false" );
	}

	Real accumulator = 0;
	Size count = 0;
	for ( Size ii = 1; ii <= pose.size(); ++ii ) {
		if ( pose.residue_type( ii ).is_virtual_residue() ) continue;

		Size n_mainchain = pose.residue( ii ).n_mainchain_atoms();

		if ( pose.residue( ii ).has_lower_connect() && native_pose.residue( ii ).has_lower_connect() ) {
			if ( pose.residue( ii ).mainchain_torsion( 1 ) != native_pose.residue( ii ).mainchain_torsion( 1 ) ) {
				accumulator += 1 - cos( numeric::conversions::radians< Real >( pose.residue( ii ).mainchain_torsion( 1 ) - native_pose.residue( ii ).mainchain_torsion( 1 ) ) );
				//accumulator += ( pose.residue( ii ).mainchain_torsion( 1 ) - native_pose.residue( ii ).mainchain_torsion( 1 ) )
				// * ( pose.residue( ii ).mainchain_torsion( 1 ) - native_pose.residue( ii ).mainchain_torsion( 1 ) );
			}
			if ( verbose ) {
				TR << "pose " << pose.residue( ii ).mainchain_torsion( 1 ) << " native " << native_pose.residue( ii ).mainchain_torsion( 1 ) << std::endl;
			}
			count++;
		}

		for ( Size jj = 2; jj <= n_mainchain - 1; ++jj ) {
			if ( pose.residue( ii ).mainchain_torsion( jj ) != native_pose.residue( ii ).mainchain_torsion( jj ) ) {
				accumulator += 1 - cos( numeric::conversions::radians< Real >( pose.residue( ii ).mainchain_torsion( jj ) - native_pose.residue( ii ).mainchain_torsion( jj ) ) );
				//accumulator += ( pose.residue( ii ).mainchain_torsion( jj ) - native_pose.residue( jj ).mainchain_torsion( 1 ) )
				// * ( pose.residue( ii ).mainchain_torsion( jj ) - native_pose.residue( jj ).mainchain_torsion( 1 ) );
			}
			if ( verbose ) {
				TR << "pose " << pose.residue( ii ).mainchain_torsion( jj ) << " native " << native_pose.residue( ii ).mainchain_torsion( jj ) << std::endl;
			}
			count++;
		}

		if ( pose.residue( ii ).has_upper_connect() && native_pose.residue( ii ).has_upper_connect() ) {
			if ( pose.residue( ii ).mainchain_torsion( n_mainchain ) != native_pose.residue( ii ).mainchain_torsion( n_mainchain ) ) {
				accumulator += 1 - cos( numeric::conversions::radians< Real >( pose.residue( ii ).mainchain_torsion( n_mainchain ) - native_pose.residue( ii ).mainchain_torsion( n_mainchain ) ) );
				//accumulator += ( pose.residue( ii ).mainchain_torsion( n_mainchain ) - native_pose.residue( ii ).mainchain_torsion( n_mainchain ) )
				// * ( pose.residue( ii ).mainchain_torsion( n_mainchain ) - native_pose.residue( ii ).mainchain_torsion( n_mainchain ) );
			}
			if ( verbose ) {
				TR << "pose " << pose.residue( ii ).mainchain_torsion( n_mainchain ) << " native " << native_pose.residue( ii ).mainchain_torsion( n_mainchain ) << std::endl;
			}
			count++;
		}
	}
	accumulator /= count;
	accumulator = std::sqrt( accumulator );

	return accumulator;
}

///////////////////////////////////////////////////////////////////////////////


using namespace  protocols::moves;

class RNA_ScoreMover : public Mover {
public:
	RNA_ScoreMover( core::scoring::ScoreFunctionOP const & sfxn,
		core::pose::PoseOP const & native_pose,
		core::chemical::ResidueTypeSetCOP const & rsd_set//,
		//utility::vector1< pose::PoseOP > const & other_poses = utility::vector1< pose::PoseOP >()
	);//,
	//core::pose::full_model_info::FullModelInfoOP const & my_model
	// );

	void apply( core::pose::Pose& pose ) override;
	std::string get_name() const override { return "RNA_ScoreMover"; }

	MoverOP clone() const override {
		return MoverOP( new RNA_ScoreMover( *this ) );
	}

	MoverOP fresh_instance() const override {
		return MoverOP( new RNA_ScoreMover( nullptr, nullptr, nullptr ) );
	}

private:
	core::scoring::ScoreFunctionOP sfxn_;
	core::pose::PoseOP native_pose_;
	core::pose::full_model_info::FullModelInfoOP my_model_;
	core::chemical::ResidueTypeSetCOP rsd_set_;
	utility::vector1< pose::PoseOP > other_poses_;
	core::scoring::rna::data::RNA_ChemicalMappingEnergyOP rna_chemical_mapping_energy_ = nullptr;
	core::io::rna::RNA_DataReaderOP rna_data_reader_ = nullptr;
};

RNA_ScoreMover::RNA_ScoreMover( core::scoring::ScoreFunctionOP const & sfxn,
	core::pose::PoseOP const & native_pose,//,
	core::chemical::ResidueTypeSetCOP const & rsd_set
	//utility::vector1< pose::PoseOP > const & other_poses
) :
	sfxn_( sfxn ),
	native_pose_( native_pose ),//,
	//other_poses_( other_poses )//,
	//my_model_( my_model ),
	rsd_set_( rsd_set )
{
	using namespace basic::options;
	using namespace basic::options::OptionKeys;
	using namespace core;

	// get scorefxn and add constraints if defined
	sfxn_ = core::scoring::get_score_function();
	//rsd_set_ = core::chemical::ChemicalManager::get_instance()->residue_type_set( core::chemical::FA_STANDARD /*RNA*/ );

	utility::vector1< pose::PoseOP > other_poses;

	if ( option[ original_input ].user() ) {
		utility::vector1< std::string > const & original_files = option[ original_input ]();
		utility::vector1< pose::PoseOP > original_poses;

		for ( Size n = 1; n <= original_files.size(); n++ ) {
			original_poses.push_back( core::import_pose::get_pdb_and_cleanup( original_files[ n ], rsd_set_ ) );
		}
		if ( option[ full_model::other_poses ].user() ) core::import_pose::get_other_poses( original_poses, option[ full_model::other_poses ](), rsd_set_ );

		//FullModelInfo (minimal object needed for add/delete)
		core::import_pose::FullModelPoseBuilder builder;
		builder.set_input_poses( original_poses );
		builder.set_options( option );
		builder.initialize_further_from_options();
		builder.build(); // hope this will update original_poses[ 1 ]
		//fill_full_model_info_from_command_line( original_poses );
		my_model_ = core::pose::full_model_info::const_full_model_info( *original_poses[ 1 ] ).clone_info();

	} else {
		// other poses -- for scoring collections of poses connected by (virtual) loops, using full_model_info.
		if ( option[ full_model::other_poses ].user() ) core::import_pose::get_other_poses( other_poses_, option[ full_model::other_poses ](), rsd_set_ );
	}

	if ( native_pose_ ) ( *sfxn_ )( *native_pose_ );


	rna_data_reader_ = utility::pointer::make_shared< core::io::rna::RNA_DataReader>( option[OptionKeys::rna::data_file ]() );
}

void RNA_ScoreMover::apply( core::pose::Pose & pose ) {

	using namespace basic::options;
	using namespace basic::options::OptionKeys;
	using namespace core::chemical;
	using namespace core::scoring::rna::data;
	using namespace core::scoring;
	using namespace core::kinematics;
	using namespace core::io::silent;
	using namespace core::import_pose::pose_stream;
	using namespace core::import_pose;
	using namespace core::pose::full_model_info;
	using namespace protocols::stepwise::modeler;

	if ( option[ convert_protein_centroid ]() ) {
		core::util::switch_to_residue_type_set( pose, core::chemical::CENTROID, false /* no sloppy match */, true /* only switch protein residues */ );
	}

	if ( option[ virtualize_free ]() ) core::pose::rna::virtualize_free_rna_moieties( pose ); // purely for testing.

	if ( !option[ in::file::silent ].user() ) {
		// If we didn't input this pose from a silent file, some work probably needs to be done on it.
		cleanup( pose );
	}

	if ( !full_model_info_defined( pose ) || option[ in::file::fasta ].user() ) {
		if ( ! option[ original_input ].user() ) {
			FullModelPoseBuilder builder;
			utility::vector1< pose::PoseOP > input_poses;
			input_poses.push_back( utility::pointer::make_shared< Pose >( pose ) );
			builder.set_input_poses( input_poses );
			builder.set_options( option );
			builder.initialize_further_from_options();
			builder.build(); // hope this will update original_poses[ 1 ]
			pose = *input_poses[ 1 ];
			//fill_full_model_info_from_command_line( pose, other_poses ); // only does something if -in:file:fasta specified.
		} else {
			// allows for scoring of PDB along with 'other_poses' supplied from command line. Was used to test loop_close score term.
			utility::vector1< Size > resnum;
			core::pose::PDBInfoCOP pdb_info = pose.pdb_info();
			if ( pdb_info ) {
				//TR << std::endl << "PDB Info available for this pose..." << std::endl << std::endl;
				for ( Size n = 1; n <= pose.size(); n++ ) resnum.push_back( pdb_info->number( n ) );
			} else {
				for ( Size n = 1; n <= pose.size(); n++ ) resnum.push_back( n );
			}
			my_model_->set_res_list( resnum );
			my_model_->set_other_pose_list( other_poses_ );
			set_full_model_info( pose, my_model_ );
		}
	}


	if ( option[params_file].user() ) utility_exit_with_message( " -params_file not supported in rna_minimize anymore." );
	// if ( option[params_file].user() ) {
	//  parameters.initialize_for_de_novo_protocol(pose, option[params_file],
	//   basic::database::full_name("sampling/rna/1jj2_RNA_jump_library.dat"),
	//   false /*ignore_secstruct*/ );
	//  parameters.setup_base_pair_constraints( pose );
	// }

	// do it
	if ( ! option[ score::just_calc_rmsd]() && !rna_data_reader_->has_reactivities() ) {
		(*sfxn_)( pose );
	}

	// tag
	std::string tag = tag_from_pose( pose );
	//BinarySilentStruct s( opts, pose, tag );

	std::map< Size, Size > resmap;
	for ( Size ii = 1; ii <= pose.size(); ++ii ) resmap[ ii ] = ii;

	if ( native_pose_ ) {
		Real rmsd;
		if ( option[ rmsd_nosuper ]() ) {
			if ( option[ rmsd_residues ].user() ) {
				rmsd = protocols::stepwise::modeler::align::get_rmsd( pose, *native_pose_, option[ rmsd_residues ]() );
			} else {
				rmsd = all_atom_rmsd_nosuper( *native_pose_, pose );
			}
		} else {
			rmsd = protocols::stepwise::modeler::align::superimpose_with_stepwise_aligner( pose, *native_pose_, option[ OptionKeys::stepwise::superimpose_over_all ]() );
			if ( option[ rmsd_residues ].user() ) {
				rmsd = protocols::stepwise::modeler::align::get_rmsd( pose, *native_pose_, option[ rmsd_residues ]() );
			}
		}
		TR << "All atom rmsd over moving residues: " << tag << " " << rmsd << std::endl;
		setPoseExtraScore( pose, "new_rms", rmsd );
		//s.add_energy( "new_rms", rmsd );

		// dihedral rms
		if ( option[ dihedral_rms ]() ) {
			//s.add_energy( "dih_rms", calc_dihedral_rms( pose, *native_pose_, false ) );
			setPoseExtraScore( pose, "dih_rms", calc_dihedral_rms( pose, *native_pose_, false ) );
		}
		if ( option[ gdt_ha ]() ) {
			// requires perfect correspondence!
			//s.add_energy( "gdt_ha", gdtsc( pose, *native_pose_, resmap ) );
			setPoseExtraScore( pose, "gdt_ha", gdtsc( pose, *native_pose_, resmap ) );
		}

		// Stem RMSD
		// if ( option[params_file].user() ) {
		//  std::list< Size > stem_residues( parameters.get_stem_residues( pose ) );
		//  if ( !stem_residues.empty()/*size() > 0*/ ) {
		//   Real const rmsd_stems = all_atom_rmsd( *native_pose_, pose, stem_residues );
		//   s.add_energy( "rms_stem", rmsd_stems );
		//   TR << "Stems rmsd: " << rmsd_stems << std::endl;
		//  }
		// }
	}

	// for data_file, don't actually re-score, just compute rna_chem_map score for now.
	if ( rna_data_reader_->has_reactivities() ) {
		if ( !rna_chemical_mapping_energy_ ) rna_chemical_mapping_energy_ = utility::pointer::make_shared< RNA_ChemicalMappingEnergy >();
		rna_data_reader_->fill_rna_data_info( pose );
		pose.update_residue_neighbors();
		//s.add_energy(  "rna_chem_map",       rna_chemical_mapping_energy_->calculate_energy( pose, false /*use_low_res*/ ) );
		//s.add_energy(  "rna_chem_map_lores", rna_chemical_mapping_energy_->calculate_energy( pose , true /*use_low_res*/ ) );
		setPoseExtraScore( pose, "rna_chem_map", rna_chemical_mapping_energy_->calculate_energy( pose, false /*use_low_res*/ ) );
		setPoseExtraScore( pose, "rna_chem_map_lores", rna_chemical_mapping_energy_->calculate_energy( pose , true /*use_low_res*/ ) );

	}

	TR << "Outputting " << tag << " to silent file: " << option[ out::file::silent ].value() << std::endl;
	//silent_file_data.write_silent_struct( s, silent_file, false /*write score only*/ );

	if ( option[ score::just_calc_rmsd ]() && tag.find( ".pdb" ) != std::string::npos ) {
		std::string out_pdb_file = utility::replace_in( tag, ".pdb", ".sup.pdb" );
		TR << "Creating: " << out_pdb_file << std::endl;
		pose.dump_pdb( out_pdb_file );
	}
	if ( option[ color_by_score ]() ) {
		do_color_by_score( pose );
		pose.dump_pdb( "COLOR_BY_SCORE.pdb" );
	}



}




///////////////////////////////////////////////////////////////
void*
my_main( void* )
{
	using namespace protocols;
	using namespace protocols::jd2;

	using namespace basic::options;
	using namespace basic::options::OptionKeys;
	using namespace core;


	using namespace basic::options;
	using namespace basic::options::OptionKeys;
	using namespace core::chemical;
	using namespace core::scoring::rna::data;
	using namespace core::scoring;
	using namespace core::kinematics;
	using namespace core::io::silent;
	using namespace core::import_pose::pose_stream;
	using namespace core::import_pose;
	using namespace core::pose::full_model_info;
	using namespace protocols::stepwise::modeler;

	auto rsd_set = core::chemical::ChemicalManager::get_instance()->residue_type_set( core::chemical::FA_STANDARD /*RNA*/ );

	// native pose setup
	pose::PoseOP native_pose;
	if ( option[ in::file::native ].user() ) {
		native_pose = get_pdb_with_full_model_info( option[ in::file::native ](), rsd_set );
		//native_pose->dump_pdb("native_pose_dump.pdb");

		if ( option[ convert_protein_centroid ]() ) {
			//native_pose->dump_pdb("native_pose_dump.pdb");
			core::util::switch_to_residue_type_set( *native_pose, core::chemical::CENTROID, false /* no sloppy match */, true /* only switch protein residues */ );
		}
		if ( option[ OptionKeys::stepwise::virtualize_free_moieties_in_native ].user() &&
				option[ OptionKeys::stepwise::virtualize_free_moieties_in_native ]() ) {
			core::pose::rna::virtualize_free_rna_moieties( *native_pose );
		}
	}

	// score function setup
	core::scoring::ScoreFunctionOP scorefxn;
	if ( basic::options::option[ basic::options::OptionKeys::score::weights ].user() ) {
		scorefxn = core::scoring::get_score_function();
	} else {
		scorefxn = core::scoring::ScoreFunctionFactory::create_score_function( core::scoring::RNA_HIRES_WTS );
	}

	// Silent file output setup
	std::string const silent_file = option[ out::file::silent  ]();
	if ( option[ out::overwrite ]() ) core::io::silent::remove_silent_file_if_it_exists( silent_file );

	//SilentFileOptions opts; // initialized from the command line
	//SilentFileData silent_file_data(opts);

	FullModelInfoOP my_model;
	utility::vector1< pose::PoseOP > other_poses;

	if ( option[ original_input ].user() ) {
		utility::vector1< std::string > const & original_files = option[ original_input ]();
		utility::vector1< pose::PoseOP > original_poses;

		for ( Size n = 1; n <= original_files.size(); n++ ) {
			original_poses.push_back( get_pdb_and_cleanup( original_files[ n ], rsd_set ) );
		}
		if ( option[ full_model::other_poses ].user() ) get_other_poses( original_poses, option[ full_model::other_poses ](), rsd_set );

		//FullModelInfo (minimal object needed for add/delete)
		FullModelPoseBuilder builder;
		builder.set_input_poses( original_poses );
		builder.set_options( option );
		builder.initialize_further_from_options();
		builder.build(); // hope this will update original_poses[ 1 ]
		//fill_full_model_info_from_command_line( original_poses );
		my_model = const_full_model_info( *original_poses[ 1 ] ).clone_info();

	} else {
		// other poses -- for scoring collections of poses connected by (virtual) loops, using full_model_info.
		if ( basic::options::option[ full_model::other_poses ].user() ) get_other_poses( other_poses, option[ full_model::other_poses ](), rsd_set );
	}

	// if trying to compute stem RMSD
	//pose::PoseOP pose( new pose::Pose );
	//pose::Pose start_pose;


	auto scoremover = utility::pointer::make_shared< RNA_ScoreMover >( scorefxn, native_pose, rsd_set );

	try {
		JobDistributor::get_instance()->go( scoremover );
	} catch (utility::excn::Exception& excn ) {
		std::cerr << "Exception: " << std::endl;
		excn.show( std::cerr );
		std::cout << "Exception: " << std::endl;
		excn.show( std::cout ); //so its also seen in a >LOG file
	}

	protocols::viewer::clear_conformation_viewers();
	exit( 0 );
}


///////////////////////////////////////////////////////////////////////////////
int
main( int argc, char * argv [] )
{
	try {
		using namespace basic::options;

		utility::vector1< Size > blank_size_vector;
		utility::vector1< std::string > blank_string_vector;
		option.add_relevant( score::weights );
		option.add_relevant( in::file::s );
		option.add_relevant( in::file::silent );
		option.add_relevant( in::file::tags );
		option.add_relevant( in::file::native );
		option.add_relevant( in::file::fasta );
		option.add_relevant( in::file::input_res );
		option.add_relevant( full_model::cutpoint_open );
		option.add_relevant( score::weights );
		option.add_relevant( score::just_calc_rmsd );
		option.add_relevant( OptionKeys::stepwise::virtualize_free_moieties_in_native );
		option.add_relevant( OptionKeys::rna::data_file );
		option.add_relevant( out::overwrite );
		NEW_OPT( original_input, "If you want to rescore the poses using the original FullModelInfo from a SWM run, input those original PDBs here", blank_string_vector );
		NEW_OPT( virtualize_free, "virtualize no-contact bases (and attached no-contact sugars/phosphates)", false );
		NEW_OPT( params_file, "Input file for pairings", "" );
		NEW_OPT( color_by_score, "color PDB by score (currently handles fa_atr & fa_rep)", false );
		NEW_OPT( soft_rep, "use soft_rep params for color_by_score", false ); // how about for actual scoring?
		NEW_OPT( rmsd_nosuper, "Calculate rmsd without superposition first", false);
		NEW_OPT( convert_protein_centroid, "convert the protein residues to centroid rep", false);
		NEW_OPT( rmsd_residues, "residues to calculate rmsd for; superimposition is still over ALL residues", utility::vector1<Size>() );
		NEW_OPT( dihedral_rms, "calculate RMS dihedral distance too", true);
		NEW_OPT( gdt_ha, "calculate heavyatom GDT too", true);

		////////////////////////////////////////////////////////////////////////////
		// setup
		////////////////////////////////////////////////////////////////////////////
		jd2::register_options();
		devel::init(argc, argv);

		////////////////////////////////////////////////////////////////////////////
		// end of setup
		////////////////////////////////////////////////////////////////////////////
		TR << std::endl << "Basic usage:  " << argv[0] << "  -s <pdb file> " << std::endl;
		TR              << "              " << argv[0] << "  -in:file:silent <silent file> " << std::endl;
		TR << std::endl << " Type -help for full slate of options." << std::endl << std::endl;

		protocols::viewer::viewer_main( my_main );
	} catch (utility::excn::Exception const & e ) {
		e.display();
		return -1;
	}
}
