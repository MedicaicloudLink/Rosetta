<!-- THIS IS AN AUTOGENERATED FILE: Don't edit it directly, instead change the schema definition in the code itself. -->

_Autogenerated Tag Syntax Documentation:_

---
This filter counts the number of residues that form sufficiently energetically favorable H-bonds to a selected residue

```xml
<HbondsToResidue name="(&string;)" partners="(&non_negative_integer;)"
        energy_cutoff="(-0.5 &real;)" bb_bb="(0 &bool;)" backbone="(0 &bool;)"
        sidechain="(1 &bool;)" residue="(&refpose_enabled_residue_number;)"
        scorefxn="(&string;)" residue_selector="(&string;)"
        from_same_chain="(true &bool;)" from_other_chains="(true &bool;)"
        confidence="(1.0 &real;)" />
```

-   **partners**: H-bonding partner threshold for passing
-   **energy_cutoff**: Energy below which a H-bond counts
-   **bb_bb**: Count backbone-backbone H-bonds
-   **backbone**: Count backbone H-bonds
-   **sidechain**: Count sidechain H-bonds
-   **residue**: Particular residue of interest
-   **scorefxn**: Name of score function to use
-   **residue_selector**: The name of the already defined ResidueSelector that will be used by this object
-   **from_same_chain**: Count residues from the same chain in H-bonds
-   **from_other_chains**: Count residues from the other chains in H-bonds
-   **confidence**: Probability that the pose will be filtered out if it does not pass this Filter

---