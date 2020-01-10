<!-- THIS IS AN AUTOGENERATED FILE: Don't edit it directly, instead change the schema definition in the code itself. -->

_Autogenerated Tag Syntax Documentation:_

---
Add composition constraints from the provided file to the selected region

```xml
<AddCompositionConstraintMover name="(&string;)" filename="(&string;)"
        selector="(&string;)" >
    <Comp entry="(&string;)" />
</AddCompositionConstraintMover>
```

-   **filename**: Name of composition constraint file
-   **selector**: Residue selector named somewhere else in the script


Subtag **Comp**:   

-   **entry**: (REQUIRED) Composition constraint entries, optionally ';' separated.

---