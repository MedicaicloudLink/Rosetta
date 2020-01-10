<!-- THIS IS AN AUTOGENERATED FILE: Don't edit it directly, instead change the schema definition in the code itself. -->

_Autogenerated Tag Syntax Documentation:_

---
XRW TO DO

```xml
<VLB name="(&string;)" scorefxn="(score4L &string;)" >
    <Bridge left="(&refpose_enabled_residue_number;)"
            right="(&refpose_enabled_residue_number;)" ss="(&string;)"
            aa="(&string;)" />
    <ConnectRight left="(&refpose_enabled_residue_number;)"
            right="(&refpose_enabled_residue_number;)" pdb="(&string;)" />
    <GrowLeft pos="(&refpose_enabled_residue_number;)" ss="(&string;)"
            aa="(&string;)" />
    <GrowRight pos="(&refpose_enabled_residue_number;)" ss="(&string;)"
            aa="(&string;)" />
    <SegmentInsert left="(&refpose_enabled_residue_number;)"
            right="(&refpose_enabled_residue_number;)" ss="(&string;)"
            keep_bb_torsions="(0 &bool;)" pdb="(&string;)" side="(&string;)" />
    <SegmentRebuild left="(&refpose_enabled_residue_number;)"
            right="(&refpose_enabled_residue_number;)" ss="(&string;)"
            aa="(&string;)" />
    <SegmentSwap left="(&refpose_enabled_residue_number;)"
            right="(&refpose_enabled_residue_number;)" pdb="(&string;)" />
</VLB>
```

-   **scorefxn**: Scorefunction to be used


Subtag **Bridge**:   XRW TODO

-   **left**: (REQUIRED) Left residue
-   **right**: (REQUIRED) Right residue
-   **ss**: (REQUIRED) XRW TO DO
-   **aa**: (REQUIRED) XRW TO DO

Subtag **ConnectRight**:   XRW TODO

-   **left**: (REQUIRED) Left residue
-   **right**: (REQUIRED) Right residue
-   **pdb**: (REQUIRED) PDB file name to be read in

Subtag **GrowLeft**:   XRW TODO

-   **pos**: (REQUIRED) The single residue from which to build
-   **ss**: (REQUIRED) XRW TO DO
-   **aa**: (REQUIRED) XRW TO DO

Subtag **GrowRight**:   XRW TODO

-   **pos**: (REQUIRED) The single residue from which to build
-   **ss**: (REQUIRED) XRW TO DO
-   **aa**: (REQUIRED) XRW TO DO

Subtag **SegmentInsert**:   XRW TODO

-   **left**: (REQUIRED) Left residue
-   **right**: (REQUIRED) Right residue
-   **ss**: (REQUIRED) XRW TO DO
-   **keep_bb_torsions**: XRW TO DO
-   **pdb**: (REQUIRED) PDB file name to be read in
-   **side**: (REQUIRED) XRW TO DO

Subtag **SegmentRebuild**:   XRW TODO

-   **left**: (REQUIRED) Left residue
-   **right**: (REQUIRED) Right residue
-   **ss**: (REQUIRED) XRW TO DO
-   **aa**: (REQUIRED) XRW TO DO

Subtag **SegmentSwap**:   XRW TODO

-   **left**: (REQUIRED) Left residue
-   **right**: (REQUIRED) Right residue
-   **pdb**: (REQUIRED) PDB file name to be read in

---