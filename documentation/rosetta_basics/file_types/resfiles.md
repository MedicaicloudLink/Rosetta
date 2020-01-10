#Resfile syntax and conventions

Author: Matthew O'Meara and Steven Lewis

This page describes the resfile format, syntax, and conventions. The resfile contains information which is input into the [[PackerTask|Packer-Task]] and controls the Packer. It is read by the TaskOperation [[ReadResfile|ReadResfileOperation]]. Internal details for the commands can be found at the [[How to write new resfile commands|resfile-reader]] residue-level options how-to.

[[_TOC_]]

Resfile Syntax and Semantics
====================

The syntax for the resfile format in extended-EBNF form is as follows.

```
<RESFILE> ::= <HEADER> | [<HEADER>] START\n <BODY> ;

<HEADER> ::= {{<COMMAND>}*\n}* ;

<BODY> ::= {<RESIDUE_IDENTIFIER> {<COMMAND>}*\n}* ;

<RESIDUE_IDENTIFIER> ::= <SINGLE_RESID> | <RANGE_RESID> | <CHAIN_RESID> ;

<SINGLE_RESID> ::= <PDBNUM>[<ICODE>] <CHAIN> ;

<RANGE_RESID> ::= <PDBNUM>[<ICODE>] - <PDBNUM>[<ICODE>] <CHAIN> ;

<CHAIN_RESID> ::= '*' <CHAIN> ;

<PDBNUM> ::= [-]{digits}+ ;

<ICODE> ::= {A-Za-z} ;

<COMMAND> ::= <COMMAND_ID> {<COMMAND_PARAMS>}* ;

<COMMAND_ID> ::= ? see command types below ? ;

<COMMAND_PARAMS> ::= ? see command types below ? ;
```

Throughout, the resfile has the following conventions:

-   The resfile is case-insensitive (except for the chain character, see comment below).
-   Comments begin with the '\#' symbol and continue to the end of the line.
-   Tokens are white space delimited (note, however, the insertion code is attached to the PDBnum)
-   The order in which the residues are specified in the body does not matter.
-   The order in which the commands are specified does not matter.

Header
------

Specify in the \<header\> section the commands that should be applied by default to all residues that are not specified in the body. NOTE: Commands in this section are not applied to any residue which has a line in the body section. For example, if the header commands include "EX 1" and residue 10 has the behavior "ALLAAxc" in the body, then residue 10 will NOT get EX 1 behavior. Command line flags (e.g. "-ex1") will apply to all residues and can be used when the user wants to quickly specify global behavior.

Example header section:

```
# These commands will be applied to all residue positions that lack a specified behavior in the body:
ALLAA # allow all amino acids
EX 1 EX 2 # allow extra chi rotamers at chi- id 1 and 2 (note: multiple commands can be on the same line.)
USE_INPUT_SC # allow the use of the input side chain conformation ( see below for more detailed description of commands)
start
#... the body would continue here.
```

Body
----

Specify in the \<body\> section residue level constraints to be sent to the packer and elsewhere. Each line should have one of the the following formats,

### Residue Identification

#### Single Residue Identification

To specify commands for a single residue, use the following form

```
<PDBNUM>[<ICODE>] <CHAIN> <COMMANDS>
```

If the pose the resfile is has pdb information associated with it (eg it was read in from a pdb file) then \<PDBNUM\>[\<ICODE\>] corresponds to columns 22-26. If the pose does not have pdb information (eg if it was generated de novo or from a silent file), the \<PBDNUM\> is the residue index in the pose and the \<ICODE\> should not be specified. The \<PDBNUM\> can be positive, zero, or negative. The \<ICODE\> is an optional character [A-Z] \(case insensitive\) that occurs in some pdbs to represent insertion or deletions in the sequence to maintain a consistent numbering scheme or the remainder of the sequence.

To accommodate structures with a large number of chains, following the PDB the, the chain can be any character [A-Za-z] where upper and lower case characters are treated as separate chains. For example

