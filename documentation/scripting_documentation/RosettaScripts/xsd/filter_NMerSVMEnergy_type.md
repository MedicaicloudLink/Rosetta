<!-- THIS IS AN AUTOGENERATED FILE: Don't edit it directly, instead change the schema definition in the code itself. -->

_Autogenerated Tag Syntax Documentation:_

---
XRW TO DO

```xml
<NMerSVMEnergy name="(&string;)" threshold="(&real;)" svm_fname="(&string;)"
        svm_list_fname="(&string;)" resnums="(XRW TO DO &string;)"
        nmer_length="(9 &non_negative_integer;)"
        nmer_svm_scorecut="(0.0 &real;)" gate_svm_scores="(false &bool;)"
        dump_table="(false &bool;)" use_rank_as_score="(false &bool;)"
        count_eps="(false &bool;)" ep_cutoff="(0.425 &real;)"
        confidence="(1.0 &real;)" />
```

-   **threshold**: (REQUIRED) cutoff, fail if score greater than cutoff
-   **svm_fname**: single name of a libsvm .model file
-   **svm_list_fname**: filename to a list of svm .model filenames
-   **resnums**: optional subset of residue numbers over which to do calculation
-   **nmer_length**: number of residues in core subsequence motif
-   **nmer_svm_scorecut**: optional lower bound floor to returned scores
-   **gate_svm_scores**: option to use lower bound floor for svm scores
-   **dump_table**: dump a table of all nmer v. svm model scores
-   **use_rank_as_score**: report normalized rank fraction instead of raw energy
-   **count_eps**: report number of epitopes w score greater than cutoff
-   **ep_cutoff**: optional epitope score cutiff for boolean count return option
-   **confidence**: Probability that the pose will be filtered out if it does not pass this Filter

---