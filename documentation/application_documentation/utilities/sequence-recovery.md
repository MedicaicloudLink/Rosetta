#The sequence\_recovery application

Metadata
========

This document was last updated on June 22, 2010, by Ron Jacak. The corresponding PI is Brian Kuhlman (bkuhlman@email.unc.edu).

Code and Demo
=============

The code for the sequence\_recovery application is in `       src/apps/public/sequence_recovery.cc      ` . A demo for the application is located in `       demo/sequence_recovery/      ` .

References
==========

Sequence recovery tests have been used extensively during the development of Rosetta. The following publications show output that can be obtained from this application:

-   Computational Protein Design with Explicit Consideration of Surface Hydrophobic Patches. R. Jacak, A. Leaver-Fay, and B. Kuhlman. *Submitted* .
-   [Native protein sequences are close to optimal for their structures.](http://www.ncbi.nlm.nih.gov/pubmed/10984534) B. Kuhlman and D. Baker. PNAS 2000 Sep 12;97(19):10383-8.

Purpose
========

This application takes as input a set of native proteins and a corresponding set of redesigned proteins and outputs a table of core, overall, and surface sequence recoveries by amino acid type. A table of the native amino acid and to what residue type Rosetta mutated these positions is also output. The output is explained in more detail in the following sections.

Algorithm
=========

Sequence recovery is measured by reading in both sets of structures and then calculating the percent identity between them by iterating over all residues. The algorithm is very straight-forward. Both sets of structures are read in as Pose objects and stored in Pose vectors. The algorithm iterates over every residue of each pose in the native structures vector, and checks whether that residue is the same in the corresponding designed protein. In addition to the overall percent identity, it also computes the percent identity for core and surface positions. Core positions are defined as those having \>= 24 neighbors (CBeta distance within 10 Angstroms) and surface positions are defined as having \<= 16 neighbors (CBeta distance within 10 Angstroms). An optional task factory can be supplied so that percent identity is only calculated for a subset of positions within the supplied structures.

Limitations
===========

The application assumes that the order of the PDBs in both lists of structures is identical, i.e. each native PDB structure has one corresponding redesigned structure and the structures are listed in the same order.

Input Files
===========

Structures - REQUIRED
---------------------

At least two lists of PDB files must be supplied. It is acceptable to have only one PDB file in each list, if one wants to measure the sequence recovery of just one protein. If the sequence recovery of multiple PDBs is desired, the structures in each list must be in the same order.

```
-native_pdb_list <listfile>     The list of native proteins to compare against
-redesign_pdb_list <listfile>   The list of redesigned proteins to compare to the native proteins
```

Tag file
--------

By default, the application measures sequence recovery over all positions. A file containing task operations to apply to each pose to restrict which residues are included for measuring sequence recovery can be specified using the -parse\_tagfile flag. A sample tag file is shown in the examples below. Most users will want to use the default behaviour.

```
-parse_tagfile <file>           An XML file containing a set of task operations to limit which residues are included in calculating
                                percent identity
```

Options
=======

Database location - REQUIRED
----------------------------

```
-database <path/to/rosetta/main/database>   Specifies the location of the rosetta database
```

Other options
-------------

```
-se_cutoff                      The maximum number of neighbors a residue can have to be considered surface exposed (default: 16).
-seq_recov_filename             The name of the file to output the sequence recovery table to (default: sequencerecovery.txt)
-sub_matrix_filename            The name of the file to output the substitution matrix to (default: submatrix.txt)
-ignore_unrecognized_res        Command line option to exclude unrecognized residues
```

A sample command line for running the sequence\_recovery application would be as follows:

```
sequencerecovery.macosgccrelease -database /path/to/rosetta/main/database/ -native_pdb_list seqrecovery.list -redesign_pdb_list redesigned.list
-ignore_unrecognized_res
```

A sample command line for running the sequence\_recovery application using a tagfile to restrict the calculation to only surface residues (even though this is done by default) would be as follows:

```
sequencerecovery.macosgccrelease -database path/to/rosetta/main/database -native_pdb_list seqrecovery.list -redesign_pdb_list redesigned.list
-parse_tagfile taskoperations.tagfile -ignore_unrecognized_res
```

Example taskoperations.tagfile

```
<TASKOPERATIONS>
  <RestrictNonSurfaceToRepacking name=rnstr surface_exposed_nb_count_cutoff=16 exclude_interfaces_from_restriction=false/>
</TASKOPERATIONS>
```

Another example where one would want to use the tagfile option would be in calculating the number of native residues recovered over all interface residues in a set of redesigned protein-protein complexes. The tagfile would specify a task operation that restricts the set of residues being considered to only those at the interface between the two proteins.

Tips
====

The easiest way to prepare the structure list files is to execute the following commands:

```
ls *.pdb > natives.list;   sort natives.list -o natives.list;
ls *.pdb > redesigns.list; sort redesigns.list -o redesigns.list;
```

Expected Outputs
================

The sequence\_recovery application outputs two text files: 1) a file containing the core, overall, and surface native sequence recovery by amino acid type and 2) a table showing the breakdown of what residues were designed in place of each type.

