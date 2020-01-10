#Multistate design and distributing the load with MPI, "mpi\_msd"

Metadata
========

This document was created Apr 14th 2008 by Andrew Leaver-Fay The `       mpi_msd      ` application is maintained by Brian Kuhlman's lab. Send questions to (bkuhlman@email.unc.edu)

Code and Demo
=============

See `       rosetta/main/tests/integration/tests/mpi_multistate_design      ` for a quick example of a multistate design trajectory. This example performs single state design using the multistate design framework. See also `       rosetta/demos/private/andrew/multistate      ` for a more involved example. In this directory, the "README" file directs your attention to a set of input files for a fairly involved multistate design problem of redesigning the interface of a homodimer so that it forms a heterodimer. This example takes about 80 minutes to run when using 10 CPUs and MPI.

The majority of the classes for the mpi\_msd application live in `       rosetta/main/source/src/protocols/pack_daemon      ` .

References
==========

This implementation of multistate design, and it is not the first, was / will-probably-be published here:

A. Leaver-Fay, R. Jacak, B. Stranges, and B. Kuhlman B (2011) "A generic algorithm for multistate protein design." PLoS-ONE, RosettaCon2010 special collection

Purpose
===========================================

This application optimizes a single sequence to perform different ways in different context. It models the various contexts with several states; the energies of these states can then be combined with a fitness function in order to capture how well that sequence meets the design goals of the task. The application distributes its work across multiple CPUs with MPI so that the application can run fairly quickly even when hundreds of states are being modeled.

Algorithm
=========

The optimization of a single sequence in multiple contexts involves several components: a genetic algorithm, a notion of statehood, a fitness function, and a rotamer optimization algorithm.

Genetic Algorithm
-----------------

The outer loop consists of a genetic algorithm that explores sequence space. Each sequence is represented as a string, and all the strings the genetic algorithm will consider will have the same length. It is up to the "correspondence files" to map positions in the outer-loop sequences to residues on the various states (more on this later).

There are three parameters that describe the genetic algorithm's behavior: the population size, the number of generations it is run for, and the percentage of children sequences generated by crossover mutations vs point mutations.

Population size. This controls the number of sequences in the pool of sequences being examined and which are allowed to "breed" when moving from generation i to generation i+1. We recommend using a population size of 100.

The Number of Generations. In moving from generation i to generation i + 1, the best half of the population from generation i will be propagated, and the remainder of the sequences created in generation i+1 are point mutants and crossover mutants from generation i. The number of generations controlls how much time is spent optimizing sequences; this is important as it often takes many generations to mature a sequence into one that generates a very good fitness.

States and the Fitness Function File
------------------------------------

This portion of the documentation is broken down into several sections. Here is a list of them.

### Section: Table of Contents

```
   Overview
   Entity and Entity Elements and the Entity Resfile
   States: an Overview
   Correspondence File Format
   Secondary resfile
   STATE command
   STATE vector command
   State-Vector File Format
   Expression Syntax
     POSE_ENERGY command
     POSE_ENERGY_VECTOR command
   SCALAR_EXPRESSION command
   VECTOR_VARIABLE command
   VECTOR_EXPRESSION command
   ENTITY_FUNCTION command
   FITNESS command
```

### Section: Overview

This section describes the file format for the state-declaration and fitness-function definition file. It begins by describing what a state is within this framework, and how it is defined. It then goes into detail on each of the seven commands that compose this fitness-function definition file

There are seven commands in a fitness file:

```
STATE
STATE_VECTOR
SCALAR_EXPRESSION
VECTOR_VARIABLE
VECTOR_EXPRESSION
ENTITY_FUNCTION
FITNESS
```

Comments may be included by beginning a line in the fitness file with a sharp symbol ("\#") Any line that begins with a sharp symbol will be ignored. Blank lines are also ignored.

### Section: Entity and Entity Elements and the Entity Resfile

