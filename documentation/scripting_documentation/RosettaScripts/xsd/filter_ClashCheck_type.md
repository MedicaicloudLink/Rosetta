<!-- THIS IS AN AUTOGENERATED FILE: Don't edit it directly, instead change the schema definition in the code itself. -->

_Autogenerated Tag Syntax Documentation:_

---
Calculate the number of heavy atoms clashing between building blocks.

```xml
<ClashCheck name="(&string;)" clash_dist="(3.5 &real;)"
        sym_dof_names="(XRW TO DO &string;)"
        nsub_bblock="(1 &non_negative_integer;)"
        cutoff="(0 &non_negative_integer;)" verbose="(0 &bool;)"
        write2pdb="(0 &bool;)"
        task_operations="(&task_operation_comma_separated_list;)"
        packer_palette="(&named_packer_palette;)" confidence="(1.0 &real;)" />
```

-   **clash_dist**: Distance between heavy atoms below which they are considered to be clashing. Note: A hard-coded cutoff of 2.6 is set for contacts between backbone carbonyl oxygens and nitrogens for bb-bb hbonds.
-   **sym_dof_names**: Only use with multicomponent systems. Comma-separated list of name(s) of the sym_dof(s) corresponding to the building block(s) between which to check for clashes.
-   **nsub_bblock**: The number of subunits in the symmetric building block. Does not need to be set for multicomponent systems.
-   **cutoff**: Maximum number of allowable clashes.
-   **verbose**: If set to true, then will output a pymol selection string to the logfile with the clashing positions/atoms.
-   **write2pdb**: If set to true, then will output a pymol selection string to the output pdb with the clashing positions/atoms.
-   **task_operations**: A comma-separated list of TaskOperations to use.
-   **packer_palette**: A previously-defined PackerPalette to use, which specifies the set of residue types with which to design (to be pruned with TaskOperations).
-   **confidence**: Probability that the pose will be filtered out if it does not pass this Filter

---