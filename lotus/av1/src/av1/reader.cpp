#include "lotus/av1/reader.h"

/// \file
/// Implementation of an AV1 OBU reader.

#include "lotus/logging.h"

#include "lotus/av1/common.h"
#include "lotus/av1/functions.h"
#include "lotus/av1/cdf_context.h"
#include "lotus/av1/symbol_decoder.h"
#include "lotus/av1/block_decoding.h"

namespace lotus::av1 {
	u8 reader::read_byte() {
		++_byte_position;
		return static_cast<u8>(_consume_byte());
	}

	bool reader::read_bit() {
		if (_residual_bits == 0) {
			_cur_byte = read_byte();
			_residual_bits = 8;
		}
		--_residual_bits;
		return _cur_byte & (1 << _residual_bits);
	}

	u32 reader::read_bits(u32 n) {
		// fast path: all bits are in the byte that have been read
		if (n <= _residual_bits) {
			_residual_bits -= n;
			return (_cur_byte >> _residual_bits) & ((1 << n) - 1);
		}

		u32 result = 0;
		// read residual bits first
		if (_residual_bits > 0) {
			result = static_cast<u32>(_cur_byte & ((1 << _residual_bits) - 1)) << (n - _residual_bits);
			n -= _residual_bits;
			_residual_bits = 0;
		}
		// read whole bytes
		for (u32 i = 0; i < 4 && n >= 8; ++i) {
			n -= 8;
			result |= static_cast<u32>(read_byte()) << n;
		}
		// read remaining bits
		if (n > 0) {
			_cur_byte = read_byte();
			_residual_bits = 8 - n;
			result |= _cur_byte >> _residual_bits;
		}
		return result;
	}

	u64 reader::get_position() const {
		return _byte_position * 8 - _residual_bits;
	}

	u32 reader::read_uvlc() {
		// TODO more efficient implementation?
		u32 leading_zeros = 0;
		for (; !read_bit(); ++leading_zeros) {
		}
		if (leading_zeros >= 32) {
			return 0xFFFFFFFFu;
		}
		return read_bits(leading_zeros) + (1u << leading_zeros) - 1u;
	}

	u64 reader::read_le(u32 n) {
		_check_byte_aligned();
		u64 t = 0;
		for (u32 i = 0; i < n; ++i) {
			const u8 byte = read_byte();
			t |= static_cast<u64>(byte) << (i * 8);
		}
		return t;
	}

	std::pair<u64, u32> reader::read_leb128() {
		_check_byte_aligned();

		u64 result = 0;
		u32 leb128_bytes = 0;
		for (u32 i = 0; i < 8; ++i) {
			const u8 b = read_byte();
			result |= static_cast<u64>(b & 0x7F) << (7 * i);
			++leb128_bytes;
			if (!(b & 0x80)) {
				break;
			}
		}
		return { result, leb128_bytes };
	}

	i32 reader::read_su(u32 n) {
		const u32 value = read_bits(n);
		// sign extend the top bit
		return static_cast<i32>(value << (32 - n)) >> (32 - n);
	}

	u32 reader::read_ns(u32 n) {
		const u32 w = functions::floor_log2(n) + 1;
		const u32 m = (1u << w) - n;
		const u32 v = read_bits(w - 1);
		if (v < m) {
			return v;
		}
		const u32 extra_bit = read_bit() ? 1 : 0;
		return (v << 1) + extra_bit - m;
	}

	obu::header reader::read_header() {
		_check_byte_aligned();

		obu::header result = zero;
		{
			const u8 b0 = read_byte();
			result.obu_type       = static_cast<obu::type>((b0 & 0x78) >> 3);
			result.extension_flag = b0 & 0x4u;
			result.has_size_field = b0 & 0x2u;
		}
		if (result.extension_flag) {
			const u8 b1 = read_byte();
			result.extension.temporal_id = (b1 & 0xE0) >> 5;
			result.extension.spatial_id  = (b1 & 0x18) >> 3;
		}
		return result;
	}

	void reader::read_trailing_bits(u64 nb_bits) {
		const bool trailing_one_bit = read_bit();
		crash_if(!trailing_one_bit);
		--nb_bits;
		while (nb_bits >= 8) {
			const u8 trailing_zero_bits = read_byte();
			crash_if(trailing_zero_bits != 0);
			nb_bits -= 8;
		}
		while (nb_bits > 0) {
			const bool trailing_zero_bit = read_bit();
			crash_if(trailing_zero_bit);
			--nb_bits;
		}
	}

	void reader::read_byte_alignment() {
		const u8 bits = _cur_byte & ((1u << _residual_bits) - 1);
		crash_if(bits != 0);
		_residual_bits = 0;
	}

	obu::sequence_header reader::read_sequence_header() {
		obu::sequence_header result = zero;
		{
			const u8 b5 = read_bits<5>();
			result.seq_profile                  = b5 >> 2;
			result.still_picture                = b5 & 0x2;
			result.reduced_still_picture_header = b5 & 0x1;
		}
		if (result.reduced_still_picture_header) {
			result.operating_points[0].sequence_level_index = read_bits<5>();
		} else {
			result.timing_info_present = read_bit();
			if (result.timing_info_present) {
				result.timing_info = read_timing_info();
				result.decoder_model_info_present = read_bit();
				if (result.decoder_model_info_present) {
					result.decoder_model_info = read_decoder_model_info();
				}
			} else {
				result.decoder_model_info_present = false;
			}
			result.initial_display_delay_present = read_bit();
			result.operating_points_count        = read_bits<5>() + 1;
			for (u32 op_idx = 0; op_idx < result.operating_points_count; ++op_idx) {
				obu::sequence_header::operating_point &op = result.operating_points[op_idx];
				op.mask                 = read_bits<12>();
				op.sequence_level_index = read_bits<5>();
				if (op.sequence_level_index > 7) {
					op.sequence_tier = read_bit();
				} else {
					op.sequence_tier = false;
				}
				if (result.decoder_model_info_present) {
					op.decoder_model_present = read_bit();
					if (op.decoder_model_present) {
						op.operating_parameters =
							read_operating_parameters_info(result.decoder_model_info.buffer_delay_length);
					}
				}
				if (result.initial_display_delay_present) {
					op.initial_display_delay_present = read_bit();
					if (op.initial_display_delay_present) {
						op.initial_display_delay = read_bits<4>() + 1;
					}
				}
			}
		}
		// TODO choose operating point
		result.frame_width_bits  = read_bits<4>() + 1;
		result.frame_height_bits = read_bits<4>() + 1;
		result.max_frame_width  = read_bits(result.frame_width_bits) + 1;
		result.max_frame_height = read_bits(result.frame_height_bits) + 1;
		if (result.reduced_still_picture_header) {
			result.frame_id_numbers_present = false;
		} else {
			result.frame_id_numbers_present = read_bit();
		}
		if (result.frame_id_numbers_present) {
			result.delta_frame_id_length      = read_bits<4>() + 2;
			result.additional_frame_id_length = read_bits<3>() + 1;
		}
		{
			const u8 b3 = read_bits<3>();
			result.use_128x128_superblock   = b3 & 0x4;
			result.enable_filter_intra      = b3 & 0x2;
			result.enable_intra_edge_filter = b3 & 0x1;
		}
		if (result.reduced_still_picture_header) {
			result.seq_force_screen_content_tools = constants::select_screen_content_tools;
			result.seq_force_integer_mv           = constants::select_integer_mv;
		} else {
			{
				const u8 b5 = read_bits<5>();
				result.enable_interintra_compound = b5 & 0x10;
				result.enable_masked_compound     = b5 & 0x8;
				result.enable_warped_motion       = b5 & 0x4;
				result.enable_dual_filter         = b5 & 0x2;
				result.enable_order_hint          = b5 & 0x1;
			}
			if (result.enable_order_hint) {
				const u8 b2 = read_bits<2>();
				result.enable_jnt_comp      = b2 & 0x2;
				result.enable_ref_frame_mvs = b2 & 0x1;
			}
			if ((result.seq_choose_screen_content_tools = read_bit())) {
				result.seq_force_screen_content_tools = constants::select_screen_content_tools;
			} else {
				result.seq_force_screen_content_tools = read_bit() ? 1 : 0;
			}
			if (result.seq_force_screen_content_tools > 0) {
				if ((result.seq_choose_integer_mv = read_bit())) {
					result.seq_force_integer_mv = constants::select_integer_mv;
				} else {
					result.seq_force_integer_mv = read_bit() ? 1 : 0;
				}
			} else {
				result.seq_force_integer_mv = constants::select_integer_mv;
			}
			if (result.enable_order_hint) {
				result.order_hint_bits = read_bits<3>() + 1;
			}
		}
		{
			const u8 b3 = read_bits<3>();
			result.enable_superres    = b3 & 0x4;
			result.enable_cdef        = b3 & 0x2;
			result.enable_restoration = b3 & 0x1;
		}
		result.color_config = read_color_config(result.seq_profile);
		result.film_grain_params_present = read_bit();
		return result;
	}

	obu::color_config reader::read_color_config(u8 seq_profile) {
		obu::color_config result = zero;
		const bool high_bitdepth = read_bit();
		if (seq_profile == 2 && high_bitdepth) {
			const bool twelve_bit = read_bit();
			result.bit_depth = twelve_bit ? 12 : 10;
		} else if (seq_profile <= 2) {
			result.bit_depth = high_bitdepth ? 10 : 8;
		}
		if (seq_profile != 1) {
			result.mono_chrome = read_bit();
		}
		if ((result.color_description_present = read_bit())) {
			const u32 b24 = read_bits<24>();
			result.color_primaries          = static_cast<color_primaries>((b24 >> 16) & 0xFF);
			result.transfer_characteristics = static_cast<transfer_characteristics>((b24 >> 8) & 0xFF);
			result.matrix_coefficients      = static_cast<matrix_coefficients>(b24 & 0xFF);
		} else {
			result.color_primaries          = color_primaries::unspecified;
			result.transfer_characteristics = transfer_characteristics::unspecified;
			result.matrix_coefficients      = matrix_coefficients::unspecified;
		}
		if (result.mono_chrome) {
			result.color_range = read_bit();
			result.subsampling_x = true;
			result.subsampling_y = true;
			result.chroma_sample_position = chroma_sample_position::unknown;
			return result;
		}
		if (
			result.color_primaries == color_primaries::bt_709 &&
			result.transfer_characteristics == transfer_characteristics::srgb &&
			result.matrix_coefficients == matrix_coefficients::identity
		) {
			result.color_range = true;
		} else {
			result.color_range = read_bit();
			if (seq_profile == 0) {
				result.subsampling_x = true;
				result.subsampling_y = true;
			} else if (seq_profile != 1) {
				if (result.bit_depth == 12) {
					if ((result.subsampling_x = read_bit())) {
						result.subsampling_y = read_bit();
					}
				} else {
					result.subsampling_x = true;
				}
			}
			if (result.subsampling_x && result.subsampling_y) {
				result.chroma_sample_position = static_cast<chroma_sample_position>(read_bits<2>());
			}
		}
		result.separate_uv_delta_q = read_bit();
		return result;
	}

	obu::timing_info reader::read_timing_info() {
		obu::timing_info result = zero;
		result.num_units_in_display_tick = read_bits<32>();
		result.time_scale                = read_bits<32>();
		if ((result.equal_picture_interval = read_bit())) {
			result.num_tickes_per_picture = read_uvlc() + 1;
		}
		return result;
	}

	obu::decoder_model_info reader::read_decoder_model_info() {
		obu::decoder_model_info result = zero;
		result.buffer_delay_length        = read_bits<5>() + 1;
		result.num_units_in_decoding_tick = read_bits<32>();
		{
			const u16 b10 = read_bits<10>();
			result.buffer_removal_time_length     = static_cast<u8>((b10 >> 5) + 1);
			result.frame_presentation_time_length = static_cast<u8>((b10 & 0x1F) + 1);
		}
		return result;
	}

	obu::operating_parameters_info reader::read_operating_parameters_info(u32 buffer_delay_length) {
		obu::operating_parameters_info result = zero;
		result.decoder_buffer_delay = read_bits(buffer_delay_length);
		result.encoder_buffer_delay = read_bits(buffer_delay_length);
		result.low_delay_mode       = read_bit();
		return result;
	}