For historical reasons, each sequence that's examined by the genetic algorithm in the outer loop is called an "entity." Positions in this sequence are the entity elements; they range from 1 to N where N is the length of the entity (all entities have the same length). This nomenclature derives from the implementation of the genetic algorithm that Colin Smith and Justin Ashworth produced.

When writing a correspondence file (described in detail in section Correspondence File Format), residues on a particular structure are declared to correspond to particular positions in the entities optimized by the genetic algorithm. Position 10 of an entity of length 15 might be corresponded to by residue 15 on chain A in one state and corresponed to by residue 15 on chain B in another state.

When writing an entity function file (described in detail in section ENTITY\_FUNCTION Command), entity elements may be accessed by their position (e.g. entity element 10) within a particular entity.

To set up a multistate design run, the user must think about an abstract protein, the entity protein, which has one chain and N residues. The sequence of this protein is being optimized, but it's structure is never considered. Instead, the entity protein's sequence is assigned to the various states in the system as perscribed by the correspondence files. To describe the sequence space available to the entity protein, the user must define an entity resfile. The entity resfile controls the set of amino acids and rotamers built for every residue that's declared to correspond to a particular entity element. If residue 15 on chain A in a particular state is declared to correspond to entity-element 10, then the resfile commands for entity-element 10 taken from the entity resfile will be applied to residue 15.

The entity resfile is almost exactly like a regular resfile, (See the resfile file format documentation here: [[Resfile syntax and conventions|resfiles]]) except it is proceeded by one extra line, which declares the number of residues in the entity protein (N). Then the usual resfile format is followed: the default commands (if any) are listed, then "start" appears on one line by itself, then the specific commands for particular residues on the entity protein (resid 1..N, chain A) are listed. For example the following is a valid entity resfile:

```
16
ALLAAxc EX 1 EX ARO 2
start
3 A PIKAA ACDEFGH EX 1 EX ARO 2
```

In this entity resfile, there are 16 residues declared in the entity protein (and 16 positions for each entity the genetic algorithm will consider) and all but residue 3 (entity element \#3) in this protein has the behavior "ALLAAxc" (all amino acids allowed, except for cysteine) and the rotamer-building instructions of "EX 1 EX 2 ARO" which builds extra rotamers at chi1 for all amino acids, and extra rotamers at chi 2 for aromatic amino acids (FHWY).

Do not use NATRO or NATAA commands with entity resfiles. Internally, there is a temporary entity reference pose constructed while considering entity elements; the "native" for NATRO/NATAA will be drawn from this polyalanine reference and bad things will happen.

If residue 15 on chain A in a particular state is declared to correspond to entity element 10, then when that state goes to build rotamers for residue 15 on chain A, it will use the "ALLAAxc EX 1 EX ARO 2" command taken from the entity resfile.

### Section: States: an Overview

In this application, states are defined by three things:

-   a) a PDB file
-   b) a correspondence file
-   c) a secondary resfile

The PDB file should be obvious; it defines the backbone coordinates that will be repacked.

The correspondence file is described in detail in the next section.

The secondary resfile states which residues, besides those listed in the correspondence file, should be allowed to repack or redesign. Be very careful with this file, as it is possible to declare that all the rest of the residues in the PDB should be redesigned – this would be very expensive and would probably generate unwanted results.

### Section: Correspondence File Format

Correspondence files are a central glue to multistate design: they specify the meaning of the entity sequence. The entity resfile will specify how many positions are in the entity protein, but it does not give any meaning to those positions. Meaning is imbued on the entity elements by declaring how those positions are followed by particular residues in particular states. It is impossible to set up a multistate design problem with this software without carefully considering what is stated in the correspondence files so do not skip this step.

The correspondence file can contain an arbitrary number of lines, each line may be either blank or may be in the following format.

```
<entity-element-id> <residue-id ( insertion-code )> <chain-id>
```

