/*
 * Copyright (c) 2014 Tim Walker <tdskywalker@gmail.com>
 *
 * This file is part of FFmpeg.
 *
 * VPS/SPS/PPS decoders
 *
 * Based on hevc_ps.c from ffmpeg (www.ffmpeg.org)
 *
 * Copyright (C) 2012 - 2103 Guillaume Martres
 * Copyright (C) 2012 - 2103 Mickael Raulet
 * Copyright (C) 2012 - 2013 Gildas Cocherel
 * Copyright (C) 2013 Vittorio Giovara
 *
 * This file is part of FFmpeg.
 *
 * slice header decoder
 *
 * Copyright (C) 2012 - 2013 Guillaume Martres
 *
 * This file is part of FFmpeg.
 *
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "tvheadend.h"
#include "parsers.h"
#include "parser_hevc.h"
#include "parser_avc.h"
#include "bitstream.h"
#include "service.h"

#define IS_IRAP_NAL(nal_type) (nal_type >= 16 && nal_type <= 23)

#define MAX_REFS 16
#define MAX_SPATIAL_SEGMENTATION 4096 // max. value of u(12) field
#define MAX_SUB_LAYERS 7
#define MAX_VPS_COUNT 16
#define MAX_SPS_COUNT 32
#define MAX_PPS_COUNT 256
#define MAX_SHORT_TERM_RPS_COUNT 64

#define B_SLICE 0
#define P_SLICE 1
#define I_SLICE 2

typedef struct HVCCNALUnitArray {
    uint8_t  NAL_unit_type;
    uint16_t numNalus;
    uint16_t *nalUnitLength;
    uint8_t  **nalUnit;
} HVCCNALUnitArray;

typedef struct HEVCDecoderConfigurationRecord {
    uint8_t  configurationVersion;
    uint8_t  general_profile_space;
    uint8_t  general_tier_flag;
    uint8_t  general_profile_idc;
    uint32_t general_profile_compatibility_flags;
    uint64_t general_constraint_indicator_flags;
    uint8_t  general_level_idc;
    uint16_t min_spatial_segmentation_idc;
    uint8_t  parallelismType;
    uint8_t  chromaFormat;
    uint8_t  bitDepthLumaMinus8;
    uint8_t  bitDepthChromaMinus8;
    uint16_t avgFrameRate;
    uint8_t  constantFrameRate;
    uint8_t  numTemporalLayers;
    uint8_t  temporalIdNested;
    uint8_t  lengthSizeMinusOne;
    uint8_t  numOfArrays;
    HVCCNALUnitArray *array;
} HEVCDecoderConfigurationRecord;

typedef struct HVCCProfileTierLevel {
    uint8_t  profile_space;
    uint8_t  tier_flag;
    uint8_t  profile_idc;
    uint32_t profile_compatibility_flags;
    uint64_t constraint_indicator_flags;
    uint8_t  level_idc;
} HVCCProfileTierLevel;

static void hvcc_update_ptl(HEVCDecoderConfigurationRecord *hvcc,
                            HVCCProfileTierLevel *ptl)
{
    /*
     * The value of general_profile_space in all the parameter sets must be
     * identical.
     */
    hvcc->general_profile_space = ptl->profile_space;

    /*
     * The level indication general_level_idc must indicate a level of
     * capability equal to or greater than the highest level indicated for the
     * highest tier in all the parameter sets.
     */
    if (hvcc->general_tier_flag < ptl->tier_flag)
        hvcc->general_level_idc = ptl->level_idc;
    else
        hvcc->general_level_idc = MAX(hvcc->general_level_idc, ptl->level_idc);

    /*
     * The tier indication general_tier_flag must indicate a tier equal to or
     * greater than the highest tier indicated in all the parameter sets.
     */
    hvcc->general_tier_flag = MAX(hvcc->general_tier_flag, ptl->tier_flag);

    /*
     * The profile indication general_profile_idc must indicate a profile to
     * which the stream associated with this configuration record conforms.
     *
     * If the sequence parameter sets are marked with different profiles, then
     * the stream may need examination to determine which profile, if any, the
     * entire stream conforms to. If the entire stream is not examined, or the
     * examination reveals that there is no profile to which the entire stream
     * conforms, then the entire stream must be split into two or more
     * sub-streams with separate configuration records in which these rules can
     * be met.
     *
     * Note: set the profile to the highest value for the sake of simplicity.
     */
    hvcc->general_profile_idc = MAX(hvcc->general_profile_idc, ptl->profile_idc);

    /*
     * Each bit in general_profile_compatibility_flags may only be set if all
     * the parameter sets set that bit.
     */
    hvcc->general_profile_compatibility_flags &= ptl->profile_compatibility_flags;

    /*
     * Each bit in general_constraint_indicator_flags may only be set if all
     * the parameter sets set that bit.
     */
    hvcc->general_constraint_indicator_flags &= ptl->constraint_indicator_flags;
}

static void hvcc_parse_ptl(bitstream_t *gb,
                           HEVCDecoderConfigurationRecord *hvcc,
                           unsigned int max_sub_layers_minus1)
{
    unsigned int i;
    HVCCProfileTierLevel general_ptl;
    uint8_t sub_layer_profile_present_flag[MAX_SUB_LAYERS];
    uint8_t sub_layer_level_present_flag[MAX_SUB_LAYERS];

    general_ptl.profile_space               = read_bits(gb, 2);
    general_ptl.tier_flag                   = read_bits1(gb);
    general_ptl.profile_idc                 = read_bits(gb, 5);
    general_ptl.profile_compatibility_flags = read_bits(gb, 32);
    general_ptl.constraint_indicator_flags  = read_bits64(gb, 48);
    general_ptl.level_idc                   = read_bits(gb, 8);
    if (hvcc)
        hvcc_update_ptl(hvcc, &general_ptl);

    for (i = 0; i < max_sub_layers_minus1; i++) {
        sub_layer_profile_present_flag[i] = read_bits1(gb);
        sub_layer_level_present_flag[i]   = read_bits1(gb);
    }

    if (max_sub_layers_minus1 > 0)
        for (i = max_sub_layers_minus1; i < 8; i++)
            skip_bits(gb, 2); // reserved_zero_2bits[i]

    for (i = 0; i < max_sub_layers_minus1; i++) {
        if (sub_layer_profile_present_flag[i]) {
            /*
             * sub_layer_profile_space[i]                     u(2)
             * sub_layer_tier_flag[i]                         u(1)
             * sub_layer_profile_idc[i]                       u(5)
             * sub_layer_profile_compatibility_flag[i][0..31] u(32)
             * sub_layer_progressive_source_flag[i]           u(1)
             * sub_layer_interlaced_source_flag[i]            u(1)
             * sub_layer_non_packed_constraint_flag[i]        u(1)
             * sub_layer_frame_only_constraint_flag[i]        u(1)
             * sub_layer_reserved_zero_44bits[i]              u(44)
             */
            skip_bits(gb, 32);
            skip_bits(gb, 32);
            skip_bits(gb, 24);
        }

        if (sub_layer_level_present_flag[i])
            skip_bits(gb, 8);
    }
}

static void skip_sub_layer_hrd_parameters(bitstream_t *gb,
                                          unsigned int cpb_cnt_minus1,
                                          uint8_t sub_pic_hrd_params_present_flag)
{
    unsigned int i;

    for (i = 0; i <= cpb_cnt_minus1; i++) {
        read_golomb_ue(gb); // bit_rate_value_minus1
        read_golomb_ue(gb); // cpb_size_value_minus1

        if (sub_pic_hrd_params_present_flag) {
            read_golomb_ue(gb); // cpb_size_du_value_minus1
            read_golomb_ue(gb); // bit_rate_du_value_minus1
        }

        skip_bits1(gb); // cbr_flag
    }
}

