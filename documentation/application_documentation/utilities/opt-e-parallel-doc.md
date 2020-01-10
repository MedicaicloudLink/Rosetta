#The optE\_parallel application

Metadata
========

Author: Andrew Leaver-Fay

This document was written by Andrew Leaver-Fay in October 2013. The optE\_parallel code is maintained in the Kuhlman lab at UNC. Send questions to (aleaverfay@gmail.com).

Code and Demo
=============

The optE application lives in Rosetta/main/source/src/apps/public/weight\_optimization/optE\_parallel.cc which serves merely as a wrapper for the code in Rosetta/main/source/src/protocols/optimize\_weights/. In particular, the main driver class is IterativeOptEDriver, which is implemented in the .cc file with the same name.

A demo for the sequence-profile recovery procedure can be found in the Rosetta/demos/public/optE directory. This directory contains an "inputs" directory and a "run" directory. See the "launch\_optE.scr" csh script for an example of how to set up and launch an optE job.

References
==========

Andrew Leaver-Fay, Matthew J. O'Meara, Mike Tyka, Ron Jacak, Yifan Song, Elizabeth H. Kellogg, James Thompson, Ian W. Davis, Roland A. Pache, Sergey Lyskov, Jeffrey J. Gray, Tanja Kortemme, Jane S. Richardson, James J. Havranek, Jack Snoeyink, David Baker, Brian Kuhlman. (2013) "Scientific Benchmarks for Guiding Macromolecular Energy Function Improvement." Methods in Enzymology. Volume 523. Chapter 6. Pages 109-143.

Purpose
=======

The optE\_parallel application serves two purposes. a) To fit the reference energies to maximize sequence recovery and minimize the KL-Divergence of the amino-acid sequence profile by performing iterative rounds of (usually, complete-protein) redesign. b) To fit the weights themselves (as well as the reference energies) using a maximum-likelihood approach, where the weights are optimized to maximize the likelihood of various experimental observations (e.g. native sequences, native rotamers, ddGs of mutation). The first scenario produces high quality reference energies which, when fed into sequence-recovery benchmarks, yield high sequence recovery rates. The second scenario is not worth very much and is not recommended for use. Its shortcomings are documented extensively in the Methods In Enzymology paper referenced above.

Algorithm
=========

Sequence profile recovery proceedure
------------------------------------

Given an input set of PDB files, the reference energy optimization procedure performs iterative redesign on these PDB files, changing the reference energies at each round. After each round of redesign, optE alters the reference energies, increasing the reference energies for amino acids that appeared in the designed structures more than they appeared in the input structures, and decreasing the reference energies for amino acids that appeared in the designed structures less often than they appeared in the input structures. For this reason, this proceedure is called the "amino acid profile recovery" or "profile recovery" procedure. Optionally, a RosettaScripts XML file which gives a set of TaskOperations can be used to restrict design to a subset of the positions on the input structures. This can be used to optimize reference energies for specific regions of proteins, e.g., at protein-protein interfaces.

There are 20 amino acids for which reference energies are fit, but since the only important property of reference energies is the difference between them for two amino acids, there are effectively only 19 free parameters. For this reason, the reference energies may all be scaled upwards or downwards by a constant value without affecting their use in design. We chose to shift the reference energies so that they sum to 0.

The code operates in an inner and an outer loop. In the inner loop, the reference energies are adjusted upwards or downwards based on the results from the previous round of design, then all input proteins are redesigned. If you are running within MPI, then this step will be parallelized across the available processors. Once design has completed, the sequence recovery rate and the cross entropy (the KL-divergence + the entropy) is measured. A score built from the weighted sum SRR + w\*XE (SRR = sequence recovery rate, XE = cross entropy, w = -0.1) is computed for the inner loop. If this score is greater than the score from the previous iteration through the outer loop, then the inner loop exits. Whenever the inner loop exits (either because it has reached six iterations, or because the score has increased over the previous iteration through the outer loop) its weight set is writen to the weightdir/ directory, and its score is assigned as the score for the current iteration through the outer loop. Each weight file that is written to disk represents either a point at which the score improved, or a significant amount of effort has been expended. On weight file is output for each iteration in the outer loop. For the first two iterations of the outer loop, the inner loop proceeds through at least four iterations. Both the number of inner and outer-loop iterations may be controlled by the command line.

Maximum-likelihood weight and reference-energy optimization
-----------------------------------------------------------

OptE was originally designed to fit both weights and reference energies using an iterative procedure, and the structure of the reference-energy fitting routine takes its shape from the maximum-likelihood procedure. The structure of this procedure oscilates between objective-function optimization and complete-protein redesign. Each oscilation represents one iteration of the outer loop. (The "inner loop" describes the behavior of the complete-protein redesign phase). In the objective-function optimization phase, the weights and reference energies are optimized using a combination of particle-swarm and gradient-based minimization procedures. The nature of the objective function is described in detail in the Methods in Enzymology paper. The weight set that results from the objective-function-optimization phase is then mixed at varying levels with the weight set from the previous iteration through the outer loop, and the sequence-recovery rate for the resulting weight set is measured. This mixing procedure and the rationale behind it is described in greater detail in the Methods In Enzymology paper. The inner loop continues until the sequence recovery rate improves over the previous iteration through the outer loop, or until six iterations have completed.