where entity-element-id is an integer; the residue-id is an integer that may end in the insertion code (e.g. 15A) and should specify the PDB residue number (e.g. if the pdb begins at residue 10 and you're interested in the fifth residue in this PDB, and this residue is residue 14, then you say "14", not "5"); the chain-id is a character referring to a particular chain in the PDB or an underscore ("\_") if the chain is not labeled.

Example correspondence files: consider the heterodimerization task where two monomers are being designed such that the "A" monomer binds the "B" monomer, but not itself, and the "B" monomer binds the "A" monomer but not itself. Then there are five arrangements of these two monomers: the A monomer, the B monomer, the AB heterodimer, the AA homodimer, and the BB homodimer.

Let's say there are 3 positions on each monomer being designed. This gives 6 positions in the entity protein. Let's also impose the convention that the first three entity elements correspond to the "A" chemical species and the last three entity elements correspond to the "B" chemical species. Let's also assume that the three residues being designed are residues 10, 15, and 20 on chains A and B of the PDBs that we will model.

Then the following correspondence files will be used for each of the five arrangements

```
<begin A monomer correspondence file>
1 10 A
2 15 A
3 20 A
<end>
```

```
<begin B monomer correspondence file>
4 10 A
5 15 A
6 20 A
<end>
```

(Note that both monomers are modeled using a single-chain PDB that has only a chain A)

```
<begin AB heterodimer correspondence file>
1 10 A
2 15 A
3 20 A
4 10 B
5 15 B
6 20 B
<end>
```

```
<begin AA homodimer correspondence file>
1 10 A
2 15 A
3 20 A
1 10 B
2 15 B
3 20 B
<end>
```

(Note that residue 10 on both chain A and chain B both correspond to entity element 1: they must correspond to the same entity element, or these two chains could end up with different sequences, and then the "AA homodimer" would not be represented by an actual homodimer. The correspondence file is responsible for defining the fact that a chemical species will end up as a homodimer. That task is not handled by any other part of the multistate design framework.)

```
<begin BB homodimer correspondence file>
4 10 A
5 15 A
6 20 A
4 10 B
5 15 B
6 20 B
<end>
```

(Again, note that residue 10 on both chains A and B are assigned to correspond to the same entity element.)

### Section: Secondary resfile

The secondary resfile states which residues, besides those already listed in the correspondence file, should be allowed to repack. The secondary resfile is formatted exactly as a regular resfile is formatted (See the resfile file format documentation here: [[Resfile syntax and conventions|resfiles]]). The secondary resfile specifies behaviors for every residue in the protein, but, if a particular residue (residue 10, e.g.) is declared to correspond to a particular entity element (entity element 6, e.g.), then the resfile behavior for that entity element is used at that residue (entity-resfile behavior for entity-element 6 is applied to residue 10, e.g.) instead of the secondary resfile behavior. The fact that this resfile's behavior is superceded by the entity resfile at some positions is why it is called "secondary."

For example, continuing to build on the heterodimerization example described above, the following secondary resfile might be used to model (all) the dimer states:

```
<begin resfile>
NATRO
start
9  A NATAA EX 1 EX 2
11 A NATAA EX 1 EX 2
14 A NATAA EX 1 EX 2
16 A NATAA EX 1 EX 2
19 A NATAA EX 1 EX 2
21 A NATAA EX 1 EX 2
9  B NATAA EX 1 EX 2
11 B NATAA EX 1 EX 2
14 B NATAA EX 1 EX 2
16 B NATAA EX 1 EX 2
19 B NATAA EX 1 EX 2
21 B NATAA EX 1 EX 2
<end resfile>
```

which states that 6 additional residues on chains A and B (let's say these are the only immediate neighbors of the 3 residues (10,15,&20) being redesigned).

NOTE: The "NATRO" as the default command is critical in that it says that any residue not listed in this file or in the correspondence file should be held fixed. If no default command is given, then the "ALLAA" command is applied, meaning that all amino acids at all positions will be considered for every residue besides those listed in these two files. All positions would be optimized at every step in the multistate design protocol, and this would be very slow and consume tremendous amounts of memory.

### Section: STATE command

A state is defined by three files

-   1) a PDB file
-   2) a correspondence file
-   3) a secondary resfile

