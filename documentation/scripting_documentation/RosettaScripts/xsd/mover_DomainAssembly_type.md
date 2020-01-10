<!-- THIS IS AN AUTOGENERATED FILE: Don't edit it directly, instead change the schema definition in the code itself. -->

_Autogenerated Tag Syntax Documentation:_

---
Do domain assembly sampling by fragment insertion in a linker region

```xml
<DomainAssembly name="(&string;)" frag9="(frag9 &string;)"
        frag3="(frag3 &string;)" linker_start_pdb_num="(&residue_number;)"
        linker_start_res_num="(&non_negative_integer;)"
        linker_end_pdb_num="(&residue_number;)"
        linker_end_res_num="(&non_negative_integer;)" />
```

-   **frag9**: Path to fragment file containing 9mers
-   **frag3**: Path to fragment file containing 3mers
-   **linker_start_pdb_num**: Residue number in PDB numbering (residue number + chain ID)
-   **linker_start_res_num**: Residue number in Rosetta numbering (sequentially with the first residue in the pose being 1
-   **linker_end_pdb_num**: Residue number in PDB numbering (residue number + chain ID)
-   **linker_end_res_num**: Residue number in Rosetta numbering (sequentially with the first residue in the pose being 1

---