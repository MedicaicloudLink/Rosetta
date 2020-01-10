#UBQ chemically conjugated docking 

Metadata
========

Author: Steven Lewis (smlewi@gmail.com)

Last edited 8/23/13. Code by Steven Lewis. Corresponding PI Brian Kuhlman (bkuhlman@email.unc.edu).

Note on contents and history
============================

This page documents three applications, UBQ\_E2\_thioester, UBQ\_Gp\_CYD-CYD, and UBQ\_Gp\_LYX-Cterm. The code and documentation were originally written for the thioester version. The other versions are modifications of the first, and they all behave nearly the same from the outside, so they are all documented here. Assume the document is talking about the thioester case, but also assume what it says is true for the other cases.

Code and Demo
=============

The code is at `       rosetta/main/source/src/apps/public/scenarios/chemically_conjugated_docking/      ` ; there's an integration test+demo at `       rosetta/main/tests/integration/tests/UBQ_E2_thioester/      ` . The same is true for the other versions of the application (altering the path as appropriate). Note that the integration test is vastly under-cycled relative to getting it to do anything useful: the number of cycles it demonstrates should be sufficient to show some remodeling but not enough to get anywhere useful. To run that demo, go to that directory and run `       [path to executeable] -database [path to database] (at-symbol)options      `

References
==========

The code was written for this paper, which treats the attachment of ubiquitin to an E2. You may want to look at the online supplemental info for that paper for a different presentation of how the code works.

-   Saha A, Kleiger G, Lewis S, Kuhlman B, Deshaies RJ. Essential role for ubiquitin-ubiquitin-conjugating enzyme interaction in ubiquitin discharge from Cdc34 to substrate. Molecular Cell. 2011 Apr 8;42(1):75-83.

The UBQ\_Gp series of executables were created for this paper, which needed a disulfide connection as an experimental mimic of the native ubiquitin isopeptide linkage, and the native isopeptide itself.

-   Baker R, Lewis SM, Sasaki AT, Wilkerson EM, Locasale JW, Cantley LC, Kuhlman B, Dohlman HG, Campbell SL. Site-specific monoubiquitination activates Ras by impeding GTPase-activating protein function. Nat Struct Mol Biol. 2013 Jan;20(1):46-52. doi: 10.1038/nsmb.2430. Epub 2012 Nov 25. PubMed PMID: 23178454; PubMed Central PMCID: PMC3537887.

The code was further updated (particularly with multi-ubiquitin code written for the first paper, genericizations written for the paper, and new multibody code) for this paper:

-   Kamadurai HB, Qiu Y, Deng A, Harrison JS, Macdonald C, Actis M, Rodrigues P, Miller DJ, Souphron J, Lewis SM, Kurinov I, Fujii N, Hammel M, Piper R, Kuhlman B, Schulman BA. Mechanism of ubiquitin ligation and lysine prioritization by a HECT E3. Elife. 2013 Aug 8;2:e00828. doi: 10.7554/eLife.00828. Print 2013. PubMed PMID: 23936628; PubMed Central PMCID: PMC3738095.

Application purpose
===========================================

This code (UBQ\_E2\_thioester) was written for a relatively singular application: modeling the thioester-linked state of an E2 enzyme charged with ubiquitin. We had hypotheses about what this state might look like and used this code to generate models to examine those hypotheses and generate testable mutations. It was initially expected to serve other purposes. Reading the previous section, boy did that guess prove wrong...

A second, unrelated experiment was built on the code for that first project. Here, we wished to examine how a G-protein (ras) would behave when chemically conjugated to ubiquitin. The LYX-cterm and CYD-CYD versions are to mimic the native and experimental versions of the linkage. These are separate executables because the differences between them, while slight, do not lend themselves to straightforward code modularization and reuse...it really needs to be rewritten.

