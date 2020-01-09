// -*- mode:c++;tab-width:2;indent-tabs-mode:t;show-trailing-whitespace:t;rm-trailing-spaces:t -*-
// vi: set ts=2 noet:
//
// (c) Copyright Rosetta Commons Member Institutions.
// (c) This file is part of the Rosetta software suite and is made available under license.
// (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
// (c) For more information, see http://www.rosettacommons.org. Questions about this can be
// (c) addressed to University of Washington CoMotion, email: license@uw.edu.

/// @file   core/conformation/UltraLightResidue.cxxtest.hh
/// @brief  test for UltraLightResidue
/// @author Sam DeLuca

#include <cxxtest/TestSuite.h>
#include <core/pose/Pose.hh>
#include <core/types.hh>
#include <test/core/init_util.hh>
#include <core/import_pose/import_pose.hh>
#include <core/conformation/Residue.hh>
#include <core/conformation/UltraLightResidue.hh>
#include <numeric/xyz.functions.hh>
#include <numeric/xyzVector.io.hh>

#include <numeric/model_quality/rms.hh>
#include <ObjexxFCL/FArray2D.hh>
#include <numeric/xyzMatrix.hh>
#include <numeric/xyzTransform.hh>

class UltraLightResidueTests : public CxxTest::TestSuite {
public:
	core::pose::Pose pose_;
	void setUp() {
		core_init();
		core::import_pose::pose_from_file(pose_, "core/conformation/4gatA.pdb", core::import_pose::PDB_file);
	}

