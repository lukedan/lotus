#include "lotus/av1/decoder.h"

/// \file
/// Implementation of the AV1 decoder.

#include <fstream>

#include "lotus/logging.h"
#include "lotus/av1/functions.h"
#include "lotus/math/vector.h"
#include "lotus/av1/reader.h"

namespace lotus::av1 {
	/// Debug export image.
	static void _debug_export_image(
		const std::filesystem::path &file_name, const state::grid2<u16> &img, u32 bit_depth
	) {
		std::ofstream fout(file_name);
		fout << "P3\n" << img.get_width() << " " << img.get_height() << "\n";
		fout << (1 << bit_depth) - 1 << "\n";
		for (u32 y = 0; y < img.get_height(); ++y) {
			for (u32 x = 0; x < img.get_width(); ++x) {
				const u16 value = img(y, x);
				fout << value << " " << value << " " << value << "\n";
			}
		}
	}
	/// Debug export all images.
	static void _debug_export_all_images(const obu::sequence_header &seq_header, const state::block &sb) {
		_debug_export_image("test_y.ppm", sb.curr_frame[0], seq_header.color_config.bit_depth);
		_debug_export_image("test_u.ppm", sb.curr_frame[1], seq_header.color_config.bit_depth);
		_debug_export_image("test_v.ppm", sb.curr_frame[2], seq_header.color_config.bit_depth);
	}

	void decoder::process_bitstream(reader &r, u64 sz) {
		for (u64 total_size = 0; total_size < sz; ) {
			const u64 temporal_unit_size = r.read_leb128().first;
			process_temporal_unit(r, temporal_unit_size);
			total_size += temporal_unit_size;
		}
	}

	void decoder::process_open_bitstream_unit(reader &r, u64 sz) {
		log().debug("Processing OBU");
		const obu::header header = r.read_header();
		u64 obu_size;
		if (header.has_size_field) {
			obu_size = r.read_leb128().first;
		} else {
			obu_size = sz - 1 - (header.extension_flag ? 1 : 0);
		}
		const u64 start_position = r.get_position();
		// TODO drop_obu if necessary
		if (header.obu_type == obu::type::sequence_header) {
			log().debug("Read sequence header OBU");
			_seq_header = r.read_sequence_header();
			_sb = state::block::allocate(cvec2u32(_seq_header.max_frame_width, _seq_header.max_frame_height));
		} else if (header.obu_type == obu::type::temporal_delimiter) {
			std::abort(); // TODO
		} else if (header.obu_type == obu::type::frame_header) {
			process_frame_header_obu(header, r);
		} else if (header.obu_type == obu::type::redundant_frame_header) {
			process_frame_header_obu(header, r);
		} else if (header.obu_type == obu::type::tile_group) {
			r.read_tile_group(_seq_header, _frame_header, _cdf.non_coeff, _cdf.coeff, _sb, obu_size);
			_debug_export_image("test.ppm");
		} else if (header.obu_type == obu::type::metadata) {
			std::abort(); // TODO
		} else if (header.obu_type == obu::type::frame) {
			process_frame_obu(header, r, obu_size);
		} else if (header.obu_type == obu::type::tile_list) {
			std::abort(); // TODO
		}
		// TODO
		const u64 current_position = r.get_position();
		const u64 payload_bits = current_position - start_position;
		if (
			obu_size > 0 &&
			header.obu_type != obu::type::tile_group &&
			header.obu_type != obu::type::tile_list &&
			header.obu_type != obu::type::frame
		) {
			r.read_trailing_bits(obu_size * 8 - payload_bits);
		}
	}

	void decoder::process_frame_header_obu(const obu::header &header, reader &r) {
		log().debug("Processing frame header OBU");
		if (_sb.seen_frame_header) {
			// TODO frame_header_copy
		} else {
			_sb.seen_frame_header = true;
			_frame_header = r.read_uncompressed_header(header, _seq_header, _sb, _cdf);
			if (_frame_header.show_existing_frame) {
				// TODO deodde_frame_wrapup
				_sb.seen_frame_header = false;
			} else {
				// TODO TileNum
				_sb.seen_frame_header = true;
			}
		}
	}

