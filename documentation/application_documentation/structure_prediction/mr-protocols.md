#Documentation for mr\_protocols application

Metadata
========

Last edited Oct 26, 2010 by Frank DiMaio. Code by Frank DiMaio; ThreadingInputter (on which this code is heavily reliant) by Mike Tyka and James Thompson. P.I.: David Baker.

Code and Demo
=============

The code for this application is in src/apps/public/electron\_density/mr\_protocols.cc. A demo, showing the role of this protocol in a molecular replacement pipeline, are in demo/electron\_density/molecular\_replacement. In particular, steps 4 and 5 of the pipeline show different ways of using of this application. (The README in this folder explains each step in detail).

Application purpose
===========================================

This application is to be used for solving difficult molecular replacement problems. It will generally be used alongside the molcular replacement software Phaser. There are two main uses for the software, related to molecular replacement:

-   When Phaser (or some other molecular replacement approach) finds a clear molecular replacement hit (Z score of \~7 or better) but the resulting density map is difficult to interpret and automated methods for model improvement (such as phenix.autobuild or ARP/wARP) fail.

-   When Phaser (or some other molecular replacement approach) is unable to find a clear molecular replacement hit, the application may be used to refine the top potential solutions, possibly pulling out a weak (but correct) solution from among the false positives.

An interface to this application will be included in a (relatively soon) future version of Phenix.

Algorithm
=========

The mr\_protocols application is used for performing template-based comparative modeling constrained by poorly phased density data, such as that resulting from weak molecular replacement solutions. It runs rosetta's standard comparative modelling, where gaps and insertions in the template are first rebuilt from backbone fragments. Additionally missing density in the template PDB is rebuilt. Only gaps up to a prespecified length (generally 8 residues) are rebuilt. Finally, an allatom relax – constrained to experimental density – allows for small backbone motion in the aligned regions of the template, as well as repositioning of the sidechains.

The density constraint uses a scoring function derived from the density correlation of a model and a density map. This term is described in DiMaio et al, JMB 2009; some additional reading may be found in the [[scoring structures with density|density-map-scoring]] section. This method has been shown to be particularly effective in solving structures via molecular replacement from templates in the 20-30% sequence identity range.

Input Files
===========

Input files are given as template/alignment-file pairs. Generally these files are automatically generated by the included helper script [[prepare_template_for_MR.pl|prepare-template-for-mr]] ; the linked document describes them in more detail.

Template files are PDB files given by "-in:file:template\_pdb". Alignment files, which describe the sequence mapping from template to target, are in "Rosetta format", which is similar to a fasta-format alignment, with a few extras fields. For example:

```
## 1CRB_ 2qo4.PHASER.1.pdb
# hhsearch
scores_from_program: 0 1.00
2 DFNGYWKMLSNENFEEYLRALDVNVALRKIANLLKPDKEIVQDGDHMIIRTLSTFRNYIMDFQVGKEFEEDLTGIDDRKCMTTVSWDGDKLQCVQKGEKEGRGWTQWIEGDELHLEMRAEGVTCKQVFKKV
0 AFSGTWQVYAQENYEEFLRAISLPEEVIKLAKDVKPVTEIQQNGSDFTITSKTPGKTVTNSFTIGKEAEIT--TMDGKKLKCIVKLDGGKLVCRTD----RFSHIQEIKAGEMVETLTVGGTTMIRKSKKI
--
```

While multiple pairs of template alignment files are accepted, it is generally recommended to run just a single template/alignment with each job.

