#Membrane Abinitio

Metadata
========

Author: Vladimir Yarov-Yarovoy

This document was last updated on July 17, 2012 by Vladimir Yarov-Yarovoy (yarovoy@ucdavis.edu) . The membrane ab initio application was developed in David Baker's (dabaker@uw.edu) group by:

-   Jack Schonbrun
-   Vladimir Yarov-Yarovoy
-   Patrick Barth
-   Bjorn Wallner

Code and Demo
=============

The membrane ab initio executable is in src/apps/public/membrane\_abinitio/membrane\_abinitio2.cc. See /rosetta/main/tests/integration/tests/membrane\_abinitio directory for an example membrane ab initio run and input files.

References
==========

-   Barth P, Wallner B, Baker D. (2009 Feb 3) Prediction of membrane protein structures with complex topologies using limited constraints. Proc Natl Acad Sci U S A. 106(5):1409-14.
-   Barth P, Schonbrun J, Baker D. (2007 Oct 2) Toward high-resolution prediction and design of transmembrane helical protein structures. Proc Natl Acad Sci U S A. 104(40):15682-7.
-   Yarov-Yarovoy V, Schonbrun J, Baker D. (2006 Mar 1) Multipass membrane protein structure prediction using Rosetta. Proteins. 62(4):1010-25.

Application purpose
===========================================

This protocol was developed to predict helical membrane protein structures.

Algorithm
=========

