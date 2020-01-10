#Residue Energy Breakdown

Metadata
========

This document was edited Aug 9th 2013 by Rocco Moretti. This application was created and documented by Rocco Moretti, et al.

Description of algorithm
========================

Occasionally one would like to know how the structural energy breaks down into the interactions between the various residues. (For example, which two residues are giving a large clash/repulsive score, or if a particular salt bridge is contributing as much to the score as you think it might.) This utility takes one or more structure files (pdbs or silent files) and then outputs as a whitespace-seperated scorefile the residue-by-residue breakdown on a per-scoreterm level. Also included are the "one body" energies of each residue, which includes the "two body" interaction of the residue with itself.

If individual residue-pair interactions are not needed, the application per\_residue\_energies outputs a similar scoretable where energies are summed at a per-residue level. (Similar to the per residue scoring table at the end of a Rosetta PDB, but in a conveniently formatted form.)

Limitations and Caveats
=======================

Residue numbering in the output is by the internal ("pose numbering") scheme, rather than by PDB-type numbering.

While the total (weighted) score should be the same as the sum of the individual terms, and the (weighted) per residue energies (e.g. as reported in the annotated output PDB or by the per\_residue\_energies utility) should be the sum of the reported one body energies and half of the sum of the reported residue pair enegies, rounding errors may mean that these sums may not match up. Additionally, while most common energy terms are reported, currently the application may not accurately report results from energy terms calculated based on "whole structure" contexts.

Input
=====

Observes standard input flags (e.g. -in:file:s, -in:file:silent, etc.). Multiple input structures may be specified.

The name of the output file is specified with -out:file:silent

Scorefunction weights files are specified with -score:weights and -score:patch, as usual.

Output
===============

A whitespace-separated table of the *weighted* score breakdown. Residue numbering is in internal sequential format ("pose numbering"). Each residue pair is listed once, with the lower numbered residue as residue 1. For one body energies, the second residue is listed as "--", with type "onebody"

Sample Output:

```
SCORE:     pose_id      resi1    restype1      resi2    restype2     fa_atr     fa_rep     fa_sol    fa_intra_rep   ...        ref      total   description
SCORE:  input.pdb       1          MET          --      onebody       0.000      0.000      0.000           0.003   ...     -0.340      0.480   input.pdb_1_onebody
SCORE:  input.pdb       2          ASP          --      onebody       0.000      0.000      0.000           0.003   ...     -0.670     -0.132   input.pdb_2_onebody
...
SCORE:  input.pdb       1          MET           2          ASP      -0.715      0.059      0.870           0.000   ...      0.000      0.214   input.pdb_1_2
SCORE:  input.pdb       1          MET           3          MET      -0.430      0.000      0.287           0.000   ...      0.000     -0.143   input.pdb_1_3
```

Command Lines
====================

Sample command:

```
residue_energy_breakdown.linuxgccrelease -database ~/rosetta/main/database -in:file:s input.pdb -out:file:silent energy_breakdown.out
```

Example
=======

See the integration test at rosetta/main/tests/integration/tests/residue\_energy\_breakdown for an example.


##See Also

* [[Analysis applications | analysis-applications]]: other design applications
* [[Application Documentation]]: Application documentation home page
* [[Running Rosetta with options]]: Instructions for running Rosetta executables.
* [[Analyzing Results]]: Tips for analyzing results generated using Rosetta
* [[Rosetta on different scales]]: Guidelines for how to scale your Rosetta runs