	void decoder::process_frame_obu(const obu::header &header, reader &r, u64 sz) {
		log().debug("Processing frame OBU");
		const u64 start_bit_pos = r.get_position();
		process_frame_header_obu(header, r);
		r.read_byte_alignment();
		const u64 end_bit_pos = r.get_position();
		const u64 header_bytes = (end_bit_pos - start_bit_pos) / 8;
		sz -= header_bytes;
		r.read_tile_group(_seq_header, _frame_header, _cdf.non_coeff, _cdf.coeff, _sb, sz);
		_debug_export_image("test.ppm");
	}

	void decoder::process_temporal_unit(reader &r, u64 sz) {
		while (sz > 0) {
			const auto [frame_unit_size, leb128_bytes] = r.read_leb128();
			sz -= leb128_bytes;
			process_frame_unit(r, frame_unit_size);
			sz -= frame_unit_size;
		}
	}

	void decoder::process_frame_unit(reader &r, u64 sz) {
		while (sz > 0) {
			const auto [obu_length, leb128_bytes] = r.read_leb128();
			sz -= leb128_bytes;
			process_open_bitstream_unit(r, obu_length);
			sz -= obu_length;
		}
	}

	cvec3u32 decoder::get_sample(cvec2u32 pos) const {
		const u32 y = _sb.curr_frame[0](pos[1], pos[0]);

		const u32 sub_x = _seq_header.color_config.subsampling_x ? 1 : 0;
		const u32 uv_x = pos[0];
		u32 sub_y = _seq_header.color_config.subsampling_y ? 1 : 0;
		u32 uv_y = pos[1];
		if (
			_seq_header.color_config.subsampling_y &&
			_seq_header.color_config.chroma_sample_position == chroma_sample_position::vertical
		) {
			++sub_y;
			uv_y = std::max(uv_y << 1, 1u) - 1;
		}

		const u32 coord_x = uv_x >> sub_x;
		const u32 coord_y = uv_y >> sub_y;
		const u32 off_x = uv_x & ((1u << sub_x) - 1);
		const u32 off_y = uv_y & ((1u << sub_y) - 1);

		const auto sample_y = [&](const state::grid2<u16> &img, u32 x) {
			const u32 v0 = img(coord_y, x);
			if (off_y == 0) {
				return v0;
			}
			const u32 v1 = img(coord_y + 1, x);
			const u32 v = v0 * ((1u << sub_y) - off_y) + v1 * off_y;
			return functions::round2(v, sub_y);
		};

		u32 u = sample_y(_sb.curr_frame[1], coord_x);
		u32 v = sample_y(_sb.curr_frame[2], coord_x);
		if (off_x != 0) {
			u = functions::round2(u + sample_y(_sb.curr_frame[1], coord_x + 1), 1);
			v = functions::round2(v + sample_y(_sb.curr_frame[2], coord_x + 1), 1);
		}
		return cvec3u32(y, u, v);
	}

	void decoder::_debug_export_image(const std::filesystem::path &file_name) const {
		const u32 w = _sb.curr_frame[0].get_width();
		const u32 h = _sb.curr_frame[0].get_height();
		const u32 bit_depth = _seq_header.color_config.bit_depth;

		std::ofstream fout(file_name);
		fout << "P3\n" << w << " " << h << "\n";
		fout << 255 << "\n";
		for (u32 y = 0; y < h; ++y) {
			for (u32 x = 0; x < w; ++x) {
				const cvec3u32 yuv = get_sample({ x, y });
				const cvec3f32 yuv_norm = (yuv.into<float>() / ((1u << bit_depth) - 1)) - cvec3f32(0.0f, 0.5f, 0.5f);
				const cvec3f32 rgb_norm = mat33f32({
					{ 1.0f,      0.0f,  1.13983f },
					{ 1.0f, -0.39465f, -0.58060f },
					{ 1.0f,  2.03211f,      0.0f }
				}) * yuv_norm;
				const cvec3u32 rgb = (rgb_norm * 255.0f + cvec3f32::filled(0.5f)).into<u32>();
				fout << rgb[0] << " " << rgb[1] << " " << rgb[2] << "\n";
			}
		}
	}
}
