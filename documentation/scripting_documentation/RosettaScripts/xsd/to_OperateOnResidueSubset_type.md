<!-- THIS IS AN AUTOGENERATED FILE: Don't edit it directly, instead change the schema definition in the code itself. -->

_Autogenerated Tag Syntax Documentation:_

---
OperateOnResidueSubset is a TaskOperation that applies one or more ResLevelTaskOperations to the residues indicated by a ResidueSelector. The ResidueSelector can either be previously defined, or constructed as a subtag beneath the main tag, or can be composed using boolean AND, OR, and ! (not) operations using the selector_logic attribute.

```xml
<OperateOnResidueSubset name="(&string;)" selector="(&string;)"
        selector_logic="(&string;)" residue_level_operations="(&string;)" >
    <Residue Selector Tag ... />
    <ResidueLevelTaskOperation Tag ... />
</OperateOnResidueSubset>
```

-   **selector**: Residue selector that indicates to which residues the operation will be applied.
-   **selector_logic**: Logically combine already-delcared ResidueSelectors using boolean AND, OR, and ! (not) operators. As convnetional, ! (not) has the highest precedence, then AND, then OR. Parentheses may be used to group operations together.
-   **residue_level_operations**: A comma-separated list of residue-level-task operations that will be retrieved from the DataMap.


"Residue Selector Tag": Any of the [[ResidueSelectors]]

"ResidueLevelTaskOperation Tag": Any of the [[Residue Level TaskOperations]]

---