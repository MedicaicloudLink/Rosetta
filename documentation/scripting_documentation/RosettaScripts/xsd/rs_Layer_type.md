<!-- THIS IS AN AUTOGENERATED FILE: Don't edit it directly, instead change the schema definition in the code itself. -->

_Autogenerated Tag Syntax Documentation:_

---
XRW TO DO

```xml
<Layer name="(&string;)" select_core="(false &bool;)"
        select_boundary="(false &bool;)" select_surface="(false &bool;)"
        cache_selection="(false &bool;)" use_sidechain_neighbors="(true &bool;)"
        ball_radius="(&real;)" sc_neighbor_dist_midpoint="(&real;)"
        sc_neighbor_denominator="(&real;)"
        sc_neighbor_angle_shift_factor="(&real;)"
        sc_neighbor_angle_exponent="(&real;)"
        sc_neighbor_dist_exponent="(&real;)" core_cutoff="(&real;)"
        surface_cutoff="(&real;)" />
```

-   **select_core**: Should the core (buried) layer be selected?
-   **select_boundary**: Should the boundary (semi-buried) layer be selected?
-   **select_surface**: Should the surface (exposed) layer be selected?
-   **cache_selection**: Should the selection be stored, so that it needn't be recomputed?
-   **use_sidechain_neighbors**: If true (the default), then the sidechain neighbour algorithm is used to determine burial.  If false, burial is based on SASA (solvent-accessible surface area) calculations.
-   **ball_radius**: The radius value for the rolling ball algorithm used for SASA (solvent-accessible surface area) calculations.  Only used if use_sidechain_neighbors=false.
-   **sc_neighbor_dist_midpoint**: The midpoint of the distance-dependent sigmoidal falloff for the sidechain-neighbours algorithm.  Only used if use_sidechain_neighbors=true.
-   **sc_neighbor_denominator**: XRW TO DO
-   **sc_neighbor_angle_shift_factor**: XRW TO DO
-   **sc_neighbor_angle_exponent**: XRW TO DO
-   **sc_neighbor_dist_exponent**: XRW TO DO
-   **core_cutoff**: XRW TO DO
-   **surface_cutoff**: XRW TO DO

---