A third set of experiments forced us to upgrade the code to be able to handle multiple "fixed" bodies (meaning, the thing that "ubiquitin" (the moving chain) is attached to may be an internally rigid multiprotein complex [except it can have loops]), as well as a second ubiquitin chemically attacking the first ubiquitin at the active site.

So...at this point, it's code that has a "moving thing", chemically attached via either its C-terminus or a terminal disulfide, to some collection of nonmoving things, and you can also have loops moving and a second "moving thing" attacking the first "moving thing".

In ALL CASES, the code does not require use of an E2 enzyme, a GTPase, or ubiquitin. Use whatever proteins you need. I am willing to entertain renames to something that captures their function better.

Algorithm
=========

There are two important novel chunks of code associated with this algorithm.

The thioester-linked structure contains an E2 enzyme (treated as a rigid body) and a ubiquitin molecule (treated mostly rigidly). The C-terminus of ubiquitin (glycine) is chemically linked to a cysteine of the E2, resulting in a thioester bond between the proteins. It is this bond that this protocol remodels. Phil Bradley deserves credit for helping set up this chemical conjugation code.

The remodeling algorithm is straightforward. It uses Rosetta's standard Metropolis/Monte Carlo random sampling tools. A series of possible Pose modifications are chosen from on each Monte Carlo cycle. These include modification of all torsions close to the thioester, including chi1 and chi2 of the cysteine, the thioester bond, and the effective psi and phi angles of ubiquitin's terminal glycine. These are treated directly by TorsionDOFMover instead of more familiar sidechain/backbone movers because the extra chemical bond changes the torsional preferences at these bonds, meaning that the Ramachandran and Dunbrack libraries do not apply. TorsionDOFMover internally checks against a molecular-mechanics bond torsion term (although this term is not in the broader scorefunction). Other possible Monte Carlo moves include standard Small/Shear moves on the penultimate ubiquitin residues (the number of mobile residues is command-line flagged), and also KIC loop modeling. After a random move, the Pose runs through Rotamer Trials (to quickly pack sidechains) and a minimization step before the Metropolis criterion is applied. Some fraction of MC cycles instead perform a full repack of the UBQ/E2 interface.

The thioester-building code first replaces the E2's cysteine with a CYX residue type, which has an open residue connection on its sulfur atom (and no hydrogen there). The UBQ glycine is then appended to this with a chemical bond, removing its terminus type. The remainder of ubiquitin is the prepended before the final residue, resulting in a normal residue ordering but a reversed atom-tree folding order (Ubiquitin folds from its C-terminus, rather than its N-terminus).

The protocol once included embedded machinery for generating constraints which are based on experimental data from Saha, Kleiger, and Deshaies; these are now provided in the integration test. (Use -publication to get the original publication behavior, but also supply those constraints!) The remaining original-system-hardcodedness is the initial thioester geometry, based on PDB 1FXT, which is also a model of a thioester-linked E2/UBQ complex.

At the end of the protocol, there is filtering machinery to automatically reject models with no signficiant interface.

Limitations
===========

This code is not safe to use with silent-file output. The contortions used to build the sidechain hookup cannot be re-processed by Rosetta's silent file input–output works fine but you can never read the files back in again. PDB-silent-file output *might* work but I've found that to be buggy in general so I can't recommend it.

The code was not originally meant for generic use and may be fragile for new uses.

Input Files
===========

See tests/integration/tests/UBQ\_E2\_thioester/ for example usage. Basically all you need is an input structure.

-   You will NOT be using standard job distributor inputs (-s/-l PDBs or silent files.). Use -UBQpdb and -E2pdb (or GTPasepdb) to pass in those structures.
-   Ensure that the code has the correct path to the CYX residue type. It will be in your rosetta database; the path is in the integration test/demo. %(database)s/chemical/residue\_type\_sets/fa\_standard/residue\_types/sidechain\_conjugation/CYX.params is its current location. (NOTE: this will probably move for post-3.5 when Talaris2013 comes out.)

Tips
====