	obu::uncompressed_header reader::read_uncompressed_header(
		const obu::header &header,
		const obu::sequence_header &seq_header,
		state::block &mi,
		state::cdf &cdf
	) {
		obu::uncompressed_header result = zero;

		u32 id_len = 0;
		if (seq_header.frame_id_numbers_present) {
			id_len = seq_header.get_frame_id_length();
		}
		constexpr u32 _all_frames = (1u << constants::num_ref_frames) - 1;
		if (seq_header.reduced_still_picture_header) {
			result.frame_type = frame_type::key_frame;
			// FrameIsIntra set implicitly
			result.show_frame = true;
		} else {
			result.show_existing_frame = read_bit();
			if (result.show_existing_frame) {
				result.frame_to_show = read_bits<3>();
				if (seq_header.decoder_model_info_present && !seq_header.timing_info.equal_picture_interval) {
					std::abort(); // TODO temporal point info
				}
				if (seq_header.frame_id_numbers_present) {
					result.display_frame_id = read_bits(id_len);
				}
				/*result.frame_type = ref_frame_type[];*/ // TODO RefFrameType
				if (result.frame_type == frame_type::key_frame) {
					result.refresh_frame_flags = _all_frames;
				}
				if (seq_header.film_grain_params_present) {
					std::abort(); // TODO grain params
				}
				return result;
			}
			{
				const u32 b3 = read_bits<3>();
				result.frame_type = static_cast<frame_type>(b3 >> 1);
				// FrameIsIntra set implicitly
				result.show_frame = b3 & 0x1;
			}
			if (
				result.show_frame &&
				seq_header.decoder_model_info_present &&
				!seq_header.timing_info.equal_picture_interval
			) {
				std::abort(); // TODO temporal point info
			}
			if (result.show_frame) {
				result.showable_frame = result.frame_type != frame_type::key_frame;
			} else {
				result.showable_frame = read_bit();
			}
			if (
				result.frame_type == frame_type::switch_frame ||
				(result.frame_type == frame_type::key_frame && result.show_frame)
			) {
				result.error_resilient_mode = true;
			} else {
				result.error_resilient_mode = read_bit();
			}
		}
		if (result.frame_type == frame_type::key_frame && result.show_frame) {
			// TODO clear values
		}
		result.disable_cdf_update = read_bit();
		if (seq_header.seq_force_screen_content_tools == constants::select_screen_content_tools) {
			result.allow_screen_content_tools = read_bit();
		} else {
			result.allow_screen_content_tools = seq_header.seq_force_screen_content_tools;
		}
		if (result.allow_screen_content_tools) {
			if (seq_header.seq_force_integer_mv == constants::select_integer_mv) {
				result.force_integer_mv = read_bit();
			} else {
				result.force_integer_mv = seq_header.seq_force_integer_mv;
			}
		}
		if (result.is_intra()) {
			result.force_integer_mv = true;
		}
		if (seq_header.frame_id_numbers_present) {
			result.current_frame_id = read_bits(id_len);
			// TODO mark_ref_frames
		}
		if (result.frame_type == frame_type::switch_frame) {
			result.frame_size_override = true;
		} else if (!seq_header.reduced_still_picture_header) {
			result.frame_size_override = read_bit();
		}
		result.order_hint = static_cast<u8>(read_bits(seq_header.order_hint_bits));
		if (result.is_intra() || result.error_resilient_mode) {
			result.primary_ref_frame = constants::primary_ref_none;
		} else {
			result.primary_ref_frame = read_bits<3>();
		}
		if (seq_header.decoder_model_info_present) {
			if ((result.buffer_removal_time_present = read_bit())) {
				for (u32 op = 0; op < seq_header.operating_points_count; ++op) {
					if (seq_header.operating_points[op].decoder_model_present) {
						const u32 op_idc = seq_header.operating_points[op].mask;
						const bool temporal_layer = op_idc & (1u << header.extension.temporal_id);
						const bool spatial_layer = op_idc & (1u << (header.extension.spatial_id + 8));
						if (op_idc == 0 || (temporal_layer && spatial_layer)) {
							result.buffer_removal_time[op] =
								read_bits(seq_header.decoder_model_info.buffer_removal_time_length);
						}
					}
				}
			}
		}
		if (
			result.frame_type == frame_type::switch_frame ||
			(result.frame_type == frame_type::key_frame && result.show_frame)
		) {
			result.refresh_frame_flags = _all_frames;
		} else {
			result.refresh_frame_flags = read_bits<8>();
		}
		if (!result.is_intra() || result.refresh_frame_flags != _all_frames) {
			if (result.error_resilient_mode && seq_header.enable_order_hint) {
				for (u32 i = 0; i < constants::num_ref_frames; ++i) {
					result.ref_order_hint[i] = static_cast<u8>(read_bits(seq_header.order_hint_bits));
					// TODO ref valid
				}
			}
		}
		if (result.is_intra()) {
			result.frame_size = read_frame_size(seq_header, result.frame_size_override);
			read_render_size(result.frame_size);
			if (
				result.allow_screen_content_tools &&
				result.frame_size.upscaled_width == result.frame_size.frame_width
			) {
				result.allow_intrabc = read_bit();
			}
		} else {
			if (seq_header.enable_order_hint) {
				if ((result.frame_refs_short_signaling = read_bit())) {
					const u8 b6 = read_bits<6>();
					result.last_frame_index = b6 >> 3;
					result.gold_frame_index = b6 & 0x7;
					// TODO set_frame_refs
				}
			}
			for (u32 i = 0; i < constants::refs_per_frame; ++i) {
				if (!result.frame_refs_short_signaling) {
					result.ref_frames[i].frame_index = read_bits<3>();
				}
				if (seq_header.frame_id_numbers_present) {
					const u32 delta_frame_id = read_bits(seq_header.delta_frame_id_length) + 1;
					result.ref_frames[i].expected_frame_id =
						(result.current_frame_id + (1u << id_len) - delta_frame_id) % (1u << id_len);
				}
			}
			if (result.frame_size_override && !result.error_resilient_mode) {
				// TODO frame size with refs
			} else {
				result.frame_size = read_frame_size(seq_header, result.frame_size_override);
				read_render_size(result.frame_size);
			}
			if (!result.force_integer_mv) {
				result.allow_high_precision_mv = read_bit();
			}
			result.interpolation_filter = read_interpolation_filter();
			result.is_motion_mode_switchable = read_bit();
			if (!(result.error_resilient_mode || !seq_header.enable_ref_frame_mvs)) {
				result.use_ref_frame_mvs = read_bit();
			}
			// TODO order hint, ref frame sign bias
		}
		if (seq_header.reduced_still_picture_header || result.disable_cdf_update) {
			result.disable_frame_end_update_cdf = true;
		} else {
			result.disable_frame_end_update_cdf = read_bit();
		}
		if (result.primary_ref_frame == constants::primary_ref_none) {
			cdf.init_non_coeff_cdfs();
			// TODO setup_past_independence
		} else {
			// TODO
		}
		if (result.use_ref_frame_mvs) {
			// TODO motion field estimation
		}
		result.tile_info = read_tile_info(seq_header, result, mi);
		result.quantization_params = read_quantization_params(seq_header.color_config);
		result.segmentation_params = read_segmentation_params(result);
		result.delta_q_params = read_delta_q_params(result.quantization_params.base_q_idx);
		result.delta_lf_params = read_delta_lf_params(result.delta_q_params.delta_q_present, result.allow_intrabc);
		if (result.primary_ref_frame == constants::primary_ref_none) {
			cdf.init_coeff_cdfs(result.quantization_params.base_q_idx);
		} else {
			// TODO load_previous_segment_ids
		}
		result.coded_lossless = true;
		for (u8 segment_id = 0; segment_id < constants::max_segments; ++segment_id) {
			const u32 qindex = functions::get_qindex(result, mi, true, static_cast<segment_id_t>(segment_id));
			const bool lossless =
				qindex == 0 &&
				result.quantization_params.delta_q_y_dc == 0 &&
				result.quantization_params.delta_q_u_ac == 0 &&
				result.quantization_params.delta_q_u_dc == 0 &&
				result.quantization_params.delta_q_v_ac == 0 &&
				result.quantization_params.delta_q_v_dc == 0;
			if (lossless) {
				result.lossless_bits |= 1u << segment_id;
			} else {
				result.coded_lossless = false;
			}
			if (result.quantization_params.using_qmatrix) {
				if (lossless) {
					result.seg_q_m_level[0][segment_id] = 15;
					result.seg_q_m_level[1][segment_id] = 15;
					result.seg_q_m_level[2][segment_id] = 15;
				} else {
					result.seg_q_m_level[0][segment_id] = result.quantization_params.qm_y;
					result.seg_q_m_level[1][segment_id] = result.quantization_params.qm_u;
					result.seg_q_m_level[2][segment_id] = result.quantization_params.qm_v;
				}
			}
		}
		result.loop_filter_params = read_loop_filter_params(seq_header, result);
		result.cdef_params = read_cdef_params(seq_header, result);
		result.lr_params = read_lr_params(seq_header, result);
		result.tx_mode = read_tx_mode(result.coded_lossless);
		{ // 5.9.23. Frame reference mode syntax
			if (!result.is_intra()) {
				result.reference_select = read_bit();
			}
		}
		// TODO skip mode params
		if (!(result.is_intra() || result.error_resilient_mode || !seq_header.enable_warped_motion)) {
			result.allow_warped_motion = read_bit();
		}
		result.reduced_tx_set = read_bit();
		result.global_motion_params = read_global_motion_params(result);
		result.film_graim_params = read_film_grain_params(seq_header, result);
		return result;
	}

	obu::frame_size reader::read_frame_size(const obu::sequence_header &seq_header, bool frame_size_override) {
		obu::frame_size result = zero;
		if (frame_size_override) {
			result.frame_width  = read_bits(seq_header.frame_width_bits);
			result.frame_height = read_bits(seq_header.frame_height_bits);
		} else {
			result.frame_width  = seq_header.max_frame_width;
			result.frame_height = seq_header.max_frame_height;
		}
		read_superres_params(result, seq_header.enable_superres);
		// TODO compute image size
		return result;
	}

	void reader::read_render_size(obu::frame_size &frame_size) {
		if ((frame_size.render_and_frame_size_different = read_bit())) {
			const u32 b32 = read_bits<32>();
			frame_size.render_width  = (b32 >> 16) + 1;
			frame_size.render_height = (b32 & 0xFFFF) + 1;
		} else {
			frame_size.render_width  = frame_size.upscaled_width;
			frame_size.render_height = frame_size.frame_height;
		}
	}

	void reader::read_superres_params(obu::frame_size &frame_size, bool enable_superres) {
		if (enable_superres) {
			frame_size.use_superres = read_bit();
		}
		if (frame_size.use_superres) {
			const u8 coded_denom = read_bits<constants::superres_denom_bits>();
			frame_size.superres_denom = coded_denom + constants::superres_denom_min;
		} else {
			frame_size.superres_denom = constants::superres_num;
		}
		frame_size.upscaled_width = frame_size.frame_width;
		frame_size.frame_width =
			(frame_size.upscaled_width * constants::superres_num + (frame_size.superres_denom / 2)) /
			frame_size.superres_denom;
	}

	interpolation reader::read_interpolation_filter() {
		const bool is_filter_switchable = read_bit();
		if (is_filter_switchable) {
			return interpolation::switchable;
		} else {
			return static_cast<interpolation>(read_bits(2));
		}
	}

	obu::loop_filter_params reader::read_loop_filter_params(
		const obu::sequence_header &seq_header, const obu::uncompressed_header &header
	) {
		obu::loop_filter_params result = zero;
		if (header.coded_lossless || header.allow_intrabc) {
			result.ref_deltas[std::to_underlying(reference_frame::intra)]   = 1;
			result.ref_deltas[std::to_underlying(reference_frame::golden)]  = -1;
			result.ref_deltas[std::to_underlying(reference_frame::altref)]  = -1;
			result.ref_deltas[std::to_underlying(reference_frame::altref2)] = -1;
			return result;
		}
		{
			const u16 b12 = read_bits<12>();
			result.level[0] = static_cast<u8>(b12 >> 6);
			result.level[1] = b12 & 0x3F;
		}
		if (seq_header.color_config.get_num_planes() > 1) {
			if (result.level[0] || result.level[1]) {
				const u16 b12 = read_bits<12>();
				result.level[2] = static_cast<u8>(b12 >> 6);
				result.level[3] = b12 & 0x3F;
			}
		}
		{
			const u8 b4 = read_bits<4>();
			result.sharpness     = b4 >> 1;
			result.delta_enabled = b4 & 0x1;
		}
		if (result.delta_enabled) {
			if ((result.delta_update = read_bit())) {
				for (u32 i = 0; i < constants::total_refs_per_frame; ++i) {
					if (const bool update_ref_delta = read_bit(); update_ref_delta) {
						result.ref_deltas[i] = static_cast<i8>(read_su(7));
					}
				}
				for (u32 i = 0; i < 2; ++i) {
					if (const bool update_mode_delta = read_bit(); update_mode_delta) {
						result.mode_deltas[i] = static_cast<i8>(read_su(7));
					}
				}
			}
		}
		return result;
	}

	obu::quantization_params reader::read_quantization_params(const obu::color_config &color_config) {
		obu::quantization_params result = zero;
		result.base_q_idx = read_bits<8>();
		result.delta_q_y_dc = read_delta_q();
		if (color_config.get_num_planes() > 1) {
			bool diff_uv_delta = false;
			if (color_config.separate_uv_delta_q) {
				diff_uv_delta = read_bit();
			}
			result.delta_q_u_dc = read_delta_q();
			result.delta_q_u_ac = read_delta_q();
			if (diff_uv_delta) {
				result.delta_q_v_dc = read_delta_q();
				result.delta_q_v_ac = read_delta_q();
			} else {
				result.delta_q_v_dc = result.delta_q_u_dc;
				result.delta_q_v_ac = result.delta_q_u_ac;
			}
		}
		if ((result.using_qmatrix = read_bit())) {
			{
				const u8 b8 = read_bits<8>();
				result.qm_y = b8 >> 4;
				result.qm_u = b8 & 0xF;
			}
			if (!color_config.separate_uv_delta_q) {
				result.qm_v = result.qm_u;
			} else {
				result.qm_v = read_bits<4>();
			}
		}
		return result;
	}

	i8 reader::read_delta_q() {
		const bool delta_coded = read_bit();
		if (delta_coded) {
			return static_cast<i8>(read_su(7));
		}
		return 0;
	}

	obu::segmentation_params reader::read_segmentation_params(const obu::uncompressed_header &header) {
		obu::segmentation_params result = zero;
		if ((result.segmentation_enabled = read_bit())) {
			if (header.primary_ref_frame == constants::primary_ref_none) {
				result.segmentation_update_map = true;
				result.segmentation_update_data = true;
			} else {
				if ((result.segmentation_update_map = read_bit())) {
					result.segmentation_temporal_update = read_bit();
				}
				result.segmentation_update_data = read_bit();
			}
			if (result.segmentation_update_data) {
				for (u32 i = 0; i < constants::max_segments; ++i) {
					for (u32 j = 0; j < std::to_underlying(features::max); ++j) {
						i16 clipped_value = 0;
						if (const bool feature_enabled = read_bit(); feature_enabled) {
							result.feature_enabled[i] |= 1u << j;
							const u32 bits_to_read = constants::segmentation_feature_bits[j];
							const u32 limit = constants::segmentation_feature_max[j];
							if (constants::segmentation_feature_signed[j]) {
								const i32 feature_value = read_su(bits_to_read + 1);
								const auto ilimit = static_cast<i32>(limit);
								clipped_value = static_cast<i16>(std::clamp(feature_value, -ilimit, ilimit));
							} else {
								const u32 feature_value = read_bits(bits_to_read);
								clipped_value = static_cast<i16>(std::clamp<u32>(feature_value, 0, limit));
							}
						}
						result.feature_data[i][j] = clipped_value;
					}
				}
			}
		}
		for (u32 i = 0; i < constants::max_segments; ++i) {
			for (u32 j = 0; j < std::to_underlying(features::max); ++j) {
				if (result.feature_enabled[i] & (1u << j)) {
					result.last_active_seg_id = static_cast<segment_id_t>(i);
					if (static_cast<features>(j) >= features::ref_frame) {
						result.seg_id_pre_skip = true;
					}
				}
			}
		}
		return result;
	}

	obu::tile_info reader::read_tile_info(
		const obu::sequence_header &seq_header, const obu::uncompressed_header &header, state::block &mi
	) {
		const u32 mi_cols = header.frame_size.get_mi_cols();
		const u32 mi_rows = header.frame_size.get_mi_rows();
		const u32 sb_cols = seq_header.use_128x128_superblock ? ((mi_cols + 31) >> 5) : ((mi_cols + 15) >> 4);
		const u32 sb_rows = seq_header.use_128x128_superblock ? ((mi_rows + 31) >> 5) : ((mi_rows + 15) >> 4);
		const u32 sb_shift = seq_header.use_128x128_superblock ? 5 : 4;
		const u32 sb_size = sb_shift + 2;
		const u32 max_tile_width_sb = constants::max_tile_width >> sb_size;
		u32 max_tile_area_sb = constants::max_tile_area >> (2 * sb_size);
		const u32 min_log2_tile_cols = functions::tile_log2(max_tile_width_sb, sb_cols);
		const u32 max_log2_tile_cols = functions::tile_log2_1(std::min(sb_cols, constants::max_tile_cols));
		const u32 max_log2_tile_rows = functions::tile_log2_1(std::min(sb_rows, constants::max_tile_rows));
		const u32 min_log2_tiles = std::max(
			min_log2_tile_cols,
			functions::tile_log2(max_tile_area_sb, sb_rows * sb_cols)
		);

		obu::tile_info result = zero;
		if ((result.uniform_tile_spacing = read_bit())) {
			result.tile_cols_log2 = min_log2_tile_cols;
			while (result.tile_cols_log2 < max_log2_tile_cols) {
				if (const bool increment_tile_cols_log2 = read_bit(); increment_tile_cols_log2) {
					++result.tile_cols_log2;
				} else {
					break;
				}
			}
			const u32 tile_width_sb = (sb_cols + (1u << result.tile_cols_log2) - 1) >> result.tile_cols_log2;
			{
				u32 i = 0;
				for (u32 start_sb = 0; start_sb < sb_cols; start_sb += tile_width_sb) {
					mi.col_starts[i] = start_sb << sb_shift;
					++i;
				}
				mi.col_starts[i] = mi_cols;
				result.tile_cols = i;
			}

			const u32 min_log2_tile_rows = std::max(min_log2_tiles, result.tile_cols_log2) - result.tile_cols_log2;
			result.tile_rows_log2 = min_log2_tile_rows;
			while (result.tile_rows_log2 < max_log2_tile_rows) {
				if (const bool increment_tile_rows_log2 = read_bit(); increment_tile_rows_log2) {
					++result.tile_rows_log2;
				} else {
					break;
				}
			}
			const u32 tile_height_sb = (sb_rows + (1u << result.tile_rows_log2) - 1) >> result.tile_rows_log2;
			{
				u32 i = 0;
				for (u32 start_sb = 0; start_sb < sb_rows; start_sb += tile_height_sb) {
					mi.row_starts[i] = start_sb << sb_shift;
					++i;
				}
				mi.row_starts[i] = mi_rows;
				result.tile_rows = i;
			}
		} else {
			u32 widest_tile_sb = 0;
			{
				u32 i = 0;
				for (u32 start_sb = 0; start_sb < sb_cols; ++i) {
					mi.col_starts[i] = start_sb << sb_shift;
					const u32 max_width = std::min(sb_cols - start_sb, max_tile_width_sb);
					const u32 size_sb = read_ns(max_width) + 1;
					widest_tile_sb = std::max(size_sb, widest_tile_sb);
					start_sb += size_sb;
				}
				mi.col_starts[i] = mi_cols;
				result.tile_cols = i;
				result.tile_cols_log2 = functions::tile_log2_1(result.tile_cols);
			}

			if (min_log2_tiles > 0) {
				max_tile_area_sb = (sb_rows * sb_cols) >> (min_log2_tiles + 1);
			} else {
				max_tile_area_sb = sb_rows * sb_cols;
			}
			const u32 max_tile_height_sb = std::max(max_tile_area_sb / widest_tile_sb, 1u);

			{
				u32 i = 0;
				for (u32 start_sb = 0; start_sb < sb_rows; ++i) {
					mi.row_starts[i] = start_sb << sb_shift;
					const u32 max_height = std::min(sb_rows - start_sb, max_tile_height_sb);
					const u32 size_sb = read_ns(max_height) + 1;
					start_sb += size_sb;
				}
				mi.row_starts[i] = mi_rows;
				result.tile_rows = i;
				result.tile_rows_log2 = functions::tile_log2_1(result.tile_rows);
			}
		}
		if (result.tile_cols_log2 > 0 || result.tile_rows_log2 > 0) {
			result.context_update_tile_id = read_bits(result.tile_rows_log2 + result.tile_cols_log2);
			result.tile_size_bytes = read_bits<2>() + 1;
		}
		return result;
	}

