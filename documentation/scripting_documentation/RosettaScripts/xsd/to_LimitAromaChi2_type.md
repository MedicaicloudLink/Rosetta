<!-- THIS IS AN AUTOGENERATED FILE: Don't edit it directly, instead change the schema definition in the code itself. -->

_Autogenerated Tag Syntax Documentation:_

---
Prevent to use the rotamers of PHE, TYR and HIS that have chi2 far from 90.

```xml
<LimitAromaChi2 name="(&string;)" chi2max="(110.0 &real;)"
        chi2min="(70.0 &real;)" include_trp="(false &bool;)" />
```

-   **chi2max**: max value of chi2 to be used
-   **chi2min**: min value of chi2 to be used
-   **include_trp**: also impose chi2 limits for tryptophans

---
