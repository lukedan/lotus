#pragma once

/// \file
/// Reader for AV1 files.

#include "lotus/utils/static_function.h"

#include "lotus/av1/obu.h"
#include "lotus/av1/state.h"

namespace lotus::av1 {
	struct symbol_decoder;

	/// Reader containing convenience functions for reading OBUs.
	struct reader {
	public:
		/// Initializes this reader using the given callback.
		explicit reader(static_function<std::byte()> consume_byte) : _consume_byte(std::move(consume_byte)) {
		}

		/// Reads the next byte.
		[[nodiscard]] u8 read_byte();
		/// Reads the next bit.
		[[nodiscard]] bool read_bit();
		/// Reads a number of bits.
		[[nodiscard]] u32 read_bits(u32 n);
		/// \overload
		template <u32 NumBits, typename T = minimum_unsigned_type_bits_t<NumBits>> [[nodiscard]] T read_bits() {
			static_assert(sizeof(T) * 8 >= NumBits, "Insufficient number of bits");
			return static_cast<T>(read_bits(NumBits));
		}

		/// \p get_position().
		[[nodiscard]] u64 get_position() const;

		/// 4.10.3. uvlc()
		[[nodiscard]] u32 read_uvlc();
		/// 4.10.4. le(n)
		[[nodiscard]] u64 read_le(u32);
		/// 4.10.5. leb128()
		///
		/// \return { Value, \p Leb128Bytes }.
		[[nodiscard]] std::pair<u64, u32> read_leb128();
		/// 4.10.6. su(n)
		[[nodiscard]] i32 read_su(u32);
		/// 4.10.7. ns(n)
		[[nodiscard]] u32 read_ns(u32);

		/// 5.3.2. OBU header syntax
		[[nodiscard]] obu::header read_header();
		/// 5.3.4. Trailing bits syntax
		void read_trailing_bits(u64 nb_bits);
		/// 5.3.5. Byte alignment syntax
		void read_byte_alignment();

		/// 5.5.1. General sequence header OBU syntax
		[[nodiscard]] obu::sequence_header read_sequence_header();
		/// 5.5.2. Color config syntax
		[[nodiscard]] obu::color_config read_color_config(u8 seq_profile);
		/// 5.5.3. Timing info syntax
		[[nodiscard]] obu::timing_info read_timing_info();
		/// 5.5.4. Decoder model info syntax
		[[nodiscard]] obu::decoder_model_info read_decoder_model_info();
		/// 5.5.5. Operating parameters info syntax
		[[nodiscard]] obu::operating_parameters_info read_operating_parameters_info(u32 buffer_delay_length);

