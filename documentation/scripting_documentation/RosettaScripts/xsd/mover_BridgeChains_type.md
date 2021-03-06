<!-- THIS IS AN AUTOGENERATED FILE: Don't edit it directly, instead change the schema definition in the code itself. -->

_Autogenerated Tag Syntax Documentation:_

---
Given a pose with a jump, this mover uses a fragment insertion monte carlo to connect the specified termini. The new fragment will connect the C-terminal residue of jump1 to the N-terminal residue of jump2, and will have secondary structure and ramachandran space given by "motif." This mover uses the VarLengthBuild code. The input pose must have at least two chains (jumps) to connect, or it will fail.

```xml
<BridgeChains name="(&string;)" chain1="(&non_negative_integer;)"
        chain2="(&non_negative_integer;)" segment1="(&string;)"
        segment2="(&string;)" motif="(&string;)" cutpoint="(&string;)"
        ideal_abego="(false &bool;)" extend_ss="(true &bool;)"
        overlap="(&non_negative_integer;)" dry_run="(&non_negative_integer;)"
        trials="(&non_negative_integer;)" scorefxn="(&string;)" />
```

-   **chain1**: Indicates the chain which is to be located at the N-terminal end of the new fragment. Building will begin at the C-terminal residue of the jump.
-   **chain2**: Indicates the chain which is to be located at the C-terminal end of the new fragment.
-   **segment1**: XSD XRW: TO DO
-   **segment2**: XSD XRW: TO DO
-   **motif**: The secondary structure + abego to be used for the backbone region to be rebuilt. Taken from input pose if not specified. The format of this string is: "[Length][SS][ABEGO]-[Length2][SS2][ABEGO2]-...-[LengthN][SSN][ABEGON]" For example, "1LX-5HA-1LB-1LA-1LB-6EB" will build a one residue loop of any abego, followed by a 5-residue helix, followed by a 3-residue loop of ABEGO BAB, followed by a 6-residue strand.
-   **cutpoint**: XSD XRW: TO DO
-   **ideal_abego**: XSD XRW: TO DO
-   **extend_ss**: XSD XRW: TO DO
-   **overlap**: Build overlap of nested BuildDeNovoBackboneMover
-   **dry_run**: Sets folder of BuildDeNovoBackboneMover to RandomTorsionPoseFolder instead of RemodelLoopMoverPoseFolde
-   **trials**: iterations per phase of nested BuildDeNovoBackboneMover
-   **scorefxn**: Name of score function to use

---
