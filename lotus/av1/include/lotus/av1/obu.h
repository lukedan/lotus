#pragma once

/// \file
/// AV1 OBUs (Open Bitstream Units).

#include "lotus/types.h"

#include "lotus/av1/common.h"
#include "lotus/av1/state.h"

namespace lotus::av1::obu {
	/// 6.2.2. obu_type
	/// All unspecified types are reserved.
	enum class type : u8 {
		sequence_header        = 1,
		temporal_delimiter     = 2,
		frame_header           = 3,
		tile_group             = 4,
		metadata               = 5,
		frame                  = 6,
		redundant_frame_header = 7,
		tile_list              = 8
	};

	/// 5.3.3. OBU extension header syntax
	struct extension_header {
		/// Zero initialization.
		extension_header(zero_t) {
		}

		u8 temporal_id = 0; ///< \p temporal_id.
		u8 spatial_id  = 0; ///< \p spatial_id.
	};

	/// 5.3.2. OBU header syntax
	struct header {
		/// Zero initialization.
		header(zero_t) {
		}

		extension_header extension = zero; ///< Extension header.

		type obu_type = zero; ///< \p obu_type.

		bool extension_flag : 1 = 0; ///< \p obu_extension_flag.
		bool has_size_field : 1 = 0; ///< \p obu_has_size_field.
	};

	/// 5.5.2. Color config syntax
	struct color_config {
		/// Zero initialization.
		color_config(zero_t) {
		}

		color_primaries color_primaries = zero; ///< \p color_primaries.
		transfer_characteristics transfer_characteristics = zero; ///< \p transfer_characteristics.
		matrix_coefficients matrix_coefficients = zero; ///< \p matrix_coefficients.
		chroma_sample_position chroma_sample_position = zero; ///< \p chroma_sample_position.
		u8 bit_depth = 0; ///< \p BitDepth.

		bool mono_chrome               : 1 = false; ///< \p mono_chrome.
		bool color_description_present : 1 = false; ///< \p color_description_present_flag.
		bool color_range               : 1 = false; ///< \p color_range.
		bool subsampling_x             : 1 = false; ///< \p subsampling_x.
		bool subsampling_y             : 1 = false; ///< \p subsampling_y.
		bool separate_uv_delta_q       : 1 = false; ///< \p separate_uv_delta_q.


		/// \p NumPlanes.
		[[nodiscard]] constexpr u8 get_num_planes() const {
			return mono_chrome ? 1 : 3;
		}
	};

	/// 5.5.3. Timing info syntax
	struct timing_info {
		/// Zero initialization.
		timing_info(zero_t) {
		}

		u32  num_units_in_display_tick = 0;     ///< \p num_units_in_display_tick.
		u32  time_scale                = 0;     ///< \p time_scale.
		u32  num_tickes_per_picture    = 0;     ///< \p num_ticks_per_picture_minus_1.
		bool equal_picture_interval    = false; ///< \p equal_picture_interval.
	};

	/// 5.5.4. Decoder model info syntax
	struct decoder_model_info {
		/// Zero initialization.
		decoder_model_info(zero_t) {
		}

		u32 num_units_in_decoding_tick     = 0; ///< \p num_units_in_decoding_tick.
		u8  buffer_delay_length            = 0; ///< \p buffer_delay_length_minus_1.
		u8  buffer_removal_time_length     = 0; ///< \p buffer_removal_time_length_minus_1.
		u8  frame_presentation_time_length = 0; ///< \p frame_presentation_time_length_minus_1.
	};

	/// 5.5.5. Operating parameters info syntax
	struct operating_parameters_info {
		/// Zero initialization.
		operating_parameters_info(zero_t) {
		}

		u32  decoder_buffer_delay = 0;     ///< \p decoder_buffer_delay.
		u32  encoder_buffer_delay = 0;     ///< \p encoder_buffer_delay.

		bool low_delay_mode : 1 = false; ///< \p low_delay_mode_flag.
	};

	/// 5.5.1. General sequence header OBU syntax
	struct sequence_header {
		/// All information associated with an operating point.
		struct operating_point {
			// no constructor to allow for aggregate zero initialization

			u16 mask                  = 0; ///< \p operating_point_idc.
			u8  sequence_level_index  = 0; ///< \p seq_level_idx.
			u8  initial_display_delay = 0; ///< \p initial_display_delay_minus_1.