static int skip_hrd_parameters(bitstream_t *gb, uint8_t cprms_present_flag,
                                unsigned int max_sub_layers_minus1)
{
    unsigned int i;
    uint8_t sub_pic_hrd_params_present_flag = 0;
    uint8_t nal_hrd_parameters_present_flag = 0;
    uint8_t vcl_hrd_parameters_present_flag = 0;

    if (cprms_present_flag) {
        nal_hrd_parameters_present_flag = read_bits1(gb);
        vcl_hrd_parameters_present_flag = read_bits1(gb);

        if (nal_hrd_parameters_present_flag ||
            vcl_hrd_parameters_present_flag) {
            sub_pic_hrd_params_present_flag = read_bits1(gb);

            if (sub_pic_hrd_params_present_flag)
                /*
                 * tick_divisor_minus2                          u(8)
                 * du_cpb_removal_delay_increment_length_minus1 u(5)
                 * sub_pic_cpb_params_in_pic_timing_sei_flag    u(1)
                 * dpb_output_delay_du_length_minus1            u(5)
                 */
                skip_bits(gb, 19);

            /*
             * bit_rate_scale u(4)
             * cpb_size_scale u(4)
             */
            skip_bits(gb, 8);

            if (sub_pic_hrd_params_present_flag)
                skip_bits(gb, 4); // cpb_size_du_scale

            /*
             * initial_cpb_removal_delay_length_minus1 u(5)
             * au_cpb_removal_delay_length_minus1      u(5)
             * dpb_output_delay_length_minus1          u(5)
             */
            skip_bits(gb, 15);
        }
    }

    for (i = 0; i <= max_sub_layers_minus1; i++) {
        unsigned int cpb_cnt_minus1            = 0;
        uint8_t low_delay_hrd_flag             = 0;
        uint8_t fixed_pic_rate_within_cvs_flag = 0;
        uint8_t fixed_pic_rate_general_flag    = read_bits1(gb);

        if (!fixed_pic_rate_general_flag)
            fixed_pic_rate_within_cvs_flag = read_bits1(gb);

        if (fixed_pic_rate_within_cvs_flag)
            read_golomb_ue(gb); // elemental_duration_in_tc_minus1
        else
            low_delay_hrd_flag = read_bits1(gb);

        if (!low_delay_hrd_flag) {
            cpb_cnt_minus1 = read_golomb_ue(gb);
            if (cpb_cnt_minus1 > 31)
                return -1;
        }

        if (nal_hrd_parameters_present_flag)
            skip_sub_layer_hrd_parameters(gb, cpb_cnt_minus1,
                                          sub_pic_hrd_params_present_flag);

        if (vcl_hrd_parameters_present_flag)
            skip_sub_layer_hrd_parameters(gb, cpb_cnt_minus1,
                                          sub_pic_hrd_params_present_flag);
    }

    return 0;
}

static void skip_timing_info(bitstream_t *gb)
{
    skip_bits(gb, 32); // num_units_in_tick
    skip_bits(gb, 32); // time_scale

    if (read_bits1(gb))          // poc_proportional_to_timing_flag
        read_golomb_ue(gb); // num_ticks_poc_diff_one_minus1
}

static void hvcc_parse_vui(bitstream_t *gb,
                           HEVCDecoderConfigurationRecord *hvcc,
                           unsigned int max_sub_layers_minus1)
{
    unsigned int min_spatial_segmentation_idc;

    if (read_bits1(gb))              // aspect_ratio_info_present_flag
        if (read_bits(gb, 8) == 255) // aspect_ratio_idc
            skip_bits(gb, 32); // sar_width u(16), sar_height u(16)

    if (read_bits1(gb))  // overscan_info_present_flag
        skip_bits1(gb); // overscan_appropriate_flag

    if (read_bits1(gb)) {  // video_signal_type_present_flag
        skip_bits(gb, 4); // video_format u(3), video_full_range_flag u(1)

        if (read_bits1(gb)) // colour_description_present_flag
            /*
             * colour_primaries         u(8)
             * transfer_characteristics u(8)
             * matrix_coeffs            u(8)
             */
            skip_bits(gb, 24);
    }

    if (read_bits1(gb)) {        // chroma_loc_info_present_flag
        read_golomb_ue(gb); // chroma_sample_loc_type_top_field
        read_golomb_ue(gb); // chroma_sample_loc_type_bottom_field
    }

    /*
     * neutral_chroma_indication_flag u(1)
     * field_seq_flag                 u(1)
     * frame_field_info_present_flag  u(1)
     */
    skip_bits(gb, 3);

    if (read_bits1(gb)) {        // default_display_window_flag
        read_golomb_ue(gb); // def_disp_win_left_offset
        read_golomb_ue(gb); // def_disp_win_right_offset
        read_golomb_ue(gb); // def_disp_win_top_offset
        read_golomb_ue(gb); // def_disp_win_bottom_offset
    }

    if (read_bits1(gb)) { // vui_timing_info_present_flag
        skip_timing_info(gb);

        if (read_bits1(gb)) // vui_hrd_parameters_present_flag
            skip_hrd_parameters(gb, 1, max_sub_layers_minus1);
    }

    if (read_bits1(gb)) { // bitstream_restriction_flag
        /*
         * tiles_fixed_structure_flag              u(1)
         * motion_vectors_over_pic_boundaries_flag u(1)
         * restricted_ref_pic_lists_flag           u(1)
         */
        skip_bits(gb, 3);

        min_spatial_segmentation_idc = read_golomb_ue(gb);

        /*
         * unsigned int(12) min_spatial_segmentation_idc;
         *
         * The min_spatial_segmentation_idc indication must indicate a level of
         * spatial segmentation equal to or less than the lowest level of
         * spatial segmentation indicated in all the parameter sets.
         */
        hvcc->min_spatial_segmentation_idc = MIN(hvcc->min_spatial_segmentation_idc,
                                                 min_spatial_segmentation_idc);

        read_golomb_ue(gb); // max_bytes_per_pic_denom
        read_golomb_ue(gb); // max_bits_per_min_cu_denom
        read_golomb_ue(gb); // log2_max_mv_length_horizontal
        read_golomb_ue(gb); // log2_max_mv_length_vertical
    }
}

static void skip_sub_layer_ordering_info(bitstream_t *gb)
{
    read_golomb_ue(gb); // max_dec_pic_buffering_minus1
    read_golomb_ue(gb); // max_num_reorder_pics
    read_golomb_ue(gb); // max_latency_increase_plus1
}

static int hvcc_parse_vps(bitstream_t *gb,
                          HEVCDecoderConfigurationRecord *hvcc)
{
    unsigned int vps_max_sub_layers_minus1;

    /*
     * vps_video_parameter_set_id u(4)
     * vps_reserved_three_2bits   u(2)
     * vps_max_layers_minus1      u(6)
     */
    skip_bits(gb, 12);

    vps_max_sub_layers_minus1 = read_bits(gb, 3);

    /*
     * numTemporalLayers greater than 1 indicates that the stream to which this
     * configuration record applies is temporally scalable and the contained
     * number of temporal layers (also referred to as temporal sub-layer or
     * sub-layer in ISO/IEC 23008-2) is equal to numTemporalLayers. Value 1
     * indicates that the stream is not temporally scalable. Value 0 indicates
     * that it is unknown whether the stream is temporally scalable.
     */
    hvcc->numTemporalLayers = MAX(hvcc->numTemporalLayers,
                                  vps_max_sub_layers_minus1 + 1);

    /*
     * vps_temporal_id_nesting_flag u(1)
     * vps_reserved_0xffff_16bits   u(16)
     */
    skip_bits(gb, 17);

    hvcc_parse_ptl(gb, hvcc, vps_max_sub_layers_minus1);

    /* nothing useful for hvcC past this point */
    return 0;
}

