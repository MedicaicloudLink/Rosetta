<!-- THIS IS AN AUTOGENERATED FILE: Don't edit it directly, instead change the schema definition in the code itself. -->

_Autogenerated Tag Syntax Documentation:_

---
scale density map intensities to match a pose's

```xml
<ScaleMapIntensities name="(&string;)" res_low="(&real;)" res_high="(&real;)"
        b_sharpen="(&real;)" truncate_only="(&real;)" fade_width="(&real;)"
        mask="(&bool;)" mask_output="(&bool;)" asymm_only="(&bool;)"
        ignore_bs="(&bool;)" bin_squared="(&bool;)"
        nresbins="(&non_negative_integer;)" outmap="(&string;)" />
```

-   **res_low**: Low resolution cut off (Angstroms), default=10000
-   **res_high**: High resolution cut off (Angstroms), default=0
-   **b_sharpen**: XRW TO DO, default=0.0
-   **truncate_only**: XRW TO DO, default=false
-   **fade_width**: XRW TO DO, default=0.1
-   **mask**: XRW TO DO, default=true
-   **mask_output**: XRW TO DO, default=false
-   **asymm_only**: XRW TO DO, default=false
-   **ignore_bs**: XRW TO DO, default=false
-   **bin_squared**: XRW TO DO,default=false
-   **nresbins**: XRW TO DO, default=50
-   **outmap**: File name for output map.

---
