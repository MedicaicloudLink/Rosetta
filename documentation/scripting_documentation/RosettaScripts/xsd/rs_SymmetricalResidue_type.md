<!-- THIS IS AN AUTOGENERATED FILE: Don't edit it directly, instead change the schema definition in the code itself. -->

_Autogenerated Tag Syntax Documentation:_

---
The SymmetricalResidueSelector, when given a selector, will return all symmetrical copies (including the original selection) of those residues. While the packer is symmetry aware, not all filters are. This selector is useful when you need to explicitly give residue numbers but you are not sure which symmetry subunit you need.

```xml
<SymmetricalResidue name="(&string;)" selector="(&string;)" >
    <Residue Selector Tag ... />
</SymmetricalResidue>
```

-   **selector**: name of the selector


"Residue Selector Tag": Any of the [[ResidueSelectors]]

---
