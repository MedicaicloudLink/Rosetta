<!-- THIS IS AN AUTOGENERATED FILE: Don't edit it directly, instead change the schema definition in the code itself. -->

_Autogenerated Tag Syntax Documentation:_

---
XRW TO DO

```xml
<SpliceOutTail name="(&string;)" debug="(false &bool;)"
        CG_const="(false &bool;)" chain_num="(1 &non_negative_integer;)"
        superimposed="(true &bool;)" thread_original_sequence="(false &bool;)"
        tail_segment="(&n_or_c;)" scorefxn="(&string;)"
        use_sequence_profile="(&bool;)" template_file="(&string;)"
        source_pdb="(&string;)"
        task_operations="(&task_operation_comma_separated_list;)"
        packer_palette="(&named_packer_palette;)"
        from_res="(0 &refpose_enabled_residue_number;)"
        torsion_database="(&string;)" design_shell="(6.0 &real;)"
        repack_shell="(8.0 &real;)" rms_cutoff="(999999 &real;)"
        rtmin="(true &bool;)" splice_filter="(&string;)" mover="(&string;)"
        restrict_to_repacking_chain2="(true &bool;)" >
    <Segments name="(&string;)" current_segment="(&string;)" >
        <segment name="(&string;)" pdb_profile_match="(&string;)" profiles="(&string;)" />
    </Segments>
</SpliceOutTail>
```

-   **debug**: XRW TO DO
-   **CG_const**: If true apply coordinate constraint on C-gammas of the segment during CCD/minimization
-   **chain_num**: The pose's chain onto which the new segment is added.
-   **superimposed**: superimpose source pdb onto current pose.
-   **thread_original_sequence**: If true, thread the seuqeunce from the refrence pdb onto the segment
-   **tail_segment**: Is this a C-ter ("c") or and N-ter ("N") segment.
-   **scorefxn**: Name of score function to use
-   **use_sequence_profile**: XRW TO DO
-   **template_file**: XRW TO DO
-   **source_pdb**: XRW TO DO
-   **task_operations**: A comma-separated list of TaskOperations to use.
-   **packer_palette**: A previously-defined PackerPalette to use, which specifies the set of residue types with which to design (to be pruned with TaskOperations).
-   **from_res**: XRW TO DO
-   **torsion_database**: XRW TO DO
-   **design_shell**: XRW TO DO
-   **repack_shell**: XRW TO DO
-   **rms_cutoff**: XRW TO DO
-   **rtmin**: XRW TO DO
-   **splice_filter**: XRW TO DO
-   **mover**: Which mover to use to optimize segment's backbone
-   **restrict_to_repacking_chain2**: XRW TO DO


Subtag **Segments**:   Wrapper for multiple segments tags

-   **current_segment**: XRW TO DO


Subtag **segment**:   individual segment tag

-   **pdb_profile_match**: XRW TO DO
-   **profiles**: XRW TO DO

---