Example output of using the standard Rosetta energy function "score12" on a set of 32 proteins is shown below. The first line denotes what each column represents.

```
Residue | No.correct core | No.native core | No.designed core | No.correct/No.native core | No.correct/No.designed core | No.correct | No.native | No.designed | No.correct/No.native | No.correct/No.designed | Residue | No.correct surface | No.native surface | No.designed surface | No.correct/No.native surface | No.correct/No.designed surface
ALA 57  103 94  0.55    0.61    118 385 259 0.31    0.46    ALA 9   144 30  0.06    0.30
CYS 0   12  0   0.00    --- 0   27  0   0.00    --- CYS 0   2   0   0.00    ---
ASP 10  20  29  0.50    0.34    80  268 354 0.30    0.23    ASP 49  167 221 0.29    0.22
GLU 2   7   16  0.29    0.12    54  289 308 0.19    0.18    GLU 31  180 158 0.17    0.20
PHE 37  60  69  0.62    0.54    98  193 302 0.51    0.32    PHE 6   23  68  0.26    0.09
GLY 56  69  66  0.81    0.85    302 400 368 0.75    0.82    GLY 164 210 205 0.78    0.80
HIS 10  18  41  0.56    0.24    30  118 176 0.25    0.17    HIS 5   37  32  0.14    0.16
ILE 41  83  70  0.49    0.59    105 242 231 0.43    0.45    ILE 7   33  39  0.21    0.18
LYS 2   11  9   0.18    0.22    62  325 285 0.19    0.22    LYS 30  193 148 0.16    0.20
LEU 66  122 92  0.54    0.72    180 378 454 0.48    0.40    LEU 18  54  174 0.33    0.10
MET 10  31  25  0.32    0.40    20  90  70  0.22    0.29    MET 1   20  5   0.05    0.20
ASN 3   14  11  0.21    0.27    33  206 158 0.16    0.21    ASN 20  110 120 0.18    0.17
PRO 6   18  7   0.33    0.86    61  229 77  0.27    0.79    PRO 29  144 40  0.20    0.73
GLN 3   17  6   0.18    0.50    18  186 172 0.10    0.10    GLN 10  94  110 0.11    0.09
ARG 4   14  20  0.29    0.20    37  185 262 0.20    0.14    ARG 10  85  76  0.12    0.13
SER 20  37  106 0.54    0.19    79  258 348 0.31    0.23    SER 25  119 128 0.21    0.20
THR 19  41  62  0.46    0.31    61  291 225 0.21    0.27    THR 30  145 110 0.21    0.27
VAL 47  98  67  0.48    0.70    107 308 194 0.35    0.55    VAL 8   60  37  0.13    0.22
TRP 10  23  21  0.43    0.48    32  76  145 0.42    0.22    TRP 4   12  56  0.33    0.07
TYR 9   40  27  0.22    0.33    39  158 224 0.25    0.17    TYR 6   23  98  0.26    0.06
Total   412 838     0.492       1516    4612        0.329       Total   462 1855        0.249
```

