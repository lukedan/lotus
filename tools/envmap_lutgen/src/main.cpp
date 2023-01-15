#include <cmath>
#include <fstream>

#include <lotus/math/vector.h>
#include <lotus/math/constants.h>
#include <lotus/math/sequences.h>
#include <lotus/utils/custom_float.h>
#include <lotus/utils/dds.h>
#include <lotus/logging.h>

using namespace lotus::matrix_types;
using namespace lotus::vector_types;
using lotus::vec;

template <typename T> [[nodiscard]] constexpr T squared(T x) {
	return x * x;
}

template <typename T> [[nodiscard]] constexpr T saturate(T v) {
	return std::clamp<T>(v, 0, 1);
}

namespace trowbridge_reitz {
	[[nodiscard]] double g2_smith(double n_dot_l, double n_dot_v, double alpha) {
		double a2 = squared(alpha);
		return
			2.0 * n_dot_l * n_dot_v / (
				n_dot_v * std::sqrt(a2 + (1.0 - a2) * squared(n_dot_l)) +
				n_dot_l * std::sqrt(a2 + (1.0 - a2) * squared(n_dot_v))
			);
	}

	[[nodiscard]] cvec2d importance_sample_d(double xi, double alpha) {
		double denom = xi * (squared(alpha) - 1.0) + 1.0;
		return cvec2d(
			std::sqrt((1.0 - xi) / denom),
			squared(denom) / (lotus::pi_f * squared(alpha))
		);
	}
}

cvec2d integrate_brdf(double roughness, double n_dot_v, std::uint32_t seq_bits) {
	n_dot_v = std::max(0.0001, n_dot_v);
	std::uint32_t num_samples = 1 << seq_bits;
	double alpha = squared(roughness);
	cvec3d v(std::sqrt(1.0 - squared(n_dot_v)), 0.0, n_dot_v);

	double a = 0.0;
	double b = 0.0;

	auto seq = lotus::sequences::hammersley<double>::create();
	for (std::uint32_t i = 0; i < num_samples; ++i) {
		cvec2d xi = seq(seq_bits, i);
		double n_dot_h = trowbridge_reitz::importance_sample_d(xi[0], alpha)[0];
		double phi = xi[1] * 2.0 * lotus::pi_f;
		double sin_theta = std::sqrt(1.0 - squared(n_dot_h));
		cvec3d h(sin_theta * std::cos(phi), sin_theta * std::sin(phi), n_dot_h);
		double v_dot_h = vec::dot(v, h);
		cvec3d l = 2.0 * v_dot_h * h - v;
		if (l[2] > 0.0) {
			double g = trowbridge_reitz::g2_smith(saturate(l[2]), n_dot_v, alpha);
			double g_vis = g * saturate(v_dot_h) / (n_dot_h * n_dot_v);
			double fc = 1.0 - saturate(v_dot_h);
			fc = squared(squared(fc)) * fc;
			a += (1.0 - fc) * g_vis;
			b += fc * g_vis;
		}
	}

	return cvec2d(a, b) / num_samples;
}

int main() {
	std::uint32_t samples_n_dot_v = 256;
	std::uint32_t samples_roughness = 256;
	std::uint32_t seq_bits = 10;

	std::ofstream fout("envmap_lut.dds", std::ios::binary);
	auto write_binary = [&fout]<typename T>(const T &x) {
		fout.write(reinterpret_cast<const char*>(&x), sizeof(T));
	};

	{
		lotus::dds::header header;
		header.size                       = sizeof(header);
		header.flags                      = lotus::dds::header_flags::required_flags;
		header.height                     = samples_n_dot_v;
		header.width                      = samples_roughness;
		header.pitch_or_linear_size       = sizeof(std::uint16_t) * header.width;
		header.depth                      = 1;
		header.mipmap_count               = 1;
		header.pixel_format.size          = sizeof(header.pixel_format);
		header.pixel_format.flags         =
			lotus::dds::pixel_format_flags::four_cc | lotus::dds::pixel_format_flags::rgb;
		header.pixel_format.four_cc       = lotus::make_four_character_code(u8"DX10");
		header.pixel_format.rgb_bit_count = 32;
		header.pixel_format.r_bit_mask    = 0x0000FFFFu; // is this correct? probably doesn't matter
		header.pixel_format.g_bit_mask    = 0xFFFF0000u;
		header.pixel_format.b_bit_mask    = 0;
		header.pixel_format.a_bit_mask    = 0;
		header.caps                       = lotus::dds::capabilities::texture;
		header.caps2                      = lotus::dds::capabilities2::none;

		lotus::dds::header_dx10 header_dx10;
		header_dx10.dxgi_format = 35; // DXGI_FORMAT_R16G16_UNORM
		header_dx10.dimension   = lotus::dds::resource_dimension::texture2d;
		header_dx10.flags       = lotus::dds::miscellaneous_flags::none;
		header_dx10.array_size  = 1;
		header_dx10.flags2      = lotus::dds::miscellaneous_flags2::alpha_mode_unknown;

		write_binary(lotus::dds::magic);
		write_binary(header);
		write_binary(header_dx10);
	}

	for (std::uint32_t i_n_dot_v = 0; i_n_dot_v < samples_n_dot_v; ++i_n_dot_v) {
		float n_dot_v = i_n_dot_v / static_cast<float>(samples_n_dot_v - 1);
		for (std::uint32_t i_roughness = 0; i_roughness < samples_roughness; ++i_roughness) {
			float roughness = i_roughness / static_cast<float>(samples_roughness);

			cvec2d values = integrate_brdf(roughness, n_dot_v, seq_bits);
			cvec2<std::uint16_t> values_f16 = lotus::vec::memberwise_operation([](double x) {
				lotus::crash_if(!std::isfinite(x));
				return static_cast<std::uint16_t>(std::clamp<double>(
					std::round(x * std::numeric_limits<std::uint16_t>::max()),
					0.0,
					std::numeric_limits<std::uint16_t>::max()
				));
			}, values);
			write_binary(values_f16);
		}
		lotus::log().debug<u8"Finished {} / {}">(i_n_dot_v + 1, samples_n_dot_v);
	}

	return 0;
}