	obu::delta_q_params reader::read_delta_q_params(u8 base_q_idx) {
		obu::delta_q_params result = zero;
		if (base_q_idx > 0) {
			result.delta_q_present = read_bit();
		}
		if (result.delta_q_present) {
			result.delta_q_res = read_bits<2>();
		}
		return result;
	}

	obu::delta_lf_params reader::read_delta_lf_params(bool delta_q_present, bool allow_intrabc) {
		obu::delta_lf_params result = zero;
		if (delta_q_present) {
			if (!allow_intrabc) {
				result.delta_lf_present = read_bit();
			}
			if (result.delta_lf_present) {
				const u8 b3 = read_bits<3>();
				result.delta_lf_res   = b3 >> 1;
				result.delta_lf_multi = b3 & 0x1;
			}
		}
		return result;
	}

	obu::cdef_params reader::read_cdef_params(
		const obu::sequence_header &seq_header, const obu::uncompressed_header &header
	) {
		obu::cdef_params result = zero;
		if (header.coded_lossless || header.allow_intrabc || !seq_header.enable_cdef) {
			result.damping = 3;
			return result;
		}
		{
			const u8 b4 = read_bits<4>();
			result.damping = (b4 >> 2) + 3;
			result.bits    = b4 & 0x3;
		}
		for (u32 i = 0; i < (1u << result.bits); ++i) {
			{
				const u8 b6 = read_bits<6>();
				result.y_pri_strength[i] = b6 >> 2;
				result.y_sec_strength[i] = b6 & 0x3;
			}
			if (result.y_sec_strength[i] == 3) {
				++result.y_sec_strength[i];
			}
			if (seq_header.color_config.get_num_planes() > 1) {
				{
					const u8 b6 = read_bits<6>();
					result.uv_pri_strength[i] = b6 >> 2;
					result.uv_sec_strength[i] = b6 & 0x3;
				}
				if (result.uv_sec_strength[i] == 3) {
					++result.uv_sec_strength[i];
				}
			}
		}
		return result;
	}

	obu::lr_params reader::read_lr_params(
		const obu::sequence_header &seq_header, const obu::uncompressed_header &header
	) {
		obu::lr_params result = zero;
		if (header.is_all_lossless() || header.allow_intrabc || !seq_header.enable_restoration) {
			result.frame_restoration_type[0] = frame_restoration::none;
			result.frame_restoration_type[1] = frame_restoration::none;
			result.frame_restoration_type[2] = frame_restoration::none;
			return result;
		}
		bool uses_chroma_lr = false;
		const u32 num_planes = seq_header.color_config.get_num_planes();
		for (u32 i = 0; i < num_planes; ++i) {
			const u8 lr_type = read_bits<2>();
			result.frame_restoration_type[i] = constants::remap_lr_type[lr_type];
			if (result.frame_restoration_type[i] != frame_restoration::none) {
				result.uses_lr = true;
				if (i > 0) {
					uses_chroma_lr = true;
				}
			}
		}
		if (result.uses_lr) {
			u32 lr_unit_shift = read_bit() ? 1 : 0;
			if (seq_header.use_128x128_superblock) {
				++lr_unit_shift;
			} else {
				if (lr_unit_shift) {
					const u32 lr_unit_extra_shift = read_bit() ? 1 : 0;
					lr_unit_shift += lr_unit_extra_shift;
				}
			}
			result.size[0] = constants::restoration_tilesize_max >> (2 - lr_unit_shift);
			bool lr_uv_shift = false;
			if (seq_header.color_config.subsampling_x && seq_header.color_config.subsampling_y && uses_chroma_lr) {
				lr_uv_shift = read_bit();
			}
			result.size[1] = result.size[2] = result.size[0] >> (lr_uv_shift ? 1 : 0);
		}
		return result;
	}

	tx_mode reader::read_tx_mode(bool coded_lossless) {
		if (coded_lossless) {
			return tx_mode::only_4x4;
		} else {
			const bool tx_mode_select = read_bit();
			if (tx_mode_select) {
				return tx_mode::select;
			} else {
				return tx_mode::largest;
			}
		}
	}

	obu::global_motion_params reader::read_global_motion_params(const obu::uncompressed_header &header) {
		obu::global_motion_params result = zero;
		for (
			i8 ref = std::to_underlying(reference_frame::last);
			ref <= std::to_underlying(reference_frame::altref);
			++ref
		) {
			obu::global_motion_params::ref &gm = result.refs[ref];
			gm.type = warp_model::identity;
			for (u32 i = 0; i < 6; ++i) {
				gm.params[i] = (i % 3 == 2) ? (1 << constants::warpedmodel_prec_bits) : 0;
			}
		}
		if (header.is_intra()) {
			return result;
		}
		for (
			i8 ref = std::to_underlying(reference_frame::last);
			ref <= std::to_underlying(reference_frame::altref);
			++ref
		) {
			obu::global_motion_params::ref &gm = result.refs[ref];
			if (const bool is_global = read_bit(); is_global) {
				if (const bool is_rot_zoom = read_bit(); is_rot_zoom) {
					gm.type = warp_model::rotzoom;
				} else {
					const bool is_translation = read_bit();
					gm.type = is_translation ? warp_model::translation : warp_model::affine;
				}
			} else {
				gm.type = warp_model::identity;
			}

			obu::global_motion_params::ref prev_gm; // TODO
			auto do_read_global_param = [&](u32 idx) {
				gm.params[idx] = read_global_param(
					gm.type, idx, prev_gm.params[idx], header.allow_high_precision_mv
				);
			};
			if (gm.type >= warp_model::rotzoom) {
				do_read_global_param(2);
				do_read_global_param(3);
				if (gm.type == warp_model::affine) {
					do_read_global_param(4);
					do_read_global_param(5);
				} else {
					gm.params[4] = -gm.params[3];
					gm.params[5] = gm.params[2];
				}
			}
			if (gm.type >= warp_model::translation) {
				do_read_global_param(0);
				do_read_global_param(1);
			}
		}
		return result;
	}

	i32 reader::read_global_param(warp_model type, u32 idx, i32 prev_gm_params, bool allow_high_precision_mv) {
		u32 abs_bits  = constants::gm_abs_alpha_bits;  // 12
		u32 prec_bits = constants::gm_alpha_prec_bits; // 15
		if (idx < 2) {
			if (type == warp_model::translation) {
				abs_bits  = constants::gm_abs_trans_only_bits  - (allow_high_precision_mv ? 0 : 1); // 9 - 1?
				prec_bits = constants::gm_trans_only_prec_bits - (allow_high_precision_mv ? 0 : 1); // 3 - 1?
			} else {
				abs_bits  = constants::gm_abs_trans_bits;  // 12
				prec_bits = constants::gm_trans_prec_bits; // 6
			}
		}
		const u32 prec_diff = constants::warpedmodel_prec_bits - prec_bits; // 16 - ?

		const i32 round = (idx % 3 == 2) ? (1 << constants::warpedmodel_prec_bits) : 0;
		const i32 sub = (idx % 3 == 2) ? (1 << prec_bits) : 0;
		const i32 mx = 1 << abs_bits;
		const i32 r = (prev_gm_params >> prec_diff) - sub;
		return (decode_signed_subexp_with_ref(-mx, mx + 1, r) << prec_diff) + round;
	}

	i32 reader::decode_signed_subexp_with_ref(i32 low, i32 high, i32 r) {
		const u32 x = decode_unsigned_subexp_with_ref(static_cast<u32>(high - low), r - low);
		return static_cast<i32>(x) + low;
	}

	u32 reader::decode_unsigned_subexp_with_ref(u32 mx, i32 r) {
		const u32 v = decode_subexp(mx);
		// TODO better impl
		if ((r << 1) <= static_cast<i32>(mx)) {
			return static_cast<u32>(functions::inverse_recenter(r, v));
		} else {
			return static_cast<u32>(
				static_cast<i32>(mx) - 1 - functions::inverse_recenter(static_cast<i32>(mx) - 1 - r, v)
			);
		}
	}

	u32 reader::decode_subexp(u32 num_syms) {
		u32 i = 0;
		u32 mk = 0;
		const u32 k = 3;
		while (true) {
			const u32 b2 = i ? k + i - 1 : k;
			const u32 a = 1u << b2;
			if (num_syms <= mk + 3 * a) {
				const u32 subexp_final_bits = read_ns(num_syms - mk);
				return subexp_final_bits + mk;
			} else {
				if (const bool subexp_more_bits = read_bit(); subexp_more_bits) {
					++i;
					mk += a;
				} else {
					const u32 subexp_bits = read_bits(b2);
					return subexp_bits + mk;
				}
			}
		}
	}

	obu::film_grain_params reader::read_film_grain_params(
		const obu::sequence_header &seq_header, const obu::uncompressed_header &header
	) {
		obu::film_grain_params result = zero;
		if (!seq_header.film_grain_params_present || (!header.show_frame && !header.showable_frame)) {
			// TODO reset_grain_params
			return result;
		}
		result.apply_grain = read_bit();
		if (!result.apply_grain) {
			// TODO reset_grain_params
			return result;
		}
		result.grain_seed = read_bits<16>();
		if (header.frame_type == frame_type::inter_frame) {
			result.update_grain = read_bit();
		} else {
			result.update_grain = true;
		}
		if (!result.update_grain) {
			const u8 film_grain_params_ref_idx = read_bits<3>();
			const u16 temp_grain_seed = result.grain_seed;
			// TODO load_grain_params
			result.grain_seed = temp_grain_seed;
			return result;
		}
		result.num_y_points = read_bits<4>();
		for (u8 i = 0; i < result.num_y_points; ++i) {
			result.point_y[i].value = read_bits<8>();
			result.point_y[i].scaling = read_bits<8>();
		}
		const bool mono_chrome = seq_header.color_config.mono_chrome;
		const bool subsampling_x = seq_header.color_config.subsampling_x;
		const bool subsampling_y = seq_header.color_config.subsampling_y;
		if (!mono_chrome) {
			result.chroma_scaling_from_luma = read_bit();
		}
		if (!(
			mono_chrome ||
			result.chroma_scaling_from_luma ||
			(subsampling_x == 1 && subsampling_y == 1 && result.num_y_points == 0)
		)) {
			result.num_cb_points = read_bits<4>();
			for (u8 i = 0; i < result.num_cb_points; ++i) {
				result.point_cb[i].value = read_bits<8>();
				result.point_cb[i].scaling = read_bits<8>();
			}
			result.num_cr_points = read_bits<4>();
			for (u8 i = 0; i < result.num_cr_points; ++i) {
				result.point_cr[i].value = read_bits<8>();
				result.point_cr[i].scaling = read_bits<8>();
			}
		}
		{
			const u8 b4 = read_bits<4>();
			result.grain_scaling = (b4 >> 2) + 8;
			result.ar_coeff_lag  = b4 & 0x3;
		}
		const u8 num_pos_luma = 2 * result.ar_coeff_lag * (result.ar_coeff_lag + 1); // max 24
		u8 num_pos_chroma; // max 25
		if (result.num_y_points > 0) {
			num_pos_chroma = num_pos_luma + 1;
			for (u32 i = 0; i < num_pos_luma; ++i) {
				result.ar_coeffs_y_plus_128[i] = read_bits<8>();
			}
		} else {
			num_pos_chroma = num_pos_luma;
		}
		if (result.chroma_scaling_from_luma || result.num_cb_points > 0) {
			for (u8 i = 0; i < num_pos_chroma; ++i) {
				result.ar_coeffs_cb_plus_128[i] = read_bits<8>();
			}
		}
		if (result.chroma_scaling_from_luma || result.num_cr_points > 0) {
			for (u8 i = 0; i < num_pos_chroma; ++i) {
				result.ar_coeffs_cr_plus_128[i] = read_bits<8>();
			}
		}
		{
			const u8 b4 = read_bits<4>();
			result.ar_coeff_shift    = (b4 >> 2) + 6;
			result.grain_scale_shift = b4 & 0x3;
		}
		if (result.num_cb_points > 0) {
			result.cb.mult = read_bits<8>();
			result.cb.luma_mult = read_bits<8>();
			result.cb.offset = read_bits<9>();
		}
		if (result.num_cr_points > 0) {
			result.cr.mult = read_bits<8>();
			result.cr.luma_mult = read_bits<8>();
			result.cr.offset = read_bits<9>();
		}
		result.overlap = read_bit();
		result.clip_to_restricted_range = read_bit();
		return result;
	}

	void reader::read_tile_group(
		const obu::sequence_header &seq_header,
		const obu::uncompressed_header &header,
		const cdf::non_coeff &non_coeff_cdfs,
		const cdf::coeff &coeff_cdfs,
		state::block &sb,
		u64 sz
	) {
		const u32 num_tiles = header.tile_info.tile_cols * header.tile_info.tile_rows;
		const u64 start_bit_pos = get_position();
		const bool tile_start_and_end_present = num_tiles > 1 ? read_bit() : false;
		u32 tg_start = 0;
		u32 tg_end = 0;
		if (num_tiles == 1 || !tile_start_and_end_present) {
			tg_end = num_tiles - 1;
		} else {
			const u32 tile_bits = header.tile_info.tile_cols_log2 + header.tile_info.tile_rows_log2;
			tg_start = read_bits(tile_bits);
			tg_end = read_bits(tile_bits);
		}
		read_byte_alignment();
		const u64 end_bit_pos = get_position();
		const u64 header_bytes = (end_bit_pos - start_bit_pos) / 8;
		sz -= header_bytes;

		for (u32 tile_num = tg_start; tile_num <= tg_end; ++tile_num) {
			const u32 tile_row = tile_num / header.tile_info.tile_cols;
			const u32 tile_col = tile_num % header.tile_info.tile_cols;
			u64 tile_size;
			if (tile_num == tg_end) {
				tile_size = sz;
			} else {
				tile_size = read_le(header.tile_info.tile_size_bytes) + 1;
				sz -= tile_size + header.tile_info.tile_size_bytes;
			}
			state::block_range sbr = zero;
			sbr.row_start = sb.row_starts[tile_row];
			sbr.row_end   = sb.row_starts[tile_row + 1];
			sbr.col_start = sb.col_starts[tile_col];
			sbr.col_end   = sb.col_starts[tile_col + 1];
			sb.current_q_index = header.quantization_params.base_q_idx;
			auto decoder = symbol_decoder::init_symbol(
				*this, non_coeff_cdfs, coeff_cdfs, header.disable_cdf_update, tile_size
			);
			decode_tile(seq_header, header, decoder, sb, sbr);
			decoder.exit_symbol();
		}
		if (tg_end == num_tiles - 1) {
			if (!header.disable_frame_end_update_cdf) {
				// TODO
			}
			// TODO decode_frame_wrapup()
			sb.seen_frame_header = false;
		}
	}

