<!-- THIS IS AN AUTOGENERATED FILE: Don't edit it directly, instead change the schema definition in the code itself. -->

_Autogenerated Tag Syntax Documentation:_

---
Author: Jared Adolf-Bryfogle (jadolfbr@gmail.com)
Applies the equivalent of a resfile line (without the resnums) to residues specified in a residue selector.

```xml
<ResfileCommandOperation name="(&string;)" command="(&string;)"
        residue_selector="(&string;)" />
```

-   **command**: @brief A resfile command string without any numbers in the front.
Example:
 POLAR
 PIKAA X[R2]X[T6]X[OP5]
-   **residue_selector**: The name of the already defined ResidueSelector that will be used by this object

---