		/// 5.9.2. Uncompressed header syntax
		[[nodiscard]] obu::uncompressed_header read_uncompressed_header(
			const obu::header&, const obu::sequence_header&, state::block&, state::cdf&
		);
		/// 5.9.5. Frame size syntax
		[[nodiscard]] obu::frame_size read_frame_size(const obu::sequence_header&, bool frame_size_override);
		/// 5.9.6. Render size syntax
		void read_render_size(obu::frame_size&);
		/// 5.9.8. Superres params syntax
		void read_superres_params(obu::frame_size&, bool enable_superres);
		/// 5.9.10. Interpolation filter syntax
		[[nodiscard]] interpolation read_interpolation_filter();
		/// 5.9.11. Loop filter params syntax
		[[nodiscard]] obu::loop_filter_params read_loop_filter_params(
			const obu::sequence_header&, const obu::uncompressed_header&
		);
		/// 5.9.12. Quantization params syntax
		[[nodiscard]] obu::quantization_params read_quantization_params(const obu::color_config&);
		/// 5.9.13. Delta quantizer syntax
		[[nodiscard]] obu::delta_q read_delta_q();
		/// 5.9.14. Segmentation params syntax
		[[nodiscard]] obu::segmentation_params read_segmentation_params(const obu::uncompressed_header&);
		/// 5.9.15. Tile info syntax
		[[nodiscard]] obu::tile_info read_tile_info(
			const obu::sequence_header&, const obu::uncompressed_header&, state::block&
		);
		/// 5.9.17. Quantizer index delta parameters syntax
		[[nodiscard]] obu::delta_q_params read_delta_q_params(u8 base_q_idx);
		/// 5.9.18. Loop filter delta parameters syntax
		[[nodiscard]] obu::delta_lf_params read_delta_lf_params(bool delta_q_present, bool allow_intrabc);
		/// 5.9.19. CDEF params syntax
		[[nodiscard]] obu::cdef_params read_cdef_params(
			const obu::sequence_header&, const obu::uncompressed_header&
		);
		/// 5.9.20. Loop restoration params syntax
		[[nodiscard]] obu::lr_params read_lr_params(const obu::sequence_header&, const obu::uncompressed_header&);
		/// 5.9.21. TX mode syntax
		[[nodiscard]] tx_mode read_tx_mode(bool coded_lossless);
		/// 5.9.24. Global motion params syntax
		[[nodiscard]] obu::global_motion_params read_global_motion_params(const obu::uncompressed_header&);
		/// 5.9.25. Global param syntax
		[[nodiscard]] i32 read_global_param(
			warp_model type, u32 idx, i32 prev_gm_params, bool allow_high_precision_mv
		);
		/// 5.9.26. Decode signed subexp with ref syntax
		[[nodiscard]] i32 decode_signed_subexp_with_ref(i32 low, i32 high, i32 r);
		/// 5.9.27. Decode unsigned subexp with ref syntax
		[[nodiscard]] u32 decode_unsigned_subexp_with_ref(u32 mx, i32 r);
		/// 5.9.28. Decode subexp syntax
		[[nodiscard]] u32 decode_subexp(u32 num_syms);
		/// 5.9.30. Film grain params syntax
		[[nodiscard]] obu::film_grain_params read_film_grain_params(
			const obu::sequence_header&, const obu::uncompressed_header&
		);

