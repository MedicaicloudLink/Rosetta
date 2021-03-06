<!-- THIS IS AN AUTOGENERATED FILE: Don't edit it directly, instead change the schema definition in the code itself. -->

_Autogenerated Tag Syntax Documentation:_

---
Author: Jared Adolf-Bryfogle (jadolfbr@gmail.com)
A metric for measuring the short and long range interaction energy between residues using two sets of residue selectors. Will use the current energies of the pose, unless force_rescore option is set.

```xml
<InteractionEnergyMetric name="(&string;)" custom_type="(&string;)"
        scoretypes_only="(&string;)" scoretypes_skip="(&string;)"
        include_rama_prepro_and_proclose="(false &bool;)"
        force_rescore="(false &bool;)" residue_selector="(&string;)"
        residue_selector2="(&string;)" scorefxn="(&string;)" />
```

-   **custom_type**: Allows multiple configured SimpleMetrics of a single type to be called in a single RunSimpleMetrics and SimpleMetricFeatures. 
 The custom_type name will be added to the data tag in the scorefile or features database.
-   **scoretypes_only**: Include only this list of ScoreTypes
-   **scoretypes_skip**: Always skip these score types
-   **include_rama_prepro_and_proclose**: Include rama_prepro energy term AND pro_close? (Which are two-body backbone score terms)
-   **force_rescore**: Should we rescore the pose, even if it has an energies object?  This will force a pose copy and rescore and will not be as efficent as having a scored pose
-   **residue_selector**: Selector specifying first set of residues.
-   **residue_selector2**: Selector specifying second set of residues. Default is TrueResidueSelector
-   **scorefxn**: Name of score function to use

---
