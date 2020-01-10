#Fixed backbone design application, "fixbb", with the 'hpatch' solubility score term

Metadata
========

This document was last edited on June 22, 2011 by Ron Jacak. The code was written by Ron Jacak and Andrew Leaver-Fay. The corresponding PI is Brian Kuhlman (bkuhlman@email.unc.edu).

Code and Demo
=============

The hpatch score is meant to be used primarily with fixed backbone design, using the [[fixbb application|fixbb]] . See `       rosetta/main/tests/integration/tests/fixbb/      ` for an example of a fixed backbone design simulation and all the necessary input files. To activate the hpatch score during a fixed backbone design simulation, one additional flag needs to be added on the command line (described below).

Because the hpatch score is a nonpairwise-decomposable score term, the logic for evaluating the score is contained inside InteractionGraph classes. These classes serve two purposes: they hold the rotamer-pair energies that are used during sequence optimization, and they evaluate the non-pairwise decomposable score terms. Most of the code is contained in `       rosetta/main/source/src/core/pack/interaction_graph/      ` . The hpatch score can be optimized using any application that performs design, but the most common way to use the score is via the fixbb application. The [[fixbb application|fixbb]] lives in `       rosetta/main/source/src/apps/public/design/fixbb.cc      ` and invokes the `       PackRotamersMover      ` that lives in `       rosetta/main/source/src/protocols/moves/PackRotamersMover.cc      `

References
==========

We recommend the following articles for further studies of RosettaDesign methodology and applications:

-   [Computational Protein Design with Explicit Consideration of Surface Hydrophobic Patches. R. Jacak, A. Leaver-Fay, and B. Kuhlman.](https://www.ncbi.nlm.nih.gov/pubmed/22223219)
-   [Design of a novel globular protein fold with atomic-level accuracy.](http://www.ncbi.nlm.nih.gov/pubmed/14631033) Kuhlman B, et al.Science. 2003 Nov 21;302(5649):1364-8.
-   [An Adaptive Dynamic Programming Algorithm for The Side Chain Placement Problem.](http://www.ncbi.nlm.nih.gov/pubmed/15759610) A. Leaver-Fay, B. Kuhlman, and J.S. Snoeyink. In PSB 2005. 17-28.

Application purpose
===========================================

The surfaces of most native proteins do not expose large amounts of hydrophobic surface area even though, generally, about 30% of the surface-exposed residues are hydrophobic. Proteins designed with the current Rosetta energy function do tend to expose large amounts of hydrophobic surface area, despite having a native-like percentage of hydrophobic amino acids for the surface positions. The purpose of the hpatch score is to prevent the formation of these surface hydrophobic patches during design simulations.

Algorithm
=========

Protein design simulations generally start by reading in a set of backbone coordinates and building conformations, or rotamers, of all possible amino acids at evey changeable position. Rosetta makes use of the Dunbrack backbone-dependent rotamer library when making rotamers. Once rotamers have been built for the desired structure, sequence optimization can begin. This phase of design usually begins with the calculation of rotamer-pair energies (RPE) - interaction energies between all possible rotamer pairs. These energies are stored in large tables in a graph data structure. Once these energies are computed, the Rosetta search algorithm tries millions of substitutions and rapidly calculates the change in energy for every substitution using the stored RPEs. The conformation with the best/lowest energy is output as the final design model at the end of the simulation.

The hpatch score term works by penalizing substitutions that create large hydrophobic patches on the surface during the sequence optimization phase of design. In addition to storing the RPEs, the hpatch interaction graph also keeps track of the solvent accessible surface area (SASA) of every atom in the structure. For every substitution considered, the hpatch score calculates what the change in every atom's SASA would be if that substitution were accepted. The score then maps all exposed, hydrophobic atoms to another graph, and places edges between nodes whose atoms have solvent-accessible surface area overlap. The union-find algorithm is run on this graph to find the connected components (basically, connected subgraphs). The connected components represent the surface hydrophobic patches on the protein. Each patch is assigned a score based on its area. The change in hpatch score can be combined with the change in score for all the pairwise-decomposable score terms (calculated by the InteractionGraph) to obtain a change in overall score for a given substitution. That change is evaluated with the Metropolis criterion to determine whether to accept or reject the substitution being considered.

The hpatch score is compatible with all of the same options that can be used with the standard fixbb application. For example, a "resfile" (which restricts certain positions from being designed) can be used with the hpatch score. All of the compatible options are relisted below.

Limitations
===========

Protein stability and solubility are competing states. Studies have shown that substituting partially buried polar residues on the surface of a protein with hydrophobic residues can lead to increases in stability [Dantas-JMB-2007]. However, other studies have shown that the rate of protein aggregation is proportional to how much hydrophobic surface area is present [Chiti-Nature-2003]. The hpatch score tries to improve protein solubility by preventing the formation of hydrophobic patches, but this may be at the expense of some protein stability.

The same limitations that are present for the fixbb application apply to design with the hpatch score. These conditions are restated here: 1) fixed backbone design does not sample backbone conformation space and, therefore, cannot guarantee that the new sequence will fold into the desired backbone conformation; 2) since the underlying side-chain optimization problem is NP-Hard, and our solution to it is stochastic, the output from separate `       fixbb      ` jobs will likely vary; 3) the score function is imperfect such that even the best-scoring designs might not express or fold experimentally.