In addition, a [[fragment file|fragment file]] may also be required, depending on the options selected. Rebuilding of gaps is done by fragment insertion (as in Rosetta ab initio); thus two backbone fragment files (3-mers and 9-mers) must be given. The [[application for building these|app-fragment-picker]] is included with rosetta but requires some external tools/databases. The easiest way to generate fragments is to use the Robetta server ( [http://robetta.bakerlab.org/fragmentsubmit.jsp](http://robetta.bakerlab.org/fragmentsubmit.jsp) ). The fragment files should be built with the full-length sequence; rosetta handles remapping the fragments if not all gaps are rebuilt.

Options
=======

A general command line one would use for refining a weak molecular replacement solution is shown below:

```
bin/mr_protocols.default.linuxgccrelease \
    -database /path/to/rosetta/main/database \
    -MR:mode cm \
    -in:file:extended_pose 1 \
    -in:file:fasta 1crb.fasta \
    -in:file:alignment 2qo4.ali \
    -in:file:template_pdb 2qo4.PHASER.1.pdb \
    -loops:frag_sizes 9 3 1 \
    -loops:frag_files xx1crb_09_05.200_v1_3.gz xx1crb_03_05.200_v1_3.gz none \
    -loops:random_order \
    -loops:random_grow_loops_by 5 \
    -loops:extended \
    -loops:remodel quick_ccd \
    -loops:relax relax \
    -relax:default_repeats 2 \
    -relax:jump_move true \
    -edensity:mapreso 3.0 \
    -edensity:grid_spacing 1.5 \
    -edensity:mapfile 2qo4.PHASER.1_2mFo-DFc.ccp4 \
    -edensity:sliding_window_wt 1.0 \
    -edensity:sliding_window 5 \
    -cm:aln_format grishin \
    -MR:max_gaplength_to_model 8 \
    -nstruct 1000 \
    -ignore_unrecognized_res -overwrite
```

For the majority of users, the only flags that will need to be changed from job to job deal with:

(1) The input structure and alignment:

```
-in:file:extended_pose 1
-in:file:fasta inputs/1crb.fasta
-in:file:alignment inputs/1crb_2qo4.ali
-in:file:template_pdb inputs/2qo4.PHASER.1.pdb
```

The fasta, alignment and template PDBs (see the section "Input Files" above). The flag 'extended\_pose 1' should always be given.

(2) The input density map:

```
-edensity:mapfile inputs/sculpt_2QO4_A.PHASER.1.map
-edensity:mapreso 3.0
-edensity:grid_spacing 1.5
-edensity:sliding_window_wt 1.0
-edensity:sliding_window 5
```

This is how the density map and scorefunction parameters are given to Rosetta. The input map (-edensity:mapfile) is CCP4 format. The flags 'mapreso' and 'grid\_spacing' define the resolution of the calculated density and the grid spacing on which correlations are computed (these do not match the input file's grid but rather the grid on which Rosetta computes things). Generally I don't go lower than these values (with grid-spacing 1/2 of mapreso); for very large structures I may run on an even coarser grid to speed things up a bit.

The flag 'sliding\_window\_wt' is the weight on the fit-to-density term. 1.0 is generally fine. If the experimental data is low resolution (3A or worse), than you might want to drop this to 0.5. 'sliding\_window' is the residue-width over which local correlations are computed. 5 is generally fine.

See the tutorial in demo/electron\_density/molecular\_replacement – in particular, step 3 – for information on creating a map from an initial molecular replacement hit.

(3) The input fragments

```
-loops:frag_sizes 9 3 1
-loops:frag_files inputs/aa1crb_09_05.200_v1_3.gz inputs/aa1crb_03_05.200_v1_3.gz none
```

Fragment files and sizes. 9 & 3 are the default produced by Robetta; 'none' means to automatically generate the 1-mer fragments from the 3-mers. See the section "Input files" for details on getting these fragment files.

(4) The number of output structures.

```
-nstruct 1000
```

The number of models needed to effectively sample conformational space varies from model to model, depending on the number of gaps in the alignment and the length of those gaps. On a set of blind cases, most needed 1-2000 output strutures to inprove the model enough to ultimately solve the structure; in a few cases 100 models were sufficient.

Some lesser-used options that may be used by advanced users to control job length and sampling space of the run:

```
-relax:default_repeats 4
-MR:max_gaplength_to_model 8
```

The first option controls the length of the "relax". All our experiments used 4 repeats; for very large systems reducing this to 2 may speed things up a bit. The second option tells Rosetta not to rebuild gaps greater than length 'n' in the alignment. We found 8 and 10 both produce good results: if sampling is limited (e.g., you only have the resources to produce 100 output models) reducing this to 4 or 6 might be enough to solve the structure.

Finally, these options are manditory, and should be left as-is:

```
-relax:jump_move true
-cm:aln_format grishin
-ignore_unrecognized_res
-loops:remodel quick_ccd
-loops:relax relax
-loops:random_grow_loops_by 5
-loops:random_order
-loops:extended
```

Tips
====

-   Generally you don't want to set your resolution better than 3A. In low-resolution cases (2.8A+) we have found setting the input resolution (in Rosetta) to 0.5A worse than the resolution of the crystallographic data works best. In all cases, the grid spacing should be at most half the resolution.

-   With a correct molecular replacement hit, the output model should score significantly better in Phaser than the template.

-   This application may be used with symmetric structures as well! If the template is symmetric, a symmetry definition file can be created using the included script [[make_symmdef_file.pl|make-symmdef-file]] . Then add the flag '-symmetry\_definition symm.def' to the input script. See [[the symmetry documentation|symmetry]] for more details.

-   When dealing with particularly low resolution data (worse than 3A) you may want to reduce the weight on the density-fitting term.

-   If you are getting "template not found" errors, then the template PDB was probably be renamed (by the MR program for example). Either rename the template or [[update the .ali file|prepare-template-for-mr]] . [NOTE: the code only checks the first 5 characters for agreement]

Expected Outputs
================

The structures from the end of the execution will be written out to .pdb files, and the score for that structure will be written to the score file, score.sc. The .pdb files will be tagged with the S\_ followed by the template name, followed by a job number.

This application uses the JD2 job-distributor; it respects all of the jd2 flags that control output.

Post Processing
===============

Models resulting from this protocol are generally subjected to a 10% scoring cut, then rescored against the raw crystallographic data with Phaser. A sample script showing the use of Phaser may be found in the demos folder demo/electron\_density/molecular\_replacement. Using this score as a selection criteria, some small set of models can then be chosen for further refinement or for autobuilding.


##See Also

* [[Prepare template for MR]]: Setup script for molecular replacement protocols.  
* [[Fragment file]]: Fragment file format
* [[Structure prediction applications]]: A list of other applications to be used for structure prediction
* [[Application Documentation]]: List of Rosetta applications
* [[Running Rosetta with options]]: Instructions for running Rosetta executables.
* [[Comparing structures]]: Essay on comparing structures
* [[Analyzing Results]]: Tips for analyzing results generated using Rosetta
* [[Solving a Biological Problem]]: Guide to approaching biological problems using Rosetta
* [[Commands collection]]: A list of example command lines for running Rosetta executable files