	void reader::decode_tile(
		const obu::sequence_header &seq_header,
		const obu::uncompressed_header &header,
		symbol_decoder &decoder,
		state::block &sb,
		const state::block_range &sbr
	) {
		functions::clear_above_context(sb);
		for (u32 i = 0; i < constants::frame_lf_count; ++i) {
			sb.delta_lf[i] = 0;
		}
		state::ref_lr slr = zero;
		for (u32 plane = 0; plane < seq_header.color_config.get_num_planes(); ++plane) {
			for (u32 pass = 0; pass < 2; ++pass) {
				slr.ref_sgr_xqd[plane][pass] = constants::sgrproj_xqd_mid[pass];
				for (u32 i = 0; i < constants::wiener_coeffs; ++i) {
					slr.ref_lr_wiener[plane][pass][i] = constants::wiener_taps_mid[i];
				}
			}
		}
		const block_size sb_size = seq_header.get_block_size();
		const u32 sb_size4 = constants::num_4x4_blocks_wide[std::to_underlying(sb_size)];
		for (u32 r = sbr.row_start; r < sbr.row_end; r += sb_size4) {
			functions::clear_left_context(sb);
			for (u32 c = sbr.col_start; c < sbr.col_end; c += sb_size4) {
				sb.read_deltas = header.delta_q_params.delta_q_present;
				functions::clear_cdef(seq_header, sb, r, c);
				functions::clear_block_decoded_flags(seq_header, sb, sbr, r, c, sb_size4);
				read_lr(seq_header, header, decoder, slr, r, c, sb_size);
				decode_partition(seq_header, header, decoder, sb, sbr, r, c, sb_size);
			}
		}
	}

	void reader::decode_partition(
		const obu::sequence_header &seq_header,
		const obu::uncompressed_header &header,
		symbol_decoder &decoder,
		state::block &sb,
		const state::block_range &mi,
		u32 r,
		u32 c,
		block_size b_size
	) {
		const u32 mi_rows = header.frame_size.get_mi_rows();
		const u32 mi_cols = header.frame_size.get_mi_cols();
		if (r >= mi_rows || c >= mi_cols) {
			return; // TODO 0?
		}

		const bool avail_u = mi.is_inside(r - 1, c);
		const bool avail_l = mi.is_inside(r, c - 1);
		const u32 num4x4 = constants::num_4x4_blocks_wide[std::to_underlying(b_size)];
		const u32 half_block4x4 = num4x4 >> 1;
		const u32 quarter_block4x4 = half_block4x4 >> 1;
		const bool has_rows = (r + half_block4x4) < mi_rows;
		const bool has_cols = (c + half_block4x4) < mi_cols;

		const auto part_state = cdf::context::partition::compute(sb, b_size, avail_u, avail_l, r, c);
		partition partition;
		if (b_size < block_size::b8x8) {
			partition = partition::none;
		} else if (has_rows && has_cols) {
			partition = decoder.read_partition(part_state);
		} else if (has_cols) {
			const bool split_or_horz = decoder.read_split_or_horz(part_state);
			partition = split_or_horz ? partition::split : partition::horz;
		} else if (has_rows) {
			const auto split_or_vert = decoder.read_split_or_vert(part_state);
			partition = split_or_vert ? partition::split : partition::vert;
		} else {
			partition = partition::split;
		}
		const block_size sub_size =
			constants::partition_subsize[std::to_underlying(partition)][std::to_underlying(b_size)];
		const block_size split_size =
			constants::partition_subsize[std::to_underlying(partition::split)][std::to_underlying(b_size)];
		if (partition == partition::none) {
			decode_block(seq_header, header, decoder, sb, mi, r, c, sub_size);
		} else if (partition == partition::horz) {
			decode_block(seq_header, header, decoder, sb, mi, r, c, sub_size);
			if (has_rows) {
				decode_block(seq_header, header, decoder, sb, mi, r + half_block4x4, c, sub_size);
			}
		} else if (partition == partition::vert) {
			decode_block(seq_header, header, decoder, sb, mi, r, c, sub_size);
			if (has_cols) {
				decode_block(seq_header, header, decoder, sb, mi, r, c + half_block4x4, sub_size);
			}
		} else if (partition == partition::split) {
			decode_partition(seq_header, header, decoder, sb, mi, r, c, sub_size);
			decode_partition(seq_header, header, decoder, sb, mi, r, c + half_block4x4, sub_size);
			decode_partition(seq_header, header, decoder, sb, mi, r + half_block4x4, c, sub_size);
			decode_partition(seq_header, header, decoder, sb, mi, r + half_block4x4, c + half_block4x4, sub_size);
		} else if (partition == partition::horz_a) {
			decode_block(seq_header, header, decoder, sb, mi, r, c, split_size);
			decode_block(seq_header, header, decoder, sb, mi, r, c + half_block4x4, split_size);
			decode_block(seq_header, header, decoder, sb, mi, r + half_block4x4, c, sub_size);
		} else if (partition == partition::horz_b) {
			decode_block(seq_header, header, decoder, sb, mi, r, c, sub_size);
			decode_block(seq_header, header, decoder, sb, mi, r + half_block4x4, c, split_size);
			decode_block(seq_header, header, decoder, sb, mi, r + half_block4x4, c + half_block4x4, split_size);
		} else if (partition == partition::vert_a) {
			decode_block(seq_header, header, decoder, sb, mi, r, c, split_size);
			decode_block(seq_header, header, decoder, sb, mi, r + half_block4x4, c, split_size);
			decode_block(seq_header, header, decoder, sb, mi, r, c + half_block4x4, sub_size);
		} else if (partition == partition::vert_b) {
			decode_block(seq_header, header, decoder, sb, mi, r, c, sub_size);
			decode_block(seq_header, header, decoder, sb, mi, r, c + half_block4x4, split_size);
			decode_block(seq_header, header, decoder, sb, mi, r + half_block4x4, c + half_block4x4, split_size);
		} else if (partition == partition::horz_4) {
			decode_block(seq_header, header, decoder, sb, mi, r + quarter_block4x4 * 0, c, sub_size);
			decode_block(seq_header, header, decoder, sb, mi, r + quarter_block4x4 * 1, c, sub_size);
			decode_block(seq_header, header, decoder, sb, mi, r + quarter_block4x4 * 2, c, sub_size);
			if (r + quarter_block4x4 * 3 < mi_rows) {
				decode_block(seq_header, header, decoder, sb, mi, r + quarter_block4x4 * 3, c, sub_size);
			}
		} else {
			decode_block(seq_header, header, decoder, sb, mi, r, c + quarter_block4x4 * 0, sub_size);
			decode_block(seq_header, header, decoder, sb, mi, r, c + quarter_block4x4 * 1, sub_size);
			decode_block(seq_header, header, decoder, sb, mi, r, c + quarter_block4x4 * 2, sub_size);
			if (c + quarter_block4x4 * 3 < mi_cols) {
				decode_block(seq_header, header, decoder, sb, mi, r, c + quarter_block4x4 * 3, sub_size);
			}
		}
	}

	void reader::decode_block(
		const obu::sequence_header &seq_header,
		const obu::uncompressed_header &header,
		symbol_decoder &decoder,
		state::block &sb,
		const state::block_range &mi,
		u32 r,
		u32 c,
		block_size sub_size
	) {
		state::block_decoding sbd = zero;
		sbd.row  = r;
		sbd.col  = c;
		sbd.size = sub_size;
		const u32 bw4 = constants::num_4x4_blocks_wide[std::to_underlying(sub_size)];
		const u32 bh4 = constants::num_4x4_blocks_high[std::to_underlying(sub_size)];
		if (bh4 == 1 && seq_header.color_config.subsampling_y && (r & 1) == 0) {
			sbd.has_chroma = false;
		} else if (bw4 == 1 && seq_header.color_config.subsampling_x && (c & 1) == 0) {
			sbd.has_chroma = false;
		} else {
			sbd.has_chroma = seq_header.color_config.get_num_planes() > 1;
		}
		sbd.avail_u = mi.is_inside(r - 1, c);
		sbd.avail_l = mi.is_inside(r, c - 1);
		sbd.avail_u_chroma = sbd.avail_u;
		sbd.avail_l_chroma = sbd.avail_l;
		if (sbd.has_chroma) {
			if (seq_header.color_config.subsampling_y && bh4 == 1) {
				sbd.avail_u_chroma = mi.is_inside(r - 2, c);
			}
			if (seq_header.color_config.subsampling_x && bw4 == 1) {
				sbd.avail_l_chroma = mi.is_inside(r, c - 2);
			}
		} else {
			sbd.avail_u_chroma = false;
			sbd.avail_l_chroma = false;
		}
		const obu::mode_info mode_info = read_mode_info(seq_header, header, decoder, sb, mi, sbd);
		const state::color_map scm = read_palette_tokens(seq_header, header, mode_info, decoder, sbd);
		read_block_tx_size(header, mode_info, decoder, sb, sbd);

		if (mode_info.skip) {
			functions::reset_block_context(seq_header, sb, sbd, bw4, bh4);
		}
		const bool is_compound = mode_info.ref_frame[1] > reference_frame::intra;
		for (u32 y = 0; y < bh4; ++y) {
			for (u32 x = 0; x < bw4; ++x) {
				sb.y_modes(r + y, c + x) = mode_info.y_mode;
				if (mode_info.ref_frame[0] == reference_frame::intra && sbd.has_chroma) {
					sb.uv_modes(r + y, c + x) = mode_info.uv_mode;
				}
				for (u32 ref_list = 0; ref_list < 2; ++ref_list) {
					sb.ref_frames[ref_list](r + y, c + x) = mode_info.ref_frame[ref_list];
				}
				if (mode_info.is_inter) {
					if (!mode_info.use_intrabc) {
						std::abort(); // TODO
					}
					for (u32 dir = 0; dir < 2; ++dir) {
						std::abort(); // TODO
					}
					for (u32 ref_list = 0; ref_list < 1 + (is_compound ? 1 : 0); ++ref_list) {
						std::abort(); // TODO
					}
				}
			}
		}
		block_decoding::compute_prediction(seq_header, header, mode_info, sb, sbd);
		read_residual(seq_header, header, mode_info, decoder, sb, sbd, scm);
		for (u32 y = 0; y < bh4; ++y) {
			for (u32 x = 0; x < bw4; ++x) {
				sb.is_inters       (r + y, c + x) = mode_info.is_inter;
				sb.skip_modes      (r + y, c + x) = mode_info.skip_mode;
				sb.skips           (r + y, c + x) = mode_info.skip;
				// TODO sb.tx_sizes(r + y, c + x) = tx_size;
				sb.sizes           (r + y, c + x) = sbd.size;
				sb.segment_ids     (r + y, c + x) = mode_info.segment_id;
				sb.palette_sizes[0](r + y, c + x) = mode_info.palette_size_y;
				sb.palette_sizes[1](r + y, c + x) = mode_info.palette_size_uv;
				for (u32 i = 0; i < mode_info.palette_size_y; ++i) {
					sb.palette_colors[0](r + y, c + x)[i] = mode_info.palette_colors_y[i];
				}
				for (u32 i = 0; i < mode_info.palette_size_uv; ++i) {
					sb.palette_colors[1](r + y, c + x)[i] = mode_info.palette_colors_u[i];
				}
				for (u32 i = 0; i < constants::frame_lf_count; ++i) {
					// TODO DeltaLF
				}
			}
		}
	}

	obu::mode_info reader::read_mode_info(
		const obu::sequence_header &seq_header,
		const obu::uncompressed_header &header,
		symbol_decoder &decoder,
		state::block &sb,
		const state::block_range &mi,
		const state::block_decoding &sbd
	) {
		if (header.is_intra()) {
			return read_intra_frame_mode_info(seq_header, header, decoder, sb, mi, sbd);
		} else {
			return read_inter_frame_mode_info(seq_header, header, decoder, sb, mi, sbd);
		}
	}

	obu::mode_info reader::read_intra_frame_mode_info(
		const obu::sequence_header &seq_header,
		const obu::uncompressed_header &header,
		symbol_decoder &decoder,
		state::block &sb,
		const state::block_range &mi,
		const state::block_decoding &sbd
	) {
		obu::mode_info result = zero;

		result.skip = false;
		if (header.segmentation_params.seg_id_pre_skip) {
			result.segment_id = read_intra_segment_id(header, result, decoder, sb, sbd);
		}
		result.skip_mode = false;
		result.skip = read_skip(header, result, decoder, sb, sbd);
		if (!header.segmentation_params.seg_id_pre_skip) {
			result.segment_id = read_intra_segment_id(header, result, decoder, sb, sbd);
		}
		read_cdef(seq_header, header, result, decoder, sb, sbd);
		read_delta_qindex(seq_header, header, result, decoder, sb, sbd);
		read_delta_lf(seq_header, header, result, decoder, sb, sbd);
		sb.read_deltas = false;
		result.ref_frame[0] = reference_frame::intra;
		result.ref_frame[1] = reference_frame::none;
		if (header.allow_intrabc) {
			result.use_intrabc = decoder.read_use_intrabc();
		} else {
			result.use_intrabc = false;
		}
		if (result.use_intrabc) {
			result.is_inter         = true;
			result.y_mode           = prediction_mode::dc;
			result.uv_mode          = prediction_mode::dc;
			result.motion_mode      = motion_compensation::simple;
			result.compound_type    = compound_method::average;
			result.palette_size_y   = 0;
			result.palette_size_uv  = 0;
			result.interp_filter[0] = interpolation::bilinear;
			result.interp_filter[1] = interpolation::bilinear;
			state::motion_vector mv = zero; // TODO find_mv_stack
			assign_mv(seq_header, header, result, decoder, mi, sbd, mv, false);
		} else {
			result.is_inter      = false;
			result.y_mode        = decoder.read_intra_frame_y_mode(sb, sbd);
			result.angle_delta_y = read_intra_angle_info_y(result, decoder, sbd);
			if (sbd.has_chroma) {
				result.uv_mode = decoder.read_uv_mode(seq_header, result, sbd.size);
				if (result.uv_mode == prediction_mode::uv_cfl) {
					result.cfl_alphas = read_cfl_alphas(decoder);
				}
				result.angle_delta_uv = read_intra_angle_info_uv(result, decoder, sbd);
			}
			result.palette_size_y  = 0;
			result.palette_size_uv = 0;
			if (
				sbd.size >= block_size::b8x8 &&
				constants::block_width(sbd.size) <= 64 &&
				constants::block_height(sbd.size) <= 64 &&
				header.allow_screen_content_tools
			) {
				read_palette_mode_info(seq_header, result, decoder, sb, sbd);
			}
			read_filter_intra_mode_info(seq_header, result, decoder, sbd);
		}

		return result;
	}

	segment_id_t reader::read_intra_segment_id(
		const obu::uncompressed_header &header,
		obu::mode_info &mode_info,
		symbol_decoder &decoder,
		const state::block &sb,
		const state::block_decoding &sbd
	) {
		segment_id_t segment_id;

		if (header.segmentation_params.segmentation_enabled) {
			segment_id = read_segment_id(header, decoder, sb, sbd, mode_info.skip);
		} else {
			segment_id = zero;
		}
		mode_info.lossless = header.lossless_bits & (1u << std::to_underlying(segment_id));

		return segment_id;
	}

	segment_id_t reader::read_segment_id(
		const obu::uncompressed_header &header,
		symbol_decoder &decoder,
		const state::block &sb,
		const state::block_decoding &sbd,
		bool skip
	) {
		std::optional<segment_id_t> prev_ul;
		if (sbd.avail_u && sbd.avail_l) {
			prev_ul = sb.segment_ids(sbd.row - 1, sbd.col - 1);
		}
		std::optional<segment_id_t> prev_u;
		if (sbd.avail_u) {
			prev_u = sb.segment_ids(sbd.row - 1, sbd.col);
		}
		std::optional<segment_id_t> prev_l;
		if (sbd.avail_l) {
			prev_l = sb.segment_ids(sbd.row, sbd.col - 1);
		}
		segment_id_t pred;
		if (!prev_u) {
			pred = prev_l.value_or(static_cast<segment_id_t>(0));
		} else if (!prev_l) {
			pred = prev_u.value();
		} else {
			pred = (prev_ul == prev_u ? prev_u : prev_l).value();
		}
		if (skip) {
			return pred;
		}
		const u32 ctx = cdf::context::compute_segment_id(prev_ul, prev_u, prev_l);
		const segment_id_t segment_id = decoder.read_segment_id(ctx);
		const u8 max = std::to_underlying(header.segmentation_params.last_active_seg_id) + 1;
		return static_cast<segment_id_t>(functions::neg_deinterleave(
			std::to_underlying(segment_id), std::to_underlying(pred), max
		));
	}