This protocol will only generate low-resolution centroid models. The protocol is using transmembrane region predictions from OCTOPUS server ( [http://octopus.cbr.su.se/](http://octopus.cbr.su.se/) ) to set initial membrane normal and membrane center vectors and define membrane-specific environment (hydrophobic core, interface, polar, and water layers). For multispan helical membrane proteins the protocol starts from embedding only two randomly selected adjacent transmembrane helical regions and then continues folding by inserting one of adjacent helices until all transmembrane helices will be embedded into the membrane.

Input Files
===========

1.  Generate structure fragments:

    Programs needed to run: fragment generation - info at [[fragment file]].

    Note - use only SAM secondary structure prediction file (\*.rdb) - jufo and psipred predict transmembrane helical regions poorly.

    Example command:

    ```
    make_fragments.pl -verbose -id BRD4 BRD4_.fasta -nojufo -nopsipred
    ```

2.  Genarate transmembrane regions (OCTOPUS) file:

    Input OCTOPUS topology file is generated at [http://octopus.cbr.su.se/](http://octopus.cbr.su.se/) using protein sequence as input.

    Sample OCTOPUS topology file:

    ```
    ##############################################################################
    OCTOPUS result file
    Generated from http://octopus.cbr.su.se/ at 2008-09-18 21:06:32
    Total request time: 6.69 seconds.
    ##############################################################################

    Sequence name: BRD4
    Sequence length: 123 aa.
    Sequence:
    PIYWARYADWLFTTPLLLLDLALLVDADQGTILALVGADGIMIGTGLVGALTKVYSYRFV
    WWAISTAAMLYILYVLFFGFTSKAESMRPEVASTFKVLRNVTVVLWSAYPVVWLIGSEGA
    GIV

    OCTOPUS predicted topology:
    oooooMMMMMMMMMMMMMMMMMMMMMiiiiMMMMMMMMMMMMMMMMMMMMMooooooMMM
    MMMMMMMMMMMMMMMMMMiiiiiiiiiiiiiiiiiiiiiMMMMMMMMMMMMMMMMMMMMM
    ooo
    ```

3.  Convert OCTOPUS file to .span file format:

    BRD4.span - transmembrane topology prediction file generated using octopus2span.pl script as follows:

    `octopus2span.pl <OCTOPUS topology file>`

    Example command: `<path to rosetta>/rosetta/main/source/src/apps/public/membrane_abinitio/octopus2span.pl BRD4.octopus`

    Sample .span file:

    ```
    TM region prediction for BRD4 predicted using OCTOPUS
    4 123
    antiparallel
    n2c
       6    26     6    26
      31    51    31    51
      58    78    58    78
      97   117    97   117
    ```

    1st line is comment line. 2nd line shows number of predicted transmembrane helices (4 in the command lines example below) and total number of residues (123 in the example below). 3rd line shows predicted topology of transmembrane helices in the membrane (currently only antiparallel topology is implemented). 4th line and all lines below show start and end residue numbers of each of the predicted transmembrane helices (current format repeats these numbers twice).

4.  Generate .lips4 file.

    BRD4.lips4 - lipophilicity prediction file created using run\_lips.pl script as follows (note that blastpgp and nr database are necessary to run run\_lips.pl script

    ```
    run_lips.pl <fasta file> <span file> <path to blastpgp> <path to nr database> <path to alignblast.pl script>
    ```

    Example command:

    ```
     <path to mini>/mini/src/apps/public/membrane_abinitio/run_lips.pl BRD4.fasta BRD4.span /work/bjornw/Apps/blast/bin/blastpgp /scratch/shared/genomes/nr ~bjornw/mini/src/apps/public/membrane_abinitio/alignblast.pl
    ```

    Sample lips4 file:

    ```
    Lipid exposed data: resnum mean-lipo lipophil entropy
          6  -1.000   3.004   1.211
          9  -1.000   2.268   2.137
         10  -1.000   4.862   1.095
         13  -1.000   1.304   1.552
         16  -1.000   3.328   2.025
    ...
    ```

Options
=======

Run membrane ab initio application with the following flags:

```
./bin/membrane_abinitio2.linuxgccrelease
-in:file:native BRD4.pdb                  Native structure (optional)
(or -in:file:fasta BRD4_.fasta)           Protein sequence in fasta format (required if native structure is not provided)
-in:file:spanfile BRD4.span               Octopus transmembrane prediction (see above)
-in:file:lipofile BRD4.lips4              Lipophilicity prediction (see above)
-in:file:frag3 aaBRD4_03_05.200_v1_3      3-residue fragments
-in:file:frag9 aaBRD4_09_05.200_v1_3      9-residue fragments
-in:path:database /path/to/rosetta/main/database       Path to rosetta database
-abinitio:membrane                        Membrane ab initio application
-score:find_neighbors_3dgrid              Use a 3D lookup table for residue neighbors calculations
-membrane:no_interpolate_Mpair            Switch off the interpolation between the two layers for the Mpair term
-membrane:Menv_penalties                  Switch on the following penalties:

    * no non-helical secondary structure fragments in predicted transmembrane helical regions in the hydrophobic layer of the membrane.
    * no N- and C- termini residues of predicted transmembrane helical regions in the hydrophobic layer of the membrane.
    * no transmembrane helices with orientation >45 degrees relative to the membrane normal vector.

-nstruct 1                                Number of output structures
```

You have a choice to use either Monte Carlo (default) or descrete search of membrane normal and membrane center.

Optional settings used for Monte Carlo based membrane normal and center search protocol:

```
-membrane:normal_cycles (default=100)     Total number of membrane normal cycles
-membrane:normal_mag (default=5)          Magnitude of membrane normal angle search step size (degrees)
-membrane:center_mag (default=1)          Magnitude of membrane center search step size (Angstroms)
```

Tip - to speedup Monte Carlo based membrane normal and center search use the following settings:

```
-membrane:normal_cycles 40
-membrane:normal_mag 15
-membrane:center_mag 2
```

Optional settings for alternative - descrete search of membrane normal and membrane center:

```
-membrane:center_search (default= false) - perform membrane center search within "center_max_delta" deviation (see below).
-membrane:normal_search (default= false) - perform membrane normal search with normal_start_angle, normal_delta_angle, and normal_max_angle values (see below).
-membrane:center_max_delta (default= 5 A) - magnitude of maximum membrane width deviation during membrane center search (Angstroms).
-membrane:normal_start_angle (default= 10 degrees) - magnitude of starting angle during membrane normal search (degrees).
-membrane:normal_delta_angle (default= 10 degrees) - magnitude of angle deviation during membrane normal search (degrees).
-membrane:normal_max_angle (default= 40 degrees) - magnitude of maximum angle deviation during membrane normal search (degrees).
```

Expected Outputs
================

Convert silent file output into pdb file using score application as follows:

```
~bin/score.macosgccrelease \
-in:file:native BRD4.pdb \
-in:file:centroid_rosetta_inputs/ \
-in:file:silent BRD4_silent.out \
-in:file:silent_struct_type binary \
-in:file:spanfile BRD4.span \
-in:file:lipofile BRD4.lips4 \
-membrane:no_interpolate_Mpair \
-membrane:Menv_penalties \
-score:find_neighbors_3dgrid \
-score:weights score_membrane.wts \
-out:nstruct 1 \
-database /path/to/rosetta/main/database \
-out:output \
-nstruct 1
```

Membrane ab initio application specific score outputs in the output score file are:

```
Mpair                         membrane pairwise residue interaction energy
Menv                          membrane residue environment energy
Mcbeta                        membrane residue centroid density energy
Mlipo                         membrane residue lipophilicity energy
Menv_hbond                    membrane non-helical secondary structure in the hydrophobic layer penalty
Menv_termini                  membrane N- and C-temini residue in the hydrophobic layer penalty
Menv_tm_proj                  transmembrane helix projection penalty
```

Post Processing
===============

Generate at least 10,000 models and then use rosetta Cluster application to identify most frequently sampled conformations. In general case, at least one of top 5-10 clusters will have models with the lowest rmsd to the native structure.

Good luck :)!
