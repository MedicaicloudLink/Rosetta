<!-- THIS IS AN AUTOGENERATED FILE: Don't edit it directly, instead change the schema definition in the code itself. -->

_Autogenerated Tag Syntax Documentation:_

---
slices up repeat proteins to miniproteins

```xml
<SliceToMiniProteinMover name="(&string;)" scorefxn="(&string;)"
        max_length="(119 &non_negative_integer;)"
        min_sse_count="(4 &non_negative_integer;)"
        min_sse_length="(13 &non_negative_integer;)"
        ddg_ala_slice_score="(-5.0 &real;)" relax_mover="(&string;)"
        output_mode="(all &string;)" residue_selector="(&string;)" />
```

-   **scorefxn**: Name of score function to use
-   **max_length**: max length of miniprotein
-   **min_sse_count**: min secondary structure element count
-   **min_sse_length**: min secondary structure element length
-   **ddg_ala_slice_score**: energy when slicing up the miniprotein
-   **relax_mover**: Optionally define a mover which will be applied prior to computing the system energy in the unbound state.
-   **output_mode**: output mode, all,longest,best_ddg
-   **residue_selector**: name of a residue selector that specifies the subset allowed to be part of the miniprotein. Note. This segment must be contiguous

---