	bool reader::read_skip_mode(
		const obu::uncompressed_header &header,
		const obu::mode_info &mode_info,
		symbol_decoder &decoder,
		const state::block &sb,
		const state::block_decoding &sbd
	) {
		if (
			mode_info.is_segment_feature_active(features::skip,      header.segmentation_params) ||
			mode_info.is_segment_feature_active(features::ref_frame, header.segmentation_params) ||
			mode_info.is_segment_feature_active(features::globalmv,  header.segmentation_params) ||
			!header.skip_mode_present ||
			constants::block_width(sbd.size) < 8 ||
			constants::block_height(sbd.size) < 8
		) {
			return false;
		} else {
			return decoder.read_skip_mode(cdf::context::compute_skip_mode(sb, sbd));
		}
	}

	bool reader::read_skip(
		const obu::uncompressed_header &header,
		const obu::mode_info &mode_info,
		symbol_decoder &decoder,
		const state::block &sb,
		const state::block_decoding &sbd
	) {
		// TODO what happens if segment_id has not been read?
		if (
			header.segmentation_params.seg_id_pre_skip &&
			header.segmentation_params.is_feature_active(mode_info.segment_id, features::skip)
		) {
			return true;
		} else {
			return decoder.read_skip(sb, sbd);
		}
	}

	void reader::read_delta_qindex(
		const obu::sequence_header &seq_header,
		const obu::uncompressed_header &header,
		const obu::mode_info &mode_info,
		symbol_decoder &decoder,
		state::block &sb,
		const state::block_decoding &sbd
	) {
		const block_size sb_size = seq_header.get_block_size();
		if (sbd.size == sb_size && mode_info.skip) {
			return;
		}
		if (sb.read_deltas) {
			u32 delta_q_abs = decoder.read_delta_q_abs();
			if (delta_q_abs == constants::delta_q_small) {
				const u32 delta_q_rem_bits = decoder.read_literal(3) + 1;
				const u32 delta_q_abs_bits = decoder.read_literal(delta_q_rem_bits);
				delta_q_abs = delta_q_abs_bits + (1u << delta_q_rem_bits) + 1u;
			}
			if (delta_q_abs > 0) {
				const bool delta_q_sign_bit = decoder.read_bool();
				const i32 reduced_delta_q_index =
					delta_q_sign_bit ? -static_cast<i32>(delta_q_abs) : static_cast<i32>(delta_q_abs);
				sb.current_q_index = static_cast<u8>(std::clamp(
					sb.current_q_index + (reduced_delta_q_index << header.delta_q_params.delta_q_res), 1, 255
				));
			}
		}
	}

	void reader::read_delta_lf(
		const obu::sequence_header &seq_header,
		const obu::uncompressed_header &header,
		const obu::mode_info &result,
		symbol_decoder &decoder,
		state::block &sb,
		const state::block_decoding &sbd
	) {
		const block_size sb_size = seq_header.get_block_size();
		if (sbd.size == sb_size && result.skip) {
			return;
		}
		if (sb.read_deltas && header.delta_lf_params.delta_lf_present) {
			u32 frame_lf_count = 1;
			if (header.delta_lf_params.delta_lf_multi) {
				frame_lf_count =
					seq_header.color_config.get_num_planes() > 1 ?
					constants::frame_lf_count :
					(constants::frame_lf_count - 2);
			}
			for (u32 i = 0; i < frame_lf_count; ++i) {
				u32 delta_lf_abs = decoder.read_delta_lf_abs(header.delta_lf_params.delta_lf_multi, i);
				if (delta_lf_abs == constants::delta_lf_small) {
					const u32 n = decoder.read_literal(3) + 1;
					const u32 delta_lf_abs_bits = decoder.read_literal(n);
					delta_lf_abs = delta_lf_abs_bits + (1u << n) + 1;
				}
				if (delta_lf_abs > 0) {
					const bool delta_lf_sign_bit = decoder.read_bool();
					const i32 reduced_delta_lf_level =
						delta_lf_sign_bit ?
						-static_cast<i32>(delta_lf_abs) :
						static_cast<i32>(delta_lf_abs);
					sb.delta_lf[i] = static_cast<i8>(std::clamp(
						sb.delta_lf[i] + (reduced_delta_lf_level << header.delta_lf_params.delta_lf_res),
						-static_cast<i32>(constants::max_loop_filter),
						static_cast<i32>(constants::max_loop_filter)
					));
				}
			}
		}
	}

	tx_size reader::read_tx_size(
		const obu::uncompressed_header &header,
		const obu::mode_info &mode_info,
		symbol_decoder &decoder,
		const state::block &sb,
		const state::block_decoding &sbd,
		bool allow_select
	) {
		if (mode_info.lossless) {
			return tx_size::size_4x4;
		}
		const tx_size max_rect_tx_size = constants::max_tx_size_rect[std::to_underlying(sbd.size)];
		const u32 max_tx_depth = constants::max_tx_depth_table[std::to_underlying(sbd.size)];
		tx_size txsize = max_rect_tx_size;
		if (sbd.size > block_size::b4x4 && allow_select && header.tx_mode == tx_mode::select) {
			const u32 ctx = cdf::context::compute_tx_depth(sb, sbd, max_rect_tx_size);
			const u8 tx_depth = decoder.read_tx_depth(max_tx_depth, ctx);
			for (u32 i = 0; i < tx_depth; ++i) {
				txsize = constants::split_tx_size[std::to_underlying(txsize)];
			}
		}
		return txsize;
	}

	void reader::read_block_tx_size(
		const obu::uncompressed_header &header,
		const obu::mode_info &mode_info,
		symbol_decoder &decoder,
		state::block &sb,
		const state::block_decoding &sbd
	) {
		const u32 bw4 = constants::num_4x4_blocks_wide[std::to_underlying(sbd.size)];
		const u32 bh4 = constants::num_4x4_blocks_high[std::to_underlying(sbd.size)];
		if (
			header.tx_mode == tx_mode::select &&
			sbd.size > block_size::b4x4 &&
			mode_info.is_inter &&
			!mode_info.skip &&
			!mode_info.lossless
		) {
			const tx_size max_tx_sz = constants::max_tx_size_rect[std::to_underlying(sbd.size)];
			const u32 tx_w4 = constants::get_tx_width(max_tx_sz) / constants::mi_size;
			const u32 tx_h4 = constants::get_tx_height(max_tx_sz) / constants::mi_size;
			for (u32 row = sbd.row; row < sbd.row + bh4; row += tx_h4) {
				for (u32 col = sbd.col; col < sbd.col + bw4; col += tx_w4) {
					read_var_tx_size(header, decoder, sb, sbd, row, col, max_tx_sz, 0);
				}
			}
		} else {
			sb.tx_size = read_tx_size(header, mode_info, decoder, sb, sbd, !mode_info.skip || !mode_info.is_inter);
			for (u32 row = sbd.row; row < sbd.row + bh4; ++row) {
				for (u32 col = sbd.col; col < sbd.col + bw4; ++col) {
					sb.inter_tx_sizes(row, col) = sb.tx_size;
				}
			}
		}
	}

	void reader::read_var_tx_size(
		const obu::uncompressed_header &header,
		symbol_decoder &decoder,
		state::block &sb,
		const state::block_decoding &sbd,
		u32 row, u32 col, tx_size tx_sz, u32 depth
	) {
		if (row >= header.frame_size.get_mi_rows() || col >= header.frame_size.get_mi_cols()) {
			return;
		}
		bool txfm_split;
		if (tx_sz == tx_size::size_4x4 || depth == constants::max_vartx_depth) {
			txfm_split = false;
		} else {
			txfm_split = decoder.read_txfm_split(cdf::context::compute_txfm_split(sb, sbd, row, col, tx_sz));
		}
		const u32 w4 = constants::get_tx_width(tx_sz) / constants::mi_size;
		const u32 h4 = constants::get_tx_height(tx_sz) / constants::mi_size;
		if (txfm_split) {
			const tx_size sub_tx_sz = constants::split_tx_size[std::to_underlying(tx_sz)];
			const u32 step_w = constants::get_tx_height(sub_tx_sz) / constants::mi_size;
			const u32 step_h = constants::get_tx_width(sub_tx_sz) / constants::mi_size;
			for (u32 i = 0; i < h4; i += step_h) {
				for (u32 j = 0; j < w4; j += step_w) {
					read_var_tx_size(header, decoder, sb, sbd, row + i, col + j, sub_tx_sz, depth + 1);
				}
			}
		} else {
			for (u32 i = 0; i < h4; ++i) {
				for (u32 j = 0; j < w4; ++j) {
					sb.inter_tx_sizes(row + i, col + j) = tx_sz;
				}
			}
			sb.tx_size = tx_sz;
		}
	}

	obu::mode_info reader::read_inter_frame_mode_info(
		const obu::sequence_header &seq_header,
		const obu::uncompressed_header &header,
		symbol_decoder &decoder,
		state::block &sb,
		const state::block_range &mi,
		const state::block_decoding &sbd
	) {
		obu::mode_info result = zero;

		result.use_intrabc = false;
		result.left_ref_frame[0]  = sbd.avail_l ? sb.ref_frames[0](sbd.row, sbd.col - 1) : reference_frame::intra;
		result.above_ref_frame[0] = sbd.avail_u ? sb.ref_frames[0](sbd.row - 1, sbd.col) : reference_frame::intra;
		result.left_ref_frame[1]  = sbd.avail_l ? sb.ref_frames[1](sbd.row, sbd.col - 1) : reference_frame::none;
		result.above_ref_frame[1] = sbd.avail_u ? sb.ref_frames[1](sbd.row - 1, sbd.col) : reference_frame::none;
		result.left_intra   = result.left_ref_frame[0]  <= reference_frame::intra;
		result.above_intra  = result.above_ref_frame[0] <= reference_frame::intra;
		result.left_single  = result.left_ref_frame[1]  <= reference_frame::intra;
		result.above_single = result.above_ref_frame[1] <= reference_frame::intra;
		result.skip = false;
		result.segment_id = read_inter_segment_id(header, result, decoder, sb, sbd, true);
		result.skip_mode = read_skip_mode(header, result, decoder, sb, sbd);
		if (result.skip_mode) {
			result.skip = true;
		} else {
			result.skip = read_skip(header, result, decoder, sb, sbd);
		}
		if (header.segmentation_params.seg_id_pre_skip) {
			result.segment_id = read_inter_segment_id(header, result, decoder, sb, sbd, false);
		}
		result.lossless = header.lossless_bits & (1u << std::to_underlying(result.segment_id));
		read_cdef(seq_header, header, result, decoder, sb, sbd);
		read_delta_qindex(seq_header, header, result, decoder, sb, sbd);
		read_delta_lf(seq_header, header, result, decoder, sb, sbd);
		sb.read_deltas = false;
		result.is_inter = read_is_inter(header, result, decoder, sbd);
		if (result.is_inter) {
			read_inter_block_mode_info(seq_header, header, result, decoder, sb, mi, sbd);
		} else {
			read_intra_block_mode_info(seq_header, header, result, decoder, sb, sbd);
		}

		return result;
	}

	segment_id_t reader::read_inter_segment_id(
		const obu::uncompressed_header &header,
		const obu::mode_info &mode_info,
		symbol_decoder &decoder,
		state::block &sb,
		const state::block_decoding &sbd,
		bool pre_skip
	) {
		if (header.segmentation_params.segmentation_enabled) {
			const segment_id_t predicted_segment_id = functions::get_segment_id(
				sb, sbd, header.frame_size.get_mi_rows(), header.frame_size.get_mi_cols()
			);
			if (header.segmentation_params.segmentation_update_map) {
				if (pre_skip && !header.segmentation_params.seg_id_pre_skip) {
					return zero;
				}
				if (!pre_skip) {
					if (mode_info.skip) {
						const bool seg_id_predicted = false;
						for (u32 i = 0; i < constants::num_4x4_blocks_wide[std::to_underlying(sbd.size)]; ++i) {
							sb.above_seg_pred_context[sbd.col + i] = seg_id_predicted;
						}
						for (u32 i = 0; i < constants::num_4x4_blocks_high[std::to_underlying(sbd.size)]; ++i) {
							sb.left_seg_pred_context[sbd.row + i] = seg_id_predicted;
						}
						return read_segment_id(header, decoder, sb, sbd, mode_info.skip);
					}
				}
				if (header.segmentation_params.segmentation_temporal_update) {
					const bool seg_id_predicted = decoder.read_seg_id_predicted(sb, sbd);
					segment_id_t segment_id;
					if (seg_id_predicted) {
						segment_id = predicted_segment_id;
					} else {
						segment_id = read_segment_id(header, decoder, sb, sbd, mode_info.skip);
					}
					for (u32 i = 0; i < constants::num_4x4_blocks_wide[std::to_underlying(sbd.size)]; ++i) {
						sb.above_seg_pred_context[sbd.col + i] = seg_id_predicted;
					}
					for (u32 i = 0; i < constants::num_4x4_blocks_high[std::to_underlying(sbd.size)]; ++i) {
						sb.left_seg_pred_context[sbd.row + i] = seg_id_predicted;
					}
					return segment_id;
				} else {
					return read_segment_id(header, decoder, sb, sbd, mode_info.skip);
				}
			} else {
				return predicted_segment_id;
			}
		} else {
			return zero;
		}
	}

	bool reader::read_is_inter(
		const obu::uncompressed_header &header,
		const obu::mode_info &mode_info,
		symbol_decoder &decoder,
		const state::block_decoding &sbd
	) {
		if (mode_info.skip_mode) {
			return true;
		} else if (mode_info.is_segment_feature_active(features::ref_frame, header.segmentation_params)) {
			const auto segment_id = std::to_underlying(mode_info.segment_id);
			const i16 feature_data =
				header.segmentation_params.feature_data[segment_id][std::to_underlying(features::ref_frame)];
			return feature_data != std::to_underlying(reference_frame::intra);
		} else if (mode_info.is_segment_feature_active(features::globalmv, header.segmentation_params)) {
			return true;
		} else {
			return decoder.read_is_inter(cdf::context::compute_is_inter(mode_info, sbd));
		}
	}

	void reader::read_intra_block_mode_info(
		const obu::sequence_header &seq_header,
		const obu::uncompressed_header &header,
		obu::mode_info &mode_info,
		symbol_decoder &decoder,
		const state::block &sb,
		const state::block_decoding &sbd
	) {
		mode_info.ref_frame[0] = reference_frame::intra;
		mode_info.ref_frame[1] = reference_frame::none;
		mode_info.y_mode = decoder.read_y_mode(sbd.size);
		mode_info.angle_delta_y = read_intra_angle_info_y(mode_info, decoder, sbd);
		if (sbd.has_chroma) {
			mode_info.uv_mode = decoder.read_uv_mode(seq_header, mode_info, sbd.size);
			if (mode_info.uv_mode == prediction_mode::uv_cfl) {
				mode_info.cfl_alphas = read_cfl_alphas(decoder);
			}
			mode_info.angle_delta_uv = read_intra_angle_info_uv(mode_info, decoder, sbd);
		}
		mode_info.palette_size_y = 0;
		mode_info.palette_size_uv = 0;
		if (
			sbd.size >= block_size::b8x8 &&
			constants::block_width(sbd.size) <= 64 &&
			constants::block_height(sbd.size) <= 64 &&
			header.allow_screen_content_tools
		) {
			read_palette_mode_info(seq_header, mode_info, decoder, sb, sbd);
		}
		read_filter_intra_mode_info(seq_header, mode_info, decoder, sbd);
	}

