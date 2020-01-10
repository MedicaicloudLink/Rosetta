<!-- THIS IS AN AUTOGENERATED FILE: Don't edit it directly, instead change the schema definition in the code itself. -->

_Autogenerated Tag Syntax Documentation:_

---
report rotamer recover features and scores to features Statistics Scientific Benchmark

```xml
<RotamerRecoveryFeatures name="(&string;)" scorefxn="(&string;)"
        mover_name="(&string;)" mover="(&string;)" reference_name="(&string;)"
        protocol="(RRProtocolMinPack &string;)" nonideal="(&bool;)"
        cartesian="(&bool;)" comparer="(&string;)" recovery_threshold="(&real;)"
        absolute_threshold="(&real;)"
        task_operations="(&task_operation_comma_separated_list;)"
        packer_palette="(&named_packer_palette;)"
        predicted_features_reporter="(&string;)" reference_pdb="(&string;)"
        reference_pose="(&string;)"
        chidiff_num_chi_to_compare="(&non_negative_integer;)" />
```

-   **scorefxn**: Name of score function to use
-   **mover_name**: mover_name and reference_name are mutually exclusive
-   **mover**: mover and reference_name are mutually exclusive
-   **reference_name**: mover and reference_name are mutually exclusive
-   **protocol**: mover and protocol are mutually exclusive
-   **nonideal**: sets RRProtocolRTMin or RRProtocolRelax to nonideal
-   **cartesian**: sets RRProtocolRTMin or RRProtocolRelax to cartesian
-   **comparer**: Rotamer recovery comparer
-   **recovery_threshold**: recovery threshold of the comparer
-   **absolute_threshold**: absolute electron density correlation that must be met by native to be considered in recovery statistics
-   **task_operations**: A comma-separated list of TaskOperations to use.
-   **packer_palette**: A previously-defined PackerPalette to use, which specifies the set of residue types with which to design (to be pruned with TaskOperations).
-   **predicted_features_reporter**: feature reporter that reports to db of type ReportToDB
-   **reference_pdb**: For use with the RRProtocolReferenceStructure. The PDB formatted file that should be compared against. Mutually exclusive with the 'reference_pose' attribute, but at least one of the two must be provided.
-   **reference_pose**: For use with the RRProtocolReferenceStructure. The Pose held in the DataMap that should be compared against. This Pose should be loaded into the DataMap using the ResourceManager and the PoseFromPoseResourceMover. Mutually exclusive with the 'reference_pose' attribute, but at least one of the two must be provided.
-   **chidiff_num_chi_to_compare**: For use only with the ChiDiff rotamer-recovery comparer, this controls the maximum number of chi dihedrals will be examined for each residue. In the absence of this attribute, the ChiDiff reporter will examine all chi dihedrals

---