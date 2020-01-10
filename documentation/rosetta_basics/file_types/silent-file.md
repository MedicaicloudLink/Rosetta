<!-- --- title: Silent File -->

Silent file is a compact format file which stores information from multiple structures. Silent files can be generated by many Rosetta simulations. Such as Abintio and Ligand Docking. You also can use silent file tools to combine regular pdb files to be binary format silent file to save computer storage spaces. We will briefly introduce silent file format here.

Protein Silent Struct is usually seen in the Abinitio outputs.

-   Header

    ```
    SEQUENCE                      Structure sequences presented by one letter amino acid code
    SCORE                         Rosetta score terms
    ```

-   Body

    ```
    Colunms 1-4                   Residue sequence number
    Colunms 5-7                   Seconary structure one letter code
    Colunms 8-17                  Phi angle
    Colunms 18-26                 Psi angle
    Colunms 27-35                 Omega angle
    Colunms 36-44                 CA atom coordinates x
    Colunms 45-53                 CA atom coordinates y
    Colunms 55-62                 CA atom coordinates z
    Colunms 64-98                 Chi angle real data if possible
    ```

Binary Silent Struct File is very useful to compress multiple pdbs and save computer spaces.

-   Header

    ```
    SEQUENCE                      Structure sequences presented by one letter amino acid code
    SCORE                         Rosetta score terms
    ```

-   Body

    ```
    Binary contents
    ```

Silent files can be converted into PDB files using the `extract_pdbs` application. Information about the options can be found in the integration test folder /Rosetta/main/tests/integration/tests/extract_pdbs/.

##See Also

* [[File types list]]: List of file types used in Rosetta
* [[Input options]]: Command line options for Rosetta input
* [[Output options]]: Command line options for Rosetta output
* [[Rosetta Basics]]: The Rosetta Basics home page
* [[Options overview]]: Overview of Rosetta command line options
* [[Glossary]]: Brief definitions of Rosetta terms
* [[RosettaEncyclopedia]]: Detailed descriptions of additional concepts in Rosetta.
* [[Rosetta overview]]: Overview of major concepts in Rosetta
* [[Application Documentation]]: Links to documentation for a variety of Rosetta applications