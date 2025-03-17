#pragma once

/// \file
/// Mathematical sequences.

#include <cstdint>

#include "vector.h"

/// Mathematical sequences.
namespace lotus::sequences {
	/// The Halton sequence.
	template <typename F = float, u32 NumBits = 32> struct halton {
	public:
		/// Initializes a new Halton sequence.
		[[nodiscard]] static constexpr halton create(u32 base) {
			return halton(base);
		}

		/// Evaluates the specified element in the sequence.
		[[nodiscard]] constexpr F operator()(u32 index) const {
			F result = 0;
			u32 i = 0;
			while (index > 0) {
				result += _coefficients[i] * (index % _base);
				index /= _base;
				++i;
			}
			return result;
		}
	private:
		/// Initializes \ref _coefficients.
		constexpr explicit halton(u32 base) : _base(base) {
			F rcp = F(1) / _base;
			F cur = rcp;
			for (u32 i = 0; i < NumBits; ++i, cur *= rcp) {
				_coefficients[i] = cur;
			}
		}

		std::array<F, NumBits> _coefficients; ///< One over \ref _base to the power of <cc>i + 1</cc>.
		u32 _base = 0; ///< The base of this sequence.
	};

	/// The Hammersley sequence.
	template <typename F, u32 NumBits = 32> struct hammersley {
	public:
		/// Creates a new Hammersley sequence.
		[[nodiscard]] static constexpr hammersley create() {
			return hammersley();
		}

		/// Evaluates the specified element in the sequence.
		[[nodiscard]] constexpr cvec2<F> operator()(
			u32 num_bits, u32 index, u32 t = 32
		) const {
			cvec2<F> result = zero;
			crash_if((index & ~((1u << num_bits) - 1u)) != 0);
			for (u32 i = 0; i < std::min(t, num_bits); ++i) {
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
			for (u32 i = 0; i < NumBits; ++i) {
				_coefficients[i] = static_cast<F>(1) / (2u << i);
			}
		}

		std::array<F, NumBits> _coefficients; ///< One over two to the power of <cc>i + 1</cc>.
	};
}
