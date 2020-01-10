#FoldTree overview and concepts

Metadata
========
This document was written 1 Oct 2007 by Ian W. Davis and last updated 1 Oct 2007.

An introductory tutorial on the FoldTree can be found [here](https://www.rosettacommons.org/demos/latest/tutorials/fold_tree/fold_tree).

The relevant code is in core::kinematics. 

The fold tree
=============

The fold tree is a coarse-grained representation of [[the atom tree|atomtree-overview]], and is implemented in the FoldTree class. The fold tree specifies connection between residues, which in most (all?) cases is enough to generate an atom tree behind the scenes.

Jump edges vs. "peptide" edges
==============================

The fold tree stores connections between residues as core::kinematics::Edge objects. Order is significant; "start" is the parent and "stop" is the child. (This means start may have a higher sequence number than stop, however.) Jump edges correspond directly to jumps in the atom tree, and connect one atom on one residue to another atom on another residue. (In practice, the first atom of each appears to be often used for convenience's sake.) The so-called "peptide" edges, on the other hand, specify polymer connectivity in shorthand form – they give just the start and end residues of a polymer chain. No atoms are specified.

For example, a single polypeptide chain of 100 residues folded N to C would have a single fold tree edge, from 1 to 100. If instead it was folded from the middle out, it would have two edges, from 50 to 1 and from 50 to 100. If it also had a small molecule attached to residue 33, that would be an additional edge, from 33 to 101 (assuming the ligand residue comes after the 100 protein residues.)

The fold tree is smart enough to merge two peptide edges into one, e.g. (1-50) + (50-100) = (1-100), but it may require a manual call to delete\_extra\_vertices().

Cutpoints
=========

"Cutpoints" are discontinuities in the fold tree (when compared to the polymer connectivity). All residues are considered cutpoints except those that are internal to a peptide edge or at the lower-numbered end of a peptide edge. The higher-numbered end of a peptide edge (whether parent or child) *is* considered a cutpoint. Thus, the highest numbered residue in the tree is *always* a cutpoint; however, it is not considered a cutpoint when counting the number of cutpoints in the tree. Assuming a connected tree, then, the number of cutpoints is always the same as the number of jump edges.

FoldTree output
===============

a typical fold-tree output looks like this: (as used by silent-files)

```
FOLD_TREE  EDGE 1 14 -1  JEDGE 14 52 1 C N  INTRA_RES_STUB  EDGE 14 42 -1  EDGE 52 47 -1  EDGE 52 58 -1  JEDGE 58 136 2 C N  INTRA_RES_STUB  JEDGE 47 118 4 N N  INTRA_RES_STUB  EDGE 47 43 -1  EDGE 118 124 -1  EDGE 136 125 -1  EDGE 58 67 -1  EDGE 118 117 -1  EDGE 136 143 -1  JEDGE 117 73 3 N N  INTRA_RES_STUB  EDGE 73 68 -1  EDGE 73 98 -1  EDGE 117 99 -1
```

EDGE are normal edges

JEDGE are jump edges

EDGE start end jump_nr/peptide(=-1) start-jump_atom stop-jump_atom "INTRA_RES_STUB" -- > build the atom-tree such that motion of torsions in this residue does not mess up your jump.

References
==========

For a complete discussion of different fold tree topologies and their uses, see Wang, Bradley, and Baker, JMB 2007.