static void skip_scaling_list_data(bitstream_t *gb)
{
    int i, j, k, num_coeffs;

    for (i = 0; i < 4; i++)
        for (j = 0; j < (i == 3 ? 2 : 6); j++)
            if (!read_bits1(gb))         // scaling_list_pred_mode_flag[i][j]
                read_golomb_ue(gb); // scaling_list_pred_matrix_id_delta[i][j]
            else {
                num_coeffs = MIN(64, 1 << (4 + (i << 1)));

                if (i > 1)
                    read_golomb_se(gb); // scaling_list_dc_coef_minus8[i-2][j]

                for (k = 0; k < num_coeffs; k++)
                    read_golomb_se(gb); // scaling_list_delta_coef
            }
}

static int parse_rps(bitstream_t *gb, unsigned int rps_idx,
                     unsigned int num_rps,
                     unsigned int num_delta_pocs[MAX_SHORT_TERM_RPS_COUNT])
{
    unsigned int i;

    if (rps_idx && read_bits1(gb)) { // inter_ref_pic_set_prediction_flag
        /* this should only happen for slice headers, and this isn't one */
        if (rps_idx >= num_rps)
            return -1;

        skip_bits1        (gb); // delta_rps_sign
        read_golomb_ue(gb); // abs_delta_rps_minus1

        num_delta_pocs[rps_idx] = 0;

        /*
         * From libavcodec/hevc_ps.c:
         *
         * if (is_slice_header) {
         *    //foo
         * } else
         *     rps_ridx = &sps->st_rps[rps - sps->st_rps - 1];
         *
         * where:
         * rps:             &sps->st_rps[rps_idx]
         * sps->st_rps:     &sps->st_rps[0]
         * is_slice_header: rps_idx == num_rps
         *
         * thus:
         * if (num_rps != rps_idx)
         *     rps_ridx = &sps->st_rps[rps_idx - 1];
         *
         * NumDeltaPocs[RefRpsIdx]: num_delta_pocs[rps_idx - 1]
         */
        for (i = 0; i <= num_delta_pocs[rps_idx - 1]; i++) {
            uint8_t use_delta_flag = 0;
            uint8_t used_by_curr_pic_flag = read_bits1(gb);
            if (!used_by_curr_pic_flag)
                use_delta_flag = read_bits1(gb);

            if (used_by_curr_pic_flag || use_delta_flag)
                num_delta_pocs[rps_idx]++;
        }
    } else {
        unsigned int num_negative_pics = read_golomb_ue(gb);
        unsigned int num_positive_pics = read_golomb_ue(gb);

        if ((num_positive_pics + (uint64_t)num_negative_pics) * 2 > remaining_bits(gb))
            return -1;

        num_delta_pocs[rps_idx] = num_negative_pics + num_positive_pics;

        for (i = 0; i < num_negative_pics; i++) {
            read_golomb_ue(gb); // delta_poc_s0_minus1[rps_idx]
            skip_bits1        (gb); // used_by_curr_pic_s0_flag[rps_idx]
        }

        for (i = 0; i < num_positive_pics; i++) {
            read_golomb_ue(gb); // delta_poc_s1_minus1[rps_idx]
            skip_bits1        (gb); // used_by_curr_pic_s1_flag[rps_idx]
        }
    }

    return 0;
}

static int hvcc_parse_sps(bitstream_t *gb,
                          HEVCDecoderConfigurationRecord *hvcc)
{
    unsigned int i, sps_max_sub_layers_minus1, log2_max_pic_order_cnt_lsb_minus4;
    unsigned int num_short_term_ref_pic_sets, num_delta_pocs[MAX_SHORT_TERM_RPS_COUNT];

    skip_bits(gb, 4); // sps_video_parameter_set_id

    sps_max_sub_layers_minus1 = read_bits (gb, 3);

    /*
     * numTemporalLayers greater than 1 indicates that the stream to which this
     * configuration record applies is temporally scalable and the contained
     * number of temporal layers (also referred to as temporal sub-layer or
     * sub-layer in ISO/IEC 23008-2) is equal to numTemporalLayers. Value 1
     * indicates that the stream is not temporally scalable. Value 0 indicates
     * that it is unknown whether the stream is temporally scalable.
     */
    hvcc->numTemporalLayers = MAX(hvcc->numTemporalLayers,
                                  sps_max_sub_layers_minus1 + 1);

    hvcc->temporalIdNested = read_bits1(gb);

    hvcc_parse_ptl(gb, hvcc, sps_max_sub_layers_minus1);

    read_golomb_ue(gb); // sps_seq_parameter_set_id

    hvcc->chromaFormat = read_golomb_ue(gb);

    if (hvcc->chromaFormat == 3)
        skip_bits1(gb); // separate_colour_plane_flag

    read_golomb_ue(gb); // pic_width_in_luma_samples
    read_golomb_ue(gb); // pic_height_in_luma_samples

    if (read_bits1(gb)) {   // conformance_window_flag
        read_golomb_ue(gb); // conf_win_left_offset
        read_golomb_ue(gb); // conf_win_right_offset
        read_golomb_ue(gb); // conf_win_top_offset
        read_golomb_ue(gb); // conf_win_bottom_offset
    }

    hvcc->bitDepthLumaMinus8          = read_golomb_ue(gb);
    hvcc->bitDepthChromaMinus8        = read_golomb_ue(gb);
    log2_max_pic_order_cnt_lsb_minus4 = read_golomb_ue(gb);

    /* sps_sub_layer_ordering_info_present_flag */
    i = read_bits1(gb) ? 0 : sps_max_sub_layers_minus1;
    for (; i <= sps_max_sub_layers_minus1; i++)
        skip_sub_layer_ordering_info(gb);

    read_golomb_ue(gb); // log2_min_luma_coding_block_size_minus3
    read_golomb_ue(gb); // log2_diff_max_min_luma_coding_block_size
    read_golomb_ue(gb); // log2_min_transform_block_size_minus2
    read_golomb_ue(gb); // log2_diff_max_min_transform_block_size
    read_golomb_ue(gb); // max_transform_hierarchy_depth_inter
    read_golomb_ue(gb); // max_transform_hierarchy_depth_intra

    if (read_bits1(gb) && // scaling_list_enabled_flag
        read_bits1(gb))   // sps_scaling_list_data_present_flag
        skip_scaling_list_data(gb);

    skip_bits1(gb); // amp_enabled_flag
    skip_bits1(gb); // sample_adaptive_offset_enabled_flag

    if (read_bits1(gb)) {           // pcm_enabled_flag
        skip_bits         (gb, 4); // pcm_sample_bit_depth_luma_minus1
        skip_bits         (gb, 4); // pcm_sample_bit_depth_chroma_minus1
        read_golomb_ue(gb);    // log2_min_pcm_luma_coding_block_size_minus3
        read_golomb_ue(gb);    // log2_diff_max_min_pcm_luma_coding_block_size
        skip_bits1        (gb);    // pcm_loop_filter_disabled_flag
    }

    num_short_term_ref_pic_sets = read_golomb_ue(gb);
    if (num_short_term_ref_pic_sets > MAX_SHORT_TERM_RPS_COUNT)
        return -1;

    for (i = 0; i < num_short_term_ref_pic_sets; i++) {
        int ret = parse_rps(gb, i, num_short_term_ref_pic_sets, num_delta_pocs);
        if (ret < 0)
            return ret;
    }

    if (read_bits1(gb)) {                               // long_term_ref_pics_present_flag
        unsigned num_long_term_ref_pics_sps = read_golomb_ue(gb);
        if (num_long_term_ref_pics_sps > 31U)
            return -1;
        for (i = 0; i < num_long_term_ref_pics_sps; i++) { // num_long_term_ref_pics_sps
            int len = MIN(log2_max_pic_order_cnt_lsb_minus4 + 4, 16);
            skip_bits (gb, len); // lt_ref_pic_poc_lsb_sps[i]
            skip_bits1(gb);      // used_by_curr_pic_lt_sps_flag[i]
        }
    }

    skip_bits1(gb); // sps_temporal_mvp_enabled_flag
    skip_bits1(gb); // strong_intra_smoothing_enabled_flag

    if (read_bits1(gb)) // vui_parameters_present_flag
        hvcc_parse_vui(gb, hvcc, sps_max_sub_layers_minus1);

    /* nothing useful for hvcC past this point */
    return 0;
}

