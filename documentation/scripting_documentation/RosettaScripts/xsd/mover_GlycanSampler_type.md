<!-- THIS IS AN AUTOGENERATED FILE: Don't edit it directly, instead change the schema definition in the code itself. -->

_Autogenerated Tag Syntax Documentation:_

---
Authors: Jared Adolf-Bryfogle (jadolfbr@gmail.com) and Jason Labonte (WLabonte@jhu.edu)
Main mover for Glycan Relax, which optimizes glycans in a pose. Each round optimizes either one residue for BB sampling, linkage, or a [part of a ] branch for minimization and packing.Minimization and packing work by default by selecting a random glycan residue from any set movemap or all of them and then selecting the rest of the downstream branch.  Those residues are then minimized or packed.  Packing includes a neighbor packing shell. Currently uses a random sampler with a set of weights to each mover for sampling.

```xml
<GlycanSampler name="(&string;)" kt="(&real;)" rounds="(&non_negative_integer;)"
        final_min="(true &bool;)" pymol_movie="(false &bool;)"
        refine="(false &bool;)" pack_distance="(&real;)"
        cartmin="(false &bool;)" tree_based_min_pack="(true &bool;)"
        use_conformer_probs="(false &bool;)"
        use_gaussian_sampling="(false &bool;)" min_rings="(false &bool;)"
        shear="(false &bool;)" randomize_torsions="(true &bool;)"
        match_sampling_of_modeler="(0 &bool;)"
        inner_bb_cycles="(0 &non_negative_integer;)"
        task_operations="(&task_operation_comma_separated_list;)"
        packer_palette="(&named_packer_palette;)" scorefxn="(&string;)"
        residue_selector="(&string;)" />
```

-   **kt**: Temperature for metropolis criterion
-   **rounds**: Number of relax rounds to perform.  Will be multiplied by the number of glycan residues.
-   **final_min**: Perform a final minimization
-   **pymol_movie**: Output a PyMOL movie of the run
-   **refine**: Do not start with a random glycan conformation.
-   **pack_distance**: Neighbor distance for packing
-   **cartmin**: Use Cartesian Minimization instead of Dihedral Minimization during packing steps.
-   **tree_based_min_pack**: Use Tree-based minimization and packing instead of minimizing and packing ALL residues each time we min.  Significantly impacts runtime.  If you are seeing crappy structures for a few sugars, turn this off.  This is default-on to decrease runtime for a large number of glycans.
-   **use_conformer_probs**: Use the populations of the conformers as probabilities during our linkage conformer sampling.  This makes it harder to overcome energy barriers with more-rare conformers!
-   **use_gaussian_sampling**: Set whether to build conformer torsions using a gaussian of the angle or through uniform sampling up to 1 SD (default)
-   **min_rings**: Minimize Carbohydrate Rings during minimization.
-   **shear**: Use the Shear Mover that is now compatible with glycans at an a probability of 10 percent
-   **randomize_torsions**: If NOT doing refinement, control whether we randomize torsions at the start, which helps to achieve low energy structures.
-   **match_sampling_of_modeler**: Option that matches the amount of sampling in a default GlycanTreeModeler run.  Used for benchmarking.
-   **inner_bb_cycles**: Inner cycles for each time we call BB sampling either through small/medium/large moves or through the SugarBB Sampler.  This is multiplied by the number of glycans.  Scales poorly with GlycanModeler.  If 0 (default), we run the protocol normally
-   **task_operations**: A comma-separated list of TaskOperations to use.
-   **packer_palette**: A previously-defined PackerPalette to use, which specifies the set of residue types with which to design (to be pruned with TaskOperations).
-   **scorefxn**: Name of score function to use
-   **residue_selector**: Residue Selector containing only glycan residues.  This is not needed, as this class will automatically select ALL glycan residues in the pose to model.  See the GlycanResidueSelector and the GlycanLayerSelector for control glycan selection.  Note that the ASN is not technically a glycan.  Since dihedral angles are defined for a sugar from the upper to lower residue, the dihedral angles between the first glycan and the ASN are defined by the first glycan.

---