-   The code has an unsupported mode allowing for KIC loop modeling. This was intended to be used with the E2 loop absent in PDB 2OB4. The loop was too long for KIC to solve effectively so we took a different path; the code is still present but senescent if you don't pass a loops file.

Options
=======

UBQ\_E2\_thioester supports three types of options: general rosetta options (packing, etc.), generic protocol options like "how many cycles" borrowed from the AnchoredDesign application, and UBQ\_E2\_thioester specific options.

UBQ\_E2\_thioester options

-   -UBQpdb - File - input UBQ PDB, we used 1UBQ.
-   -E2pdb - File - input E2 PDB; we used a trimmed version of 2OB4. Renamed GTPasepdb for the UBQ\_Gp series.
-   -E2\_residue - Integer - Which E2 residue is the active cysteine? 85 is default; correct for 2OB4. Renamed GPTase\_residue for the UBQ\_Gp series.
-   -SASAfilter - Real - minimum SASA filter value. Default 1000 used for production.
-   -scorefilter - Real - filter value for maximum total score. Default 10; 0 used for production.
-   -publication - Boolean - Publication mode? In publication mode, the code runs an extra analysis step to find the neighbors of ubiquitin I44. This is used for the integration test and for reproducibility but has no other purpose. Defaults to false, but use true for reproduction. If you use this, (because you wanted to test the "original publication mode", use the filter and constraint values suggested in the integration test demo).
-   -n\_tail\_res - Integer - number of moving C-terminal tail residues on attached protein (ubiquitin). Defaults to 3, which matches the original publication. Not present at the time of that publication.

Options introduced for that third paper above Experimental UBQ\_E2\_thioester options

-   -two\_ubiquitins - Boolean - Mind-blowing - use two ubiquitins (assembled for a K48 linkage) to try to examine the transition state.
-   -extra\_bodies - FileVector - extra structures to add before modeling. Should be in the coordinate frame of the non-moving partner. Will not move during modeling. Will be detected as part of the nonmoving body for repacking purposes.
-   -UBQ2\_lys - Integer - which Lys on the second UB will be conjugated, default=48. I think the code will crash if this residue is a terminus.
-   -UBQ2\_pdb - File - PDB for second ubiquitin (second moving chain). Only active if -two\_ubiquitins is used; inactive otherwise. Optional; defaults to value of -UBQpdb if not passed.
-   -dont\_minimize\_omega - Boolean - disable minimization of omega angles near thioester in MoveMap; not present in original publications (Saha; Baker), default=false

Experimental UBQ\_Gp\* options

-   -pdz - Boolean - For the UBQ\_Gp\_LYX-Cterm executable, if -publication is already on, switch to the PDZ center of mass instead of ubiquitin center of mass for the extra statistics calculations. Don\\'t use this option unless trying to reproduce that second paper above. default=false

AnchoredDesign options (borrowed for simplicity, not tied to AnchoredDesign in any other way); all are in the AnchoredDesign namespace

-   -AnchoredDesign::refine\_temp - real - Monte Carlo temperature for refine phase (0.8 used for production)
-   -AnchoredDesign::refine\_cycles - unsigned integer - number of refine phase cycles (20000 used for production)
-   -AnchoredDesign::refine\_repack\_cycles - unsigned integer - Perform a repack/minimize every N cycles of refine mode (100 used for production)

General options: All packing namespace options loaded by the PackerTask are respected. jd2 namespace options are respected, although input modes are not. Anything very low-level, like the database paths, is respected.

-   -run::min\_type - string - [[Minimization overview and concepts|minimization-overview]] - minimizer type. dfpmin\_armijo\_nonmonotone used for production.
-   -nblist\_autoupdate - boolean - I'm told this makes minimization faster
-   -nstruct - integer - number of structures to generate
-   -packing:repack\_only - boolean - because you don't want to design, you should pass this
-   -generic\_job\_name - file - this tags the output with a particular name, useful because since there's no normal job distributor input it give strange names to the outputs.
-   -cst\_fa\_weight - weight for constraints. Was originally implemented as -constraintweight. 50 used for production.
-   -cst\_fa\_file - file path for constraints. Originally, these were hard-coded into the file; instead they are now supplied as publication.cst in the integration test/demo.
-   -loops:loop\_file - file - a Rosetta standard loops file; will activate KIC loop modeling of that loop (randomly interspersed with the rest of the modeling). This is probably VERY fragile to poorly chosen loops (like if the chemical conjugated points are in a loop!) so be careful.