static int hvcc_parse_pps(bitstream_t *gb,
                          HEVCDecoderConfigurationRecord *hvcc)
{
    uint8_t tiles_enabled_flag, entropy_coding_sync_enabled_flag;

    read_golomb_ue(gb); // pps_pic_parameter_set_id
    read_golomb_ue(gb); // pps_seq_parameter_set_id

    /*
     * dependent_slice_segments_enabled_flag u(1)
     * output_flag_present_flag              u(1)
     * num_extra_slice_header_bits           u(3)
     * sign_data_hiding_enabled_flag         u(1)
     * cabac_init_present_flag               u(1)
     */
    skip_bits(gb, 7);

    read_golomb_ue(gb); // num_ref_idx_l0_default_active_minus1
    read_golomb_ue(gb); // num_ref_idx_l1_default_active_minus1
    read_golomb_se(gb); // init_qp_minus26

    /*
     * constrained_intra_pred_flag u(1)
     * transform_skip_enabled_flag u(1)
     */
    skip_bits(gb, 2);

    if (read_bits1(gb))          // cu_qp_delta_enabled_flag
        read_golomb_ue(gb); // diff_cu_qp_delta_depth

    read_golomb_se(gb); // pps_cb_qp_offset
    read_golomb_se(gb); // pps_cr_qp_offset

    /*
     * pps_slice_chroma_qp_offsets_present_flag u(1)
     * weighted_pred_flag               u(1)
     * weighted_bipred_flag             u(1)
     * transquant_bypass_enabled_flag   u(1)
     */
    skip_bits(gb, 4);

    tiles_enabled_flag               = read_bits1(gb);
    entropy_coding_sync_enabled_flag = read_bits1(gb);

    if (entropy_coding_sync_enabled_flag && tiles_enabled_flag)
        hvcc->parallelismType = 0; // mixed-type parallel decoding
    else if (entropy_coding_sync_enabled_flag)
        hvcc->parallelismType = 3; // wavefront-based parallel decoding
    else if (tiles_enabled_flag)
        hvcc->parallelismType = 2; // tile-based parallel decoding
    else
        hvcc->parallelismType = 1; // slice-based parallel decoding

    /* nothing useful for hvcC past this point */
    return 0;
}

static uint8_t *nal_unit_extract_rbsp(const uint8_t *src, uint32_t src_len,
                                      uint32_t *dst_len)
{
    uint8_t *dst;
    uint32_t i, len;

    dst = malloc(src_len);
    if (!dst)
        return NULL;

    /* NAL unit header (2 bytes) */
    i = len = 0;
    while (i < 2 && i < src_len)
        dst[len++] = src[i++];

    while (i + 2 < src_len)
        if (!src[i] && !src[i + 1] && src[i + 2] == 3) {
            dst[len++] = src[i++];
            dst[len++] = src[i++];
            i++; // remove emulation_prevention_three_byte
        } else
            dst[len++] = src[i++];

    while (i < src_len)
        dst[len++] = src[i++];

    *dst_len = len;
    return dst;
}



static void nal_unit_parse_header(bitstream_t *gb, uint8_t *nal_type)
{
    skip_bits1(gb); // forbidden_zero_bit

    *nal_type = read_bits(gb, 6);

    /*
     * nuh_layer_id          u(6)
     * nuh_temporal_id_plus1 u(3)
     */
    skip_bits(gb, 9);
}

static int hvcc_array_add_nal_unit(uint8_t *nal_buf, uint32_t nal_size,
                                   uint8_t nal_type,
                                   HEVCDecoderConfigurationRecord *hvcc)
{
    uint8_t index;
    uint16_t numNalus;
    HVCCNALUnitArray *array;
    void *n;

    for (index = 0; index < hvcc->numOfArrays; index++)
        if (hvcc->array[index].NAL_unit_type == nal_type)
            break;

    if (index >= hvcc->numOfArrays) {
        uint8_t i;

        n = realloc(hvcc->array, (index + 1) * sizeof(HVCCNALUnitArray));
        if (n == NULL)
            return -1;
        hvcc->array = n;

        for (i = hvcc->numOfArrays; i <= index; i++)
            memset(&hvcc->array[i], 0, sizeof(HVCCNALUnitArray));
        hvcc->numOfArrays = index + 1;
    }

    array    = &hvcc->array[index];
    numNalus = array->numNalus;

    n = realloc(array->nalUnit, (numNalus + 1) * sizeof(uint8_t*));
    if (n == NULL)
        return -1;
    array->nalUnit = n;

    n = realloc(array->nalUnitLength, (numNalus + 1) * sizeof(uint16_t));
    if (n == NULL)
        return -1;
    array->nalUnitLength = n;

    array->nalUnit      [numNalus] = nal_buf;
    array->nalUnitLength[numNalus] = nal_size;
    array->NAL_unit_type           = nal_type;
    array->numNalus++;

    return 0;
}

static int hvcc_add_nal_unit(uint8_t *nal_buf, uint32_t nal_size,
                             HEVCDecoderConfigurationRecord *hvcc)
{
    int ret = 0;
    bitstream_t bs;
    uint8_t nal_type;
    uint8_t *rbsp_buf;
    uint32_t rbsp_size;

    rbsp_buf = nal_unit_extract_rbsp(nal_buf, nal_size, &rbsp_size);
    if (!rbsp_buf) {
        ret = -1;
        goto end;
    }

    ret = init_rbits(&bs, rbsp_buf, rbsp_size);
    if (ret < 0)
        goto end;

    nal_unit_parse_header(&bs, &nal_type);

    /*
     * Note: only 'declarative' SEI messages are allowed in
     * hvcC. Perhaps the SEI playload type should be checked
     * and non-declarative SEI messages discarded?
     */
    switch (nal_type) {
    case HEVC_NAL_VPS:
    case HEVC_NAL_SPS:
    case HEVC_NAL_PPS:
    case HEVC_NAL_SEI_PREFIX:
    case HEVC_NAL_SEI_SUFFIX:
        ret = hvcc_array_add_nal_unit(nal_buf, nal_size, nal_type, hvcc);
        if (ret < 0)
            goto end;
        else if (nal_type == HEVC_NAL_VPS)
            ret = hvcc_parse_vps(&bs, hvcc);
        else if (nal_type == HEVC_NAL_SPS)
            ret = hvcc_parse_sps(&bs, hvcc);
        else if (nal_type == HEVC_NAL_PPS)
            ret = hvcc_parse_pps(&bs, hvcc);
        if (ret < 0)
            goto end;
        break;
    default:
        ret = -1;
        goto end;
    }

end:
    free(rbsp_buf);
    return ret;
}

static void hvcc_init(HEVCDecoderConfigurationRecord *hvcc)
{
    memset(hvcc, 0, sizeof(HEVCDecoderConfigurationRecord));
    hvcc->configurationVersion = 1;
    hvcc->lengthSizeMinusOne   = 3; // 4 bytes

    /*
     * The following fields have all their valid bits set by default,
     * the ProfileTierLevel parsing code will unset them when needed.
     */
    hvcc->general_profile_compatibility_flags = 0xffffffff;
    hvcc->general_constraint_indicator_flags  = 0xffffffffffffLL;

    /*
     * Initialize this field with an invalid value which can be used to detect
     * whether we didn't see any VUI (in which case it should be reset to zero).
     */
    hvcc->min_spatial_segmentation_idc = MAX_SPATIAL_SEGMENTATION + 1;
}

