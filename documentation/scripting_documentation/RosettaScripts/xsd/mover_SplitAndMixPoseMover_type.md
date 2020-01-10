<!-- THIS IS AN AUTOGENERATED FILE: Don't edit it directly, instead change the schema definition in the code itself. -->

_Autogenerated Tag Syntax Documentation:_

---
Splits a Pose into requested pieces and joins them together. Allows to swap order

```xml
<SplitAndMixPoseMover name="(&string;)" order="(&string;)"
        merge_chains="(0 &bool;)" try_all_pairs="(0 &bool;)"
        exclude_consecutive="(1 &bool;)" max_distance="(-1 &real;)"
        residue_selector="(&string;)" />
```

-   **order**: Coma separated numbers defining the orders to rejoin the segments
-   **merge_chains**: When true, all the pieces are merged as a single chain. Otherwise the chain of a piece is assigned according to its original chain.
-   **try_all_pairs**: When true, all the pieces are merged as a single chain
-   **exclude_consecutive**: When true, and try_all_pairs true, it will avoit trying consecutive subposes. Only applies when try_all_pairs=True.
-   **max_distance**: When positive, and try_all_pairs true, limits the distance between N-term and C-term to generate a pair. Only applies when try_all_pairs=True.
-   **residue_selector**: (REQUIRED) ResidueSelector to define the regions of the pose to keep

---