Input Files
===========

Structures - REQUIRED
---------------------

At least one input PDB file must always be given. A single PDB, or a list of PDBs, must be specified on the command line. The hpatch score is compatible with PDBs containing ligands and/or non-natural amino acids.

```
-s <pdb1> <pdb2>                   A list of one or more PDBs to run fixbb upon
-l <listfile>                      A file that lists one or more PDBs to run fixbb upon, one PDB per line
```

The fixbb application will apply the PackRotamersMover to each of the input structures given.

Resfile
-------

A single resfile may be specified on the command line with the resfile flag. This resfile will be used for all structures that are input. If a resfile cannot be applied to all the structures, then the job should be split up. The resfile format is described at [[Resfile syntax and conventions|resfiles]] .

```
-resfile <fname>                  The resfile that is to be used for this job
```

Options
=======

Database location - REQUIRED
----------------------------

```
-database <path/to/rosetta/main/database>     Specifies the location of the rosetta_database
```

Weights file - REQUIRED
-----------------------

The hpatch score is activated by giving the term a nonzero weight in the energy function. The score:weights flag can be used to specify a different set of weights for a simulation. By default, the option will look for weights files in the database directory rosetta/main/database/scoring/weights/ or in the current directory. The file design\_hpatch.wts is distributed with Rosetta and has the standard Rosetta energy function weights with the hpatch score turned on and reweighted reference energies.

```
-score:weights design_hpatch.wts  This flag instructs the application to use the weights specified in the database file 'design_hpatch.wts'
```

**IMPORTANT: If the -score:weights flag is not used, then "score12" will be used by default. The hpatch score WILL NOT BE USED.**

Interaction Graph
-----------------

(Default: Precompute all rotamer pair energies)

```
-linmem_ig 10                     Activate the linear-memory interaction graph [Leaver-Fay et al. 2008]
```

Rotamers
--------

```
-ex1                              Increase chi1 rotamer sampling for buried* residues +/- 1 standard deviation - RECOMMENDED
-ex1_aro                          Increase chi1 rotamer sampling for buried* aromatic** residues +/- 1 standard deviation
-ex2                              Increase chi2 rotamer sampling for buried* residues +/- 1 standard deviation - RECOMMENDED
-ex2_aro                          Increase chi2 rotamer sampling for buried* aromatic** residues +/- 1 standard deviation
-ex3                              Increase chi3 rotamer sampling for buried* residues +/- 1 standard deviation
-ex4                              Increase chi4 rotamer sampling for buried* residues +/- 1 standard deviation

-ex1:level <int>                  Increase chi1 sampling for buried* residues to the given sampling level***
-ex1_aro:level <int>              Increase chi1 sampling for buried* aromatic residues to the given sampling level
-ex2:level <int>                  Increase chi1 sampling for buried* residues to the given sampling level
-ex2_aro:level <int>              Increase chi1 sampling for buried* aromatic residues to the given sampling level
-ex3:level <int>                  Increase chi1 sampling for buried* residues to the given sampling level
-ex4:level <int>                  Increase chi1 sampling for buried* residues to the given sampling level

-extrachi_cutoff <int>            Set the number of Cbeta neighbors (counting its own) at which a residue is considered buried.
                                  A value of "1" will mean that all residues are considered buried for the purpose of rotamer building.
                                  Use this option when you want to use extra rotamers for less buried positions.

-preserve_input_cb                Do not idealize the CA-CB bond vector -- instead, use the CB coordinates of the input PDB
-use_input_sc                     Include the side chain from the input PDB.  Default: false
                                  Including the input sidechain is "cheating" if your goal is to measure sequence recovery,
                                  but a good idea if your goal is to eventually synthesize the designed sequence

-no_his_his_pairE                 Exclude the favorable pair term energy for HIS-HIS residue pairs - RECOMMENDED
```