```
10 _ PIKAA W # Allow only Trp at residue 10 in the unlabeled chain
40 B PIKAA X[A20]X[B47]X[B48] # Disallow residues except for noncanonical types A20, B47, and B48.  Note that a PackerPalette must be specified that allows design with residue types A20, B47, and B48.
40 B PIKAA X[DA20]X[DB47]X[DB48] # Disallow residues except for the D- stereoisomer of the noncanonical types A20, B47, and B48 (these were originally D20, E47, and E48).  Noted that the PackerPalette must also allow design with DA20, DB47, and DB48.
40A Q ALLAA # Residue 40, insertion code A, on chain Q, use any residue type
```

#### Residue Range Identification

To specify commands for a sequence of residues at once, use the form

```
<PDBNUM>[<ICODE>] - <PDBNUM>[<ICODE>] <CHAIN> <COMMANDS>
```

See the section on specifying a single residue, above, for commends on the format for the \<PDBNUM\>, \<ICODE\>, and \<CHAIN\> identifiers.

To determine which residues fall into the specified, range, the sequence of residues in pose are used. NOTE: it is an error if the first residue does not come before the second residue.

#### Chain Identification

To specify commands for all the residues of a specific chain, use the following form

```
* <CHAIN> <COMMANDS>
```

See the section on specifying a single residue, above, for commends on the format for the \<PDBNUM\>, \<ICODE\>, and \<CHAIN\> identifiers.

#### Residues specified multiple times

If a residue is specified at multiple levels, e.g. as a single residue, in a range and as part of a whole chain, then the specific specification supersedes the others. If a residue is specified multiple times at the same level, eg in multiple single residue commands, then all the commands are used together. Note: the order in which commands are specified is not important.

For example,

```
NATRO
START

* A NATAA
3C - 20 A APOLAR
15 A PIKAA Y
```

This task, sets residue 15 on chain A to be a tyrosine, all the residues in between residue 3C (PDBnum=3, icode='C') and residue 20 except residue 15 on chain A to be a-polar, the rest of the residues on chain A to be their native amino acid but packable, and the rest of the residues in the pose to keep their fixed rotamer.

The body of the above resfile could have been written like this:

```
15 A PIKAA Y
3C - 20 A APOLAR
* A NATAA
```

Commands
========

Commands for for controlling sequence identity
------------------------------------------------

Each command acts to restrict the allowed amino acids allowable at each position. If multiple commands are combined, only amino acids that are allowed by each command individually are included. This is a consequence of the commutativity property for operations on the PackerTask class.

NOTE: It should be remembered that resfile commands are restrictive, rather than permissive. While residues which are disallowed will not be repacked/redesigned, various protocols provide additional restrictions which may further limit mutational identities. In particular, many protocols will prohibit redesigning disulfide cysteines, even when explicitly listed as mutatable in the resfile.

- ALLAA ................ allow all 20 amino acids INCLUDING cysteine (same as ALLAAwc)

- ALLAAwc .............. allow all 20 amino acids ( default )

- ALLAAxc .............. allow all amino acids except cysteine

- POLAR ................ allow only canonical polar amino acids (DEHKNQRST)

- APOLAR ............... allow only canonical non polar amino acids (ACFGILMPVWY)

- PROPERTY \<property\> .. disallow any residue type that lacks the given property

- NOTAA \<list of AAs\> .. disallow only the specified amino acids ( Use one letter codes, undelimited like ACFYRT.  For NCAAs, use X[\<full name\>]. )

- PIKAA \<list of AAs\> .. allow only the specified amino acids ( Use one letter codes, undelimited like ACFYRT.  For NCAAs, use X[\<full name\>].)

- NATAA ................ allow only the native amino acid (NATive Amino Acid) - repack without design

- NATRO ................ preserve the input rotamer ( do not pack at all) (NATive ROtamer)

```
NATRO # default command that applies to everything without a non- default setting; do not repack

start

10 A POLAR # consider polar amino acids at position 10
11 A POLAR PIKAA ACDEFGH # allow mutations to those in the intersection of two sets:
# ........................ the polar amino acids and {ALA, CYS, ASP, GLU, PHE, GLY & HIS}
# ........................ the intersection set {ASP, GLU, HIS}
```