	void reader::read_inter_block_mode_info(
		const obu::sequence_header &seq_header,
		const obu::uncompressed_header &header,
		obu::mode_info &mode_info,
		symbol_decoder &decoder,
		const state::block &sb,
		const state::block_range &mi,
		const state::block_decoding &sbd
	) {
		mode_info.palette_size_y = 0;
		mode_info.palette_size_uv = 0;
		// TODO read_ref_frames()
		const bool is_compound = mode_info.ref_frame[1] > reference_frame::intra;
		state::motion_vector mv = zero; // TODO find_mv_stack
		if (mode_info.skip_mode) {
			mode_info.y_mode = prediction_mode::nearest_nearestmv;
		} else if (
			mode_info.is_segment_feature_active(features::skip, header.segmentation_params) ||
			mode_info.is_segment_feature_active(features::globalmv, header.segmentation_params)
		) {
			mode_info.y_mode = prediction_mode::globalmv;
		} else if (is_compound) {
			const u8 compound_mode = decoder.read_compound_mode(mv);
			mode_info.y_mode =
				static_cast<prediction_mode>(std::to_underlying(prediction_mode::nearest_nearestmv) + compound_mode);
		} else {
			const bool new_mv = decoder.read_new_mv(mv.new_mv_context);
			if (!new_mv) {
				mode_info.y_mode = prediction_mode::newmv;
			} else {
				const bool zero_mv = decoder.read_zero_mv(mv.zero_mv_context);
				if (!zero_mv) {
					mode_info.y_mode = prediction_mode::globalmv;
				} else {
					const bool ref_mv = decoder.read_ref_mv(mv.ref_mv_context);
					mode_info.y_mode = ref_mv ? prediction_mode::nearmv : prediction_mode::nearestmv;
				}
			}
		}
		mode_info.ref_mv_idx = 0;
		if (mode_info.y_mode == prediction_mode::newmv || mode_info.y_mode == prediction_mode::new_newmv) {
			for (u8 idx = 0; idx < 2; ++idx) {
				if (mv.num_mv_found > idx + 1) {
					const bool drl_mode = decoder.read_drl_mode(mv.drl_ctx_stack[idx]);
					if (!drl_mode) {
						mode_info.ref_mv_idx = idx;
						break;
					}
					mode_info.ref_mv_idx = idx + 1;
				}
			}
		} else if (functions::has_nearmv(mode_info.y_mode)) {
			mode_info.ref_mv_idx = 1;
			for (u8 idx = 1; idx < 3; ++idx) {
				if (mv.num_mv_found > idx + 1) {
					const bool drl_mode = decoder.read_drl_mode(mv.drl_ctx_stack[idx]);
					if (!drl_mode) {
						mode_info.ref_mv_idx = idx;
						break;
					}
					mode_info.ref_mv_idx = idx + 1;
				}
			}
		}
		assign_mv(seq_header, header, mode_info, decoder, mi, sbd, mv, is_compound);
		read_interintra_mode(seq_header, mode_info, decoder, sbd, is_compound);
		// TODO read_motion_mode
		// TODO read_compound_type
		if (header.interpolation_filter == interpolation::switchable) {
			for (u32 dir = 0; dir < (seq_header.enable_dual_filter ? 2 : 1); ++dir) {
				if (functions::needs_interp_filter(header.global_motion_params, mode_info, sbd)) {
					mode_info.interp_filter[dir] = decoder.read_interp_filter(
						cdf::context::compute_interp_filter(mode_info, sb, sbd, dir)
					);
				} else {
					mode_info.interp_filter[dir] = interpolation::eighttap;
				}
			}
			if (!seq_header.enable_dual_filter) {
				mode_info.interp_filter[1] = mode_info.interp_filter[0];
			}
		} else {
			for (u32 dir = 0; dir < 2; ++dir) {
				mode_info.interp_filter[dir] = header.interpolation_filter;
			}
		}
	}

	void reader::read_filter_intra_mode_info(
		const obu::sequence_header &seq_header,
		obu::mode_info &mode_info,
		symbol_decoder &decoder,
		const state::block_decoding &sbd
	) {
		mode_info.use_filter_intra = false;
		if (
			seq_header.enable_filter_intra &&
			mode_info.y_mode == prediction_mode::dc &&
			mode_info.palette_size_y == 0 &&
			std::max(constants::block_width(sbd.size), constants::block_height(sbd.size)) <= 32
		) {
			mode_info.use_filter_intra = decoder.read_use_filter_intra(sbd.size);
			if (mode_info.use_filter_intra) {
				mode_info.filter_intra_mode = decoder.read_filter_intra_mode();
			}
		}
	}

	void reader::assign_mv(
		const obu::sequence_header &seq_header,
		const obu::uncompressed_header &header,
		const obu::mode_info &mode_info,
		symbol_decoder &decoder,
		const state::block_range &mi,
		const state::block_decoding &sbd,
		state::motion_vector &mv,
		bool is_compound
	) {
		for (u32 i = 0; i < (is_compound ? 2 : 1); ++i) {
			prediction_mode comp_mode;
			if (mode_info.use_intrabc) {
				comp_mode = prediction_mode::newmv;
			} else {
				comp_mode = functions::get_mode(mode_info, i);
			}
			if (mode_info.use_intrabc) {
				mv.pred_mv[0] = mv.ref_stack_mv[0][0];
				if (mv.pred_mv[0] == zero) {
					mv.pred_mv[0] = mv.ref_stack_mv[1][0];
				}
				if (mv.pred_mv[0] == zero) {
					const block_size sb_size =
						seq_header.use_128x128_superblock ? block_size::b128x128 : block_size::b64x64;
					const u32 sb_size4 = constants::num_4x4_blocks_high[std::to_underlying(sb_size)];
					if (sbd.row - sb_size4 < mi.row_start) {
						mv.pred_mv[0] = cvec2i32(
							0, -static_cast<i32>(sb_size4 * constants::mi_size + constants::intrabc_delay_pixels) * 8
						);
					} else {
						mv.pred_mv[0] = cvec2i32(-static_cast<i32>(sb_size4 * constants::mi_size) * 8, 0);
					}
				}
			} else if (comp_mode == prediction_mode::globalmv) {
				mv.pred_mv[i] = mv.global_mvs[i];
			} else {
				u32 pos = comp_mode == prediction_mode::nearestmv ? 0 : mode_info.ref_mv_idx;
				if (comp_mode == prediction_mode::newmv && mv.num_mv_found <= 1) {
					pos = 0;
				}
				mv.pred_mv[i] = mv.ref_stack_mv[pos][i];
			}
			if (comp_mode == prediction_mode::newmv) {
				read_mv(header, mode_info, decoder, mv, i);
			} else {
				mv.mv[i] = mv.pred_mv[i];
			}
		}
	}

	void reader::read_interintra_mode(
		const obu::sequence_header &seq_header,
		obu::mode_info &mode_info,
		symbol_decoder &decoder,
		const state::block_decoding &sbd,
		bool is_compound
	) {
		if (
			!mode_info.skip_mode &&
			seq_header.enable_interintra_compound &&
			!is_compound &&
			sbd.size >= block_size::b8x8 &&
			sbd.size <= block_size::b32x32
		) {
			mode_info.interintra = decoder.read_interintra(sbd.size);
			if (mode_info.interintra) {
				mode_info.interintra_mode = decoder.read_interintra_mode(sbd.size);
				mode_info.ref_frame[1] = reference_frame::intra;
				mode_info.angle_delta_y  = 0;
				mode_info.angle_delta_uv = 0;
				mode_info.use_filter_intra = false;
				mode_info.wedge_interintra = decoder.read_wedge_interintra(sbd.size);
				if (mode_info.wedge_interintra) {
					mode_info.wedge_index = decoder.read_wedge_index(sbd.size);
					mode_info.wedge_sign = false;
				}
			}
		} else {
			mode_info.interintra = false;
		}
	}

	void reader::read_mv(
		const obu::uncompressed_header &header,
		const obu::mode_info &mode_info,
		symbol_decoder &decoder,
		state::motion_vector &mv,
		u32 ref
	) {
		cvec2i32 diff_mv = zero;
		u32 mv_ctx;
		if (mode_info.use_intrabc) {
			mv_ctx = constants::mv_intrabc_context;
		} else {
			mv_ctx = 0;
		}
		const mv_joint mv_joint = decoder.read_mv_joint(mv_ctx);
		if (mv_joint == mv_joint::hzvnz || mv_joint == mv_joint::hnzvnz) {
			diff_mv[0] = read_mv_component(header, decoder, mv_ctx, 0);
		}
		if (mv_joint == mv_joint::hnzvz || mv_joint == mv_joint::hnzvnz) {
			diff_mv[1] = read_mv_component(header, decoder, mv_ctx, 1);
		}
		mv.mv[ref] = mv.pred_mv[ref] + diff_mv;
	}

	i32 reader::read_mv_component(
		const obu::uncompressed_header &header, symbol_decoder &decoder, u32 mv_ctx, u32 comp
	) {
		const bool mv_sign = decoder.read_mv_sign(mv_ctx, comp);
		const u8 mv_class = decoder.read_mv_class(mv_ctx, comp);
		i32 mag;
		if (mv_class == 0) {
			const bool mv_class0_bit = decoder.read_mv_class0_bit(mv_ctx, comp);
			u8 mv_class0_fr;
			bool mv_class0_hp;
			if (header.force_integer_mv) {
				mv_class0_fr = 3;
			} else {
				mv_class0_fr = decoder.read_mv_class0_fr(mv_ctx, comp, mv_class0_bit);
			}
			if (header.allow_high_precision_mv) {
				mv_class0_hp = decoder.read_mv_class0_hp(mv_ctx, comp);
			} else {
				mv_class0_hp = true;
			}
			mag = (((mv_class0_bit ? 1 : 0) << 3) | (mv_class0_fr << 1) | (mv_class0_hp ? 1 : 0)) + 1;
		} else {
			i32 d = 0;
			for (u8 i = 0; i < mv_class; ++i) {
				const bool mv_bit = decoder.read_mv_bit(mv_ctx, comp, i);
				if (mv_bit) {
					d |= 1 << i;
				}
			}
			mag = static_cast<i32>(constants::class0_size << (mv_class + 2));
			u8 mv_fr;
			bool mv_hp;
			if (header.force_integer_mv) {
				mv_fr = 3;
			} else {
				mv_fr = decoder.read_mv_fr(mv_ctx, comp);
			}
			if (header.allow_high_precision_mv) {
				mv_hp = decoder.read_mv_hp(mv_ctx, comp);
			} else {
				mv_hp = true;
			}
			mag += ((d << 3) | (mv_fr << 1) | (mv_hp ? 1 : 0)) + 1;
		}
		return mv_sign ? -mag : mag;
	}

	void reader::read_residual(
		const obu::sequence_header &seq_header,
		const obu::uncompressed_header &header,
		const obu::mode_info &mode_info,
		symbol_decoder &decoder,
		state::block &sb,
		const state::block_decoding &sbd,
		const state::color_map &scm
	) {
		const u32 sb_mask = seq_header.use_128x128_superblock ? 31 : 15;

		const u32 width_chunks = std::max(1u, constants::block_width(sbd.size) >> 6);
		const u32 height_chunks = std::max(1u, constants::block_height(sbd.size) >> 6);

		const block_size mi_size_chunk = width_chunks > 1 || height_chunks > 1 ? block_size::b64x64 : sbd.size;

		for (u32 chunk_y = 0; chunk_y < height_chunks; ++chunk_y) {
			for (u32 chunk_x = 0; chunk_x < width_chunks; ++chunk_x) {
				const u32 mi_row_chunk = sbd.row + (chunk_y << 4);
				const u32 mi_col_chunk = sbd.col + (chunk_x << 4);
				/*const u32 sub_block_mi_row = mi_row_chunk & sb_mask;
				const u32 sub_block_mi_col = mi_col_chunk & sb_mask;*/ // unused?

				for (u32 plane = 0; plane < 1 + (sbd.has_chroma ? 1 : 0) * 2; ++plane) {
					const tx_size tx_sz =
						mode_info.lossless ?
						tx_size::size_4x4 :
						functions::get_tx_size(seq_header.color_config, sbd, plane, sb.tx_size);
					const u32 step_x = constants::get_tx_width(tx_sz) >> 2;
					const u32 step_y = constants::get_tx_height(tx_sz) >> 2;
					const block_size plane_sz =
						functions::get_plane_residual_size(seq_header.color_config, mi_size_chunk, plane);
					const u32 num_4x4_w = constants::num_4x4_blocks_wide[std::to_underlying(plane_sz)];
					const u32 num_4x4_h = constants::num_4x4_blocks_high[std::to_underlying(plane_sz)];
					const u32 sub_x = plane > 0 ? (seq_header.color_config.subsampling_x ? 1 : 0) : 0;
					const u32 sub_y = plane > 0 ? (seq_header.color_config.subsampling_y ? 1 : 0) : 0;
					const u32 base_x = (mi_col_chunk >> sub_x) * constants::mi_size;
					const u32 base_y = (mi_row_chunk >> sub_y) * constants::mi_size;
					if (mode_info.is_inter && !mode_info.lossless && plane == 0) {
						read_transform_tree(
							seq_header,
							header,
							mode_info,
							decoder,
							sb,
							sbd,
							scm,
							base_x,
							base_y,
							num_4x4_w * 4,
							num_4x4_h * 4
						);
					} else {
						const u32 base_x_block = (sbd.col >> sub_x) * constants::mi_size;
						const u32 base_y_block = (sbd.row >> sub_y) * constants::mi_size;
						for (u32 y = 0; y < num_4x4_h; y += step_y) {
							for (u32 x = 0; x < num_4x4_w; x += step_x) {
								read_transform_block(
									seq_header,
									header,
									mode_info,
									decoder,
									sb,
									sbd,
									scm,
									plane,
									base_x_block,
									base_y_block,
									tx_sz,
									x + ((chunk_x << 4) >> sub_x),
									y + ((chunk_y << 4) >> sub_y)
								);
							}
						}
					}
				}
			}
		}
	}

	void reader::read_transform_block(
		const obu::sequence_header &seq_header,
		const obu::uncompressed_header &header,
		const obu::mode_info &mode_info,
		symbol_decoder &decoder,
		state::block &sb,
		const state::block_decoding &sbd,
		const state::color_map &scm,
		u32 plane, u32 base_x, u32 base_y, tx_size tx_sz, u32 x, u32 y
	) {
		const u32 start_x = base_x + 4 * x;
		const u32 start_y = base_y + 4 * y;
		const u32 sub_x = plane > 0 ? (seq_header.color_config.subsampling_x ? 1 : 0) : 0;
		const u32 sub_y = plane > 0 ? (seq_header.color_config.subsampling_y ? 1 : 0) : 0;
		const u32 row = (start_y << sub_y) >> constants::mi_size_log2;
		const u32 col = (start_x << sub_x) >> constants::mi_size_log2;
		const u32 sb_mask = seq_header.use_128x128_superblock ? 31 : 15;
		const u32 sub_block_mi_row = row & sb_mask;
		const u32 sub_block_mi_col = col & sb_mask;
		const u32 step_x = constants::get_tx_width(tx_sz) >> constants::mi_size_log2;
		const u32 step_y = constants::get_tx_height(tx_sz) >> constants::mi_size_log2;
		const u32 max_x = (header.frame_size.get_mi_cols() * constants::mi_size) >> sub_x;
		const u32 max_y = (header.frame_size.get_mi_rows() * constants::mi_size) >> sub_y;
		if (start_x >= max_x || start_y >= max_y) {
			return;
		}
		if (!mode_info.is_inter) {
			if ((plane == 0 && mode_info.palette_size_y != 0) || (plane != 0 && mode_info.palette_size_uv != 0)) {
				block_decoding::predict_palette(mode_info, sb, scm, plane, start_x, start_y, x, y, tx_sz);
			} else {
				const bool is_cfl = plane > 0 && mode_info.uv_mode == prediction_mode::uv_cfl;
				prediction_mode mode;
				if (plane == 0) {
					mode = mode_info.y_mode;
				} else {
					mode = is_cfl ? prediction_mode::dc : mode_info.uv_mode;
				}
				const u32 log2w = constants::get_tx_width_log2(tx_sz);
				const u32 log2h = constants::get_tx_height_log2(tx_sz);
				block_decoding::predict_intra(
					seq_header,
					header,
					mode_info,
					sb,
					sbd,
					plane,
					start_x,
					start_y,
					(plane == 0 ? sbd.avail_l : sbd.avail_l_chroma) || x > 0,
					(plane == 0 ? sbd.avail_u : sbd.avail_u_chroma) || y > 0,
					sb.block_decoded(
						plane,
						static_cast<i32>(sub_block_mi_row >> sub_y) - 1,
						static_cast<i32>((sub_block_mi_col >> sub_x) + step_x)
					),
					sb.block_decoded(
						plane,
						static_cast<i32>((sub_block_mi_row >> sub_y) + step_y),
						static_cast<i32>(sub_block_mi_col >> sub_x) - 1
					),
					mode,
					log2w,
					log2h
				);
				if (is_cfl) {
					block_decoding::predict_chroma_from_luma(
						seq_header, mode_info, sb, plane, start_x, start_y, tx_sz
					);
				}
			}

			if (plane == 0) {
				sb.max_luma_w = start_x + step_x * 4;
				sb.max_luma_h = start_y + step_y * 4;
			}
		}
		if (!mode_info.skip) {
			state::coeffs sc = zero;
			const u32 eob = read_coeffs(
				seq_header, header, mode_info, decoder, sb, sbd, sc, plane, start_x, start_y, tx_sz
			);
			if (eob > 0) {
				block_decoding::reconstruct(seq_header, header, mode_info, sb, sc, plane, start_x, start_y, tx_sz);
			}
		}
		for (u32 i = 0; i < step_y; ++i) {
			for (u32 j = 0; j < step_x; ++j) {
				// TODO LoopfilterTxSizes
				sb.block_decoded(
					plane,
					static_cast<i32>((sub_block_mi_row >> sub_y) + i),
					static_cast<i32>((sub_block_mi_col >> sub_x) + j)
				) = true;
			}
		}
	}

