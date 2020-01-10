<!-- THIS IS AN AUTOGENERATED FILE: Don't edit it directly, instead change the schema definition in the code itself. -->

_Autogenerated Tag Syntax Documentation:_

---
XRW TO DO

```xml
<TaskAwareCsts name="(&string;)"
        task_operations="(&task_operation_comma_separated_list;)"
        packer_palette="(&named_packer_palette;)"
        cst_type="(coordinate &string;)" anchor_resnum="(&string;)" />
```

-   **task_operations**: A comma-separated list of TaskOperations to use.
-   **packer_palette**: A previously-defined PackerPalette to use, which specifies the set of residue types with which to design (to be pruned with TaskOperations).
-   **cst_type**: What type of constraint you intend to apply
-   **anchor_resnum**: What residue number ought to serve as anchor (relevant for coordinate constraints -- please specify if so -- and less so otherwise.

---