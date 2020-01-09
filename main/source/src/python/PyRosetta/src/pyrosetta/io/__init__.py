# :noTabs=true:
#
# (c) Copyright Rosetta Commons Member Institutions.
# (c) This file is part of the Rosetta software suite and is made available under license.
# (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
# (c) For more information, see http://www.rosettacommons.org.
# (c) Questions about this can be addressed to University of Washington CoMotion, email: license@uw.edu.


import pyrosetta.rosetta as rosetta

from pyrosetta.rosetta.core.pose import make_pose_from_sequence, Pose
from pyrosetta.rosetta.core.import_pose import pose_from_file
from pyrosetta.io.silent_file_map import SilentFileMap

# for backward-compatibility
# This needs to be here because of all the people using the Workshops and
# because otherwise, it wrecks a lot of people's scripts.  ~Labonte
def pose_from_pdb(filename):
    return pose_from_file(filename)


def pose_from_sequence(seq, res_type="fa_standard", auto_termini=True):
    """
    Returns a pose generated from a single-letter sequence of amino acid
    residues in <seq> using the <res_type> ResidueType and creates N- and C-
    termini if <auto_termini> is set to True.

    Unlike make_pose_from_sequence(), this method generates a default PDBInfo
    and sets all torsion angles to 180 degrees.

    Example:
        pose = pose_from_sequence("THANKSEVAN")
    See also:
        Pose
        make_pose_from_sequence()
        pose_from_file()
        pose_from_rcsb()
    """
    pose = Pose()
    make_pose_from_sequence(pose, seq, res_type, auto_termini)
    #print 'Setting phi, psi, omega...'
    for i in range(0, pose.total_residue()):
        res = pose.residue(i + 1)
        if not res.is_protein() or res.is_peptoid() or res.is_carbohydrate():
            continue

        pose.set_phi(i + 1, 180)
        pose.set_psi(i + 1, 180)
        pose.set_omega(i + 1, 180)
    #print 'Attaching PDBInfo...'
    # Empty PDBInfo (rosetta.core.pose.PDBInfo()) is not correct here;
    # we have to reserve space for atoms....
    pose.pdb_info(rosetta.core.pose.PDBInfo(pose))
    pose.pdb_info().name(seq[:8])
    # print pose
    return pose


def poses_from_silent(silent_filename):
    """Returns an Iterator object which is composed of Pose objects from a silent file.

    @atom-moyer
    """
    sfd = rosetta.core.io.silent.SilentFileData(rosetta.core.io.silent.SilentFileOptions())
    sfd.read_file(silent_filename)
    for tag in sfd.tags():
        ss = sfd.get_structure(tag)
        pose = Pose()
        ss.fill_pose(pose)
        pose.pdb_info().name(tag)
        yield pose


def poses_to_silent(poses, output_filename):
    """Takes a Pose or list of poses and outputs them as a binary silent file.
    This method requires a Pose object. 
    If you are using a PackedPose, use pyrosetta.distributed.io.to_silent()

    Inputs:
    poses: Pose or list of poses. This function automatically detects which one.
    output_filename: The desired name of the output silent file.

    Example:
    poses_to_silent(poses, "mydesigns.silent")

    The decoy name written to your silent file is take from pose.pdb_info().name()
    To set a different decoy name, change it in your pose before calling this function.
    Example:
    pose.pdb_info().name("my_tag.pdb")

    @srgerb
    """

    silentOptions = rosetta.core.io.silent.SilentFileOptions()
    silentOptions.set_binary_output(True)
    silentFile = rosetta.core.io.silent.SilentFileData(silentOptions)
    silentStruct = rosetta.core.io.silent.BinarySilentStruct(silentOptions)

    def output_silent(pose):
        decoy_tag = pose.pdb_info().name()
        silentStruct.fill_struct(pose, tag=decoy_tag)
        silentFile.write_silent_struct(silentStruct, filename=output_filename)

    if isinstance(poses, (list, tuple, set)):
        for pose in poses:
            output_silent(pose=pose)
    else:
        output_silent(pose=poses)