\*   Buried residues are those with \>= threshold (default: 18) neighbors within 10 Angstroms (Cbeta-distance). This threshold can be controlled by the -extrachi\_cutoff flag.

\*\* Aromatic residues are HIS, TYR, TRP, and PHE. Note: Including both -ex1 and -ex1\_aro does not increase the sampling for aromatic residues any more than including only the -ex1 flag. If however, both -ex1 and -ex1\_aro:level 4 are included on the command line, then aromatic residues will have more chi1 rotamer samples than non aromatic residues. Note also that -ex1\_aro can *only increase* the sampling for aromatic residues beyond that for non-aromatic residues. -ex1:level 4 and -ex1\_aro:level 1 together will have the same effect as -ex1:level 4 alone.

\*\*\* More information about [[extra rotamer sampling levels|resfiles#Extra-Rotamer-Commands:]], including recommended values, can be found on the [[resfile documentation page|resfiles]] .

Because of the large number of metal-coordinating structures in the PDB, the pair energy term, which is meant to favor electrostatic interactions, gives a bonus to HIS-HIS residue pairs. Adding the -no\_his\_his\_pairE flag removes the bonus given to HIS-HIS residue pairs and reduces the number of designed histidines.

Annealer
--------

By default, fixbb uses the standard annealer used in [Kuhlman et al. Science, 2003].

```
-multi_cool_annealer <int>        Use an alternate annealer that spends more time at low temperatures. This annealer produces
                                  consistently lower energies than the standard annealer. Recommended value: 10.
```

Other options
-------------

```
-nstruct <int>                    The number of iterations to perform per input structure; e.g. with 10 input structures and an -nstruct
                                  of 10, 100 trajectories will be performed. Default: 1.
-overwrite                        Overwrite the output files, if they already exist. Not used by default.

-minimize_sidechains              Follow the packing phase with gradient-based minimization of the sidechains for residues that were either
                                  repacked or designed in the packing phase. Not used by default.
-min_type <string>                When combined with the -minimize_sidechains flag, specifies the line-search algorithm to use in the
                                  gradient-based minimization . "dfpmin" by default.

-constant_seed                    Fix the random seed
-jran <int>                       Specify the random seed; if unspecified, and -constant_seed appears on the command line, then the seed 11111111 will be used
```

Tips
====

-   The flags -ex1 and -ex2 are recommended to get well-packed output structures.
-   A two-line resfile where the first line is "NATAA" and the second line is "start" is sufficient to say "only repack the input sequence". This optimizes only the input structures side-chain rotamers, and leaves the sequence the same.
-   If you are redesigning with many rotamers (\>6K), then using the linear-memory interaction graph will save both time and memory. Add the flag "-linmem\_ig 10" to activate the linear-memory interaction graph.

Expected Outputs
================

The structure from the end of the execution will be written out to a .pdb file, and the score for that structure will be written to the score file, score.sc. The .pdb file will be named with the input file + job number.pdb. For example, "-s 1ubq.pdb" produces the output file 1ubq\_0001.pdb. Because the `       fixbb      ` application uses the [[JD2 job_distributor|jd2]] , it respects all of the jd2 flags that control output.

Post Processing
===============

The [[fixbb application|fixbb]] is commonly used for sequence-recovery tests; the [[sequence_recovery|sequence-recovery]] application can be used to measure the sequence recovery between a native and a redesigned protein.

New things since last release
=============================

The hpatch score is being released for the first time with Rosetta v3.3.


##See Also

* [[Fixbb | fixbb ]]: More documentation for fixbb
* [[Design applications | design-applications]]: other design applications
* [[Application Documentation]]: Application documentation home page
* [[Running Rosetta with options]]: Instructions for running Rosetta executables.
* [[Analyzing Results]]: Tips for analyzing results generated using Rosetta
* [[Rosetta on different scales]]: Guidelines for how to scale your Rosetta runs
* [[Preparing structures]]: How to prepare structures for use in Rosetta
