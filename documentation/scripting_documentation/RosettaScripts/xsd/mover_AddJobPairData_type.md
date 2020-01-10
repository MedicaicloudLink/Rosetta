<!-- THIS IS AN AUTOGENERATED FILE: Don't edit it directly, instead change the schema definition in the code itself. -->

_Autogenerated Tag Syntax Documentation:_

---
Adds either a String/Real or String/String pair to the JD2 Job for output

```xml
<AddJobPairData name="(&string;)" value_type="(&value_type_type;)"
        key="(&string;)" value="(&string;)" value_from_ligand_chain="(&char;)" />
```

-   **value_type**: (REQUIRED) type of value to add; must be 'string' or 'real'
-   **key**: (REQUIRED) the string name for this item
-   **value**: Value to report; Of 'value' and 'value_from_ligand_chain', you must specify exactly one.
-   **value_from_ligand_chain**: This should be a single letter describing the chain to get a value from.  It queries using the key.Of 'value' and 'value_from_ligand_chain', you must specify exactly one.

---