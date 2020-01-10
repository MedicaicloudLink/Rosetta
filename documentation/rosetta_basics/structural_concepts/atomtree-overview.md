#AtomTree overview and concepts

Metadata
========

This document was written 18 Sep 2007 by Ian W. Davis and last updated 18 Sep 2007.

The relevant code is in core::kinematics and core::kinematics::tree. ("Kinematics" deals with conversion between Cartesian coordinates and [[internal coordinates]], often discussed in terms of robot arms.) 



Handedness and conventions
==========================

Rosetta uses a right-handed coordinate system, just as PDB files do: if the X axis points to the right and the Y axis points up, the Z axis points out of the screen towards you. Rotation matrices premultiply the coordinates they transform, i.e. Mv = v'. Matrices are 3x3 (rather than 4x4 "homogenous" coordinates) so rotation and translation are specified as two separate objects, a matrix and a vector.

The atom tree
=============

The atom tree is a data structure for tracking and manipulating both the Cartesian (xyz) and internal (length, angle, dihedral) coordinates of atoms, while keeping the two in sync. It is a tree in the computer science / graph theory sense: there is one atom that acts as the root of the tree, and all other atoms have exactly one parent and zero or more children.

Trees cannot contain cycles, so any cyclic structures must be "cut" by omitting a bond. This comes up most often in terms of ring structures (Phe, Tyr, Trp, His, nucleic acid bases), though long-range connections like disulfide bonds also introduce cycles.

Stubs
=====

A "stub" is a local coordinate system. Every atom in the atom tree has an associated stub, which defines a local coordinate system with said atom at the origin. A Stub object contains the position of the stub atom in the global coordinate frame, and a 3x3 rotation matrix that rotates coordinates from the local frame to the global frame. (For going the other way, the stub atom is always at the origin of the stub coordinate system and the transpose of the matrix rotates coordinates from the global to the local frame.)

A stub is usually defined by three atoms. The first marks the origin, the second sets the direction of the X axis, and the third sets the X-Y plane and the general direction of the Y axis. (Cross products are used to make truly orthogonal axes.)

Jumps
=====

Mathematically, a jump contains the same information as a stub: the rotation and translation to go from one coordinate system to another. However, stubs relate a local coordinate system to the global one, whereas jumps relate two local coordinate systems to each other. That is, a jump specifies the six-degree-of-freedom, rigid-body position and orientation of one group with respect to another. Jumps generally specify the relationship between chemically independent groups, such as a protein and its (nonbonded) ligand.

JumpAtoms vs. BondedAtoms
=========================

The root of the atom tree and the pseudo-root of each new independent group (e.g. a ligand associated with a protein) is a JumpAtom, related to its parent in the tree (or to the global origin, in the case of the root) by a Jump. All other atoms are BondedAtoms, the descendents of some JumpAtom and connected to it by a series of chemical bonds. The position of each bonded atom is specified in terms of spherical coordinates relative to its parent's coordinate frame (stub).

Every bonded atom needs to know about two stubs: its "input stub", which is set up by its parent and is the system in which its internal coordinates are specified; and its own stub with itself at the origin, which it sets up for determining the input stubs of its children.

[[Internal coordinates]], the simple case
=====================================

The simplest case arises in a long, unbranched chain of atoms, such as in a straight-chain hydrocarbon or lysine sidechain. Consider child atom A with parent B, B's parent C, and C's parent D. The internal coordinates of A are given as the distance, d, from B; the angle, theta, between vectors C-\>B and B-\>A (i.e., 180 - theta is the bond angle C-B-A); and the torsion angle A-B-C-D, which is called phi.

An equivalent statement is that d, theta, and phi specify the position of A in spherical coordinates, relative to its input stub coordinate system. The vector from the origin to A is d units long, it forms an angle theta with the X axis, and its projection into the Y-Z plane forms an angle phi with the Y axis. Put another way, to construct the position of A, start with a vector d units long along the positive X axis, rotate by theta around the Z axis (putting it somewhere in the X-Y plane), and then rotate by phi around the X axis to get the final position.

Another equivalent statement is that to generate A's stub from B's stub (A's input stub), postmultiply B's stub matrix, M\_B, with matrices to rotate around X by phi and around Z by theta, then translate B's stub center, v\_B, by that matrix times the vector (d,0,0). That is,

```
M_A = M_B * M_phi * M_theta
v_A = v_B + M_A * (d,0,0)
```

Now A's stub center, v\_A, is the position of atom A in the global coordinate frame.

Internal coordinates, the previous sibling trick
================================================

Things get more complicated in a branched chain, when a parent atom has more than one child (e.g. the end of a leucine sidechain). The internal coordinates for the first child are handled as above. However, the dihedral angle for each subsequent child is specified relative to its previous sibling, as an improper torsion. E.g. for child A2 the dihedral is A2-B-C-A1; for child A3 the dihedral is A3-B-C-A2; etc.

This means that each child of the parent has a different input stub, which is derived from the parent's stub by rotation (rotation == 0 for the first child though). For this reason, when updating the Cartesian coordinates of bonded atoms, the parent's stub is actually modified by each child in turn, which applies its phi rotation BEFORE copying the Stub object and applying the other changes to generate its own stub (which will in turn be modified by its children).

This means that the input\_stub\_atoms of the first child are the same as the stub\_atoms of the parent, but input\_stub\_atom3 of the subsequent children is not equal to the parent's stub\_atom3.

The reason for this elaborate set-up is efficiency. The most common operation in Rosetta is to rotate around some bond, but if every atom's dihedral were defined relative to its great-grandparent, then rotating around a bond would require updating the dihedral coordinate for all of the children of the child atom in the bond. By specifying most dihedrals as rotional OFFSETS from the previous one, all the children "ride" on one another and only the dihedral of the first of them need be updated to accomplish a rotation around the bond. This is especially important for e.g. minimizing torsions, where we allow the first child's phi to vary and hold the other children's phis fixed; without the previous sibling trick, all those degrees of freedom would have to be free and yet be tethered to one another!

Special cases
=============

Special cases arise near the root of the tree or immediately after a jump, when we lack a (bonded) great-grandparent atom to define the stub coordinate frame. Read the code for details, but basically the first three atoms in the tree end up acting as stub reference points for each other. In at least one case this means the stub needs to be "flipped" because it points the wrong way – up the atom tree instead of down.

Special cases also arise with very small groups (1 or 2) of bonded atoms that are connected to the rest of the structure only by jumps. The local group of bonded atoms is insufficient to define a coordinate system even with the special case rules above, so some parts of the stub are taken from the parent atom on the originating side of the jump.

If a parent has some children attached by jumps and others attached by bonds, all the jump children come before the bonded children. Since jumps don't modify their parent's stubs when calculating their coordinates, this keeps the jump groups from rotating when the parent - grandparent bond does.