		/// 5.11.1. General tile group OBU syntax
		void read_tile_group(
			const obu::sequence_header&,
			const obu::uncompressed_header&,
			const cdf::non_coeff&,
			const cdf::coeff&,
			state::block&,
			u64 sz
		);
		/// 5.11.2. Decode tile syntax
		void decode_tile(
			const obu::sequence_header&,
			const obu::uncompressed_header&,
			symbol_decoder&,
			state::block&,
			const state::block_range&
		);
		/// 5.11.4. Decode partition syntax
		void decode_partition(
			const obu::sequence_header&,
			const obu::uncompressed_header&,
			symbol_decoder&,
			state::block&,
			const state::block_range&,
			u32 r,
			u32 c,
			block_size b_size
		);
		/// 5.11.5. Decode block syntax
		void decode_block(
			const obu::sequence_header&,
			const obu::uncompressed_header&,
			symbol_decoder&,
			state::block&,
			const state::block_range&,
			u32 r,
			u32 c,
			block_size sub_size
		);
		/// 5.11.6. Mode info syntax
		[[nodiscard]] obu::mode_info read_mode_info(
			const obu::sequence_header&,
			const obu::uncompressed_header&,
			symbol_decoder&,
			state::block&,
			const state::block_range&,
			const state::block_decoding&
		);
		/// 5.11.7. Intra frame mode info syntax
		[[nodiscard]] obu::mode_info read_intra_frame_mode_info(
			const obu::sequence_header&,
			const obu::uncompressed_header&,
			symbol_decoder&,
			state::block&,
			const state::block_range&,
			const state::block_decoding&
		);
		/// 5.11.8. Intra segment ID syntax
		[[nodiscard]] segment_id_t read_intra_segment_id(
			const obu::uncompressed_header&,
			obu::mode_info&,
			symbol_decoder&,
			const state::block&,
			const state::block_decoding&
		);
		/// 5.11.9. Read segment ID syntax
		[[nodiscard]] segment_id_t read_segment_id(
			const obu::uncompressed_header&,
			symbol_decoder&,
			const state::block&,
			const state::block_decoding&,
			bool skip
		);
		/// 5.11.10. Skip mode syntax
		bool read_skip_mode(
			const obu::uncompressed_header&,
			const obu::mode_info&,
			symbol_decoder&,
			const state::block&,
			const state::block_decoding&
		);
		/// 5.11.11. Skip syntax
		[[nodiscard]] bool read_skip(
			const obu::uncompressed_header&,
			const obu::mode_info&,
			symbol_decoder&,
			const state::block&,
			const state::block_decoding&
		);
		/// 5.11.12. Quantizer index delta syntax
		void read_delta_qindex(
			const obu::sequence_header&,
			const obu::uncompressed_header&,
			const obu::mode_info&,
			symbol_decoder&,
			state::block&,
			const state::block_decoding&
		);
		/// 5.11.13. Loop filter delta syntax
		void read_delta_lf(
			const obu::sequence_header&,
			const obu::uncompressed_header&,
			const obu::mode_info&,
			symbol_decoder&,
			state::block&,
			const state::block_decoding&
		);
		/// 5.11.15. TX size syntax
		[[nodiscard]] tx_size read_tx_size(
			const obu::uncompressed_header&,
			const obu::mode_info&,
			symbol_decoder&,
			const state::block&,
			const state::block_decoding&,
			bool allow_select
		);
		/// 5.11.16. Block TX size syntax
		void read_block_tx_size(
			const obu::uncompressed_header&,
			const obu::mode_info&,
			symbol_decoder&,
			state::block&,
			const state::block_decoding&
		);
		/// 5.11.17. Var TX size syntax
		void read_var_tx_size(
			const obu::uncompressed_header&,
			symbol_decoder&,
			state::block&,
			const state::block_decoding&,
			u32 row, u32 col, tx_size, u32 depth
		);
		/// 5.11.18. Inter frame mode info syntax
		[[nodiscard]] obu::mode_info read_inter_frame_mode_info(
			const obu::sequence_header&,
			const obu::uncompressed_header&,
			symbol_decoder&,
			state::block&,
			const state::block_range&,
			const state::block_decoding&
		);
		/// 5.11.19. Inter segment ID syntax
		[[nodiscard]] segment_id_t read_inter_segment_id(
			const obu::uncompressed_header&,
			const obu::mode_info&,
			symbol_decoder&,
			state::block&,
			const state::block_decoding&,
			bool pre_skip
		);
		/// 5.11.20. Is inter syntax
		[[nodiscard]] bool read_is_inter(
			const obu::uncompressed_header&,
			const obu::mode_info&,
			symbol_decoder&,
			const state::block_decoding&
		);
		/// 5.11.22. Intra block mode info syntax
		void read_intra_block_mode_info(
			const obu::sequence_header&,
			const obu::uncompressed_header&,
			obu::mode_info&,
			symbol_decoder&,
			const state::block&,
			const state::block_decoding&
		);
		/// 5.11.23. Inter block mode info syntax
		void read_inter_block_mode_info(
			const obu::sequence_header&,
			const obu::uncompressed_header&,
			obu::mode_info&,
			symbol_decoder&,
			const state::block&,
			const state::block_range&,
			const state::block_decoding&
		);
		/// 5.11.24. Filter intra mode info syntax
		void read_filter_intra_mode_info(
			const obu::sequence_header&,
			obu::mode_info&,
			symbol_decoder&,
			const state::block_decoding&
		);
		/// 5.11.26. Assign MV syntax
		void assign_mv(
			const obu::sequence_header&,
			const obu::uncompressed_header&,
			const obu::mode_info&,
			symbol_decoder&,
			const state::block_range&,
			const state::block_decoding&,
			state::motion_vector&,
			bool is_compound
		);
		/// 5.11.28. Read inter intra syntax
		void read_interintra_mode(
			const obu::sequence_header&,
			obu::mode_info&,
			symbol_decoder&,
			const state::block_decoding&,
			bool is_compound
		);
		/// 5.11.31. MV syntax
		void read_mv(
			const obu::uncompressed_header&,
			const obu::mode_info&,
			symbol_decoder&,
			state::motion_vector&,
			u32 ref
		);
		/// 5.11.32. MV component syntax
		[[nodiscard]] i32 read_mv_component(const obu::uncompressed_header&, symbol_decoder&, u32 mv_ctx, u32 comp);
		/// 5.11.34. Residual syntax
		void read_residual(
			const obu::sequence_header&,
			const obu::uncompressed_header&,
			const obu::mode_info&,
			symbol_decoder&,
			state::block&,
			const state::block_decoding&,
			const state::color_map&
		);
		/// 5.11.35. Transform block syntax
		void read_transform_block(
			const obu::sequence_header&,
			const obu::uncompressed_header&,
			const obu::mode_info&,
			symbol_decoder&,
			state::block&,
			const state::block_decoding&,
			const state::color_map&,
			u32 plane, u32 base_x, u32 base_y, tx_size tx_sz, u32 x, u32 y
		);
		/// 5.11.36. Transform tree syntax
		void read_transform_tree(
			const obu::sequence_header&,
			const obu::uncompressed_header&,
			const obu::mode_info&,
			symbol_decoder&,
			state::block&,
			const state::block_decoding&,
			const state::color_map&,
			u32 start_x, u32 start_y, u32 w, u32 h
		);
		/// 5.11.39. Coefficients syntax
		[[nodiscard]] u32 read_coeffs(
			const obu::sequence_header&,
			const obu::uncompressed_header&,
			const obu::mode_info&,
			symbol_decoder&,
			state::block&,
			const state::block_decoding&,
			state::coeffs&,
			u32 plane, u32 start_x, u32 start_y, tx_size tx_sz
		);
		/// 5.11.42. Intra angle info luma syntax
		[[nodiscard]] i8 read_intra_angle_info_y(
			const obu::mode_info&,
			symbol_decoder&,
			const state::block_decoding&
		);
		/// 5.11.43. Intra angle info chroma syntax
		[[nodiscard]] i8 read_intra_angle_info_uv(
			const obu::mode_info&,
			symbol_decoder&,
			const state::block_decoding&
		);
		/// 5.11.45. Read CFL alphas syntax
		[[nodiscard]] obu::cfl_alphas read_cfl_alphas(symbol_decoder&);
		/// 5.11.46. Palette mode info syntax
		void read_palette_mode_info(
			const obu::sequence_header&,
			obu::mode_info&,
			symbol_decoder&,
			const state::block&,
			const state::block_decoding&
		);
		/// 5.11.47. Transform type syntax
		void read_transform_type(
			const obu::uncompressed_header&,
			const obu::mode_info&,
			symbol_decoder&,
			state::block&,
			u32 x4, u32 y4, tx_size tx_sz
		);
		/// 5.11.49. Palette tokens syntax
		[[nodiscard]] state::color_map read_palette_tokens(
			const obu::sequence_header&,
			const obu::uncompressed_header&,
			const obu::mode_info&,
			symbol_decoder&,
			const state::block_decoding&);
		/// 5.11.56. Read CDEF syntax
		void read_cdef(
			const obu::sequence_header&,
			const obu::uncompressed_header&,
			const obu::mode_info&,
			symbol_decoder&,
			state::block&,
			const state::block_decoding&
		);
		/// 5.11.57. Read loop restoration syntax
		void read_lr(
			const obu::sequence_header&,
			const obu::uncompressed_header&,
			symbol_decoder&,
			state::ref_lr&,
			u32 r, u32 c, block_size b_size
		);
		/// 5.11.58. Read loop restoration unit syntax
		void read_lr_unit(
			const obu::uncompressed_header&, symbol_decoder&, state::ref_lr&, u32 plane, u32 unit_row, u32 unit_col
		);
	private:
		static_function<std::byte()> _consume_byte; ///< Callback function that consumes a single byte.
		u64 _byte_position = 0; ///< Current byte position.
		u32 _residual_bits = 0; ///< Number of remaining bits in \ref _cur_byte.
		u8 _cur_byte = 0; ///< Remaining bits in the current byte.

		/// Asserts that the current position is byte aligned.
		void _check_byte_aligned() {
			crash_if(_residual_bits > 0);
		}
	};
}
