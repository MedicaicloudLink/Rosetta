<!-- THIS IS AN AUTOGENERATED FILE: Don't edit it directly, instead change the schema definition in the code itself. -->

_Autogenerated Tag Syntax Documentation:_

---
Add ResidueTypeLinkingConstraints to the pose such that a symmetric sequence (CATCATCAT)will be favored during design. You should add this mover before sequence design is performed.

```xml
<FavorSymmetricSequence name="(&string;)" penalty="(&real;)"
        symmetric_units="(&positive_integer;)" />
```

-   **penalty**: (REQUIRED) Penalty applied to a pair of asymmetric residues
-   **symmetric_units**: (REQUIRED) Number of symmetric units in the sequence. It should be a value of 2 or greater

---