the STATE command has the following syntax:

```
STATE <varname> <pdbfilename> <correspondencefilename> <secondaryrefilename>
```

which declares one new state that should be modeled and declares the scalar variable, named "varname" which will be assigned the energy of the state after its rotamers have been packed with the a particular sequence.

### Section: STATE\_VECTOR command

The State vector command has the following syntax:

```
STATE_VECTOR <varname> <statevectorfile>
```

where this command declares a set of states, one for every line in the state vector file, and declares a vector variable named varname. The format of the state vector file is described in the next section.

### Section: State-Vector File Format

The file format for the state vector file is a set of ordered triples, one per line, consisting of a 1) pdb file name, 2) a correspondence file name, and 3) a secondary resfile file name. Each line is turned into a state. Internally, each line in this file is assigned a name based on which position in this file it appears. This is important when examining the output from multistate design.

E.g. If the fitness file contained this line:

```
<snip fitness file begin>
STATE_VECTOR ab_vect_variable AB.states
<end>
```

and the AB.states file contained:

```
<begin AB.states>
Conf1.pdb AB.corr AB.2res
Conf2.pdb AB.corr AB.2res
Conf3.pdb AB.corr AB.2res
<end AB.states>
```

then, at the conclusion of multistate design, if the file named "msd\_output\_1\_ab\_vect\_variable\_2.pdb" is created, then the "2" in this file name refers to the second state defined in the AB.states file, so you can know that the backbone conformation came from "Conf2.pdb". (the variable name for this states file is also used to name the file, "ab\_vect\_variable")

NOTE: there is nothing preventing the "same state" from being defined multiple times; e.g. the following is a valid state-vector file:

```
<begin AB.states>
Conf1.pdb AB.corr AB.2res
Conf1.pdb AB.corr AB.2res
Conf1.pdb AB.corr AB.2res
<end AB.states>
```

then, at the conclusion of multistate design, if the file named "msd\_output\_1\_ab\_vect\_variable\_2.pdb" is created, then the "2" in this file name refers to the second state defined in the AB.states file, so you can know that the backbone conformation came from "Conf2.pdb". (the variable name for this states file is also used to name the file, "ab\_vect\_variable")

NOTE: there is nothing preventing the "same state" from being defined multiple times; e.g. the following is a valid state-vector file:

```
<begin AB.states>
Conf1.pdb AB.corr AB.2res
Conf1.pdb AB.corr AB.2res
Conf1.pdb AB.corr AB.2res
<end AB.states>
```

In this case, three states with identical backbones, correspondence files, and secondary resfiles will be included in the design trajectory. This is useful when stochastic algorithms are used to pack the rotamers, so that a different stochastic trajectory will be used for each of the states. (The packing algorithms are all stochastic in this implementation of multistate design).

### Section: Expression Syntax

The SCALAR\_EXPRESSION, VECTOR\_VARIABLE, VECTOR\_EXPRESSION, and FITNESS commands depened on an "expression syntax." Expressions are recursively defined by the following production rules given in an EBNF-like syntax:

```
EXP ::= TERM (  ( '+' | '-' ) TERM )*
TERM ::= FACTOR ( ('*' | '/') FACTOR )*
FACTOR ::= '(' EXP ')' | variable | literal | function '(' EXP ( ',' EXP )* ')'
```

The purpose of the TERM and FACTOR components of this grammar is to encode the arithmetic precidence rules: multiplication and division take precidence over addition and subtraction. Parenthetical expressions and functions take the highest precidence. What does this mean? The expression "5 + 3 \* 2" would evaluate to 11 and not to 16 because the multiplication between 3 and 2 takes predicence over the addition between 5 and 3. The expression "(5 + 3) \* 2" on the other hand would evaluate to 16 and not to 11.