			[[no_unique_address]] operating_parameters_info operating_parameters = zero; ///< Operating parameters.

			bool sequence_tier                 : 1 = false; ///< \p seq_tier.
			bool decoder_model_present         : 1 = false; ///< \p decoder_model_present_for_this_op.
			bool initial_display_delay_present : 1 = false; ///< \p initial_display_delay_present_for_this_op.
		};

		/// Zero initialization.
		sequence_header(zero_t) {
		}

		operating_point operating_points[constants::max_num_operating_points] = {}; ///< Operating point information.
		color_config color_config = zero; ///< Color config.
		timing_info timing_info = zero; ///< Timing info.
		decoder_model_info decoder_model_info = zero; ///< Decoder model info.

		u32 max_frame_width            = 0; ///< \p max_frame_width_minus_1.
		u32 max_frame_height           = 0; ///< \p max_frame_height_minus_1.
		u8  seq_profile                = 0; ///< \p seq_profile.
		u8  operating_points_count     = 0; ///< \p operating_points_cnt_minus_1.
		u8  frame_width_bits           = 0; ///< \p frame_width_bits_minus_1.
		u8  frame_height_bits          = 0; ///< \p frame_height_bits_minus_1.
		u8  delta_frame_id_length      = 0; ///< \p delta_frame_id_length_minus_2.
		u8  additional_frame_id_length = 0; ///< \p additional_frame_id_length_minus_1.
		u8  order_hint_bits            = 0; ///< \p order_hint_bits_minus_1.

		bool still_picture                   : 1 = false; ///< \p still_picture.
		bool reduced_still_picture_header    : 1 = false; ///< \p reduced_still_picture_header.
		bool timing_info_present             : 1 = false; ///< \p timing_info_present_flag.
		bool decoder_model_info_present      : 1 = false; ///< \p decoder_model_info_present_flag.
		bool initial_display_delay_present   : 1 = false; ///< \p initial_display_delay_present_flag.
		bool frame_id_numbers_present        : 1 = false; ///< \p frame_id_numbers_present_flag.
		bool use_128x128_superblock          : 1 = false; ///< \p use_128x128_superblock.
		bool enable_filter_intra             : 1 = false; ///< \p enable_filter_intra.

		bool enable_intra_edge_filter        : 1 = false; ///< \p enable_intra_edge_filter.
		bool enable_interintra_compound      : 1 = false; ///< \p enable_interintra_compound.
		bool enable_masked_compound          : 1 = false; ///< \p enable_masked_compound.
		bool enable_warped_motion            : 1 = false; ///< \p enable_warped_motion.
		bool enable_dual_filter              : 1 = false; ///< \p enable_dual_filter.
		bool enable_order_hint               : 1 = false; ///< \p enable_order_hint.
		bool enable_jnt_comp                 : 1 = false; ///< \p enable_jnt_comp.
		bool enable_ref_frame_mvs            : 1 = false; ///< \p enable_ref_frame_mvs.

		bool seq_choose_screen_content_tools : 1 = false; ///< \p seq_choose_screen_content_tools.
		bool seq_choose_integer_mv           : 1 = false; ///< \p seq_choose_integer_mv.
		bool enable_superres                 : 1 = false; ///< \p enable_superres.
		bool enable_cdef                     : 1 = false; ///< \p enable_cdef.
		bool enable_restoration              : 1 = false; ///< \p enable_restoration.
		bool film_grain_params_present       : 1 = false; ///< \p film_grain_params_present.

		u8   seq_force_screen_content_tools  : 2 = 0;     ///< \p seq_force_screen_content_tools.
		u8   seq_force_integer_mv            : 2 = 0;     ///< \p seq_force_integer_mv.


		/// \p idLen.
		[[nodiscard]] u8 get_frame_id_length() const {
			crash_if(!frame_id_numbers_present);
			return additional_frame_id_length + delta_frame_id_length;
		}
		/// \p sbSize.
		[[nodiscard]] block_size get_block_size() const {
			return use_128x128_superblock ? block_size::b128x128 : block_size::b64x64;
		}
	};

	/// 5.9.5. Frame size syntax
	/// 5.9.6. Render size syntax
	/// 5.9.8. Superres params syntax
	struct frame_size {
		/// Zero initialization.
		frame_size(zero_t) {
		}

