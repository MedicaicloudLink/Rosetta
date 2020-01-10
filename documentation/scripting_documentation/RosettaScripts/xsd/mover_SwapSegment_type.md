<!-- THIS IS AN AUTOGENERATED FILE: Don't edit it directly, instead change the schema definition in the code itself. -->

_Autogenerated Tag Syntax Documentation:_

---
XRW TO DO

```xml
<SwapSegment name="(&string;)" scorefxn="(&string;)"
        copy_sidechains="(1 &bool;)" swap_segment="(0 &bool;)"
        swap_chain="(0 &non_negative_integer;)"
        from_chain="(1 &non_negative_integer;)"
        to_chain="(1 &non_negative_integer;)" seeds_pdb="(&string;)"
        template_pdb="(&string;)" previously_grown="(0 &bool;)" >
    <Seeds begin="(&string;)" end="(&string;)" />
</SwapSegment>
```

-   **scorefxn**: Name of score function to use
-   **copy_sidechains**: XRW TO DO
-   **swap_segment**: XRW TO DO
-   **swap_chain**: XRW TO DO
-   **from_chain**: XRW TO DO
-   **to_chain**: XRW TO DO
-   **seeds_pdb**: XRW TO DO, required if template_pdb is not specified.
-   **template_pdb**: XRW TO DO, required if seeds_pdb is not specified.
-   **previously_grown**: XRW TO DO


Subtag **Seeds**:   

-   **begin**: (REQUIRED) XRW TO DO
-   **end**: (REQUIRED) XRW TO DO

---