The available functions are: abs, exp, ln, vmax, vmin, pow, sqrt, eq, gt, gte, lt, lte, and, or, not, and ite. Some functions operate on scalar expressions, some operate on vector expressions; vector expressions should not be given as inputs to functions that operate on scalar expressions, and scalar expressions should not be given as inputs to functions that operate on vector expressions.

These functions (and parenthetically, the number of arguments they expect) are

```
abs  (1) : the absolute value of an expression (scalar -> scalar)
exp  (1) : the exponential function; exp(x) is e raised to the power of x. (scalar -> scalar)
ln   (1) : the natural logorithm of an expression (scalar -> scalar)
vmax (1) : the vector maximum (vector -> scalar)
vmin (1) : the vector minimum (vector -> scalar)
pow  (2) : raise the first argument to the power of the second argument.
           pow( x, y ) = x ^ y.  ( scalar X scalar -> scalar )
sqrt (1) : the square root function (scalar->scalar)
eq   (2) : pseudo-boolean equality function; returns 1.0 if its two arguments are equal
           and 0.0 otherwise (scalar X scalar -> scalar )
gt   (2) : pseudo-boolean greater-than function; returns 1.0 if the first argument is
           greater than the second argument ( scalar X scalar -> scalar )
gte  (2) : pseudo-boolean greater-than-or-equal-to function: returns 1.0 if the first
           argument is greater than or equal to the second argument ( scalar X scalar -> scalar )
lt   (2) : pseudo-boolean less-than function; returns 1.0 if the first argument is
           less than the second argument ( scalar X scalar -> scalar )
lte  (2) : pseudo-boolean less-than-or-equal-to function: returns 1.0 if the first
           argument is less than or equal to the second argument ( scalar X scalar -> scalar )
and  (2) : pseudo-boolean logical and: returns 1.0 if both the first and second arguments are
           non-zero and 0.0 otherwise (scalar X scalar -> scalar )
or   (2) : pseudo-boolean logical or: returns 1.0 if either the first or the second arguments
           are non-zero (or both) and 0.0 otherwise (scalar X scalar -> scalar )
not  (1) : pseudo-boolean locial not: returns 1.0 if its argument evaluates to 0.0 and 0.0 if
           its argument evalutes to a non-zero value (scalar X scalar -> scalar )
ite  (3) : if-then-else function: returns the value of the second argument if the first argument
           evaluates to a non-zero value, and returns the value of the third argument otherwise
           (scalar X scalar X scalar -> scalar )
```

Variable names: valid variable names must start with a character, but can include digits and underscores.

Literals are scientifically formatted numerals: e.g. "10" or "-2.3" or "6.4e-5"

CAUTION: include spaces around everything. The string "3-4" would tokenize as "Literal(3) Literal(-4)" and this would produce a (confusing!) parsing error. Instead, the string "3 - 4" would tokenize as "Literal(3) MinusSign Literal(4)" and this would parse correctly.

### Section: POSE\_ENERGY command

The POSE\_ENERGY command allows you to score a PDB file once at the beginning of execution and to include that score as a constant in subsequent calculations. This could be useful if you want to subtract-out the difference between two different backbone conformations for the same chemical species.

The syntax for the POSE\_ENERGY command is

```
POSE_ENERGY <varname> <pdb>
```

### Section: POSE\_ENERGY\_VECTOR command

Like the POSE\_ENERGY command, the POSE\_ENERGY\_VECTOR command allows you to score structures and to use those scores in subsequent calculations. This command creates a vector variable instead of a scalar variable, so it could for example be used in VECTOR\_EXPRESSION commands.

The syntax for the POSE\_ENERGY\_VECTOR command is

```
POSE_ENERGY_VECTOR <varname> <listfilename>
```

