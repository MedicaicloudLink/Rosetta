GlycanRelax
===========

[[_TOC_]]

MetaData
========

Application created by Dr. Jared Adolf-Bryfogle (jadolfbr@gmail.com) and Dr. Jason Labonte (JWLabonte@jhu.edu), in collaboration with Dr. Sebastian Rämisch (raemisch@scripps.edu)

PIs: Dr. William Schief (schief@scripps.edu) and Dr. Jeffrey Gray (jgray@jhu.edu)


Description
===========

App: glycan_relax


Glycan Relax aims to sample potential conformational states of carbohydrates, either attached to a protein or free.  It is extremely fast.  Currently, it uses a few strategies to do so, using statistics from various papers, the CHI (CarboHydrate Intrinsic) energy term, and a new framework for backbone dihedral sampling. Conformer statistics adapted from Schief lab Glycan Relax app, originally used/written by Yih-En Andrew Ban.

It can model glycans from ideal geometry, or refine already-modeling glycans.

See [[Working With Glycans | WorkingWithGlycans ]] for more.



## See Also
* [[WorkingWithGlycans]]

- ### Apps
* [[GlycanInfo]] - Get information on all glycan trees within a pose
* [[GlycanClashCheck]] - Obtain data on model clashes with and between glycans, or between glycans and other protein chains.

- ### RosettaScript Components
* [[GlycanRelaxMover]] - Model glycan trees using known carbohydrate information.  Works for full denovo modeling or refinement.
* [[SimpleGlycosylateMover]] - Glycosylate poses with glycan trees.  
* [[GlycanTreeSelector]] - Select individual glcyan trees or all of them
* [[GlycanResidueSelector]] - Select specific residues of each glycan tree of interest.

- ### Other
* [[Application Documentation]]: List of Rosetta applications
* [[Running Rosetta with options]]: Instructions for running Rosetta executables.
* [[Comparing structures]]: Essay on comparing structures
* [[Analyzing Results]]: Tips for analyzing results generated using Rosetta
* [[Solving a Biological Problem]]: Guide to approaching biological problems using Rosetta
* [[Commands collection]]: A list of example command lines for running Rosetta executable files