	void reader::read_transform_tree(
		const obu::sequence_header &seq_header,
		const obu::uncompressed_header &header,
		const obu::mode_info &mode_info,
		symbol_decoder &decoder,
		state::block &sb,
		const state::block_decoding &sbd,
		const state::color_map &scm,
		u32 start_x, u32 start_y, u32 w, u32 h
	) {
		const u32 max_x = header.frame_size.get_mi_cols() * constants::mi_size;
		const u32 max_y = header.frame_size.get_mi_rows() * constants::mi_size;
		if (start_x >= max_x || start_y >= max_y) {
			return;
		}
		const u32 row = start_y >> constants::mi_size_log2;
		const u32 col = start_x >> constants::mi_size_log2;
		const tx_size luma_tx_sz = sb.inter_tx_sizes(row, col);
		const u32 luma_w = constants::get_tx_width(luma_tx_sz);
		const u32 luma_h = constants::get_tx_height(luma_tx_sz);
		if (w <= luma_w && h <= luma_h) {
			const tx_size tx_sz = functions::find_tx_size(w, h);
			read_transform_block(seq_header, header, mode_info, decoder, sb, sbd, scm, 0, start_x, start_y, tx_sz, 0, 0);
		} else {
			if (w > h) {
				read_transform_tree(seq_header, header, mode_info, decoder, sb, sbd, scm, start_x,         start_y, w / 2, h);
				read_transform_tree(seq_header, header, mode_info, decoder, sb, sbd, scm, start_x + w / 2, start_y, w / 2, h);
			} else if (w < h) {
				read_transform_tree(seq_header, header, mode_info, decoder, sb, sbd, scm, start_x, start_y,         w, h / 2);
				read_transform_tree(seq_header, header, mode_info, decoder, sb, sbd, scm, start_x, start_y + h / 2, w, h / 2);
			} else {
				read_transform_tree(seq_header, header, mode_info, decoder, sb, sbd, scm, start_x,         start_y,         w / 2, h / 2);
				read_transform_tree(seq_header, header, mode_info, decoder, sb, sbd, scm, start_x + w / 2, start_y,         w / 2, h / 2);
				read_transform_tree(seq_header, header, mode_info, decoder, sb, sbd, scm, start_x,         start_y + h / 2, w / 2, h / 2);
				read_transform_tree(seq_header, header, mode_info, decoder, sb, sbd, scm, start_x + w / 2, start_y + h / 2, w / 2, h / 2);
			}
		}
	}

	u32 reader::read_coeffs(
		const obu::sequence_header &seq_header,
		const obu::uncompressed_header &header,
		const obu::mode_info &mode_info,
		symbol_decoder &decoder,
		state::block &sb,
		const state::block_decoding &sbd,
		state::coeffs &sc,
		u32 plane, u32 start_x, u32 start_y, tx_size tx_sz
	) {
		const u32 x4 = start_x >> 2;
		const u32 y4 = start_y >> 2;
		const u32 w4 = constants::get_tx_width(tx_sz) >> 2;
		const u32 h4 = constants::get_tx_height(tx_sz) >> 2;

		const u32 tx_sz_ctx = (
			std::to_underlying(constants::get_tx_size_sqr(tx_sz)) +
			std::to_underlying(constants::get_tx_size_sqr_up(tx_sz)) +
			1
		) >> 1;
		const u32 ptype = plane > 0 ? 1 : 0;
		const u32 seg_eob =
			tx_sz == tx_size::size_16x64 || tx_sz == tx_size::size_64x16 ?
			512 :
			std::min(1024u, constants::get_tx_width(tx_sz) * constants::get_tx_height(tx_sz));

		for (u32 c = 0; c < seg_eob; ++c) {
			sc.quant[c] = 0;
		}
		for (u32 i = 0; i < 64; ++i) {
			for (u32 j = 0; j < 64; ++j) {
				sc.dequant[i][j] = 0;
			}
		}

		u32 eob = 0;
		u32 cul_level = 0;
		u8 dc_category = 0;

		const bool all_zero = decoder.read_all_zero(
			tx_sz_ctx, cdf::context::compute_all_zero(seq_header, header, sb, sbd, plane, tx_sz, x4, y4, w4, h4)
		);
		if (all_zero) {
			if (plane == 0) {
				for (u32 i = 0; i < w4; ++i) {
					for (u32 j = 0; j < h4; ++j) {
						sb.tx_types(y4 + j, x4 + i) = inverse_transform::dct_dct;
					}
				}
			}
		} else {
			if (plane == 0) {
				read_transform_type(header, mode_info, decoder, sb, x4, y4, tx_sz);
			}
			sb.plane_tx_type =
				functions::compute_tx_type(seq_header, header, mode_info, sb, sbd, plane, tx_sz, x4, y4);
			const constants::scan::table_t scan = functions::get_scan(sb, tx_sz);

			const u32 eob_multisize =
				std::min(constants::get_tx_width_log2(tx_sz), 5u) +
				std::min(constants::get_tx_height_log2(tx_sz), 5u) -
				4;
			u32 eob_pt;
			if (eob_multisize == 0) {
				const u32 eob_pt_16 = decoder.read_eob_pt_16(
					ptype, cdf::context::compute_eob_pt(seq_header, header, mode_info, sb, sbd, plane, tx_sz, x4, y4)
				);
				eob_pt = eob_pt_16 + 1;
			} else if (eob_multisize == 1) {
				const u32 eob_pt_32 = decoder.read_eob_pt_32(
					ptype, cdf::context::compute_eob_pt(seq_header, header, mode_info, sb, sbd, plane, tx_sz, x4, y4)
				);
				eob_pt = eob_pt_32 + 1;
			} else if (eob_multisize == 2) {
				const u32 eob_pt_64 = decoder.read_eob_pt_64(
					ptype, cdf::context::compute_eob_pt(seq_header, header, mode_info, sb, sbd, plane, tx_sz, x4, y4)
				);
				eob_pt = eob_pt_64 + 1;
			} else if (eob_multisize == 3) {
				const u32 eob_pt_128 = decoder.read_eob_pt_128(
					ptype, cdf::context::compute_eob_pt(seq_header, header, mode_info, sb, sbd, plane, tx_sz, x4, y4)
				);
				eob_pt = eob_pt_128 + 1;
			} else if (eob_multisize == 4) {
				const u32 eob_pt_256 = decoder.read_eob_pt_256(
					ptype, cdf::context::compute_eob_pt(seq_header, header, mode_info, sb, sbd, plane, tx_sz, x4, y4)
				);
				eob_pt = eob_pt_256 + 1;
			} else if (eob_multisize == 5) {
				const u32 eob_pt_512 = decoder.read_eob_pt_512(ptype);
				eob_pt = eob_pt_512 + 1;
			} else {
				const u32 eob_pt_1024 = decoder.read_eob_pt_1024(ptype);
				eob_pt = eob_pt_1024 + 1;
			}

			eob = eob_pt < 2 ? eob_pt : (1u << (eob_pt - 2)) + 1;
			i32 eob_shift = std::max(-1, static_cast<i32>(eob_pt) - 3);
			if (eob_shift >= 0) {
				const bool eob_extra = decoder.read_eob_extra(tx_sz_ctx, ptype, eob_pt);
				if (eob_extra) {
					eob += 1u << eob_shift;
				}

				for (u32 i = 1; i < std::max(2u, eob_pt) - 2; ++i) {
					eob_shift = std::max(0, static_cast<i32>(eob_pt) - 2) - 1 - static_cast<i32>(i);
					const bool eob_extra_bit = decoder.read_bool();
					if (eob_extra_bit) {
						eob += 1u << eob_shift;
					}
				}
			}
			for (u32 c = eob; c > 0; ) {
				--c;

				const u32 pos = scan[c];
				u32 level;
				if (c == eob - 1) {
					const u32 coeff_base_eob = decoder.read_coeff_base_eob(
						tx_sz_ctx,
						ptype,
						functions::get_coeff_base_ctx(
							seq_header, header, mode_info, sb, sbd, sc, tx_sz, plane, x4, y4, scan[c], c, true
						) - constants::sig_coef_contexts + constants::sig_coef_contexts_eob
					);
					level = coeff_base_eob + 1;
				} else {
					const u32 coeff_base = decoder.read_coeff_base(
						tx_sz_ctx, ptype, functions::get_coeff_base_ctx(
							seq_header, header, mode_info, sb, sbd, sc, tx_sz, plane, x4, y4, scan[c], c, false
						)
					);
					level = coeff_base;
				}

				if (level > constants::num_base_levels) {
					for (u32 idx = 0; idx < constants::coeff_base_range / (constants::br_cdf_size - 1); ++idx) {
						const u32 coeff_br = decoder.read_coeff_br(
							tx_sz_ctx,
							ptype,
							cdf::context::compute_coeff_br(
								seq_header, header, mode_info, sb, sbd, sc, tx_sz, plane, x4, y4, pos
							)
						);
						level += coeff_br;
						if (coeff_br < constants::br_cdf_size - 1) {
							break;
						}
					}
				}
				sc.quant[pos] = static_cast<i32>(level);
			}

			for (u32 c = 0; c < eob; ++c) {
				const u32 pos = scan[c];
				bool sign;
				if (sc.quant[pos] != 0) {
					if (c == 0) {
						const bool dc_sign = decoder.read_dc_sign(
							ptype, cdf::context::compute_dc_sign(seq_header, header, sb, plane, x4, y4, w4, h4)
						);
						sign = dc_sign;
					} else {
						const bool sign_bit = decoder.read_bool();
						sign = sign_bit;
					}
				} else {
					sign = false;
				}
				if (sc.quant[pos] > static_cast<i32>(constants::num_base_levels + constants::coeff_base_range)) {
					u32 length = 0;
					bool golomb_length_bit;
					do {
						++length;
						golomb_length_bit = decoder.read_bool();
					} while (!golomb_length_bit);
					u32 x = 1;
					for (i32 i = static_cast<i32>(length) - 2; i >= 0; --i) {
						const bool golomb_data_bit = decoder.read_bool();
						x = (x << 1) | (golomb_data_bit ? 1 : 0);
					}
					sc.quant[pos] = static_cast<i32>(x + constants::coeff_base_range + constants::num_base_levels);
				}
				if (pos == 0 && sc.quant[pos] > 0) {
					dc_category = sign ? 1 : 2;
				}
				sc.quant[pos] = sc.quant[pos] & 0xFFFFF;
				cul_level += static_cast<u32>(sc.quant[pos]);
				if (sign) {
					sc.quant[pos] = -sc.quant[pos];
				}
			}
			cul_level = std::min(63u, cul_level);
		}

		for (u32 i = 0; i < w4; ++i) {
			sb.above_level_context[plane][x4 + i] = static_cast<u8>(cul_level);
			sb.above_dc_context[plane][x4 + i] = dc_category;
		}
		for (u32 i = 0; i < h4; ++i) {
			sb.left_level_context[plane][y4 + i] = static_cast<u8>(cul_level);
			sb.left_dc_context[plane][y4 + i] = dc_category;
		}

		return eob;
	}

	i8 reader::read_intra_angle_info_y(
		const obu::mode_info &mode_info,
		symbol_decoder &decoder,
		const state::block_decoding &sbd
	) {
		if (sbd.size >= block_size::b8x8) {
			if (functions::is_directional_mode(mode_info.y_mode)) {
				const i8 angle_delta_y = decoder.read_angle_delta_y(mode_info.y_mode);
				return angle_delta_y - static_cast<i32>(constants::max_angle_delta);
			}
		}
		return 0;
	}

	i8 reader::read_intra_angle_info_uv(
		const obu::mode_info &mode_info,
		symbol_decoder &decoder,
		const state::block_decoding &mi
	) {
		if (mi.size >= block_size::b8x8) {
			if (functions::is_directional_mode(mode_info.uv_mode)) {
				const i8 angle_delta_uv = decoder.read_angle_delta_uv(mode_info.uv_mode);
				return angle_delta_uv - static_cast<i32>(constants::max_angle_delta);
			}
		}
		return 0;
	}

	obu::cfl_alphas reader::read_cfl_alphas(symbol_decoder &decoder) {
		obu::cfl_alphas result = zero;
		const u8 cfl_alpha_signs = decoder.read_cfl_alpha_signs();
		const auto sign_u = static_cast<cfl_sign>((cfl_alpha_signs + 1) / 3);
		const auto sign_v = static_cast<cfl_sign>((cfl_alpha_signs + 1) % 3);
		if (sign_u != cfl_sign::zero) {
			result.u = static_cast<i8>(decoder.read_cfl_alpha_u(cfl_alpha_signs) + 1);
			if (sign_u == cfl_sign::negative) {
				result.u = static_cast<i8>(-result.u);
			}
		} else {
			result.u = 0;
		}
		if (sign_v != cfl_sign::zero) {
			result.v = static_cast<i8>(decoder.read_cfl_alpha_v(cfl_alpha_signs) + 1);
			if (sign_v == cfl_sign::negative) {
				result.v = static_cast<i8>(-result.v);
			}
		} else {
			result.v = 0;
		}
		return result;
	}

