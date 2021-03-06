<!-- THIS IS AN AUTOGENERATED FILE: Don't edit it directly, instead change the schema definition in the code itself. -->

_Autogenerated Tag Syntax Documentation:_

---
Extracts and analyzes beta sandwiches.

```xml
<SandwichFeatures name="(&string;)"
        min_num_strands_to_deal="(4 &non_negative_integer;)"
        max_num_strands_to_deal="(140 &non_negative_integer;)"
        min_res_in_strand="(2 &non_negative_integer;)"
        min_CA_CA_dis="(3.5 &real;)" max_CA_CA_dis="(6.2 &real;)"
        min_N_O_dis_between_sheets="(3.3 &real;)"
        min_N_H_O_angle_between_two_sheets="(154.0 &real;)"
        min_C_O_N_angle="(120.0 &real;)" min_sheet_dis="(7.0 &real;)"
        max_sheet_dis="(15.0 &real;)"
        max_sheet_angle_with_cen_res="(130.0 &real;)"
        min_sheet_angle_by_four_term_cen_res="(25.0 &real;)"
        max_sheet_angle_by_four_term_cen_res="(150.0 &real;)"
        min_sheet_torsion_cen_res="(-150.0 &real;)"
        max_sheet_torsion_cen_res="(150.0 &real;)"
        min_num_strands_in_sheet="(2 &non_negative_integer;)"
        min_inter_sheet_dis_CA_CA="(4.0 &real;)"
        max_inter_sheet_dis_CA_CA="(24.0 &real;)"
        max_inter_strand_angle_to_not_be_same_direction_strands="(120.0 &real;)"
        max_abs_inter_strand_dihedral_to_not_be_same_direction_strands="(100.0 &real;)"
        max_num_sw_per_pdb="(100 &non_negative_integer;)"
        check_N_to_C_direction_by="(PE &string;)"
        check_canonicalness_cutoff="(80.0 &real;)"
        count_AA_with_direction="(false &bool;)"
        do_not_write_resfile_of_sandwich_that_has_non_canonical_LR="(false &bool;)"
        exclude_desinated_pdbs="(false &bool;)"
        exclude_sandwich_that_has_near_backbone_atoms_between_sheets="(false &bool;)"
        exclude_sandwich_that_has_non_canonical_LR="(false &bool;)"
        exclude_sandwich_that_has_non_canonical_properties="(false &bool;)"
        exclude_sandwich_that_has_non_canonical_shortest_dis_between_facing_aro_in_sw="(false &bool;)"
        exclude_sandwich_that_is_linked_w_same_direction_strand="(false &bool;)"
        exclude_sandwich_that_is_suspected_to_have_not_facing_2_sheets="(true &bool;)"
        exclude_sandwich_with_SS_bond="(false &bool;)"
        max_starting_loop_size="(6 &non_negative_integer;)"
        max_ending_loop_size="(6 &non_negative_integer;)"
        max_E_in_extracted_sw_loop="(10 &non_negative_integer;)"
        max_H_in_extracted_sw_loop="(100 &non_negative_integer;)"
        no_helix_in_pdb="(false &bool;)"
        inter_sheet_distance_to_see_whether_a_sheet_is_surrounded_by_other_sheets="(13.0 &real;)"
        allowed_deviation_for_turn_type_id="(40.0 &real;)"
        primary_seq_distance_cutoff_for_beta_sheet_capping_before_N_term_capping="(2 &integer;)"
        primary_seq_distance_cutoff_for_beta_sheet_capping_after_N_term_capping="(0 &integer;)"
        primary_seq_distance_cutoff_for_beta_sheet_capping_before_C_term_capping="(0 &integer;)"
        primary_seq_distance_cutoff_for_beta_sheet_capping_after_C_term_capping="(2 &integer;)"
        distance_cutoff_for_electrostatic_interactions="(7.0 &real;)"
        CB_b_factor_cutoff_for_electrostatic_interactions="(10000 &real;)"
        min_primary_seq_distance_diff_electrostatic_interactions="(4 &non_negative_integer;)"
        do_not_connect_sheets_by_loops="(false &bool;)"
        extract_sandwich="(true &bool;)"
        wt_for_pro_in_starting_loop="(20 &real;)"
        wt_for_pro_in_1st_inter_sheet_loop="(10 &real;)"
        wt_for_pro_in_3rd_inter_sheet_loop="(1 &real;)"
        write_all_info_files="(false &bool;)"
        write_AA_kind_files="(false &bool;)"
        write_loop_AA_distribution_files="(false &bool;)"
        write_strand_AA_distribution_files="(false &bool;)"
        write_beta_sheet_capping_info="(false &bool;)"
        write_chain_B_resnum="(false &bool;)"
        write_electrostatic_interactions_of_all_residues="(false &bool;)"
        write_electrostatic_interactions_of_all_residues_in_a_strand="(false &bool;)"
        write_electrostatic_interactions_of_surface_residues_in_a_strand="(false &bool;)"
        write_heading_directions_of_all_AA_in_a_strand="(false &bool;)"
        write_p_aa_pp_files="(false &bool;)"
        write_phi_psi_of_all="(false &bool;)"
        write_phi_psi_of_E="(false &bool;)"
        write_rama_at_AA_to_files="(false &bool;)"
        write_resfile="(false &bool;)"
        write_resfile_NOT_FWY_on_surface="(false &bool;)"
        write_resfile_when_seq_rec_is_bad="(false &bool;)"
        write_resfile_to_minimize_too_many_core_heading_FWY_on_core_strands="(false &bool;)"
        write_resfile_to_minimize_too_many_core_heading_FWY_on_edge_strands="(false &bool;)"
        write_resfile_to_minimize_too_much_hydrophobic_surface="(false &bool;)" />
```