where the contents of \<listfilename\> should contain a list of pdbs, one per line. Empty lines and lines beginning with '\#' are ignored.

### Section: SCALAR\_EXPRESSION command

The syntax for the scalar expression command is:

```
SCALAR_EXPRESSION <varname> = <expression>
```

where expression is a well-formed expression, and this command declares the scalar variable named "varname" which can be used in later expressions.

### Section: VECTOR\_VARIABLE command

The vector variable command allows the user to combine scalar variables into vector variables. This could be useful if the user wants to use the "vector max" or "vector min" functions over a set of scalar variables. (To avoid confusion of between the vector max and min functions, the scalar max and min functions are not allowed.)

The syntax for this command is:

```
VECTOR_VARIABLE <varname> = <scalar_varname>( <scalar_varname2>*)
```

Where "\*" denotes zero or more occurances of a white-space separated variable name i.e. an arbitrarily long white-space delimited list of scalar variable names on the right-hand-side of the equals sign is allowed. This command will declare a new vector variable named "varname".

For example, if the scalar variables "best\_AA" and "best\_BB" were defined previously in the file, then the following command would declare a new vector variable "homodimers":

```
VECTOR_VARIABLE homodimers = bestAA bestBB
```

and then homodimers could be used in a subsequent vmin command, for example

```
SCALAR_EXPRESSION best_homodimer = vmin( homodimers )
```

### Section: VECTOR\_EXPRESSION command

The vector expression command can be used to define more complicated vector variable operations. The command itself is significantly more complicated than the scalar vector commands. It can operate on multiple vector variables simultaneously as long as those vector variables have the same length.

The command syntax is:

```
VECTOR_EXPRESSION FOR <localvar> IN <vectorvar>( , <localvar2> IN <vectorvar2>)* : <newvarname> = <expression>
```

where the iteration variables "localvar" and "localvar2" etc. are allowed to appear in the expression on the right hand side of the equals sign, but must not have been declared as non-local variables in any previous variable-declaring statement (i.e. they are allowed to have appeared as local variables in previous VECTOR\_EXPRESSION commands). This creates the new vector variable with the name "newvarname."

Example 1: if the vector variable "trpcage" had been previously declared, then a new vector variable could be declared as follows:

```
VECTOR_EXPRESSION FOR x IN trpcage : trpcage_plus_2 = x + 2
```

where every entry in this new vector variable, trpcage\_plus\_2, has a value 2 greater than its corresponding value in the original function.

Example 2: if the vector variables "trpcage1" and "trpcage2" both had the same length, then a new vector variable could be declared as follows:

```
VECTOR_EXPRESSION FOR x IN trpcage1 , y IN trpcage2 : trpcage_plus_2 = x + y + 2
```

Such a command would be useful if two state-vector files had been created such that the backbone conformations matched up between corresponding positions and you only wanted to compare energies between states that had come from the same backbone conformations.

### Section: ENTITY\_FUNCTION command

The ENTITY\_FUNCTION command is a launching point in the fitness file for the processing of an entirely separate file, an entity-function file. The ENTITY\_FUNCTON command merely declares a new scalar variable and points towards the entity-function file. Its syntax is

```
ENTITY_FUNCTION <entvarname> <entfuncfilename>
```

Given the entity (a sequence assignment), the functions defined in this file will be evaluated and their result will be stored in the new variable named "entvarname".

The entity funtion file is significantly more complicated, basically as complicated as the fitness function itself. Its purpose is to add sequence-level constraints into the fitness function; these constraints are able to look at the amino acid identities of the entity elements, but they have no access to the rotamer assignments of any of the states.