When run in this manner, the user must provide a list of the weights that are to be held fixed (the fixed weights) and a list of the weights that are allowed to change (the free weights). OptE does not directly place any limitations on where the free and fixed weights may explore which can result in weights that get very large or that go negative. In particular, the weight on both the Ramachandran term (rama) and the repulsive component of the Lennard-Jones term (fa\_rep), when allowed to float, often go negative. To avoid this problem, the user may provide arbitrary flat-bottom harmonic constraints to apply to the weights, which will be included in the objective function.

Limitations
===========

Limitations of the reference-energy optimization procedure
----------------------------------------------------------

OptE continues iterating even after it has begun to converge. Sometimes the best weight set (the one with the highest score) appears after the 10th iteration through the outer loop, sometimes after the 6th iteration. The recommended weight set is the one with the highest score. The iteration can be deduced from the log file by looping for the last occurrence of the word "accepting."

Independent executions of optE will produce very similar reference energies, and these reference energies may produce similar sequence recovery rates, but the differences between these rates are probably statistically significant. That is, the stochastic sequence recovery benchmark is surprisingly precise so that the standard deviation is often less than 0.05%. On the other hand, the sequence recovery rates resulting from independent runs of optE will have a standard deviation of about 0.5%. Triplicate optE runs are recommended.

Surprisingly, it turns out that sampling strategies are closely connected with the set of reference energies you need. If you want to use the min-packer, then optE has to be run using the min packer. If you want to use the discrete packer with the -ex1 -ex2 flags, then optE has to be run using the discrete packer -ex1 -ex2. OptE will use the discrete packer by default (extra rotamer flags have to be provided on the command line) and has to be instructed to use the min-packer.

Limitations of the maximum-likelihood weight and reference-energy optimization procedure
----------------------------------------------------------------------------------------

Though the maximum-likelihood optimization procedure selects weight sets based solely on sequence recovery, and the reference-energy optimization procedure selects weights based on both sequence recovery and sequence profile recovery, the reference-energy optimization procedure yields (dramatically) higher sequence recovery rates.

Options
=======

Options for the sequence profile recovery optimization proceedure
-----------------------------------------------------------------

The following options are recommended for the reference energy fitting procedure:

-   `        -s </path/to/listfile>       ` : the listfile should contain the path to the set of PDB files on which sequence recovery will be performed
-   `        -optE:fit_reference_energies_to_aa_profile_recovery       ` : activate the sequence profile recovery procedure.
-   `        -optE::optimize_nat_aa       ` : a holdover from the maximum-likelihood protocol, required, but doesn't do anything in particular.
-   `        -mute all       ` : disable all tracer output
-   `        -unmute protocols.optimize_weights.IterativeOptEDriver core.pack.min_pack       ` : unmute these tracers to get useful output messages to the log file
-   `        -ex1       ` : expand rotamer sampling by +/- 1 standard deviation about chi 1 for buried residues
-   `        -ex2       ` : expand rotamer sampling by +/- 1 standard deviation about chi 2 for buried residues
-   `        -linmem_ig 10       ` : Use the linear-memory on-the-fly interaction graph as it both is faster and uses less memory than the precomputed pair energies interaction graph
-   `        -optE:free </path/to/free_wts.txt>       ` : List the set of score function terms whose weights should be free to vary. This should be an empty file.
-   `        -optE:fixed </path/to/fixed_wts.txt>       ` : List the set of score function terms whose weights should be held fixed, and give the weight for each.
-   `        -mpi_weight_minimization       ` : Distribute weight minimization over all MPI jobs, which, makes the code run just a little faster, even though no significant time is spent in weight minimization.
-   `        -starting_refEs </path/to/score12prime.wts>       ` : Seed the reference weight optimization proceedure with the reference energies given in the input file.
-   `        -optE:no_design_pdb_output       ` : Do not dump PDBs holding the redesigned sequences that are generated at each iteration through the outer loop. These PDB files are not generally that interesting, and they can take up a lot of disk space.

Other useful flags for the sequence-profile recovery procedure
--------------------------------------------------------------

The following options are sometimes useful when running the reference energy fitting procedure