		u32 frame_width    = 0; ///< \p FrameWidth.
		u32 frame_height   = 0; ///< \p FrameHeight.
		u32 upscaled_width = 0; ///< \p UpscaledWidth.
		u32 render_width   = 0; ///< \p RenderWidth.
		u32 render_height  = 0; ///< \p RenderHeight.
		u8  superres_denom = 0; ///< \p SuperresDenom.

		bool use_superres                    : 1 = false; ///< \p use_superres.
		bool render_and_frame_size_different : 1 = false; ///< \p render_and_frame_size_different.


		/// \p MiCols.
		[[nodiscard]] u32 get_mi_cols() const {
			return 2 * ((frame_width + 7) >> 3);
		}
		/// \p MiRows.
		[[nodiscard]] u32 get_mi_rows() const {
			return 2 * ((frame_height + 7) >> 3);
		}
	};

	/// 5.9.11. Loop filter params syntax
	struct loop_filter_params {
		/// Zero initialization.
		loop_filter_params(zero_t) {
		}

		u8 level[4] = {}; ///< \p loop_filter_level.
		i8 ref_deltas[constants::total_refs_per_frame] = {}; ///< \p loop_filter_ref_deltas.
		i8 mode_deltas[2] = {}; ///< \p loop_filter_mode_deltas.

		u8 sharpness = 0; ///< \p loop_filter_sharpness.

		bool delta_enabled : 1 = false; ///< \p loop_filter_delta_enabled.
		bool delta_update  : 1 = false; ///< \p loop_filter_delta_update.
	};

	/// 5.9.13. Delta quantizer syntax
	using delta_q = i8;

	/// 5.9.12. Quantization params syntax
	struct quantization_params {
		/// Zero initialization.
		quantization_params(zero_t) {
		}

		u8      base_q_idx   = 0; ///< \p base_q_idx.
		delta_q delta_q_y_dc = 0; ///< \p DeltaQYDc.
		delta_q delta_q_u_dc = 0; ///< \p DeltaQUDc.
		delta_q delta_q_u_ac = 0; ///< \p DeltaQUAc.
		delta_q delta_q_v_dc = 0; ///< \p DeltaQVDc.
		delta_q delta_q_v_ac = 0; ///< \p DeltaQVAc.
		u8      qm_y         = 0; ///< \p qm_y.
		u8      qm_u         = 0; ///< \p qm_u.
		u8      qm_v         = 0; ///< \p qm_v.

		bool using_qmatrix : 1 = false; ///< \p using_qmatrix.
	};

	/// 5.9.14. Segmentation params syntax
	struct segmentation_params {
		/// Zero initialization.
		segmentation_params(zero_t) {
		}

		i16 feature_data[constants::max_segments][std::to_underlying(features::max)] = {}; ///< \p FeatureData.
		u8 feature_enabled[constants::max_segments] = {}; ///< \p FeatureEnabled.

		segment_id_t last_active_seg_id = zero; ///< \p LastActiveSegId.

		bool segmentation_enabled         : 1 = false; ///< \p segmentation_enabled.
		bool segmentation_update_map      : 1 = false; ///< \p segmentation_update_map.
		bool segmentation_temporal_update : 1 = false; ///< \p segmentation_temporal_update.
		bool segmentation_update_data     : 1 = false; ///< \p segmentation_update_data.
		bool seg_id_pre_skip              : 1 = false; ///< \p SegIdPreSkip.


		/// 5.11.14. Segmentation feature active function
		/// \p seg_feature_active_idx().
		[[nodiscard]] bool is_feature_active(segment_id_t idx, features feature) const {
			return
				segmentation_enabled &&
				(feature_enabled[std::to_underlying(idx)] & (1u << std::to_underlying(feature)));
		}
	};

	/// 5.9.15. Tile info syntax
	struct tile_info {
		/// Zero initialization.
		tile_info(zero_t) {
		}

		u32 context_update_tile_id = 0; ///< \p context_update_tile_id.
		u32 tile_cols              = 0; ///< \p TileCols.
		u32 tile_rows              = 0; ///< \p TileRows.
		u32 tile_cols_log2         = 0; ///< \p TileColsLog2.
		u32 tile_rows_log2         = 0; ///< \p TileRowsLog2.
		u32 tile_size_bytes        = 0; ///< \p TileSizeBytes.

