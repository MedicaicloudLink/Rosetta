<!-- THIS IS AN AUTOGENERATED FILE: Don't edit it directly, instead change the schema definition in the code itself. -->

_Autogenerated Tag Syntax Documentation:_

---
XSD_XRW: TO DO

```xml
<ScriptCM name="(&string;)" >
    <Mover Tag ... />
    <CutBiasClaim label="(&string;)" bias="(&real;)"
            region_start="(&non_negative_integer;)"
            region_end="(&non_negative_integer;)" />
    <JumpClaim jump_label="(&string;)" position1="(&string;)" position2="(&string;)"
            control_strength="(&envclaim_ctrl_str;)"
            initialization_strength="(DOES_NOT_CONTROL &envclaim_ctrl_str;)"
            cut="(&string;)" atom1="(&string;)" atom2="(&string;)"
            physical_cut="(&bool;)" />
    <TorsionClaim control_strength="(&envclaim_ctrl_str;)"
            initialization_strength="(DOES_NOT_CONTROL &envclaim_ctrl_str;)"
            backbone="(false &bool;)" sidechain="(false &bool;)"
            selector="(&string;)" />
    <VirtResClaim vrt_name="(&string;)" jump_label="(&string;)" parent="(&string;)"
            jump_control_strength="(DOES_NOT_CONTROL &envclaim_ctrl_str;)" />
    <XYZClaim control_strength="(&envclaim_ctrl_str;)"
            initialization_strength="(DOES_NOT_CONTROL &envclaim_ctrl_str;)"
            relative_only="(false &bool;)" selection="(&string;)" />
</ScriptCM>
```



"Mover Tag": Any of the [[RosettaScripts Mover|Movers-RosettaScripts]] tags

Subtag **CutBiasClaim**:   XRW TO DO

-   **label**: (REQUIRED) XRW TO DO
-   **bias**: (REQUIRED) XRW TO DO
-   **region_start**: (REQUIRED) XRW TO DO
-   **region_end**: (REQUIRED) XRW TO DO

Subtag **JumpClaim**:   XRW TO DO

-   **jump_label**: (REQUIRED) XRW TO DO
-   **position1**: (REQUIRED) XRW TO DO
-   **position2**: (REQUIRED) XRW TO DO
-   **control_strength**: (REQUIRED) XRW TO DO
-   **initialization_strength**: XRW TO DO
-   **cut**: XRW TO DO
-   **atom1**: XRW TO DO
-   **atom2**: XRW TO DO
-   **physical_cut**: XRW TO DO

Subtag **TorsionClaim**:   XRW TO DO

-   **control_strength**: (REQUIRED) XRW TO DO
-   **initialization_strength**: XRW TO DO
-   **backbone**: XRW TO DO
-   **sidechain**: XRW TO DO
-   **selector**: (REQUIRED) Name of previously defined residue selector that defines where to apply this claim

Subtag **VirtResClaim**:   XRW TO DO

-   **vrt_name**: (REQUIRED) XRW TO DO
-   **jump_label**: Defaults to vrt_name with _jump appended
-   **parent**: (REQUIRED) XRW TO DO
-   **jump_control_strength**: XRW TO DO

Subtag **XYZClaim**:   XRW TO DO

-   **control_strength**: (REQUIRED) XRW TO DO
-   **initialization_strength**: XRW TO DO
-   **relative_only**: XRW TO DO
-   **selection**: (REQUIRED) XRW TO DO

---