static void hvcc_close(HEVCDecoderConfigurationRecord *hvcc)
{
    uint8_t i;

    for (i = 0; i < hvcc->numOfArrays; i++) {
        hvcc->array[i].numNalus = 0;
        free(hvcc->array[i].nalUnit);
        hvcc->array[i].nalUnit = NULL;
        free(hvcc->array[i].nalUnitLength);
        hvcc->array[i].nalUnitLength = NULL;
    }

    hvcc->numOfArrays = 0;
    free(hvcc->array);
    hvcc->array = NULL;
}

static int hvcc_write(sbuf_t *pb, HEVCDecoderConfigurationRecord *hvcc)
{
    uint8_t i;
    uint16_t j, vps_count = 0, sps_count = 0, pps_count = 0;

    /*
     * We only support writing HEVCDecoderConfigurationRecord version 1.
     */
    hvcc->configurationVersion = 1;

    /*
     * If min_spatial_segmentation_idc is invalid, reset to 0 (unspecified).
     */
    if (hvcc->min_spatial_segmentation_idc > MAX_SPATIAL_SEGMENTATION)
        hvcc->min_spatial_segmentation_idc = 0;

    /*
     * parallelismType indicates the type of parallelism that is used to meet
     * the restrictions imposed by min_spatial_segmentation_idc when the value
     * of min_spatial_segmentation_idc is greater than 0.
     */
    if (!hvcc->min_spatial_segmentation_idc)
        hvcc->parallelismType = 0;

    /*
     * It's unclear how to properly compute these fields, so
     * let's always set them to values meaning 'unspecified'.
     */
    hvcc->avgFrameRate      = 0;
    hvcc->constantFrameRate = 0;

    tvhtrace("hevc", "configurationVersion:                %"PRIu8,
            hvcc->configurationVersion);
    tvhtrace("hevc", "general_profile_space:               %"PRIu8,
            hvcc->general_profile_space);
    tvhtrace("hevc",  "general_tier_flag:                   %"PRIu8,
            hvcc->general_tier_flag);
    tvhtrace("hevc",  "general_profile_idc:                 %"PRIu8,
            hvcc->general_profile_idc);
    tvhtrace("hevc", "general_profile_compatibility_flags: 0x%08"PRIx32,
            hvcc->general_profile_compatibility_flags);
    tvhtrace("hevc", "general_constraint_indicator_flags:  0x%012"PRIx64,
            hvcc->general_constraint_indicator_flags);
    tvhtrace("hevc",  "general_level_idc:                   %"PRIu8,
            hvcc->general_level_idc);
    tvhtrace("hevc",  "min_spatial_segmentation_idc:        %"PRIu16,
            hvcc->min_spatial_segmentation_idc);
    tvhtrace("hevc",  "parallelismType:                     %"PRIu8,
            hvcc->parallelismType);
    tvhtrace("hevc",  "chromaFormat:                        %"PRIu8,
            hvcc->chromaFormat);
    tvhtrace("hevc",  "bitDepthLumaMinus8:                  %"PRIu8,
            hvcc->bitDepthLumaMinus8);
    tvhtrace("hevc",  "bitDepthChromaMinus8:                %"PRIu8,
            hvcc->bitDepthChromaMinus8);
    tvhtrace("hevc",  "avgFrameRate:                        %"PRIu16,
            hvcc->avgFrameRate);
    tvhtrace("hevc",  "constantFrameRate:                   %"PRIu8,
            hvcc->constantFrameRate);
    tvhtrace("hevc",  "numTemporalLayers:                   %"PRIu8,
            hvcc->numTemporalLayers);
    tvhtrace("hevc",  "temporalIdNested:                    %"PRIu8,
            hvcc->temporalIdNested);
    tvhtrace("hevc",  "lengthSizeMinusOne:                  %"PRIu8,
            hvcc->lengthSizeMinusOne);
    tvhtrace("hevc",  "numOfArrays:                         %"PRIu8,
            hvcc->numOfArrays);
    for (i = 0; i < hvcc->numOfArrays; i++) {
        tvhtrace("hevc", "NAL_unit_type[%"PRIu8"]:                    %"PRIu8,
                i, hvcc->array[i].NAL_unit_type);
        tvhtrace("hevc", "numNalus[%"PRIu8"]:                         %"PRIu16,
                i, hvcc->array[i].numNalus);
        for (j = 0; j < hvcc->array[i].numNalus; j++)
            tvhtrace("hevc",
                    "nalUnitLength[%"PRIu8"][%"PRIu16"]:                 %"PRIu16,
                    i, j, hvcc->array[i].nalUnitLength[j]);
    }

    /*
     * We need at least one of each: VPS, SPS and PPS.
     */
    for (i = 0; i < hvcc->numOfArrays; i++)
        switch (hvcc->array[i].NAL_unit_type) {
        case HEVC_NAL_VPS:
            vps_count += hvcc->array[i].numNalus;
            break;
        case HEVC_NAL_SPS:
            sps_count += hvcc->array[i].numNalus;
            break;
        case HEVC_NAL_PPS:
            pps_count += hvcc->array[i].numNalus;
            break;
        default:
            break;
        }
    if (!vps_count || vps_count > MAX_VPS_COUNT ||
        !sps_count || sps_count > MAX_SPS_COUNT ||
        !pps_count || pps_count > MAX_PPS_COUNT)
        return -1;

    /* unsigned int(8) configurationVersion = 1; */
    sbuf_put_byte(pb, hvcc->configurationVersion);

    /*
     * unsigned int(2) general_profile_space;
     * unsigned int(1) general_tier_flag;
     * unsigned int(5) general_profile_idc;
     */
    sbuf_put_byte(pb, hvcc->general_profile_space << 6 |
                hvcc->general_tier_flag     << 5 |
                hvcc->general_profile_idc);

    /* unsigned int(32) general_profile_compatibility_flags; */
    sbuf_put_be32(pb, hvcc->general_profile_compatibility_flags);

    /* unsigned int(48) general_constraint_indicator_flags; */
    sbuf_put_be32(pb, hvcc->general_constraint_indicator_flags >> 16);
    sbuf_put_be16(pb, hvcc->general_constraint_indicator_flags);

    /* unsigned int(8) general_level_idc; */
    sbuf_put_byte(pb, hvcc->general_level_idc);

    /*
     * bit(4) reserved = ‘1111’b;
     * unsigned int(12) min_spatial_segmentation_idc;
     */
    sbuf_put_be16(pb, hvcc->min_spatial_segmentation_idc | 0xf000);

    /*
     * bit(6) reserved = ‘111111’b;
     * unsigned int(2) parallelismType;
     */
    sbuf_put_byte(pb, hvcc->parallelismType | 0xfc);

    /*
     * bit(6) reserved = ‘111111’b;
     * unsigned int(2) chromaFormat;
     */
    sbuf_put_byte(pb, hvcc->chromaFormat | 0xfc);

    /*
     * bit(5) reserved = ‘11111’b;
     * unsigned int(3) bitDepthLumaMinus8;
     */
    sbuf_put_byte(pb, hvcc->bitDepthLumaMinus8 | 0xf8);

    /*
     * bit(5) reserved = ‘11111’b;
     * unsigned int(3) bitDepthChromaMinus8;
     */
    sbuf_put_byte(pb, hvcc->bitDepthChromaMinus8 | 0xf8);

    /* bit(16) avgFrameRate; */
    sbuf_put_be16(pb, hvcc->avgFrameRate);

    /*
     * bit(2) constantFrameRate;
     * bit(3) numTemporalLayers;
     * bit(1) temporalIdNested;
     * unsigned int(2) lengthSizeMinusOne;
     */
    sbuf_put_byte(pb, hvcc->constantFrameRate << 6 |
                  hvcc->numTemporalLayers << 3 |
                  hvcc->temporalIdNested  << 2 |
                  hvcc->lengthSizeMinusOne);

    /* unsigned int(8) numOfArrays; */
    sbuf_put_byte(pb, hvcc->numOfArrays);

    for (i = 0; i < hvcc->numOfArrays; i++) {
        /*
         * bit(1) array_completeness;
         * unsigned int(1) reserved = 0;
         * unsigned int(6) NAL_unit_type;
         */
        sbuf_put_byte(pb, (0 << 7) |
                      (hvcc->array[i].NAL_unit_type & 0x3f));

        /* unsigned int(16) numNalus; */
        sbuf_put_be16(pb, hvcc->array[i].numNalus);

        for (j = 0; j < hvcc->array[i].numNalus; j++) {
            /* unsigned int(16) nalUnitLength; */
            sbuf_put_be16(pb, hvcc->array[i].nalUnitLength[j]);

            /* bit(8*nalUnitLength) nalUnit; */
            sbuf_append(pb, hvcc->array[i].nalUnit[j],
                        hvcc->array[i].nalUnitLength[j]);
        }
    }

    return 0;
}

