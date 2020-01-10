<!-- THIS IS AN AUTOGENERATED FILE: Don't edit it directly, instead change the schema definition in the code itself. -->

_Autogenerated Tag Syntax Documentation:_

---
The LongestContinuousPolarSegment filter counts the number of amino acid residues in the longest continuous stretch of polar amino acids in any chain of your pose (or, optionally, in a residue selection).  Optionally, it can ignore the polar stretches at the N- and C-termini of chains.

```xml
<LongestContinuousPolarSegment name="(&string;)"
        exclude_chain_termini="(true &bool;)" count_gly_as_polar="(true &bool;)"
        filter_out_high="(true &bool;)" cutoff="(5 &non_negative_integer;)"
        residue_selector="(&string;)" confidence="(1.0 &real;)" />
```

-   **exclude_chain_termini**: If true, polar stretches at the ends of chains are not counted.  If false, they are.  True by default.  Note that if this is set to true, an entirely polar chain will not be counted.
-   **count_gly_as_polar**: If true, glycine is considered "polar" for purposes of this filter.  True by default.
-   **filter_out_high**: If true, poses with more than the cutoff number of residues in the longest polar stretch will be rejected.  If false, poses with fewer than the cutoff number of residues in the longest polar stretch will be rejected.  True by default.
-   **cutoff**: The maximum (or minimum, if "filter_out_high" is set to "false") number of residues in the longest polar stretch that will still allow the pose to pass this filter.  Default 5.
-   **residue_selector**: An optional, previously-defined residue selector.  If provided, the filter will only consider stretches of polar residues that have at least one residue in the selection.  Not used if not specified.
-   **confidence**: Probability that the pose will be filtered out if it does not pass this Filter

---