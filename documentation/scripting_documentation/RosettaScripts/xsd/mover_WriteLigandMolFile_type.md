<!-- THIS IS AN AUTOGENERATED FILE: Don't edit it directly, instead change the schema definition in the code itself. -->

_Autogenerated Tag Syntax Documentation:_

---
Output a V2000 mol file record containing all atoms of the specified ligand chain and all data stored in the current Job for each processed pose

```xml
<WriteLigandMolFile name="(&string;)" hash_file_names="(false &string;)"
        chain="(&string;)" directory="(&string;)" prefix="(&string;)" />
```

-   **hash_file_names**: Seems to be unused.
-   **chain**: (REQUIRED) The PDB chain ID of the ligand to be output.
-   **directory**: (REQUIRED) The directory all mol records will be output to. Directory will be created if it does not exist.
-   **prefix**: (REQUIRED) Set a file prefix for the output files.

---