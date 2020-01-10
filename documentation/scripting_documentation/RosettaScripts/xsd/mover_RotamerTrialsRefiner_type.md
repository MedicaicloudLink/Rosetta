<!-- THIS IS AN AUTOGENERATED FILE: Don't edit it directly, instead change the schema definition in the code itself. -->

_Autogenerated Tag Syntax Documentation:_

---
Use rotamer trials to quickly optimize the sidechains in and around the loop being sampled.Rotamer trials is a greedy algorithm for packing sidechains. Each residue is considered only once, and is placed in its optimal rotamer

```xml
<RotamerTrialsRefiner name="(&string;)" loops_file="(&string;)"
        scorefxn="(&string;)"
        task_operations="(&task_operation_comma_separated_list;)"
        packer_palette="(&named_packer_palette;)" />
```

-   **loops_file**: path to loops file
-   **scorefxn**: Name of score function to use
-   **task_operations**: A comma-separated list of TaskOperations to use.
-   **packer_palette**: A previously-defined PackerPalette to use, which specifies the set of residue types with which to design (to be pruned with TaskOperations).

---