The entity element file predefines a set of entity-element variables whose values are determined by the given entity. These variables are named "ee\_1" to "ee\_\<N\>" (where N is the number of residues in the entity protein). For example, if there are 6 residues in the entity protein, then it is valid to refer to entity-element variables "ee\_1", "ee\_2", "ee\_3", "ee\_4", "ee\_5", and "ee\_6". These variables will hold the numeric values of the 1-letter codes (1 for "ALA/A", 2 for "CYS/C", 20 for "TYR/Y"), which may or may not serve useful. This means that it is possible to compare whether two entities have the same assigned amino acid (e.g. the expression "eq( ee\_1, ee\_4 )" would evaluate to 1.0 if these two positions in the entity are assigned the same amino acids). It is probably not useful to try and add or multiply these values.

There are four commands available in the entity function file:

```
AA_SET
SET_CONDITION
SUB_EXPRESSION (equivalent to the fitness-function file's "SCALAR_EXPRESSION" command)
```

and

```
SCORE (equivalent to the fitness-function file's "FITNESS" command).
```

The entity-function file must define exactly one "SCORE" command, and the value that the SCORE expression evaluates to is the one that will be assigned to the variable ("entvarname" above) in the fitness-function file.

The syntax for these four commands is as follows.

The AA\_SET command allows the user to define a set of amino acids, which may prove convenient.

```
AA_SET <setname> = { A1 (, A2)* }
```

which will declare a new named set of amino acids, called "setname", and "A1" and "A2" are one-letter amino-acid codes, in either lower or upper case. For example, the following set is valid:

```
AA_SET polar = { D, E, H, K, N, Q, R, S, T }
```

so that later, it is possible to ask if a particular entity element is a member of a set. To ask such a question, the SET\_CONDITION command must be used.

The syntax for the SET\_CONDITON command is:

```
SET_CONDITON <varname> = <ee_X> IN [ { A1 (, A2)* } | <setname> ]
```

which will declare a psueduo-boolean scalar variable named "varname" whose value will be 1.0 if the set condition holds and 0.0 otherwise. For example, both of the following SET\_CONDITON commands are valid:

```
SET_CONDITON ee1_is_aromatic = ee1 IN { H, P, Y, W }
SET_CONDITON ee1_is_polar = ee1 IN polar
```

assuming that the set "polar" has been previously declared.

The SUB\_EXPRESSION command defines a scalar expression in the same way the "SCALAR\_EXPRESSION" command of the FITNESS\_FUNCTION does. There are no vector expressions available to the entity function. The syntax for this command is:

```
SUB_EXPRESSION <varname> = <expression>
```

where the new variable "varname" should not have been declared previously in the entity-function file, but may have been declared in the fitness-function file (the variables between these two files are not shared).

for example, continuing the heterodimerization task example from above, to count the net charge on chain A, (assuming a pH of 7 and that besides the three residues being designed, chain A has a net charge of 0), the following commands could be combined:

```
<begin entity-function file snippet>

AA_SET pos_aas = { K, R }
AA_SET neg_aas = { D, E }

SET_CONDITON ee1_pos = ee_1 in pos_aas
SET_CONDITON ee2_pos = ee_2 in pos_aas
SET_CONDITON ee3_pos = ee_3 in pos_aas
SET_CONDITON ee1_neg = ee_1 in neg_aas
SET_CONDITON ee2_neg = ee_2 in neg_aas
SET_CONDITON ee3_neg = ee_3 in neg_aas

SUB_EXPRESSION chainA_charge = ee1_pos + ee2_pos + ee3_pos - ee1_neg - ee2_neg - ee3_neg

<end snippet>
```

The syntax for the SCORE command, which must appear once in the file, is:

```
SCORE <expression>
```

for example,

```
SCORE -1 * lt( chainA_charge, 1 )
```

would return a negative value (which is favored) if chain A has a neutral or negative charge.

### Section: FITNESS command

Exactly one FITNESS command must appear in the fitness function file. The value to which the expression defined in the FITNESS command evaluates for a particular sequence is the one that is used by the genetic algorithm to determine whether that sequence should be propagated to the next generation.

