<!-- THIS IS AN AUTOGENERATED FILE: Don't edit it directly, instead change the schema definition in the code itself. -->

_Autogenerated Tag Syntax Documentation:_

---
XRW TO DO

```xml
<RepeatPropagationMover name="(&string;)"
        first_template_res="(9999 &non_negative_integer;)"
        last_template_res="(9999 &non_negative_integer;)"
        numb_repeats="(&non_negative_integer;)" ideal_repeat="(true &bool;)"
        repeat_without_replacing_pose="(false &bool;)"
        maintain_cap="(false &bool;)"
        maintain_cap_sequence_only="(false &bool;)"
        nTerm_cap_size="(&non_negative_integer;)"
        cTerm_cap_size="(&non_negative_integer;)"
        maintain_ligand="(false &bool;)"
        extract_repeat_info_from_pose="(false &bool;)"
        extract_repeat_template_repeat="(1 &non_negative_integer;)"
        start_pose_numb_repeats="(4 &non_negative_integer;)"
        start_pose_length="(0 &non_negative_integer;)"
        start_pose_duplicate_residues="(0 &non_negative_integer;)" />
```

-   **first_template_res**: first template residue
-   **last_template_res**: last template residue
-   **numb_repeats**: (REQUIRED) number of repeats to output
-   **ideal_repeat**: is the repeat internally ideal
-   **repeat_without_replacing_pose**: for speed when the duplicating the same length pose
-   **maintain_cap**: if you want the cap structure and sequence maintained
-   **maintain_cap_sequence_only**: maintain only the sequence in cap
-   **nTerm_cap_size**: Size of n_term of cap
-   **cTerm_cap_size**: Size of c_term of cap
-   **maintain_ligand**: maintain ligand
-   **extract_repeat_info_from_pose**: mode where repeat info is extracted from input pose
-   **extract_repeat_template_repeat**: location where repeat info is extracted from input pose
-   **start_pose_numb_repeats**: number of repeats in input pose
-   **start_pose_length**: orig length of input pose. This is used to catch pose length changes
-   **start_pose_duplicate_residues**: used when only 1 repeat in input pose and there are identical residues in N_term and C_term

---