#if 0
int ff_hevc_annexb2mp4(AVIOContext *pb, const uint8_t *buf_in,
                       int size, int filter_ps, int *ps_count)
{
    int num_ps = 0, ret = 0;
    uint8_t *buf, *end, *start = NULL;

    if (!filter_ps) {
        ret = ff_avc_parse_nal_units(pb, buf_in, size);
        goto end;
    }

    ret = ff_avc_parse_nal_units_buf(buf_in, &start, &size);
    if (ret < 0)
        goto end;

    ret = 0;
    buf = start;
    end = start + size;

    while (end - buf > 4) {
        uint32_t len = MIN(RB32(buf), end - buf - 4);
        uint8_t type = (buf[4] >> 1) & 0x3f;

        buf += 4;

        switch (type) {
        case HEVC_NAL_VPS:
        case HEVC_NAL_SPS:
        case HEVC_NAL_PPS:
            num_ps++;
            break;
        default:
            ret += 4 + len;
            sbuf_put_be32(pb, len);
            sbuf_append(pb, buf, len);
            break;
        }

        buf += len;
    }

end:
    av_free(start);
    if (ps_count)
        *ps_count = num_ps;
    return ret;
}

int ff_hevc_annexb2mp4_buf(const uint8_t *buf_in, uint8_t **buf_out,
                           int *size, int filter_ps, int *ps_count)
{
    AVIOContext *pb;
    int ret;

    ret = avio_open_dyn_buf(&pb);
    if (ret < 0)
        return ret;

    ret   = ff_hevc_annexb2mp4(pb, buf_in, *size, filter_ps, ps_count);
    *size = avio_close_dyn_buf(pb, buf_out);

    return ret;
}
#endif

int isom_write_hvcc(sbuf_t *pb, const uint8_t *data, int size)
{
    int ret = 0;
    sbuf_t sb;
    uint8_t *buf, *end;
    HEVCDecoderConfigurationRecord hvcc;

    sbuf_init(&sb);
    hvcc_init(&hvcc);

    if (size < 6) {
        /* We can't write a valid hvcC from the provided data */
        ret = -1;
        goto end;
    } else if (*data == 1) {
        /* Data is already hvcC-formatted */
        sbuf_append(pb, data, size);
        goto end;
    } else if (!(RB24(data) == 1 || RB32(data) == 1)) {
        /* Not a valid Annex B start code prefix */
        ret = -1;
        goto end;
    }

    size = avc_parse_nal_units(&sb, data, size);

    buf = sb.sb_data;
    end = buf + size;

    while (end - buf > 4) {
        uint32_t len = MIN(RB32(buf), end - buf - 4);
        uint8_t type = (buf[4] >> 1) & 0x3f;

        buf += 4;

        switch (type) {
        case HEVC_NAL_VPS:
        case HEVC_NAL_SPS:
        case HEVC_NAL_PPS:
        case HEVC_NAL_SEI_PREFIX:
        case HEVC_NAL_SEI_SUFFIX:
            ret = hvcc_add_nal_unit(buf, len, &hvcc);
            if (ret < 0)
                goto end;
            break;
        default:
            break;
        }

        buf += len;
    }

    ret = hvcc_write(pb, &hvcc);

end:
    hvcc_close(&hvcc);
    sbuf_free(&sb);
    return ret;
}

th_pkt_t *
hevc_convert_pkt(th_pkt_t *src)
{
  sbuf_t payload;
  th_pkt_t *pkt = pkt_copy_nodata(src);

  sbuf_init(&payload);

  avc_parse_nal_units(&payload, pktbuf_ptr(src->pkt_payload),
                      pktbuf_len(src->pkt_payload));

  pkt->pkt_payload = pktbuf_make(payload.sb_data, payload.sb_ptr);
  return pkt;
}

/*
 *
 */

typedef struct hevc_vps {
  uint8_t valid: 1;
  uint32_t num_units_in_tick;
  uint32_t time_scale;
} hevc_vps_t;

typedef struct hevc_vui {
  struct {
    uint32_t num;
    uint32_t den;
  } sar;
  uint32_t num_units_in_tick;
  uint32_t time_scale;
} hevc_vui_t;

typedef struct hevc_sps {
  uint8_t valid: 1;
  uint8_t vps_id;
  uint32_t width;
  uint32_t height;
  uint32_t ctb_width;
  uint32_t ctb_height;
  hevc_vui_t vui;
} hevc_sps_t;

typedef struct hevc_pps {
  uint8_t valid: 1;
  uint8_t dependent_slice_segments: 1;
  uint8_t sps_id;
  uint8_t num_extra_slice_header_bits;
} hevc_pps_t;

typedef struct hevc_private {

  hevc_vps_t vps[MAX_VPS_COUNT];
  hevc_sps_t sps[MAX_SPS_COUNT];
  hevc_pps_t pps[MAX_PPS_COUNT];

} hevc_private_t;

static const uint8_t vui_sar[][2] = {
  {  0,   1 },
  {  1,   1 },
  { 12,  11 },
  { 10,  11 },
  { 16,  11 },
  { 40,  33 },
  { 24,  11 },
  { 20,  11 },
  { 32,  11 },
  { 80,  33 },
  { 18,  11 },
  { 15,  11 },
  { 64,  33 },
  { 160, 99 },
  {  4,   3 },
  {  3,   2 },
  {  2,   1 },
};

static uint_fast32_t ilog2(uint32_t v)
{
  uint_fast32_t r = 0;
  while (v) {
    v >>= 1;
    r++;
  }
  return r;
}

static inline int check_width(uint32_t w)
{
  return w < 100 || w > 16384;
}

static inline int check_height(uint32_t h)
{
  return h < 100 || h > 16384;
}

