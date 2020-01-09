// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file   protocols/moves/SmallMover.cxxtest.hh
/// @brief  test suite for protocols::simple_moves::SmallMover.cc


// Test headers
#include <cxxtest/TestSuite.h>
#include <test/core/init_util.hh>
#include <test/util/pose_funcs.hh>

// Unit headers
#include <protocols/simple_moves/BackboneMover.hh>

// Project headers
#include <core/types.hh>
#include <core/id/AtomID_Mask.hh>
#include <core/kinematics/MoveMap.hh>
#include <core/conformation/Residue.hh>
#include <core/pose/annotated_sequence.hh>

// Utility Header
#include <utility/vector1.hh>

// Basic Header
#include <basic/Tracer.hh>


static basic::Tracer TR( "protocols.moves.SmallMover.cxxtest" );

// using declarations
using namespace core;
using namespace core::pose;
using namespace protocols::moves;


class SmallMoverTest : public CxxTest::TestSuite {

public:
	chemical::ResidueTypeSetCAP residue_set;

	PoseOP the_pose;
	PoseOP the_sugar_pose;
	SmallMoverTest() {}

	void setUp() {
		core_init_with_additional_options( "-include_sugars" );
		residue_set = chemical::ChemicalManager::get_instance()->residue_type_set( chemical::FA_STANDARD );

		the_pose = utility::pointer::make_shared< Pose >();
		core::import_pose::pose_from_file( *the_pose, "protocols/moves/test_in.pdb" , core::import_pose::PDB_file);

		the_sugar_pose = utility::pointer::make_shared< Pose >();
		make_pose_from_saccharide_sequence( *the_sugar_pose, "->6)-a-D-Glcp-(1->6)-a-D-Glcp" );

		basic::random::init_random_generators(1000, "mt19937");
	}

	void tearDown() {
		the_pose.reset();
	}

	/// @author Labonte <JWLabonte@jhu.edu>
	void test_one_small_move() {
		protocols::simple_moves::SmallMover mover;

		// Force the 77th residue of this pose to move.
		core::kinematics::MoveMapOP mm( new core::kinematics::MoveMap );
		mm->set_bb( false );
		mm->set_bb( 77, true );
		mover.movemap( mm );

		Angle const initial_phi( the_pose->phi( 77 ) ), initial_psi( the_pose->psi( 77 ) );
		mover.apply( *the_pose );

		TS_ASSERT( the_pose->phi( 77 ) != initial_phi );
		TS_ASSERT( the_pose->psi( 77 ) != initial_psi );
	}

	/// @author Monica Berrondo
	void test_SmallMover() {
		// this is assuming that the residue is of type L so that the max_angle is 6.0
		const int size = 6; ///< size of array to store numbers
		const int N = 1000; ///< number of trials
		const int resnum = 77; ///< residue number to change phis and psis
		// create array and fill it
		std::vector<int> phi_V(size), psi_V(size);
		for ( unsigned int i=0; i<phi_V.size(); ++i )  {
			phi_V[i] = 0;
			psi_V[i] = 0;
		}

		// set up the mover to only change on residue and loop 100 times
		protocols::simple_moves::SmallMover mover; // create a default small mover
		core::kinematics::MoveMapOP movemap( new core::kinematics::MoveMap );
		movemap->set_bb( false );
		movemap->set_bb( resnum, true );
		mover.movemap( movemap );

		// make an initial move and get the phi and psis
		mover.apply( *the_pose );
		core::Real phi = the_pose->phi( resnum );
		core::Real psi = the_pose->psi( resnum );
		// store the phi and psi as the original ones to go back to
		core::Real orig_phi = phi;
		core::Real orig_psi = psi;

		// make the move 100 times
		for ( int i=0; i<N; ++i ) {
			mover.apply( *the_pose );
			phi = the_pose->phi( resnum );
			psi = the_pose->psi( resnum );
			int phi_ind = int( phi - orig_phi + int(size/2) );
			int psi_ind = int( psi - orig_psi + int(size/2) );
			phi_V[phi_ind] += 1;
			psi_V[psi_ind] += 1;

			the_pose->set_phi( resnum, orig_phi );
			the_pose->set_psi( resnum, orig_psi );
		}

		for ( unsigned int i=0; i<phi_V.size(); ++i )  {
			TR << "phi of " << i << " : " << phi_V[i];
			TR << " / psi of " << i << " : " << psi_V[i] << std::endl;
		}

		int min_c_phi = N;
		int max_c_phi = 0;
		int min_c_psi = N;
		int max_c_psi = 0;
		for ( unsigned int i=0; i<phi_V.size(); i++ ) {
			if ( min_c_phi > phi_V[i] ) min_c_phi = phi_V[i];
			if ( max_c_phi < phi_V[i] ) max_c_phi = phi_V[i];
			if ( min_c_psi > psi_V[i] ) min_c_psi = psi_V[i];
			if ( max_c_psi < psi_V[i] ) max_c_psi = psi_V[i];
		}
		double phi_rate = double(min_c_phi)/max_c_phi;
		TR << min_c_phi << " ";
		TR << max_c_phi << " ";
		TR << phi_rate << " ";
		TS_ASSERT_LESS_THAN(0.6, phi_rate);
		double psi_rate = double(min_c_psi)/max_c_psi;
		TR << min_c_psi << " ";
		TR << max_c_psi << " ";
		TR << psi_rate << " ";
		TS_ASSERT_LESS_THAN(0.6, psi_rate);
		TR << "test_SmallMover completed!! " << std::endl;
	}

	/// @author Labonte <JWLabonte@jhu.edu>
	void test_SmallMover_w_sugar() {
		protocols::simple_moves::SmallMover mover;

		// Force the 2nd residue of this two-residue sugar to move by only setting its parent's torsions free to move.
		core::kinematics::MoveMapOP mm( new core::kinematics::MoveMap );
		mm->set_bb( false );
		mm->set_bb( 1, true );
		mover.movemap( mm );

		Angle const initial_phi( the_sugar_pose->phi( 2 ) ),
			initial_psi( the_sugar_pose->psi( 2 ) ),
			initial_omega( the_sugar_pose->omega( 2 ) );
		mover.apply( *the_sugar_pose );

		TS_ASSERT( the_sugar_pose->phi( 2 ) != initial_phi );
		TS_ASSERT( the_sugar_pose->psi( 2 ) != initial_psi );
		TS_ASSERT( the_sugar_pose->omega( 2 ) != initial_omega );
	}
};

