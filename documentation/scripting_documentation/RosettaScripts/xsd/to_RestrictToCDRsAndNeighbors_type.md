<!-- THIS IS AN AUTOGENERATED FILE: Don't edit it directly, instead change the schema definition in the code itself. -->

_Autogenerated Tag Syntax Documentation:_

---
Author: Jared Adolf-Bryfogle (jadolfbr@gmail.com)
  Restrict (design and/or packing) to specific CDRs and neighbors

```xml
<RestrictToCDRsAndNeighbors name="(&string;)" cdrs="(&string;)"
        neighbor_dis="(6.0 &real;)" design_cdrs="(false &bool;)"
        design_antigen="(false &bool;)" design_framework="(false &bool;)"
        stem_size="(0 &non_negative_integer;)" cdr_definition="(&string;)"
        input_ab_scheme="(&string;)" />
```

-   **cdrs**: List of CDR regions (string) devided by one of the following characters: ":,'`~+*|;. "Recognized CDRs: "Aroop|Chothia|Kabat|Martin|North|"
-   **neighbor_dis**: Neighbor detection distance
-   **design_cdrs**: Should we allow design to the CDRs?
-   **design_antigen**: Shoule we allow design to neighboring antigen residues?
-   **design_framework**: Shoule we allow design to neighboring framework residues?
-   **stem_size**: Length of the stem to also select
-   **cdr_definition**: The Specific CDR definition to use.
-   **input_ab_scheme**: The Sepcific input antibody scheme to use

---