		bool uniform_tile_spacing : 1 = false; ///< \p uniform_tile_spacing_flag.
	};

	/// 5.9.17. Quantizer index delta parameters syntax
	struct delta_q_params {
		/// Zero initialization.
		delta_q_params(zero_t) {
		}

		bool delta_q_present : 1 = false; ///< \p delta_q_present.
		u8   delta_q_res     : 2 = 0;     ///< \p delta_q_res.
	};

	/// 5.9.18. Loop filter delta parameters syntax
	struct delta_lf_params {
		/// Zero initialization.
		delta_lf_params(zero_t) {
		}

		bool delta_lf_present : 1 = false; ///< \p delta_lf_present.
		u8   delta_lf_res     : 2 = 0;     ///< \p delta_lf_res.
		bool delta_lf_multi   : 1 = false; ///< \p delta_lf_multi.
	};

	/// 5.9.19. CDEF params syntax
	struct cdef_params {
		/// Zero initialization.
		cdef_params(zero_t) {
		}

		u8 y_pri_strength[8]  = {}; ///< \p cdef_y_pri_strength.
		u8 y_sec_strength[8]  = {}; ///< \p cdef_y_sec_strength.
		u8 uv_pri_strength[8] = {}; ///< \p cdef_uv_pri_strength.
		u8 uv_sec_strength[8] = {}; ///< \p cdef_uv_sec_strength.

		u8 damping = 0; ///< \p CdefDamping.
		u8 bits    = 0; ///< \p cdef_bits.
	};

	/// 5.9.20. Loop restoration params syntax
	struct lr_params {
		/// Zero initialization.
		lr_params(zero_t) {
		}

		frame_restoration frame_restoration_type[3] = {}; ///< FrameRestorationType.
		u16 size[3] = {}; ///< \p LoopRestorationSize.

		bool uses_lr : 1 = false; ///< \p UsesLr.
	};

	/// 5.9.24. Global motion params syntax
	struct global_motion_params {
		/// Global motion params for a single reference image.
		struct ref {
			// no constructor to allow for aggregate zero initialization

			i32 params[6] = {}; ///< \p gm_params.

			warp_model type = zero; ///< \p GmType.
		};

		/// Zero initialization.
		global_motion_params(zero_t) {
		}

		ref refs[8] = {}; ///< Params for all reference frames.
	};

	/// 5.9.30. Film grain params syntax
	struct film_grain_params {
		/// A color channel point.
		struct point {
			// no constructor to allow for aggregate zero initialization

			u8 value   = 0; ///< Value.
			u8 scaling = 0; ///< Scaling.
		};
		/// Channel specific properties for Cb and Cr channels.
		struct channel_properties {
			/// Zero initialization.
			channel_properties(zero_t) {
			}

			u8  mult      = 0; ///< Mult.
			u8  luma_mult = 0; ///< Luma mult.
			u16 offset    = 0; ///< Offset.
		};

		/// Zero initialization.
		film_grain_params(zero_t) {
		}

		point point_y[16]  = {}; ///< \p point_y_value, \p point_y_scaling.
		point point_cb[16] = {}; ///< \p point_cb_value, \p point_cb_scaling.
		point point_cr[16] = {}; ///< \p point_cr_value, \p point_cr_scaling.
		u8 ar_coeffs_y_plus_128[24] = {}; ///< \p ar_coeffs_y_plus_128.
		u8 ar_coeffs_cb_plus_128[25] = {}; ///< \p ar_coeffs_cb_plus_128.
		u8 ar_coeffs_cr_plus_128[25] = {}; ///< \p ar_coeffs_cr_plus_128.

		channel_properties cb = zero; ///< \p cb_mult, \p cb_luma_mult, \p cb_offset.
		channel_properties cr = zero; ///< \p cr_mult, \p cr_luma_mult, \p cr_offset.

		u16 grain_seed        = 0; ///< \p grain_seed.
		u8  num_y_points      = 0; ///< \p num_y_points.
		u8  num_cb_points     = 0; ///< \p num_cb_points.
		u8  num_cr_points     = 0; ///< \p num_cr_points.
		u8  grain_scaling     = 0; ///< \p grain_scaling_minus_8.
		u8  ar_coeff_lag      = 0; ///< \p ar_coeff_lag.
		u8  ar_coeff_shift    = 0; ///< \p ar_coeff_shift_minus_6.
		u8  grain_scale_shift = 0; ///< \p grain_scale_shift.