void
hevc_decode_vps(elementary_stream_t *st, bitstream_t *bs)
{
  hevc_private_t *p;
  hevc_vps_t *vps;
  uint32_t vps_id, max_sub_layers;
  uint32_t sub_layer_ordering_info_present;
  uint32_t max_layer_id, num_layer_sets;
  uint32_t u, v;

  if (read_bits1(bs)) /* zero bit */
    return ;

  if((p = st->es_priv) == NULL)
    p = st->es_priv = calloc(1, sizeof(hevc_private_t));

  skip_bits(bs, 15);  /* NAL type, Layer ID, Temporal ID */

  vps_id = read_bits(bs, 4);
  if (vps_id >= MAX_VPS_COUNT)
    return;
  vps = &p->vps[vps_id];

  if (read_bits(bs, 2) != 3) /* vps_reserved_three_2bits */
    return;

  skip_bits(bs, 6); /* vps_max_sublayers */
  max_sub_layers = read_bits(bs, 3) + 1;
  skip_bits1(bs);   /* vps_temporal_id_nesting */

  if (read_bits(bs, 16) != 0xffff) /* reserved */
    return;

  if (max_sub_layers > MAX_SUB_LAYERS)
    return;

  hvcc_parse_ptl(bs, NULL, max_sub_layers - 1);

  sub_layer_ordering_info_present = read_bits1(bs);
  u = sub_layer_ordering_info_present ? 0 : max_sub_layers - 1;
  for ( ; u < max_sub_layers; u++) {
    read_golomb_ue(bs); /* vps_max_dec_pic_buffering */
    read_golomb_ue(bs); /* vps_num_reorder_pics */
    read_golomb_ue(bs); /* vps_max_latency_increase */
  }

  max_layer_id   = read_bits(bs, 6); /* vps_max_layer_id */
  num_layer_sets = read_golomb_ue(bs) + 1;
  if (num_layer_sets < 1 || num_layer_sets > 1024 ||
      (num_layer_sets - 1LL) * (max_layer_id + 1LL) > remaining_bits(bs))
    return;

  for (u = 1; u < num_layer_sets; u++)
    for (v = 0; v < max_layer_id; v++)
      skip_bits1(bs);  /* layer_id_included */

  vps->valid = 1;

  if (!read_bits1(bs)) { /* vps_timing_info_present */
    vps->num_units_in_tick = read_bits(bs, 32);
    vps->time_scale        = read_bits(bs, 32);
  }
}

static int
hevc_decode_vui(hevc_vui_t *vui, bitstream_t *bs)
{
  uint32_t u, sar_num, sar_den;
  bitstream_t backup;

  sar_num = 1;
  sar_den = 1;
  if (read_bits1(bs)) { /* sar_present */
    u = read_bits(bs, 8);
    if (u < ARRAY_SIZE(vui_sar)) {
      sar_num = vui_sar[u][0];
      sar_den = vui_sar[u][1];
    } else if (u == 255) {
      sar_num = read_bits(bs, 16);
      sar_den = read_bits(bs, 16);
    } else
      return -1;
  }
  vui->sar.num = sar_num;
  vui->sar.den = sar_den;

  if (read_bits1(bs)) /* overscan_info_present */
    skip_bits1(bs);   /* overscan_appropriate */

  if (read_bits1(bs)) { /* video_signal_type_present */
    skip_bits(bs, 3);   /* video_format */
    skip_bits1(bs);     /* video_full_range */
    if (read_bits1(bs)) { /* colour_description_present */
      skip_bits(bs, 8); /* colour_primaries */
      skip_bits(bs, 8); /* transfer_characteristic */
      skip_bits(bs, 8); /* matrix_coeffs */
    }
  }

  if (read_bits1(bs)) { /* chroma_loc_info_present */
    read_golomb_ue(bs); /* chroma_sample_loc_type_top_field */
    read_golomb_ue(bs); /* chroma_sample_loc_type_bottom_field */
  }

  skip_bits1(bs); /* neutra_chroma_indication_flag */
  skip_bits1(bs); /* field_seq_flag */
  skip_bits1(bs); /* frame_field_info_present_flag */

  if (remaining_bits(bs) >= 68 && show_bits(bs, 21) == 0x100000)
    u = 0; /* Invalid default display window */
  else
    u = read_bits1(bs); /* default_display_window */
  /* Backup context in case an alternate header is detected */
  memcpy(&backup, bs, sizeof(backup));
  if (u) { /* default_display_window */
    read_golomb_ue(bs); /* left_offset */
    read_golomb_ue(bs); /* right_offset */
    read_golomb_ue(bs); /* top_offset */
    read_golomb_ue(bs); /* bottom_offset */
  }

  if (read_bits1(bs)) { /* vui_timing_info_present */
    if (remaining_bits(bs) < 66) {
      /* Strange VUI timing information */
      memcpy(bs, &backup, sizeof(backup));
    }
    vui->num_units_in_tick = read_bits(bs, 32);
    vui->time_scale        = read_bits(bs, 32);
  }

  return 0;
}