As can be seen from the native sequence recovery table, the standard Rosetta energy function "score12" obtains an overall native sequence recovery of 32.9% on a set of 32 proteins. Core recovery is much higher with 49.2% of the native residues being recovered. In addition to providing the core, overall, and surface native sequence recoveries, users can see which residue types Rosetta is recovering best and worst. For example, Rosetta designed less than one-third as many prolines as are present in the native structures overall, and nearly twice as many tryptophans. These tryptophans appear to be getting placed on the surface as the number of tryptophans on the surface is nearly four times the number in the native proteins. Serine is overrepresented in the core, with there being nearly three times as many serines in the core of the redesigns as in the core of the native structures. This implies that either Rosetta is having trouble fitting other residues in the core (perhaps due to clashes from the Lennard-Jones term), the hydrogen bond energy terms are weighted too heavily and favoring too many serines, or the desolvation penalty for serine is not high enough.

The corresponding substitution matrix for the above run is below:

```
AA_TYPE nat_ALA nat_CYS nat_ASP nat_GLU nat_PHE nat_GLY nat_HIS nat_ILE nat_LYS nat_LEU nat_MET nat_ASN nat_PRO nat_GLN nat_ARG nat_SER nat_THR nat_VAL nat_TRP nat_TYR
sub_ALA 118 6   6   2   5   8   1   8   6   7   7   5   15  9   7   19  9   15  6   0
sub_CYS 0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0   0
sub_ASP 29  3   80  22  4   14  7   6   28  19  4   34  31  13  8   20  12  12  0   8
sub_GLU 26  1   22  54  6   7   9   9   26  13  7   18  12  16  23  17  19  14  3   6
sub_PHE 9   1   12  11  98  5   11  4   15  18  3   4   3   6   9   6   16  11  10  50
sub_GLY 10  1   7   2   2   302 3   1   9   4   1   10  1   4   2   6   0   1   0   2
sub_HIS 7   1   7   9   17  5   30  6   5   18  2   7   8   11  5   5   9   6   5   13
sub_ILE 6   1   4   10  3   2   3   105 10  16  2   5   0   2   8   5   11  34  4   0
sub_LYS 20  0   14  34  1   6   8   7   62  18  4   19  4   20  15  19  16  10  3   5
sub_LEU 8   2   21  31  5   6   6   19  41  180 15  15  17  21  13  7   17  18  5   7
sub_MET 5   1   1   3   1   0   1   4   4   11  20  0   1   4   7   1   1   0   4   1
sub_ASN 22  0   18  5   0   14  5   1   9   5   4   33  7   8   8   12  3   2   0   2
sub_PRO 7   0   2   1   0   0   0   2   2   1   0   0   61  0   0   0   0   0   1   0
sub_GLN 17  0   12  20  3   7   3   6   20  6   1   7   8   18  9   8   18  9   0   0
sub_ARG 13  0   13  22  3   7   6   11  31  15  6   13  6   19  37  21  19  12  1   7
sub_SER 62  3   20  11  2   11  5   4   14  16  3   12  40  6   10  79  30  14  1   5
sub_THR 7   7   6   13  5   2   2   12  16  15  5   8   6   9   5   17  61  27  1   1
sub_VAL 3   0   4   6   1   0   0   27  9   5   2   2   0   4   1   3   18  107 0   2
sub_TRP 4   0   9   10  8   2   6   4   8   6   2   4   5   6   9   2   13  5   32  10
sub_TYR 12  0   10  23  29  2   12  6   10  5   2   10  4   10  9   11  19  11  0   39
```

Post Processing
===============

The output sequence recovery and substitution matrix files can be easily imported into Excel (split on whitespace) to further analyze the results.

New things since last release
=============================

The sequence\_recovery application is being released for the first time with Rosetta v3.3.


## See Also

* [[Utility applications | utilities-applications]]: other utility applications
* [[Application Documentation]]: Application documentation home page
* [[Running Rosetta with options]]: Instructions for running Rosetta executables.
