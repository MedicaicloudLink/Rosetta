<!-- THIS IS AN AUTOGENERATED FILE: Don't edit it directly, instead change the schema definition in the code itself. -->

_Autogenerated Tag Syntax Documentation:_

---
Calculate the direction of the concave face of a strand, and its curvarure based on the angle formed by alternating CA atoms. The "levels" are the number of CA pais left between the vertex and the end of the arm.

```xml
<StrandCurvatureByLevels name="(&string;)" bend_level="(1 &positive_integer;)"
        min_bend="(0.0 &real;)" max_bend="(180 &real;)"
        twist_level="(1 &positive_integer;)" min_twist="(0.0 &real;)"
        max_twist="(90 &real;)" StrandID="(1 &positive_integer;)"
        output_type="(bend &string;)"
        concavity_reference_residue="(first &string;)"
        concavity_direction="(true &xs:boolean;)" blueprint="(&string;)"
        confidence="(1.0 &real;)" />
```

-   **bend_level**: Number of CA pairs left between the vertex and the end of the arm used to calculate the bend angle.
-   **min_bend**: Minimum allowed for bend angle, the complementary to CA1-CA2-CA3, so that the higher this angle, the more curved the strand is. Value is in degrees.
-   **max_bend**: Maximum allowed for bend angle, the complementary to CA1-CA2-CA3, so that the higher this angle, the more curved the strand is. Value is in degrees.
-   **twist_level**: Number of CA-CB vecgor pairs left between the vertex and the end of the arm used to calculate the twist angle (dihedral between two CA-CB vetors).
-   **min_twist**: Minimum allowed for strand twist angle, the complementary to a CA-CB - CA-CB dihedral, so that the higher this angle, the more twisted the strand is. Value is in degrees.
-   **max_twist**: Maximum allowed for strand twist angle, the complementary to a CA-CB - CA-CB dihedral, so that the higher this angle, the more twisted the strand is. Value is in degrees.
-   **StrandID**: Strand number over which to calculate bend, twist and concavity, according to blueprint numbers
-   **output_type**: What magnitude to inform: "bend" or "twist" angle
-   **concavity_reference_residue**: Use first or last residue as a reference for concavity: "first" or "last" are the only options. See "concavity_direction" for more info on how to use this option. Defaults should be fine for regular (not bulged) strands.
-   **concavity_direction**: Does the reference residue CA-CB vector point in the same face as the residue in the center of the strand? If yes, then set this option to true, otherwise, false. Defaults should be fine for regular (not bulged) strands.
-   **blueprint**: path to blueprint file from which to parse strands
-   **confidence**: Probability that the pose will be filtered out if it does not pass this Filter

---