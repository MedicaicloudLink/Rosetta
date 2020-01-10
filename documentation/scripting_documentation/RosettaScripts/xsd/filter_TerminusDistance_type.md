<!-- THIS IS AN AUTOGENERATED FILE: Don't edit it directly, instead change the schema definition in the code itself. -->

_Autogenerated Tag Syntax Documentation:_

---
True, if all residues in the interface are more than n residues from the N or C terminus --i.e. distance in sequence space. If fails, reports how far failing residue was from the terminus. If passes, returns '1000'.

```xml
<TerminusDistance name="(&string;)" jump_number="(1 &positive_integer;)"
        distance="(5 &non_negative_integer;)" confidence="(1.0 &real;)" />
```

-   **jump_number**: Which jump to use for calculating the interface?
-   **distance**: How many residues must each interface residue be from a terminus in sequence distance?
-   **confidence**: Probability that the pose will be filtered out if it does not pass this Filter

---