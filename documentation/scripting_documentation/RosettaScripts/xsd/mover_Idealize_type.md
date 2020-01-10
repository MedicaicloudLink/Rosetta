<!-- THIS IS AN AUTOGENERATED FILE: Don't edit it directly, instead change the schema definition in the code itself. -->

_Autogenerated Tag Syntax Documentation:_

---
Idealizes the bond lengths and angles of a pose. It then minimizes the pose in a stripped-down energy function in the presence of coordinate constraints

```xml
<Idealize name="(&string;)" atom_pair_constraint_weight="(0.0 &real;)"
        coordinate_constraint_weight="(0.01 &real;)" fast="(false &bool;)"
        chainbreaks="(false &bool;)" report_CA_rmsd="(true &bool;)"
        ignore_residues_in_csts="(&residue_number_cslist;)"
        impose_constraints="(true &bool;)" constraints_only="(false &bool;)"
        pos_list="(&int_cslist;)" />
```

-   **atom_pair_constraint_weight**: Weight for atom pair constraints
-   **coordinate_constraint_weight**: Weight for coordinate constraints
-   **fast**: Don't minimize or calculate stats after each idealization
-   **chainbreaks**: Keep chainbreaks if they exist
-   **report_CA_rmsd**: Report CA RMSD?
-   **ignore_residues_in_csts**: Ignore these residues when applying constraints
-   **impose_constraints**: Impose constraints on the pose?
-   **constraints_only**: Only impose constraints and don't idealize
-   **pos_list**: List of positions to idealize

---