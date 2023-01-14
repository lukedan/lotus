#pragma once

/// \file
/// Mathematical sequences.

#include <cstdint>

#include "vector.h"

/// Mathematical sequences.
namespace lotus::sequences {
	/// The Halton sequence.
	template <typename F = float, std::uint32_t NumBits = 32> struct halton {
	public:
		/// Initializes a new Halton sequence.
		[[nodiscard]] static constexpr halton create(std::uint32_t base) {
			return halton(base);
		}

		/// Evaluates the specified element in the sequence.
		[[nodiscard]] constexpr F operator()(std::uint32_t index) const {
			F result = 0;
			std::uint32_t i = 0;
			while (index > 0) {
				result += _coefficients[i] * (index % _base);
				index /= _base;
				++i;
			}
			return result;
		}
	private:
		/// Initializes \ref _coefficients.
		constexpr explicit halton(std::uint32_t base) : _base(base) {
			F rcp = F(1) / _base;
			F cur = rcp;
			for (std::uint32_t i = 0; i < NumBits; ++i, cur *= rcp) {
				_coefficients[i] = cur;
			}
		}

		std::array<F, NumBits> _coefficients; ///< One over \ref _base to the power of <cc>i + 1</cc>.
		std::uint32_t _base = 0; ///< The base of this sequence.
	};

	/// The Hammersley sequence.
	template <typename F, std::uint32_t NumBits = 32> struct hammersley {
	public:
		/// Creates a new Hammersley sequence.
		[[nodiscard]] static constexpr hammersley create() {
			return hammersley();
		}

		/// Evaluates the specified element in the sequence.
		[[nodiscard]] constexpr cvec2<F> operator()(
			std::uint32_t num_bits, std::uint32_t index, std::uint32_t t = 32
		) const {
			cvec2<F> result = zero;
			crash_if_constexpr((index & ~((1u << num_bits) - 1u)) != 0);
			for (std::uint32_t i = 0; i < std::min(t, num_bits); ++i) {
				if (index & (1 << i)) {
					result[0] += _coefficients[i];
				}
				if (index & (1u << (num_bits - i - 1))) {
					result[1] += _coefficients[i];
				}
			}
			return result;
		}
	private:
		/// Initializes \ref _coefficients.
		constexpr hammersley() {
			for (std::uint32_t i = 0; i < NumBits; ++i) {
				_coefficients[i] = static_cast<F>(1) / (2u << i);
			}
		}

		std::array<F, NumBits> _coefficients; ///< One over two to the power of <cc>i + 1</cc>.
	};
}