NOTE: The lower the fitness the better. This may seem contrary to the way genetic algorithms usually operate, but makes sense in the general context of minimizing energies.

The syntax for the FITNESS command is:

```
FITNESS <expression>
```

Limitations
===========

Please see the discussion section in [Leaver-Fay et al.2011] describing three important problems in negative design: rotamer-optimization failure, the missing rotamer problem, and the fixed-backbone assumption.

Options
=======

Options to control the genetic algorithm's behavior

```
-ms::pop_size <int>                    # Recommended: 100
-ms::generations <int>                 # Recommended: 15 * the number of residues being designed (i.e. the number on the first line of the entity resfile)
-ms::fraction_by_recombination <float> # Recommended: 0.02
-ms::numresults <int>                  # the number of output sequences to write out
```

Options specific to the mpi\_msd application

```
-msd::entity_resfile <filename>        # Required: The name of the entity resfile
-msd::fitness_file   <filename>        # Required: The name of the fitness-function definition file
-msd::double_lazy_ig_mem_limit <int>   # Limit the amount of memory that each state will consume representing the rotamer pair energies.  In MB.
-msd::dont_score_bbhbonds              # Do not count bb/bb hbonds -- not recommended.
-msd::exclude_background_energies      # Do not include the energies of "background residues" into the state energies -- not recommended.
```

Tips
====

-   The flags -ex1 and -ex2 are recommended to get well-packed output structures, or they can be given on a per-residue basis in the entity resfile or in the secondary resfiles with the "EX 1 EX 2" resfile commands.

Expected Outputs
================

Output files are written for each of the states that contribute to the fitness function. The output files are named as a concattenation of

-   1) the prefix "msd\_output\_"
-   2) the variable name given to either the STATE or STATE\_VECTOR variable used to declare that state
-   3) the index of the design (i.e. if -ms::numresults 10 were given, then 10 sets of output structures would be written)
-   and 4) the index of the state (which, if declared in a STATE\_VECTOR command, will identify its position in the input .states file)

The logic for which states contribute to the fitness function is fairly simple:

In binary arithmetic expressions, e.g. `       exp1 + exp2      ` , the set of states that contribute to exp1 and to exp2 are all are considered to contribute. In logical selection expressions, e.g. ite( lt( exp1, exp2 ), exp3, exp4 ), the set of states that contributed to the expression exp3 would be considered to contribute if exp1 was less than exp2, otherwise, the set of states that contribute to exp4 are considered to contribute. NOTE: states that contribute to exp1 or exp2 are not considered to contribute, unless of course they contribute to expression (either the exp3 or exp4 expression) that is chosen. Note: it is often better to multiply the energy of a state or a variable by 0 than to deselect that variable or state; states whose energies are multiplied by zero but that otherwise are considered to contribute to the fitness will still have output pdbs generated for them. In the "vmax" and "vmin" expressions that select one value among a set of many, only the states that contribute to that one selected value are considered to contribute to the expression.

Post Processing
===============

If you are using multistate design to design against a particular conformation or interaction, then it will likely have introduced collisions, and it is very likely that those collisions could be resolved by relaxing that conformation. It depends of course on your design problem what kind of post-processing relaxation you should perform. For an example of using docking to relax protien/protein interfaces, look to the Supplemental Material of the [Leaver-Fay, et al., 2001] paper.

New Features In Rosetta3.3
==========================

This is the first time that the "mpi\_msd" multistate design application has been released.

##See Also

* [[Design applications | design-applications]]: other design applications
* [[ProteinInterfaceMSMover]]: RosettaScripts mover for multistate design
* [[Application Documentation]]: Application documentation home page
* [[Running Rosetta with options]]: Instructions for running Rosetta executables.
* [[Analyzing Results]]: Tips for analyzing results generated using Rosetta
* [[Rosetta on different scales]]: Guidelines for how to scale your Rosetta runs
* [[Preparing structures]]: How to prepare structures for use in Rosetta