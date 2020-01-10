<!-- THIS IS AN AUTOGENERATED FILE: Don't edit it directly, instead change the schema definition in the code itself. -->

_Autogenerated Tag Syntax Documentation:_

---
Convert pose into poly XXX ( XXX can be any amino acid )

```xml
<MakePolyX name="(&string;)" aa="(ALA &string;)" keep_pro="(false &bool;)"
        keep_gly="(true &bool;)" keep_disulfide_cys="(false &bool;)"
        residue_selector="(&string;)" />
```

-   **aa**: using amino acid type for converting
-   **keep_pro**: Pro is not converted to XXX
-   **keep_gly**: Gly is not converted to XXX
-   **keep_disulfide_cys**: disulfide CYS is not converted to XXX
-   **residue_selector**: The name of the already defined ResidueSelector that will be used by this object

---