Post Processing
===============

Pick the best models by total score and look at the satisfaction of your experimentally-derived constraints to decide which you think is most plausible. We used the models to successfully predict a mutation to rescue a defect caused by UBQ I44A.

Extra terms in scorefile
========================

Most of the terms in the scorefile will be the regular scorefunction terms. Assuming you ran with relatively default options, this will be score12; you can change that with -weights. Here is a brief description of the remaining terms. These do not contribute to total\_score, but may be useful for filtering/postprocessing. Some terms are from InterfaceAnalyzer and not relevant/applicable.

-   amide\_CE-NZ-C-CA - torsion of that bond, in degrees
-   complex\_normalized - total score / number of residues
-   dG\_cross - binding energy by the cross-interface-term method
-   dG\_cross/dSASAx100 - binding energy by the cross-interface-term method, normalized by SASA
-   dG\_separated - binding energy by the more accurate separation method
-   dG\_separated/dSASAx100 - binding energy by the more accurate separation method, normalized by sasa
-   dSASA\_hphobic - SASA of hydrophobic residues?
-   dSASA\_int - SASA of the interface
-   dSASA\_polar - SASA of polar residues
-   delta\_unsatHbonds - number of unsatisfied hydrogen bonds at the interface, that would be satisfied if the proteins weren't bound
-   glycine\_phi\_C-CA-N-C - torsion of that bond, in degrees
-   glycine\_psi\_NZ-C-CA-N - torsion of that bond, in degrees
-   hbond\_E\_fraction - fraction of interface energy due to hbond terms (useless)
-   lysine\_chi3\_CG-CD-CE-NZ - torsion of that bond, in degrees
-   lysine\_chi4\_CD-CE-NZ-C - torson of that bond, in degrees
-   nres\_all - \# residues
-   nres\_int - \# interface residues
-   packstat - packstat score (disabled)
-   per\_residue\_energy\_int - total score / number of interface residues?
-   sc\_value - sc is a shape complementarity score I know nothing about
-   side1\_normalized - score of one side of the interface, normalized by \# of residues on that side
-   side1\_score - score of one side of the interface
-   side2\_normalized - score of one side of the interface, normalized by \# of residues on that side
-   side2\_score - score of one side of the interface

Changes since last release
==========================

Rosetta 3.3 was the first release. For the 3.4 release, the UBQ\_Gp series of applications was added. For 3.5, the constraint code was factored out into a constraint file publication.cst, and some system-specific details in the code were altered to run under a "publication" flag. With these changes, UBQ\_Gp\_CYX-Cterm was deprecated and deleted. Two-ubiquitins, multi-body, and loops modes were added. Also for 3.5, an issue with omega angles near the conjugation bond was corrected. The omega nearest the bond now starts at 180 degrees instead of 140.

##See Also

* [[Docking Applications]]: Home page for docking applications
* [[Preparing structures]]: Notes on preparing structures for use in Rosetta
* [[Application Documentation]]: List of Rosetta applications
* [[Running Rosetta with options]]: Instructions for running Rosetta executables.
* [[RosettaScripts]]: Homepage for the RosettaScripts interface to Rosetta
* [[Comparing structures]]: Essay on comparing structures
* [[Analyzing Results]]: Tips for analyzing results generated using Rosetta
* [[Solving a Biological Problem]]: Guide to approaching biological problems using Rosetta
* [[Commands collection]]: A list of example command lines for running Rosetta executable files