-   `        -no_optH true       ` : Do not run optimizeH on PDBs as they are being read in.
-   `        -ignore_unrecognized_res       ` : for reading input PDBs; don't exit simply because the input pdbs contain residues that Rosetta does not recognize
-   `        -skip_set_reasonable_fold_tree       ` : Avoid the call to set\_reasonable\_fold\_tree when a Pose is created from a PDB file. May or may not be necessary for the set of input PDBs that you are using.
-   `        -optE:n_design_cycles       ` : Set the number of iterations that optE should take through the outer loop
-   `        -optE:design_with_minpack       ` : When performing sequence redesign in the inner loop, use the min packer instead of the discrete packer.
-   `        -multi_cool_annealer <int>       ` : Use the multicool annealer in the design steps (slower, doesn't seem to produce better reference energies). "10" is the recommended value for this integer option.
-   `        -optE:optE_soft_rep       ` : Use the softrep Lennard-Jones parameters in optE.
-   `        -optE:no_hb_env_dependence       ` : Disable the environmental dependence in the hydrogen bond term
-   `        -optE:no_hb_env_dependence_DNA       ` : Disable the environmental dependence in the hydrogen bond term for DNA
-   `        -optE:optE_no_protein_fa_elec       ` : Disable protein/protein (but not protein/DNA) electrostatic interactions
-   `        -optE:constant_logic_taskops_file <xmlfile>       ` : A file in utility::tag format that optE uses to build a task that will not change with the context of the pose after design

Tips for running OptE
=====================

Here are a set of recommendations for managing optE job control and generally getting the most out of optE.

-   Build the optE\_parallel application with the "extras=mpi" flag on your scons command line. Though OptE can be run on a single processor, it is very slow. (I'm sorry that I cannot answer questions on how to build with mpi on any particular system, or how to launch mpi jobs. These details vary from system to system, and I do not have advice for the vast majority of systems. I will offer one piece of advice, though: if scons says something to you like "Could not find mpiCC," but when you copy-and-paste the mpiCC command line that scons was executing and it works just fine, then you need to uncomment two lines in the main/source/tools/build/user.settings file: the first one one line 25 ("\#import os" –\> "import os") and the second on line 38 ("\#"program\_path" : os.environ["PATH"].split(":")," –\> ""program\_path" : os.environ["PATH"].split(":"), " ).

-   Create a new directory each time you run optE. If you run optE in a directory where you had previously run it, you will overwrite your old results, or, if you should use a different set of input files (in that directory) each time you run, you will have trouble keeping track of what files went with what job.

-   Since optE waits for every design simulation to finish before going on to the next round, you may waste a lot of CPU time waiting for one very large PDB in your design set to finish (i.e. the other N-1 nodes are doing nothing waiting for the last one to finish). If you have a lot of idle CPUs then this is not really a problem. Otherwise, I recommend using N/2 cpus to design N structures where you have sorted your input PDBs by size and then interleaved them so the biggest and the smallest are next to each other in the list file that is passed in to the `        -s       ` flag. The IterativeOptEDriver will assign groups of two PDBs to each processor and will assign them in the order that they appear in the input list file. That is, if you had 26 PDB files you were training on and the PDBs are A.pdb, B.pdb... Z.pdb, where A.pdb is the smallest and Z.pdb is the biggest, then use 13 processors for the optE job, and sort your list file to look like this:

```
/path/to/Z.pdb
/path/to/A.pdb
/path/to/Y.pdb
/path/to/B.pdb
...
```

-   Keep a separate flags file for the flags that control how optE should behave and the set of flags that control the score function so that it's easy to take the score function flags and use them in other contexts (e.g. a sequence-recovery benchmark, or a rotamer-recovery benchmark).

-   Keep an excel spreadsheet of all of your optE jobs, the sha-1 of Rosetta that you used, the contents of all the input files that optE reads (e.g. the fixed.wts file and the free.wts file), all the flags you had on the command line, the sequence recovery rate for the highest-scoring weight set, and the cross-entropy for the highest-scoring weight set.

-   Here is a useful csh script for creating the directories that optE expects to write its output to and for launching optE. Edit this script as needed.

```
#!/bin/csh -f

@ num_procs = 27;
@ i = 0

#clean up
rm -rf workdir_* weightdir logdir

while ( $i < $num_procs )
   mkdir workdir_$i
   #echo "making directory workdir_" $i
   @ i = $i + 1
end
mkdir weightdir
mkdir logdir

bsub -n $num_procs -q week -o logdir/optE_log -a mvapich mpirun /nas02/home/l/e/leaverfa/GIT/main/source/bin/optE_parallel.mpi.linuxgccrelease @optE_seqprof.flags @score.flags > submission.log
```

Expected Outputs
================

OptE expects to be run in a directory that contains several subdirectories:

-   weightdir: the directory where the weights will be written
-   logdir: the directory where the "minimization\_dat" will be written; recommended directory for redirecting stdout to a file.
-   rundir\_{0..N-1}: the N directories required so that each MPI node can write to a separate directory

The weight files are all written to the weightdir directory.

Post Processing
===============

The script find\_best\_weight\_set.py identifies the weight set with the highest score that should be considered the output from optE. This script can be found in the Rosetta/demos/public/optE/scripts/ directory.

New things since last release
=============================

More coming soon.

## See Also

* [[Utility applications | utilities-applications]]: other utility applications
* [[Application Documentation]]: Application documentation home page
* [[Running Rosetta with options]]: Instructions for running Rosetta executables.