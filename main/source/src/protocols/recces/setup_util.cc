// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file protocols/recces/setup_util.cc
/// @brief
/// @detailed
/// @author Rhiju Das, rhiju@stanford.edu


#include <protocols/recces/setup_util.hh>
#include <protocols/recces/options/RECCES_Options.hh>
#include <core/chemical/ResidueTypeSet.hh>
#include <core/chemical/ResidueType.hh>
#include <core/chemical/VariantType.hh>
#include <core/chemical/ChemicalManager.hh>
#include <core/chemical/rna/util.hh>
#include <core/pose/Pose.hh>
#include <core/pose/variant_util.hh>
#include <core/pose/rna/RNA_SecStruct.hh>
#include <core/pose/annotated_sequence.hh>
#include <core/pose/rna/util.hh>
#include <core/import_pose/import_pose.hh>
#include <core/import_pose/RNA_HelixAssembler.hh>
#include <core/import_pose/FullModelPoseBuilder.hh>
#include <protocols/stepwise/modeler/util.hh>
#include <core/pose/rna/secstruct_legacy/RNA_SecStructLegacyInfo.hh>
#include <protocols/rna/util.hh>
#include <basic/Tracer.hh>
#include <basic/options/option.hh>

#include <utility/io/ozstream.hh>

static basic::Tracer TR( "protocols.recces.setup_util" );

using namespace core;
using namespace core::chemical;
using namespace core::pose;

namespace protocols {
namespace recces {


//////////////////////////////////////////////////////////////////////////////
/// TODO: Unify -- read pose from file, and then fill_full_model_info_from_command_line().
PoseOP
recces_pose_setup( options::RECCES_Options const & options )
{

	PoseOP pose;
	/// TODO -- allow user to still use -seq1, -seq2 input but also allow specification of
	///         RNA secondary structure *from command line*, and get that stored into full_model_info().
	if ( options.legacy_turner_mode() ) {

		TR << TR.Green << "Assuming RECCES Turner mode, due to specification of -seq1" << std::endl;
		runtime_assert( options.seq1().size() > 0 );
		pose = pose_setup_turner( options.seq1(), options.seq2() );

	} else {

		runtime_assert( options.infile().size()  > 0 );
		pose = pose_setup_from_file( options );
	}

	// Obtains base pairs and constraints from pose
	if ( options.setup_base_pair_constraints() ) {
		utility::vector1< std::pair< Size, Size > > pairings( options.rna_secstruct().base_pairs() ); // could be user-specified.
		if ( pairings.empty() ) core::pose::rna::get_base_pairing_list( *pose, pairings ); // infer from pose
		core::pose::rna::setup_base_pair_constraints( *pose, pairings, 1.0 /*scale_factor*/, true /*use_flat_harmonic*/ );
	}

	return pose;
}

//////////////////////////////////////////////////////////////////////////////
core::pose::PoseOP
pose_setup_turner(
	std::string const & seq1,
	std::string const & seq2)
{
	using namespace core::chemical;
	using namespace core::pose::rna;
	using namespace core::pose::rna::secstruct_legacy;

	core::import_pose::RNA_HelixAssembler assembler;
	assembler.use_phenix_geo( true );
	PoseOP pose( assembler.build_init_pose( seq1, seq2 ) );
	virtualize_5prime_phosphates( *pose );

	// Need to specify which nucleotides get 'A-form' range, which will
	//  also determine how large the Gaussian steps are during sampling.
	std::string secstruct_legacy;
	Size const len1( seq1.size() );
	Size const len2( seq2.size() );
	Size const n_bp( std::min( len1, len2 ) );
	Size const total_len( len1 + len2 );
	for ( Size i = 1; i <= total_len; ++i ) {
		if ( i > n_bp && i <= total_len - n_bp ) secstruct_legacy.push_back( 'X' );
		else secstruct_legacy.push_back( 'H' );
	}
	set_rna_secstruct_legacy( *pose, secstruct_legacy );

	return pose;
}


//////////////////////////////////////////////////////////////////////////////
core::pose::PoseOP
pose_setup_from_file( options::RECCES_Options const & options )
{
	using namespace core::chemical;
	using namespace core::chemical::rna;


	ResidueTypeSetCAP rsd_set = ChemicalManager::get_instance()->residue_type_set( FA_STANDARD );
	//PoseOP pose = core::import_pose::get_pdb_and_cleanup( options.infile() );
	// // needs to accept command-line input -- need to guess sequence info if fasta not specified!
	// // also -- force sample_res all on if not specified from command-line. [different from stepwise default behavior]
	// a GREAT builder use case. We need to be able to intervene with the builder and override its idea of sample_res.
	core::import_pose::FullModelPoseBuilder builder;

	utility::vector1< pose::PoseOP > input_poses;
	input_poses.push_back( core::import_pose::get_pdb_and_cleanup( options.infile() ) );

	//std::pair< vector1< Size >, vector1< char > > const & input_resnum_and_chain = options[ in::file::input_res ].resnum_and_chain();

	builder.set_input_poses( input_poses );
	builder.set_options( basic::options::option );
	builder.initialize_further_from_options();

	PoseOP pose = builder.build();

	core::pose::fix_up_residue_type_variants( *pose ); //virtualizes phosphates, etc.

	TR << "Annotated sequence of pose from " << options.infile() << ": " << pose->annotated_sequence() << std::endl;

	// replace this with fold_tree, full_model_info definition, + cleanup_variants
	// need block_stack option in FullModelSetupFromCommandLine
	if ( pose->size() == 2 ) {
		kinematics::FoldTree f( 2 );
		f.new_jump( 1, 2, 1 );
		f.set_jump_atoms( 1, default_jump_atom(pose->residue_type(1)), default_jump_atom(pose->residue_type(2) )) ;
		pose->fold_tree( f );
		for ( Size n = 1; n <= 2; n++ ) {
			pose::add_variant_type_to_pose_residue( *pose, VIRTUAL_PHOSPHATE, n );
			pose::add_variant_type_to_pose_residue( *pose, VIRTUAL_RIBOSE, n );
			if ( options.block_stack() ) {
				pose::add_variant_type_to_pose_residue( *pose, BLOCK_STACK_ABOVE, n );
				pose::add_variant_type_to_pose_residue( *pose, BLOCK_STACK_BELOW, n );
			}
		}
	}

	return pose;
}

} //recces
} //protocols