		bool apply_grain              : 1 = false; ///< \p apply_grain.
		bool update_grain             : 1 = false; ///< \p update_grain.
		bool chroma_scaling_from_luma : 1 = false; ///< \p chroma_scaling_from_luma.
		bool overlap                  : 1 = false; ///< \p overlap_flag.
		bool clip_to_restricted_range : 1 = false; ///< \p clip_to_restricted_range.
	};

	/// 5.9.2. Uncompressed header syntax
	struct uncompressed_header {
		/// Reference frame.
		struct reference_frame {
			// no constructor to allow for aggregate zero initialization

			u32 expected_frame_id = 0; ///< \p expectedFrameId.
			u8  frame_index       = 0; ///< \p ref_frame_idx.
		};

		/// Zero initialization.
		uncompressed_header(zero_t) {
		}

		reference_frame ref_frames[constants::refs_per_frame] = {}; ///< Reference frames.
		u32 buffer_removal_time[constants::max_num_operating_points] = {}; ///< \p buffer_removal_time.
		u8 ref_order_hint[constants::num_ref_frames] = {}; ///< \p ref_order_hint.
		u8 seg_q_m_level[3][constants::max_segments] = {}; ///< \p SegQMLevel.

		frame_size frame_size = zero; ///< Frame size, render size, and superres params.
		tile_info tile_info = zero; ///< Tile info.
		quantization_params quantization_params = zero; ///< Quantization params.
		segmentation_params segmentation_params = zero; ///< Segmentation params.
		delta_q_params delta_q_params = zero; ///< Delta Q params.
		delta_lf_params delta_lf_params = zero; ///< Loop filter delta parameters.
		loop_filter_params loop_filter_params = zero; ///< Loop filter parameters.
		cdef_params cdef_params = zero; ///< CDEF parameters.
		lr_params lr_params = zero; ///< Loop restoration parameters.
		global_motion_params global_motion_params = zero; ///< Global motion parameters.
		film_grain_params film_graim_params = zero; ///< Film grain parameters.

		u32 display_frame_id      = 0; ///< \p display_frame_id.
		u32 current_frame_id      = 0; ///< \p current_frame_id.
		u8  frame_to_show_map_idx = 0; ///< \p frame_to_show_map_idx.
		u8  refresh_frame_flags   = 0; ///< \p refresh_frame_flags.
		u8  order_hint            = 0; ///< \p order_hint.
		u8  primary_ref_frame     = 0; ///< \p primary_ref_frame.
		u8  last_frame_index      = 0; ///< \p last_frame_idx.
		u8  gold_frame_index      = 0; ///< \p gold_frame_idx.
		u8  lossless_bits         = 0; ///< \p LosslessArray.

		frame_type frame_type = zero; ///< \p frame_type.
		tx_mode tx_mode = zero; ///< \p TxMode.
		interpolation interpolation_filter = zero; ///< \p interpolation_filter.

		bool show_existing_frame          : 1 = false; ///< \p show_existing_frame.
		bool show_frame                   : 1 = false; ///< \p show_frame.
		bool showable_frame               : 1 = false; ///< \p showable_frame.
		bool error_resilient_mode         : 1 = false; ///< \p error_resilient_mode.
		bool disable_cdf_update           : 1 = false; ///< \p disable_cdf_update.
		bool allow_screen_content_tools   : 1 = false; ///< \p allow_screen_content_tools.
		bool force_integer_mv             : 1 = false; ///< \p force_integer_mv.
		bool frame_size_override          : 1 = false; ///< \p frame_size_override_flag.

		bool buffer_removal_time_present  : 1 = false; ///< \p buffer_removal_time_present_flag.
		bool allow_intrabc                : 1 = false; ///< \p allow_intrabc.
		bool frame_refs_short_signaling   : 1 = false; ///< \p frame_refs_short_signaling.
		bool allow_high_precision_mv      : 1 = false; ///< \p allow_high_precision_mv.
		bool is_motion_mode_switchable    : 1 = false; ///< \p is_motion_mode_switchable.
		bool use_ref_frame_mvs            : 1 = false; ///< \p use_ref_frame_mvs.
		bool disable_frame_end_update_cdf : 1 = false; ///< \p disable_frame_end_update_cdf.
		bool coded_lossless               : 1 = false; ///< \p CodedLossless.