-   **min_num_strands_to_deal**: Minimum number of strands in beta sandwich
-   **max_num_strands_to_deal**: Maximum number of strands to consider
-   **min_res_in_strand**: Minimum number of residues per beta strand to count it
-   **min_CA_CA_dis**: Minimum Calpha distance between strands (in Angstroms)
-   **max_CA_CA_dis**: Maximum Calpha distance between strands (in Angstroms)
-   **min_N_O_dis_between_sheets**: Minimum N-O distance between beta sheets
-   **min_N_H_O_angle_between_two_sheets**: Minimum N-H-O distance between two beta sheets
-   **min_C_O_N_angle**: Minimum C-O-N angle for a hydrogen bond between strands
-   **min_sheet_dis**: Minimum distance between beta sheets in a sandwich (in Angstroms)
-   **max_sheet_dis**: Maximum distance between beta sheets in a sandwich (in Angstroms)
-   **max_sheet_angle_with_cen_res**: Maximum angle of the plane of the beta sheet with its center residue (defines the twist of the sheet)
-   **min_sheet_angle_by_four_term_cen_res**: Minimum sheet angle defined by the four terminal residues and central residue
-   **max_sheet_angle_by_four_term_cen_res**: Maximum sheet angle defined by the four terminal residues and central residue
-   **min_sheet_torsion_cen_res**: Minimum sheet torsion about the center residue
-   **max_sheet_torsion_cen_res**: Maximum sheet torsion about the central residue
-   **min_num_strands_in_sheet**: Minimum number of strands per beta sheet
-   **min_inter_sheet_dis_CA_CA**: Minimum Calpha distance between sheets
-   **max_inter_sheet_dis_CA_CA**: Maximum Calpha distance between sheets
-   **max_inter_strand_angle_to_not_be_same_direction_strands**: Maximum angle between strands at which the strands will still be considered parallel
-   **max_abs_inter_strand_dihedral_to_not_be_same_direction_strands**: Maximum absolute value of the interstrand dihedral angle for them to be considered antiparallel
-   **max_num_sw_per_pdb**: Maximum number of sandwiches per PDB
-   **check_N_to_C_direction_by**: How to determine the N to C direction?
-   **check_canonicalness_cutoff**: Cutoff for considering a sandwich canonical
-   **count_AA_with_direction**: Count amino acids in each direction?
-   **do_not_write_resfile_of_sandwich_that_has_non_canonical_LR**: Do not write resfile of beta sandwiches with non-canonical loop regions
-   **exclude_desinated_pdbs**: Exclude designated PDBs
-   **exclude_sandwich_that_has_near_backbone_atoms_between_sheets**: Exclude beta sandwiches with nearby backbone atoms between the two sheets
-   **exclude_sandwich_that_has_non_canonical_LR**: Exclude sandwiches with noncanonical loop regions
-   **exclude_sandwich_that_has_non_canonical_properties**: Exclude noncanonical beta sandwiches
-   **exclude_sandwich_that_has_non_canonical_shortest_dis_between_facing_aro_in_sw**: Exclude sandwiches with noncanonical shortest distances between aromatics in the sandwich
-   **exclude_sandwich_that_is_linked_w_same_direction_strand**: Exclude sanwiches linked with a parallel strand?
-   **exclude_sandwich_that_is_suspected_to_have_not_facing_2_sheets**: Exclude sandwiches that do not have two facing sheets?
-   **exclude_sandwich_with_SS_bond**: Exclude disulfide bonded sandwiches?
-   **max_starting_loop_size**: Maximum starting loop size
-   **max_ending_loop_size**: Maximum ending loop size
-   **max_E_in_extracted_sw_loop**: Maximum strand size in extracted sandwich loop
-   **max_H_in_extracted_sw_loop**: Maximum strand size in the extracted sandwich loop
-   **no_helix_in_pdb**: Do not include sandwiches with helices in the PDB
-   **inter_sheet_distance_to_see_whether_a_sheet_is_surrounded_by_other_sheets**: Minimum cutoff for inter-sheet distance for a sheet to be associated with surrounding sheets. WIthin this distance, they are too close to each other.
-   **allowed_deviation_for_turn_type_id**: Maximum deviation for turn type id
-   **primary_seq_distance_cutoff_for_beta_sheet_capping_before_N_term_capping**: maximum distance from beta sheet capping and N terminus
-   **primary_seq_distance_cutoff_for_beta_sheet_capping_after_N_term_capping**: How many residues after the N terminal capping residues must beta sheet capping residues occur
-   **primary_seq_distance_cutoff_for_beta_sheet_capping_before_C_term_capping**: How many residues before the C terminal capping residues must beta sheet capping residues occur
-   **primary_seq_distance_cutoff_for_beta_sheet_capping_after_C_term_capping**: How many residues after the C terminal capping residues must beta sheet capping residues occur
-   **distance_cutoff_for_electrostatic_interactions**: Only score electrostatic interactions within this distance
-   **CB_b_factor_cutoff_for_electrostatic_interactions**: Cbeta B factor above which electrostatics will not be scored
-   **min_primary_seq_distance_diff_electrostatic_interactions**: Minimum primary sequence distance between residues for electrostatics to be scored
-   **do_not_connect_sheets_by_loops**: Do not connect sheets with loops
-   **extract_sandwich**: Extract beta sandwiches
-   **wt_for_pro_in_starting_loop**: Keep wild type residue for prolines in initial loop
-   **wt_for_pro_in_1st_inter_sheet_loop**: Keep wild type residues for prolines in the first intersheet loop
-   **wt_for_pro_in_3rd_inter_sheet_loop**: Keep wild type prolines in the third intersheet loop
-   **write_all_info_files**: Write all information to files
-   **write_AA_kind_files**: Write files specifying amino acid distribution
-   **write_loop_AA_distribution_files**: Write files specifying loop amino acid distribution
-   **write_strand_AA_distribution_files**: Write files specifying beta strand amino acid distribution
-   **write_beta_sheet_capping_info**: Output information on beta sheet capping
-   **write_chain_B_resnum**: Write chain_B_resnum file for InterfaceAnalyzer
-   **write_electrostatic_interactions_of_all_residues**: Output info on all electrostatic interactions
-   **write_electrostatic_interactions_of_all_residues_in_a_strand**: Output info for strand electrostatic interactions
-   **write_electrostatic_interactions_of_surface_residues_in_a_strand**: Output surface electrostatic interactions from strands
-   **write_heading_directions_of_all_AA_in_a_strand**: Output strand direction for all strand residues
-   **write_p_aa_pp_files**: Output p_aa_pp score for residues
-   **write_phi_psi_of_all**: Output all phi and psi angles
-   **write_phi_psi_of_E**: Output strand backbone torsion angles
-   **write_rama_at_AA_to_files**: Output all rama scores to file
-   **write_resfile**: Output a resfile for this strand
-   **write_resfile_NOT_FWY_on_surface**: Do not allow aromatics on the sandwich surface
-   **write_resfile_when_seq_rec_is_bad**: Output resfile even if sequence recovery is bad
-   **write_resfile_to_minimize_too_many_core_heading_FWY_on_core_strands**: Write a resfile that will help prevent having too many internal-facing aromatics on core strands
-   **write_resfile_to_minimize_too_many_core_heading_FWY_on_edge_strands**: Write a resfile that will help prevent having too many internal-facing aromatics on core strands
-   **write_resfile_to_minimize_too_much_hydrophobic_surface**: Write a resfile that will minimize surface hydrophobic residues

---