	void reader::read_palette_mode_info(
		const obu::sequence_header &seq_header,
		obu::mode_info &mode_info,
		symbol_decoder &decoder,
		const state::block &sb,
		const state::block_decoding &sbd
	) {
		const u32 bsize_ctx =
			constants::mi_width_log2[std::to_underlying(sbd.size)] +
			constants::mi_height_log2[std::to_underlying(sbd.size)] -
			2;
		if (mode_info.y_mode == prediction_mode::dc) {
			const bool has_palette_y =
				decoder.read_has_palette_y(bsize_ctx, cdf::context::compute_has_palette_y(sb, sbd));
			if (has_palette_y) {
				mode_info.palette_size_y = decoder.read_palette_size_y_minus_2(bsize_ctx) + 2;
				const functions::palette_cache_t palette_cache = functions::get_palette_cache(sb, sbd, 0);
				u32 idx = 0;
				for (u32 i = 0; i < palette_cache.size() && idx < mode_info.palette_size_y; ++i) {
					const bool use_palette_color_cache_y = decoder.read_bool();
					if (use_palette_color_cache_y) {
						mode_info.palette_colors_y[idx] = palette_cache[i];
						++idx;
					}
				}
				if (idx < mode_info.palette_size_y) {
					mode_info.palette_colors_y[idx] =
						static_cast<channel_t>(decoder.read_literal(seq_header.color_config.bit_depth));
					++idx;
				}
				if (idx < mode_info.palette_size_y) {
					const u32 min_bits = seq_header.color_config.bit_depth - 3;
					const u32 palette_num_extra_bits_y = decoder.read_literal(2);
					u32 palette_bits = min_bits + palette_num_extra_bits_y;
					while (idx < mode_info.palette_size_y) {
						const u32 palette_delta_y = decoder.read_literal(palette_bits) + 1;
						mode_info.palette_colors_y[idx] = functions::clip1(
							seq_header.color_config, mode_info.palette_colors_y[idx - 1] + palette_delta_y
						);
						const u32 range =
							(1u << seq_header.color_config.bit_depth) - mode_info.palette_colors_y[idx] - 1;
						palette_bits = std::min(palette_bits, functions::ceil_log2(range));
						++idx;
					}
				}
				std::sort(mode_info.palette_colors_y, mode_info.palette_colors_y + mode_info.palette_size_y - 1);
			}
		}
		if (sbd.has_chroma && mode_info.uv_mode == prediction_mode::dc) {
			const bool has_palette_uv = decoder.read_has_palette_uv(mode_info.palette_size_y);
			if (has_palette_uv) {
				mode_info.palette_size_uv = decoder.read_palette_size_uv_minus_2(bsize_ctx) + 2;
				{
					const functions::palette_cache_t palette_cache = functions::get_palette_cache(sb, sbd, 1);
					u32 idx = 0;
					for (u32 i = 0; i < palette_cache.size() && idx < mode_info.palette_size_uv; ++i) {
						const bool use_palette_color_cache_u = decoder.read_bool();
						if (use_palette_color_cache_u) {
							mode_info.palette_colors_u[idx] = palette_cache[i];
							++idx;
						}
					}
					if (idx < mode_info.palette_size_uv) {
						mode_info.palette_colors_u[idx] =
							static_cast<channel_t>(decoder.read_literal(seq_header.color_config.bit_depth));
						++idx;
					}
					if (idx < mode_info.palette_size_uv) {
						const u32 min_bits = seq_header.color_config.bit_depth - 3;
						const u32 palette_num_extra_bits_u = decoder.read_literal(2);
						u32 palette_bits = min_bits + palette_num_extra_bits_u;
						while (idx < mode_info.palette_size_uv) {
							const u32 palette_delta_u = decoder.read_literal(palette_bits);
							mode_info.palette_colors_u[idx] = functions::clip1(
								seq_header.color_config, mode_info.palette_colors_u[idx - 1] + palette_delta_u
							);
							const u32 range =
								(1u << seq_header.color_config.bit_depth) - mode_info.palette_colors_u[idx];
							palette_bits = std::min(palette_bits, functions::ceil_log2(range));
							++idx;
						}
					}
					std::sort(
						mode_info.palette_colors_u, mode_info.palette_colors_u + mode_info.palette_size_uv - 1
					);
				}

				const bool delta_encode_palette_colors_v = decoder.read_bool();
				if (delta_encode_palette_colors_v) {
					const u32 min_bits = seq_header.color_config.bit_depth - 4;
					const i32 max_val = 1 << seq_header.color_config.bit_depth;
					const u32 palette_num_extra_bits_v = decoder.read_literal(2);
					const u32 palette_bits = min_bits + palette_num_extra_bits_v;
					mode_info.palette_colors_v[0] =
						static_cast<channel_t>(decoder.read_literal(seq_header.color_config.bit_depth));
					for (u32 idx = 1; idx < mode_info.palette_size_uv; ++idx) {
						auto palette_delta_v = static_cast<i32>(decoder.read_literal(palette_bits));
						if (palette_delta_v != 0) {
							const bool palette_delta_sign_bit_v = decoder.read_bool();
							if (palette_delta_sign_bit_v) {
								palette_delta_v = -palette_delta_v;
							}
						}
						i32 val = static_cast<i32>(mode_info.palette_colors_v[idx - 1]) + palette_delta_v;
						val = val < 0 ? val + max_val : val;
						val = val >= max_val ? val - max_val : val;
						mode_info.palette_colors_v[idx] = functions::clip1(seq_header.color_config, val);
					}
				} else {
					for (u32 idx = 0; idx < mode_info.palette_size_uv; ++idx) {
						mode_info.palette_colors_v[idx] =
							static_cast<channel_t>(decoder.read_literal(seq_header.color_config.bit_depth));
					}
				}
			}
		}
	}

	void reader::read_transform_type(
		const obu::uncompressed_header &header,
		const obu::mode_info &mode_info,
		symbol_decoder &decoder,
		state::block &sb,
		u32 x4, u32 y4, tx_size tx_sz
	) {
		const tx_set set = functions::get_tx_set(header, mode_info, tx_sz);

		inverse_transform tx_type; // TxType
		if (
			const u8 q_index =
				header.segmentation_params.segmentation_enabled ?
				functions::get_qindex(header, sb, true, mode_info.segment_id) :
				header.quantization_params.base_q_idx;
			set > zero && q_index > 0
		) {
			if (mode_info.is_inter) {
				const u8 inter_tx_type = decoder.read_inter_tx_type(set, tx_sz);
				if (set == tx_set::inter_1) {
					tx_type = constants::tx_type_inter_inv_set1[inter_tx_type];
				} else if (set == tx_set::inter_2) {
					tx_type = constants::tx_type_inter_inv_set2[inter_tx_type];
				} else {
					tx_type = constants::tx_type_inter_inv_set3[inter_tx_type];
				}
			} else {
				const u8 intra_tx_type =
					decoder.read_intra_tx_type(set, tx_sz, cdf::context::compute_intra_tx_type(mode_info));
				if (set == tx_set::intra_1) {
					tx_type = constants::tx_type_intra_inv_set1[intra_tx_type];
				} else {
					tx_type = constants::tx_type_intra_inv_set2[intra_tx_type];
				}
			}
		} else {
			tx_type = inverse_transform::dct_dct;
		}
		for (u32 i = 0; i < (constants::get_tx_width(tx_sz) >> 2); ++i) {
			for (u32 j = 0; j < (constants::get_tx_height(tx_sz) >> 2); ++j) {
				sb.tx_types(y4 + j, x4 + i) = tx_type;
			}
		}
	}

	state::color_map reader::read_palette_tokens(
		const obu::sequence_header &seq_header,
		const obu::uncompressed_header &header,
		const obu::mode_info &mode_info,
		symbol_decoder &decoder,
		const state::block_decoding &sbd
	) {
		state::color_map color_map = uninitialized;

		u32 block_height = constants::block_height(sbd.size);
		u32 block_width = constants::block_width(sbd.size);
		u32 onscreen_height =
			std::min(block_height, (header.frame_size.get_mi_rows() - sbd.row) * constants::mi_size);
		u32 onscreen_width =
			std::min(block_width, (header.frame_size.get_mi_cols() - sbd.col) * constants::mi_size);

		if (mode_info.palette_size_y != 0) {
			const u32 color_index_map_y = decoder.read_ns(mode_info.palette_size_y);
			color_map.y[0][0] = color_index_map_y;
			for (i32 i = 1; i < static_cast<i32>(onscreen_height + onscreen_width) - 1; ++i) {
				for (
					i32 j = std::min(i, static_cast<i32>(onscreen_width) - 1);
					j >= std::max(0, i - static_cast<i32>(onscreen_height) + 1);
					--j
				) {
					const auto ctx = functions::palette_color_context::get(
						color_map.y, static_cast<u32>(i - j), static_cast<u32>(j), mode_info.palette_size_y
					);
					const u8 palette_color_idx_y =
						decoder.read_palette_color_idx_y(mode_info.palette_size_y, ctx.hash);
					color_map.y[static_cast<usize>(i - j)][static_cast<usize>(j)] =
						ctx.color_order[palette_color_idx_y];
				}
			}
			for (u32 i = 0; i < onscreen_height; ++i) {
				for (u32 j = onscreen_width; j < block_width; ++j) {
					color_map.y[i][j] = color_map.y[i][onscreen_width - 1];
				}
			}
			for (u32 i = onscreen_height; i < block_height; ++i) {
				for (u32 j = 0; j < block_width; ++j) {
					color_map.y[i][j] = color_map.y[onscreen_height - 1][j];
				}
			}
		}

		if (mode_info.palette_size_uv != 0) {
			const u32 color_index_map_uv = decoder.read_ns(mode_info.palette_size_uv);
			color_map.uv[0][0] = color_index_map_uv;
			block_height >>= seq_header.color_config.subsampling_y ? 1 : 0;
			block_width  >>= seq_header.color_config.subsampling_x ? 1 : 0;
			onscreen_height >>= seq_header.color_config.subsampling_y ? 1 : 0;
			onscreen_width  >>= seq_header.color_config.subsampling_x ? 1 : 0;
			if (block_width < 4) {
				block_width += 2;
				onscreen_width += 2;
			}
			if (block_height < 4) {
				block_height += 2;
				onscreen_height += 2;
			}

			for (i32 i = 1; i < static_cast<i32>(onscreen_height + onscreen_width) - 1; ++i) {
				for (
					i32 j = std::min(i, static_cast<i32>(onscreen_width) - 1);
					j >= std::max(0, i - static_cast<i32>(onscreen_height) + 1);
					--j
				) {
					const auto ctx = functions::palette_color_context::get(
						color_map.uv, static_cast<u32>(i - j), static_cast<u32>(j), mode_info.palette_size_uv
					);
					const u8 palette_color_idx_uv =
						decoder.read_palette_color_idx_uv(mode_info.palette_size_uv, ctx.hash);
					color_map.uv[static_cast<usize>(i - j)][static_cast<usize>(j)] =
						ctx.color_order[palette_color_idx_uv];
				}
			}
			for (u32 i = 0; i < onscreen_height; ++i) {
				for (u32 j = onscreen_width; j < block_width; ++j) {
					color_map.uv[i][j] = color_map.uv[i][onscreen_width - 1];
				}
			}
			for (u32 i = onscreen_height; i < block_height; ++i) {
				for (u32 j = 0; j < block_width; ++j) {
					color_map.uv[i][j] = color_map.uv[onscreen_height - 1][j];
				}
			}
		}

		return color_map;
	}

	void reader::read_cdef(
		const obu::sequence_header &seq_header,
		const obu::uncompressed_header &header,
		const obu::mode_info &result,
		symbol_decoder &decoder,
		state::block &sb,
		const state::block_decoding &sbd
	) {
		if (result.skip || header.coded_lossless || !seq_header.enable_cdef || header.allow_intrabc) {
			return; // TODO values from previous?
		}
		const u32 cdef_size4 = constants::num_4x4_blocks_wide[std::to_underlying(block_size::b64x64)];
		const u32 cdef_mask4 = ~(cdef_size4 - 1);
		const u32 r = sbd.row & cdef_mask4;
		const u32 c = sbd.col & cdef_mask4;
		if (sb.cdef_idx(r, c) == -1) {
			sb.cdef_idx(r, c) = static_cast<i8>(decoder.read_literal(header.cdef_params.bits));
			const u32 w4 = constants::num_4x4_blocks_wide[std::to_underlying(sbd.size)];
			const u32 h4 = constants::num_4x4_blocks_high[std::to_underlying(sbd.size)];
			for (u32 i = r; i < r + h4; i += cdef_size4) {
				for (u32 j = c; j < c + w4; j += cdef_size4) {
					sb.cdef_idx(i, j) = sb.cdef_idx(r, c);
				}
			}
		}
	}

	void reader::read_lr(
		const obu::sequence_header &seq_header,
		const obu::uncompressed_header &header,
		symbol_decoder &decoder,
		state::ref_lr &slr,
		u32 r, u32 c, block_size b_size
	) {
		if (header.allow_intrabc) {
			return;
		}
		const u32 w = constants::num_4x4_blocks_wide[std::to_underlying(b_size)];
		const u32 h = constants::num_4x4_blocks_high[std::to_underlying(b_size)];
		for (u32 plane = 0; plane < seq_header.color_config.get_num_planes(); ++plane) {
			if (header.lr_params.frame_restoration_type[plane] != frame_restoration::none) {
				const u32 subx = plane == 0 ? 0 : (seq_header.color_config.subsampling_x ? 1 : 0);
				const u32 suby = plane == 0 ? 0 : (seq_header.color_config.subsampling_y ? 1 : 0);
				const u32 unit_size = header.lr_params.size[plane];
				const u32 unit_rows = functions::count_units_in_frame(
					unit_size, functions::round2(header.frame_size.frame_height, suby)
				);
				const u32 unit_cols = functions::count_units_in_frame(
					unit_size, functions::round2(header.frame_size.upscaled_width, subx)
				);
				const u32 unit_row_start = (r * (constants::mi_size >> suby) + unit_size - 1) / unit_size;
				const u32 unit_row_end =
					std::min(unit_rows, ((r + h) * (constants::mi_size >> suby) + unit_size - 1) / unit_size);
				u32 numerator;
				u32 denominator;
				if (header.frame_size.use_superres) {
					numerator = (constants::mi_size >> subx) * header.frame_size.superres_denom;
					denominator = unit_size * constants::superres_num;
				} else {
					numerator = constants::mi_size >> subx;
					denominator = unit_size;
				}
				const u32 unit_col_start = (c * numerator + denominator - 1) / denominator;
				const u32 unit_col_end = std::min(unit_cols, ((c + w) * numerator + denominator - 1) / denominator);
				for (u32 unit_row = unit_row_start; unit_row < unit_row_end; ++unit_row) {
					for (u32 unit_col = unit_col_start; unit_col < unit_col_end; ++unit_col) {
						read_lr_unit(header, decoder, slr, plane, unit_row, unit_col);
					}
				}
			}
		}
	}

	void reader::read_lr_unit(
		const obu::uncompressed_header &header,
		symbol_decoder &decoder,
		state::ref_lr &slr,
		u32 plane, u32 unit_row, u32 unit_col
	) {
		frame_restoration restoration_type;
		if (header.lr_params.frame_restoration_type[plane] == frame_restoration::wiener) {
			const bool use_wiener = decoder.read_use_wiener();
			restoration_type = use_wiener ? frame_restoration::wiener : frame_restoration::none;
		} else if (header.lr_params.frame_restoration_type[plane] == frame_restoration::sgrproj) {
			const bool use_sgrproj = decoder.read_use_sgrproj();
			restoration_type = use_sgrproj ? frame_restoration::sgrproj : frame_restoration::none;
		} else {
			restoration_type = decoder.read_restoration_type();
		}
		// TODO LrType
		if (restoration_type == frame_restoration::wiener) {
			for (u32 pass = 0; pass < 2; ++pass) {
				u32 first_coeff;
				if (plane != 0) {
					first_coeff = 1;
					// TODO LrWiener
				} else {
					first_coeff = 0;
				}
				for (u32 j = first_coeff; j < 3; ++j) {
					const i32 min = constants::wiener_taps_min[j];
					const i32 max = constants::wiener_taps_max[j];
					const u32 k = constants::wiener_taps_k[j];
					const i32 v = decoder.decode_signed_subexp_with_ref_bool(
						min, max + 1, k, slr.ref_lr_wiener[plane][pass][j]
					);
					// TODO LrWiener
					slr.ref_lr_wiener[plane][pass][j] = v;
				}
			}
		} else if (restoration_type == frame_restoration::sgrproj) {
			const u32 lr_sgr_set = decoder.read_literal(constants::sgrproj_params_bits);
			// TODO LrSgrSet
			for (u32 i = 0; i < 2; ++i) {
				const u32 radius = constants::sgr_params[lr_sgr_set][i * 2];
				const i32 min = constants::sgrproj_xqd_min[i];
				const i32 max = constants::sgrproj_xqd_max[i];
				i32 v;
				if (radius != 0) {
					v = decoder.decode_signed_subexp_with_ref_bool(
						min, max + 1, constants::sgrproj_prj_subexp_k, slr.ref_sgr_xqd[plane][i]
					);
				} else {
					v = 0;
					if (i == 1) {
						v = std::clamp((1 << constants::sgrproj_prj_bits) - slr.ref_sgr_xqd[plane][0], min, max);
					}
				}
				// TODO LrSgrXqd
				slr.ref_sgr_xqd[plane][i] = v;
			}
		}
	}
}