		bool reference_select             : 1 = false; ///< \p reference_select.
		bool skip_mode_present            : 1 = false; ///< \p skip_mode_present.
		bool allow_warped_motion          : 1 = false; ///< \p allow_warped_motion.
		bool reduced_tx_set               : 1 = false; ///< \p reduced_tx_set.


		/// \p FrameIsIntra.
		[[nodiscard]] bool is_intra() const {
			return frame_type == frame_type::intra_only_frame || frame_type == frame_type::key_frame;
		}
		/// \p AllLossless.
		[[nodiscard]] bool is_all_lossless() const {
			return coded_lossless && frame_size.frame_width == frame_size.upscaled_width;
		}
	};

	/// 5.11.45. Read CFL alphas syntax
	struct cfl_alphas {
		/// Zero initialization.
		cfl_alphas(zero_t) {
		}

		i8 u = 0; ///< \p CflAlphaU.
		i8 v = 0; ///< \p CflAlphaV.
	};

	/// 5.11.6. Mode info syntax
	struct mode_info {
		/// Zero initialization.
		mode_info(zero_t) {
		}

		cfl_alphas cfl_alphas = zero; ///< CFL alpha values.

		segment_id_t segment_id = zero; ///< \p segment_id.
		prediction_mode y_mode = zero; ///< \p YMode.
		prediction_mode uv_mode = zero; ///< \p UVMode.
		motion_compensation motion_mode = zero; ///< \p motion_mode.
		compound_method compound_type = zero; ///< \p compound_type.
		interpolation interp_filter[2] = {}; ///< \p interp_filter.
		u8 palette_size_y = 0; ///< \p PaletteSizeY.
		u8 palette_size_uv = 0; ///< \p paletteSizeUV.
		u8 ref_mv_idx = 0; ///< \p RefMvIdx.
		i8 angle_delta_y = 0; ///< \p AngleDeltaY.
		i8 angle_delta_uv = 0; ///< \p AngleDeltaUV.
		reference_frame ref_frame[2] = { zero, zero }; ///< \p RefFrame.
		intra_filtering filter_intra_mode = zero; ///< \p filter_intra_mode.
		channel_t palette_colors_y[constants::palette_colors] = {}; ///< \p palette_colors_y.
		channel_t palette_colors_u[constants::palette_colors] = {}; ///< \p palette_colors_u.
		channel_t palette_colors_v[constants::palette_colors] = {}; ///< \p palette_colors_v.

		// Only available for 5.11.18. Inter frame mode info syntax
		reference_frame left_ref_frame[2]  = { zero, zero }; ///< \p LeftRefFrame.
		reference_frame above_ref_frame[2] = { zero, zero }; ///< \p AboveRefFrame.
		intra_prediction interintra_mode = zero; ///< \p interintra_mode.
		u8 wedge_index = 0; ///< \p wedge_index.

		bool lossless         : 1 = false; ///< \p Lossless.
		bool skip             : 1 = false; ///< \p skip.
		bool use_intrabc      : 1 = false; ///< \p use_intrabc.
		bool is_inter         : 1 = false; ///< \p is_inter.
		bool skip_mode        : 1 = false; ///< \p skip_mode.
		bool use_filter_intra : 1 = false; ///< \p use_filter_intra.

		// Only available for 5.11.18. Inter frame mode info syntax
		bool left_intra       : 1 = false; ///< \p LeftIntra.
		bool above_intra      : 1 = false; ///< \p AboveIntra.
		bool left_single      : 1 = false; ///< \p LeftSingle.
		bool above_single     : 1 = false; ///< \p AboveSingle.
		bool interintra       : 1 = false; ///< \p interintra.
		bool wedge_interintra : 1 = false; ///< \p wedge_interintra.
		bool wedge_sign       : 1 = false; ///< \p wedge_sign.


		/// 5.11.14. Segmentation feature active function
		/// \p seg_feature_active()
		[[nodiscard]] bool is_segment_feature_active(features feature, const segmentation_params &seg_params) const {
			return seg_params.is_feature_active(segment_id, feature);
		}
	};
}