Commands for noncanonical residue types
---------------------------------------

Noncanonical residue types obey the same rules as canonical types: resfile commands and `TaskOperation`s can only turn types _off_.  By default, in the absence of any `TaskOperation`, the 20 canonical amino acids are allowed at any given position.  If you wish to design with noncanonical amino acids, you must therefore specify a [[PackerPalette|PackerPalette]] that includes additional residue types.  Please see the `PackerPalette` documentation for details.  Once the expanded palette has been specified, the residues in the palette are on by default (in the absence of any `TaskOperation`), and can be turned off by `TaskOperations` just like canonical residues.  The `PIKAA` command can be used for this purpose: to specify a noncanonical residue to keep, use `X[###]`, where `###` is the full name of the residue (e.g. `DALA` for D-alanine).

```
NATRO # default command that applies to everything without a non- default setting; do not repack

start

10 A PIKAA ACDEFGHIKLMNPQRSTVWYX[ET1] # allow all 20 amino acids, plus noncanonical ET1
11 A PIKAA X[R2] # disallow all 20 amino acids, allow only noncanonical R2
15 A PIKAA ATSGX[SM1] # allow only ALA, THR, SER, GLY, and noncanonical SM1
56 A PIKAA X[R2]X[T6]X[OP5] #allow only noncanonicals R2, T6, and OP5 (notice separate X statements within the same PIKAA command)
```

Commands for controlling conformational freedom:
---------------------------------------

```
- NATRO ....................................... fix NATive ROtamer ( fix identity and conformation )
- EX (ARO) <chi-id> ( LEVEL <sample level> ) .. ( see below for detailed description )
- EX_CUTOFF <num-neighbors> ................... about how to specify EX commands )
- USE_INPUT_SC ................................ include native rotamer
```

Miscellaneous commands for controlling the packer:
--------------------------------------------------

```
-   AUTO .......... add the behavior 'AUTO'
-   SCAN .......... add the behavior 'SCAN'
-   NO_ADDUCTS .... disallow DNA adducts
-   TARGET <AA> ... specify target amino acid ( use one letter codes )
```

This miscellaneous commands have meanings to particular protocols, but not to all.

Extra Rotamer Commands:
-----------------------

The packer considers discrete sampling of sidechain conformations; it samples discretely from the Dunbrack rotamer library, and by default, samples only at the center of the rotamer wells. The "extra" flags below control the inclusion of additional discrete samples.

-   The command "EX \<chi-id\>" specifies that a extra samples are to be considered for a particular chi angle ( only numbers 1-4 are allowed ) for BURIED residues (see EX\_CUTOFF below). There are additional options that can be given for the EX command, but it is sufficient to state only "EX \<chi-id\>" and to mean "Consider additional rotamers for chi \<chi-id\> at +/- 1 standard deviation from the mean chi angle for each rotamer well for buried residues."
    -   The ARO flag can be included in the EX command, e.g. "EX ARO \<chi-id \>", thereby limiting the effect of the command to only aromatic amino acids (FHWY). For instance, if you would like to entertain extra rotamers at chi 1 for all residues and at chi 2 for only the aromatic residues, then the two commands together in the string "EX 1 EX ARO 2" state this. NOTE: Using "EX ARO \<chi-id\>" does not increase the number of rotamers for FHWY amino acids any more than simply using "EX \<chi-id\>"; both commands would state that additional rotamers should be sampled at +/- 1 standard deviation for buried residues. NOTE: the ARO flag can be combined with the LEVEL flag (below)
    -   The LEVEL \<sample-level\> flag can be included in the EX command specifies how many extra chi angles to sample for each allowed rotamer. The higher the sample-level given, the more rotamers the packer will build.
        -   0 ...... no extra chi angles
        -   1 ...... sample at 1 standard deviation
        -   2 ...... sample at 1/2 standard deviation
        -   3 ...... sample at two full standard deviations
        -   4 ...... sample at two 1/2 standard deviations
        -   5 ...... sample at four 1/2 standard deviations
        -   6 ...... sample at three 1/3 standard deviations
        -   7 ...... sample at six 1/4 standard deviations

    -   NOTE: When combining multiple EX levels for a single chi for a single amino acid, the maximum value is taken. This means that the level for PHE chi 1 when the commands "EX 1 LEVEL 4 EX ARO 1 LEVEL 3" will be 4 and not 3. When extra-chi flags are given without the ARO restriction, they apply to all residues, including aromatic residues.

