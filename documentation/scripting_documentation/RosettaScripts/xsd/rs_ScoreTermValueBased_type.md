<!-- THIS IS AN AUTOGENERATED FILE: Don't edit it directly, instead change the schema definition in the code itself. -->

_Autogenerated Tag Syntax Documentation:_

---
Selects residues based on per residue score of the given score_type.

```xml
<ScoreTermValueBased name="(&string;)" selector="(&string;)"
        resnums="(ALL &string;)" lower_threshold="(&real;)"
        upper_threshold="(&real;)" score_type="(&string;)"
        score_fxn="(&string;)" >
    <Residue Selector Tag ... />
</ScoreTermValueBased>
```

-   **selector**: residue selector to be used, in case only a subset of residues need to be considered
-   **resnums**: pose number of the subset of residues to be considered. The default is 'ALL'.
-   **lower_threshold**: (REQUIRED) residues scoring equal to or above this threshold will be selected
-   **upper_threshold**: (REQUIRED) residues scoring equal to or below this threshold will be selected
-   **score_type**: (REQUIRED) the score type to be used for selection
-   **score_fxn**: (REQUIRED) the score function to be used for evaluation


"Residue Selector Tag": Any of the [[ResidueSelectors]]

---