void
hevc_decode_sps(elementary_stream_t *st, bitstream_t *bs)
{
  hevc_private_t *p;
  hevc_vps_t *vps;
  hevc_sps_t *sps;
  uint32_t vps_id, sps_id, max_sub_layers, u, v, i;
  uint32_t width, height;
  uint32_t chroma_format_idc, bit_depth, bit_depth_chroma;
  uint32_t log2_max_poc_lsb;
  uint32_t log2_min_cb_size, log2_min_tb_size;
  uint32_t log2_diff_max_min_coding_block_size;
  uint32_t log2_diff_max_min_transform_block_size;
  uint32_t log2_ctb_size, nb_st_rps;

  if (read_bits1(bs)) /* zero bit */
    return;

  if((p = st->es_priv) == NULL)
    p = st->es_priv = calloc(1, sizeof(hevc_private_t));

  skip_bits(bs, 15);  /* NAL type, Layer ID, Temporal ID */

  vps_id = read_bits(bs, 4);
  if (vps_id >= MAX_VPS_COUNT)
    return;
  vps = &p->vps[vps_id];
  if (!vps->valid)
    return;

  max_sub_layers = read_bits(bs, 3) + 1;
  if (max_sub_layers > MAX_SUB_LAYERS)
    return;
  skip_bits1(bs);     /* temporal_id_nesting */

  hvcc_parse_ptl(bs, NULL, max_sub_layers - 1);

  sps_id = read_golomb_ue(bs);
  if (sps_id >= MAX_SPS_COUNT)
    return;
  sps = &p->sps[sps_id];

  chroma_format_idc = read_golomb_ue(bs);
  if (chroma_format_idc == 3)
    if (read_bits1(bs))   /* separate_colour_plane */
      chroma_format_idc = 0;
  width  = read_golomb_ue(bs);
  height = read_golomb_ue(bs);
  if (check_width(width) || check_height(height))
    return;

  if (read_bits1(bs)) { /* pic_conformance */
    read_golomb_ue(bs); /* left_offset */
    read_golomb_ue(bs); /* right_offset */
    read_golomb_ue(bs); /* top_offset */
    read_golomb_ue(bs); /* bottom_offset */
  }

  bit_depth        = read_golomb_ue(bs);
  bit_depth_chroma = read_golomb_ue(bs);
  if (chroma_format_idc && bit_depth_chroma != bit_depth)
    return;

  log2_max_poc_lsb = read_golomb_ue(bs) + 4;
  if (log2_max_poc_lsb > 16)
    return;

  u = read_bits1(bs);   /* sublayer_ordering_info */
  for (u = u ? 0 : max_sub_layers - 1; u < max_sub_layers; u++) {
    read_golomb_ue(bs); /* max_dec_pic_buffering */
    read_golomb_ue(bs); /* num_reorder_pics */
    read_golomb_ue(bs); /* max_latency_increase */
  }

  log2_min_cb_size = read_golomb_ue(bs) + 3;
  if (log2_min_cb_size < 3 || log2_min_cb_size > 30)
    return;
  log2_diff_max_min_coding_block_size = read_golomb_ue(bs);
  if (log2_diff_max_min_coding_block_size > 30)
    return;
  log2_min_tb_size = read_golomb_ue(bs) + 2;
  if (log2_min_tb_size >= log2_min_cb_size || log2_min_tb_size < 2)
    return;
  log2_diff_max_min_transform_block_size = read_golomb_ue(bs);
  if (log2_diff_max_min_transform_block_size > 30)
    return;

  read_golomb_ue(bs); /* max_transform_hierarchy_depth_inter */
  read_golomb_ue(bs); /* max_transform_hierarchy_depth_intra */

  if (read_bits1(bs)) { /* scaling_list_enable_flag */
    if (read_bits1(bs)) { /* scaling_list_data */
      for (u = 0; u < 4; u++) {
        for (v = 0; v < 6; v += ((u == 3) ? 3 : 1)) {
          if (read_bits1(bs)) { /* scaling_list_pred_mode */
            read_golomb_ue(bs); /* delta */
          } else {
            if (u > 1)
              read_golomb_se(bs); /* scaling_list_dc_coef */
            i = MIN(64, 1 << (4 + (u << 1)));
            while (i--)
              read_golomb_se(bs); /* scaling_list_delta_coef */
          }
        }
      }
    }
  }

  skip_bits1(bs); /* amp_enabled */
  skip_bits1(bs); /* sao_enabled */

  if (read_bits1(bs)) { /* pcm_enabled */
    skip_bits(bs, 4);   /* pcm.bit_depth */
    skip_bits(bs, 4);   /* pcm.bit_depth_chroma */
    read_golomb_ue(bs); /* pcm.log2_min_pcm_cb_size */
    read_golomb_ue(bs); /* pcm.log2_max_pcm_cb_size */
    skip_bits1(bs);     /* pcm.loop_filter_disable_flag */
  }

  nb_st_rps = read_golomb_ue(bs);
  if (nb_st_rps > MAX_SHORT_TERM_RPS_COUNT)
    return;

  for (u = 0; u < nb_st_rps; u++) {
    if (u > 0 && read_bits1(bs)) { /* rps_predict */
      skip_bits1(bs);         /* delta_rps_sign */
      read_golomb_ue(bs);     /* abs_delta_rps */
    } else {
      u = read_golomb_ue(bs); /* rps->num_negative_pics */
      v = read_golomb_ue(bs); /* nb_positive_pics */
      if (u > MAX_REFS || v > MAX_REFS)
        return;
      for (i = 0; i < u; i++) {
        read_golomb_ue(bs);   /* delta_poc */
        skip_bits1(bs);       /* used */
      }
      for (i = 0; i < v; i++) {
        read_golomb_ue(bs);   /* delta_poc */
        skip_bits1(bs);       /* used */
      }
    }
  }

  if (read_bits1(bs)) { /* long_term_ref_pics_present */
    u = read_golomb_ue(bs); /* num_long_term_ref_pics_sps */
    if (u > 31)
      return;
    for (i = 0; i < u; i++) {
      skip_bits(bs, log2_max_poc_lsb);
      skip_bits1(bs);
    }
  }

  skip_bits1(bs); /* sps_temporal_mvp_enabled_flag */
  skip_bits1(bs); /* sps_strong_intra_smoothing_enable_flag */

  if (read_bits1(bs))  /* vui_present */
    if (hevc_decode_vui(&sps->vui, bs))
      return;

  if (!vps->num_units_in_tick && !vps->time_scale &&
      !sps->vui.num_units_in_tick && !sps->vui.time_scale)
    return;

  sps->width = width;
  sps->height = height;

  log2_ctb_size = log2_min_cb_size +
                  log2_diff_max_min_coding_block_size;
  if (log2_ctb_size > 18)
    return;

  sps->ctb_width  = (width  + (1 << log2_ctb_size) - 1) >> log2_ctb_size;
  sps->ctb_height = (height + (1 << log2_ctb_size) - 1) >> log2_ctb_size;

  sps->vps_id = vps_id;
  sps->valid = 1;
}

void
hevc_decode_pps(elementary_stream_t *st, bitstream_t *bs)
{
  hevc_private_t *p;
  hevc_pps_t *pps;
  uint32_t pps_id, sps_id;

  if (read_bits1(bs)) /* zero bit */
    return ;

  if((p = st->es_priv) == NULL)
    p = st->es_priv = calloc(1, sizeof(hevc_private_t));

  skip_bits(bs, 15);  /* NAL type, Layer ID, Temporal ID */

  pps_id = read_golomb_ue(bs);
  if (pps_id >= MAX_PPS_COUNT)
    return;
  pps = &p->pps[pps_id];

  sps_id = read_golomb_ue(bs);
  if (sps_id >= MAX_SPS_COUNT || !p->sps[sps_id].valid)
    return;

  pps->sps_id = sps_id;
  pps->dependent_slice_segments = read_bits1(bs);
  skip_bits1(bs); /* output_flag_present */
  pps->num_extra_slice_header_bits = read_bits(bs, 3);

  pps->valid = 1;
}

int
hevc_decode_slice_header(struct elementary_stream *st, bitstream_t *bs,
                         int *pkttype)
{
  hevc_private_t *p;
  hevc_vps_t *vps;
  hevc_sps_t *sps;
  hevc_pps_t *pps;
  int nal_type;
  int first_slice_in_pic, dependent_slice_segment;
  uint32_t pps_id, sps_id, vps_id, u, v, width, height;
  uint64_t d;

  if (read_bits1(bs)) /* zero bit */
    return -1;

  if((p = st->es_priv) == NULL)
      p = st->es_priv = calloc(1, sizeof(hevc_private_t));

  nal_type = read_bits(bs, 6);
  skip_bits(bs, 6);   /* Layer ID */
  skip_bits(bs, 3);   /* Temporal ID */

  first_slice_in_pic = read_bits1(bs);

  if (IS_IRAP_NAL(nal_type))
    skip_bits1(bs);  /* no_output_of_prior_pics_flag */

  pps_id = read_golomb_ue(bs);
  if (pps_id >= MAX_PPS_COUNT)
    return -1;
  pps = &p->pps[pps_id];
  if (!pps->valid)
    return -1;

  sps_id = pps->sps_id;
  if (sps_id >= MAX_SPS_COUNT)
    return -1;
  sps = &p->sps[sps_id];
  if (!sps->valid)
    return -1;

  dependent_slice_segment = 0;
  if (!first_slice_in_pic) {
    if (pps->dependent_slice_segments)
      dependent_slice_segment = read_bits1(bs);
    v = sps->ctb_width * sps->ctb_height;
    u = ilog2((v - 1) << 1);
    if (u >= v)
      return -1;
  }

  if (dependent_slice_segment)
    return 1; /* append */

  for (u = 0, v = pps->num_extra_slice_header_bits; u < v; u++)
    skip_bits1(bs);

  switch (read_golomb_ue(bs)) {
  case B_SLICE: *pkttype = PKT_B_FRAME; break;
  case P_SLICE: *pkttype = PKT_P_FRAME; break;
  case I_SLICE: *pkttype = PKT_I_FRAME; break;
  default: return -1;
  }

  vps_id = sps->vps_id;
  width  = sps->width;
  height = sps->height;

  vps = &p->vps[vps_id];

  d = 0;
  if (sps->vui.time_scale) {
    d = 180000 * (uint64_t)sps->vui.num_units_in_tick / (uint64_t)sps->vui.time_scale;
  } else if (vps->time_scale) {
    d = 180000 * (uint64_t)vps->num_units_in_tick / (uint64_t)vps->time_scale;
  }

  if (width && height && d)
    parser_set_stream_vparam(st, width, height, d);

  if (sps->vui.sar.num && sps->vui.sar.den) {
    width  *= sps->vui.sar.num;
    height *= sps->vui.sar.den;
    if (width && height) {
      v = gcdU32(width, height);
      st->es_aspect_num = width / v;
      st->es_aspect_den = height / v;
    }
  } else {
    st->es_aspect_num = 0;
    st->es_aspect_den = 1;
  }

  return 0;
}