-   The EX\_CUTOFF \<num-neighbors\> command specifies the number of neighbors (counting itself) each residue must have within a 10 Angstrom cutoff to be considered "buried" for the purpose of including extra rotamers. Rosetta will not include extra rotamers for surface positions unless explicitly directed to do so (by setting the EX\_CUTOFF to a small value \>= 1). The EX\_CUTOFF command in the resfile allows per-position control over where extra rotamers should be considered; the command-line option -extrachi\_cutoff \<num-neighbors\> can be used to apply the same cutoff to all residues.

Here are some examples:

```
10 B EX 1 EX 2
9 B EX 3 # WARNING: probably want to include EX 1 and EX 2 if EX 3 is specified!
8 B EX ARO 2
7 B EX ARO 3 # ERROR: ARO only works for chi ids 1 and 2
6 B EX 1 LEVEL 7
13 B EX 1 EX ARO 1 LEVEL 4 # include extra rotamers at chi 1 for all amino acids,
# but use sample level 4 for the aromatic amino acids
```

This resfile will provide packability at most locations, fixed rotamers at a few, and designability at a few. Note the liberal use of comments for clarity. NOTAA C is equivalent to ALLAAxc.

```
NATAA # this default command applies to all residues that are not given non-default commands

start

#anchor
81 - 82 B NATAA #anchor
83 - 86 B NATRO #anchor

#loops
#133 B NOTAA C #loop
134 - 142 B NOTAA C #loop
#143 B NOTAA C #loop
#144 B NOTAA C #loop
#145 B NOTAA C #loop
#77 B NOTAA C #loop
#78 B NOTAA C #loop
#79 B NOTAA C #loop
80 B NOTAA C #loop
#ANCHOR
87 B NOTAA C #loop
88 B NOTAA C #loop
#89 B NOTAA C #loop
```

##Deprecated commands:

When [[PackerPalettes|PackerPalette]] were introduced, previously-needed resfile commands that broke TaskOperation commutativity were no longer necessary.  As a result, the following commands are no longer recognized by the resfile reader:

```
- EMPTY ................ Disallow all canonical amino acids (for use with non canonicals).  This throws away all previously applied task operations, and so will break the commutativity of task operations.  DEPRECATED AND NO LONGER RECOGNIZED.

- RESET ................ Resets the task to its default state of canonicals ON and non-canonicals OFF (for use with non canonicals)  This throws away all previously applied task operations, and so will break the commutativity of task operations.  DEPRECATED AND NO LONGER RECOGNIZED.

- NC \<ResidueTypeName\> . Allow the specific possibly non canonical residue type; one residue type per NC command.  Note that "GLY:N_Methylation" is a special case that is entered as "SAR" (sarcosine) with this command.  DEPRECATED AND NO LONGER RECOGNIZED.
```

Instead, `PackerPalette`s allow users to define the "palette" of residue types that are allowed for design, and `TaskOperation`s are now restricted to pruning the allowed set at a given position.  Please see the [[PackerPalette|PackerPalette]] documentation for details.

##See Also

* [[File types list]]: List of file types used in Rosetta
* [[Packer Task]]: Description of the PackerTask
* [[Rosetta overview]]: Overview of key Rosetta concepts
* [[Options overview]]: Overview of main Rosetta options groups
* [[Full options list]]
* [[Writing new resfile commands|resfile-reader]]

<!--- search engine optimization
resfile
resfile
resfile
resfile
resfile
resfile
resfile
resfile
resfile
resfile
resfile
resfile
resfile
resfile
resfile
resfile
resfile
resfile
resfile
resfile
resfile
resfile
resfile
resfile
resfile -->