	void test_ultralight() {

		core::conformation::UltraLightResidue original_res(pose_.residue(3).get_self_ptr());
		core::conformation::UltraLightResidue test_res(pose_.residue(3).get_self_ptr());

		core::Vector test_translation(1.5,2.5,3.0);
		core::Vector inverse_translation(-1.5,-2.5,-3.0);

		numeric::xyzMatrix<core::Real> test_rotation(
			numeric::z_rotation_matrix_degrees( 115.0 ) * (
			numeric::y_rotation_matrix_degrees( 85.0 ) *
			numeric::x_rotation_matrix_degrees( 5.0 ) ));

		//******************Perform a random transformation and then test align to residue to see if alignment works

		test_res.transform(test_rotation, test_translation);
		test_res.align_to_residue(original_res);

		for ( core::Size atom_index =1; atom_index <= test_res.natoms(); ++atom_index ) {
			TS_ASSERT_DELTA(original_res[atom_index],test_res[atom_index],0.001);
		}

		core::Real deviation = 0;
		utility::vector1<core::PointPosition > target_coords = original_res.coords_vector();
		utility::vector1<core::PointPosition > copy_coords = test_res.coords_vector();

		for ( core::Size i=1; i <= copy_coords.size(); ++i ) {
			core::Real deviation_x = ((copy_coords[i][0]-target_coords[i][0]) * (copy_coords[i][0]-target_coords[i][0]));
			core::Real deviation_y = ((copy_coords[i][1]-target_coords[i][1]) * (copy_coords[i][1]-target_coords[i][1]));
			core::Real deviation_z = ((copy_coords[i][2]-target_coords[i][2]) * (copy_coords[i][2]-target_coords[i][2]));

			core::Real total_dev = deviation_x + deviation_y + deviation_z;
			deviation += total_dev;
		}

		deviation /= (core::Real)copy_coords.size();
		deviation = sqrt(deviation);

		TS_ASSERT_LESS_THAN(deviation, 0.001);

		target_coords.clear();
		copy_coords.clear();

		//******************************************END Perform a random transformation and then test align to residue to see if alignment works

		//******************Perform a simple 1D rotation and check against correct coordinates

		core::conformation::UltraLightResidue light_res(pose_.residue(3).get_self_ptr());
		TS_ASSERT_EQUALS(pose_.residue(3).natoms(),light_res.natoms());

		numeric::xyzMatrix<core::Real> matrix(numeric::x_rotation_matrix_degrees(40.0));
		core::Vector null_translation(0.0,0.0,0.0);

		light_res.transform(matrix,null_translation);

		utility::vector1<core::PointPosition> correct_values;

		/// Values based on the original fa_standard residue geometries
		//correct_values.push_back(core::PointPosition(18.89500000000000, 22.15231212888653, 32.39375812542860));
		//correct_values.push_back(core::PointPosition(19.63800000000000, 20.92591105578865, 32.07091078084550));
		//correct_values.push_back(core::PointPosition(18.97800000000000, 20.52435624619454, 30.75882704308642));
		//correct_values.push_back(core::PointPosition(19.46700000000000, 19.70952348969867, 30.00460918401562));
		//correct_values.push_back(core::PointPosition(19.41500000000000, 19.83285791109637, 33.12620070445812));
		//correct_values.push_back(core::PointPosition(20.17500000000000, 20.16753758353558, 34.41219390695026));
		//correct_values.push_back(core::PointPosition(19.58100000000000, 20.29507775309662, 35.46432769366577));
		//correct_values.push_back(core::PointPosition(21.47300000000000, 20.30363101537560, 34.37757342442899));
		//correct_values.push_back(core::PointPosition(18.06100000000000, 22.30939339202285, 31.90941271482631));
		//correct_values.push_back(core::PointPosition(20.69000000000000, 21.13266737818832, 31.92718596340691));
		//correct_values.push_back(core::PointPosition(18.41286600575046, 19.45808729661438, 33.33863377746288));
		//correct_values.push_back(core::PointPosition(19.97003785324535, 19.08441380647961, 32.56046556194600));
		//correct_values.push_back(core::PointPosition(21.97475191135342, 20.52174190297195, 35.21519558275942));
		//correct_values.push_back(core::PointPosition(21.96481459076445, 20.18967154960102, 33.51494841696830));

		/// Values based on the 05.2009_icoor parameter set
		correct_values.push_back(core::PointPosition(18.895,22.15893618713942,32.3862));
		correct_values.push_back(core::PointPosition(19.638,20.93253511404154,32.0633));
		correct_values.push_back(core::PointPosition(18.978,20.53098030444744,30.7512));
		correct_values.push_back(core::PointPosition(19.467,19.71614754795157,29.9970));
		correct_values.push_back(core::PointPosition(19.415,19.83948196934927,33.1186));
		correct_values.push_back(core::PointPosition(20.175,20.17416164178848,34.4046));
		correct_values.push_back(core::PointPosition(19.581,20.30170181134952,35.4567));
		correct_values.push_back(core::PointPosition(21.473,20.3102550736285,34.3700));
		correct_values.push_back(core::PointPosition(18.061,22.31601745027575,31.9018));
		correct_values.push_back(core::PointPosition(20.69,21.13929143644122,31.9196));
		correct_values.push_back(core::PointPosition(18.3600,19.7630,33.3361));
		correct_values.push_back(core::PointPosition(19.7700,18.8947,32.7328));
		correct_values.push_back(core::PointPosition(21.9650,20.5202,35.1921));
		correct_values.push_back(core::PointPosition(21.9550,20.1997,33.5251));

		//std::cout.precision( 16 );
		//for ( core::Size ii = 1; ii <= light_res.natoms(); ++ii ) {
		// std::cout << "correct_values.push_back(core::PointPosition(";
		// std::cout << light_res[ii].x() << ",";
		// std::cout << light_res[ii].y() << ",";
		// std::cout << light_res[ii].z() << "));" << std::endl;
		//}


		for ( core::Size atom_index =1; atom_index <= light_res.natoms(); ++atom_index ) {
			// The original way this was written compared the objects. As such it was
			// quite hard to tell what was different (and the delta of 0.001 was
			// meaningless.
			//TS_ASSERT_DELTA(correct_values[atom_index],light_res[atom_index],0.001);
			TS_ASSERT_DELTA(correct_values[atom_index].x(),light_res[atom_index].x(),0.001);
			TS_ASSERT_DELTA(correct_values[atom_index].y(),light_res[atom_index].y(),0.001);
			TS_ASSERT_DELTA(correct_values[atom_index].z(),light_res[atom_index].z(),0.001);
		}

		light_res.update_conformation(pose_.conformation());
		TS_ASSERT_EQUALS(pose_.residue(3).natoms(),light_res.natoms());

		for ( core::Size atom_index = 1; atom_index <= light_res.natoms(); ++atom_index ) {
			// Ditto.
			//TS_ASSERT_EQUALS(light_res[atom_index],pose_.residue(3).xyz(atom_index));
			TS_ASSERT_EQUALS(light_res[atom_index].x(),pose_.residue(3).xyz(atom_index).x());
			TS_ASSERT_EQUALS(light_res[atom_index].y(),pose_.residue(3).xyz(atom_index).y());
			TS_ASSERT_EQUALS(light_res[atom_index].z(),pose_.residue(3).xyz(atom_index).z());
		}

		//******************END- Perform a simple 1D rotation and check against correct coordinates

	}
};

