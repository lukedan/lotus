#pragma once

/// \file
/// 16-bit floating point utilities.

#include <cstdint>
#include <cassert>
#include <algorithm>
#include <type_traits>
#include <bit>

#include "lotus/common.h"

namespace lotus {
	namespace _details {
		/// Safely shifts the given number to the left, returning 0 when the number of bits is too large.
		template <u64 Bits, typename T> [[nodiscard]] inline constexpr T shl_safe(T x) {
			if constexpr (Bits >= sizeof(T) * 8) {
				return static_cast<T>(0);
			} else {
				return x << Bits;
			}
		}
		/// Safely shifts the given number to the left, returning 0 when the number of bits is too large.
		template <u64 Bits, typename T> [[nodiscard]] inline constexpr T shr_safe(T x) {
			if constexpr (Bits >= sizeof(T) * 8) {
				return static_cast<T>(0);
			} else {
				return x >> Bits;
			}
		}

		/// Adjusts the position of bit \p From to bit \p To in the given value by shifting the value.
		template <
			u64 From, u64 To, typename ToType, typename T
		> [[nodiscard]] inline constexpr ToType adjust_position(T value) {
			static_assert(
				std::is_signed_v<ToType> == std::is_signed_v<T>,
				"This functions require signedness to be the same between input and output"
			);
			if constexpr (From > To) {
				return static_cast<ToType>(shr_safe<From - To>(value));
			} else {
				return shl_safe<To - From>(static_cast<ToType>(value));
			}
		}
	}

	namespace custom_float {
		/// Rounding mode for \ref custom_float.
		enum class rounding_mode {
			towards_zero, ///< Rounds towards zero. Fastest rounding mode.

			num_enumerators ///< Number of enumerators.
		};
		/// Underflow mode for \ref custom_float.
		enum class underflow_mode {
			to_denorm,  ///< Convert to a denormalized number.
			round,      ///< Rounds up to the smallest non-denorm value or down to zero, depending on the value.
			round_down, ///< Rounds down to zero.
			undefined,  ///< Does not care, results in undefined values.

			num_enumerators ///< Number of enumerators.
		};

		/// Settings for bit width conversions.
		template <
			rounding_mode RoundingMode,
			underflow_mode UnderflowMode,
			bool CheckDegenerate,
			bool CheckDenorm,
			bool CheckOverflow
		> struct basic_conversion_profile {
			constexpr static rounding_mode rounding   = RoundingMode;    ///< Rounding mode.
			constexpr static underflow_mode underflow = UnderflowMode;   ///< Underflow mode.
			constexpr static bool check_degenerate    = CheckDegenerate; ///< Whether to check for NaN and infinity.
			constexpr static bool check_denorm        = CheckDenorm;     ///< Whether to handle denormalized values.
			constexpr static bool check_overflow      = CheckOverflow;   ///< Whether to handle overflow.
		};
		/// Correctly handles all values.
		template <rounding_mode RoundingMode> using conversion_profile_full =
			basic_conversion_profile<RoundingMode, underflow_mode::to_denorm, true, true, true>;
		/// Fastest profile that handles all values. Does not always try to preserve the value.
		using conversion_profile_safe_fastest =
			basic_conversion_profile<rounding_mode::towards_zero, underflow_mode::round_down, true, true, true>;
		/// Fastest profile that does not handle any special values.
		using conversion_profile_fastest =
			basic_conversion_profile<rounding_mode::towards_zero, underflow_mode::undefined, false, false, false>;
	}
	/// A custom IEEE 754 floating-point number.
	template <
		u32 ExponentBits,
		u32 MantissaBits,
		typename Storage
	> struct basic_custom_float {
		template <u32, u32, typename> friend struct basic_custom_float;
		static_assert(
			std::is_unsigned_v<Storage> && ExponentBits + MantissaBits + 1 <= sizeof(Storage) * 8,
			"Not enough storage space"
		);
	public:
		constexpr static u32 exponent_bits = ExponentBits; ///< Number of bits used to store the exponent.
		constexpr static u32 mantissa_bits = MantissaBits; ///< Number of bits used to store the mantissa.
		using storage_type = Storage; ///< Type for storing the binary representation of this number.

		/// Initializes this value to zero. This is for compatibility with matrix types.
		constexpr basic_custom_float() : _binary(zero) {
		}
		/// Initializes this number to zero.
		constexpr basic_custom_float(zero_t) : _binary(zero) {
		}
		/// Returns positive infinity.
		[[nodiscard]] inline static constexpr basic_custom_float infinity() {
			return _make(false, _exponent_mask);
		}

		/// Negates the value by inverting the sign bit.
		[[nodiscard]] constexpr basic_custom_float operator-() const {
			return basic_custom_float(_binary ^ _sign_mask);
		}

		/// Reinterprets the given built-in floating point type as a \ref basic_custom_float.
		template <typename T> [[nodiscard]] inline static constexpr std::enable_if_t<
			sizeof(Storage) == sizeof(T) && (
				(ExponentBits == 8 && MantissaBits == 23 && std::is_same_v<T, f32>) ||
				(ExponentBits == 11 && MantissaBits == 52 && std::is_same_v<T, f64>)
			),
			basic_custom_float
		> reinterpret(T value) {
			return basic_custom_float(std::bit_cast<Storage>(value));
		}
		/// Reinterprets the binary value as the given type.
		template <typename T> [[nodiscard]] constexpr std::enable_if_t<
			sizeof(Storage) == sizeof(T), T
		> reinterpret_as() const {
			return std::bit_cast<T>(_binary);
		}
		/// Converts this into a custom float type that is parameterized differently.
		template <
			typename F,
			typename Profile = custom_float::conversion_profile_full<custom_float::rounding_mode::towards_zero>
		> [[nodiscard]] constexpr F into() const {
			using _ufrom = Storage;
			using _uto = typename F::storage_type;
			constexpr u32 _mant_to = F::mantissa_bits;
			constexpr u32 _exp_to = F::exponent_bits;

			bool sign = _binary & _sign_mask;
			_ufrom signless = _binary & ~_sign_mask;
			if (signless == 0) { // fast path for 0
				return F::_make(sign, 0);
			}

			if constexpr (Profile::check_degenerate) {
				if (!is_finite()) {
					if (_binary & _mantissa_mask) {
						// preserve quietness, fill all other bits
						return F::_make(sign, (_binary & _quiet_mask) ? ~F::_zero : ~F::_quiet_mask);
					} else {
						return F::_make(sign, F::_exponent_mask);
					}
				}
			}

			if constexpr (ExponentBits > _exp_to) {
				constexpr auto _exponent_offset_unshifted = static_cast<_ufrom>(
					(_details::shl_safe<ExponentBits - 1>(_one) - 1) - (_details::shl_safe<_exp_to - 1>(_one) - 1)
				);
				constexpr auto _exponent_offset =
					static_cast<_ufrom>(_details::shl_safe<MantissaBits>(_exponent_offset_unshifted));

				if constexpr (Profile::check_denorm) {
					if (is_denorm()) {
						auto shifted = _details::adjust_position<
							MantissaBits + _exponent_offset_unshifted, _mant_to, _uto
						>(signless);
						return F::_make(sign, shifted);
					}
				}

				if constexpr (Profile::check_overflow) { // check for overflow
					constexpr auto _overflow_mask = static_cast<_ufrom>(
						_details::shl_safe<MantissaBits>(_details::shl_safe<_exp_to>(_one) - 1) + _exponent_offset
					);

					if (signless >= _overflow_mask) {
						if constexpr (Profile::rounding == custom_float::rounding_mode::towards_zero) {
							return F::_make(sign, F::_max_value);
						} else {
							return F::_make(sign, F::_exponent_mask);
						}
					}
				}

				if constexpr (Profile::underflow != custom_float::underflow_mode::undefined) {
					constexpr auto _underflow_mask =
						static_cast<_ufrom>(_details::shl_safe<MantissaBits>(_one) + _exponent_offset);

					if (signless < _underflow_mask) { // underflow
						if constexpr (Profile::underflow == custom_float::underflow_mode::round_down) {
							return zero;
						} else if constexpr (Profile::underflow == custom_float::underflow_mode::round) {
							return
								(_binary & _details::shl_safe<MantissaBits - 1>(_one)) ?
								F::_make(sign, _min_value_nondenorm) :
								F(zero);
						} else if constexpr (Profile::underflow == custom_float::underflow_mode::to_denorm) {
							auto full_mant =
								_details::adjust_position<MantissaBits, _mant_to - 1, _uto>(
									(_binary & _mantissa_mask) | _details::shl_safe<MantissaBits>(_one)
								);
							auto shr_bits = std::min(
								_exponent_offset_unshifted - _details::shr_safe<MantissaBits>(signless),
								static_cast<_ufrom>(sizeof(_uto) * 8 - 1)
							);
							auto new_mant = static_cast<_uto>(full_mant >> shr_bits);
							return F::_make(sign, F::_zero, new_mant);
						} else {
							static_assert(sizeof(F*) == 0, "Unhandled underflow mode");
						}
					}
				}

				_ufrom exp_mant_old = signless - _exponent_offset;
				// TODO handle rounding?
				auto exp_mant = _details::adjust_position<MantissaBits, _mant_to, _uto>(exp_mant_old);
				return F::_make(sign, exp_mant);
			} else { // ExponentBits <= _exp_to
				constexpr auto _exponent_offset_unshifted = static_cast<_uto>(
					(_details::shl_safe<_exp_to - 1>(F::_one) - 1) -
					(_details::shl_safe<ExponentBits - 1>(F::_one) - 1)
				);
				constexpr auto _exponent_offset =
					static_cast<_uto>(_details::shl_safe<_mant_to>(_exponent_offset_unshifted));

				if constexpr (Profile::check_denorm) {
					if (is_denorm()) {
						constexpr auto _still_denorm_mask =
							_details::shr_safe<_exponent_offset_unshifted>(~_mantissa_mask) & _mantissa_mask;

						if constexpr (_exponent_offset_unshifted < _mant_to && _still_denorm_mask != 0) {
							if ((signless & _still_denorm_mask) == 0) { // the result is still a denorm
								auto new_mant = _details::adjust_position<
									MantissaBits, _mant_to + _exponent_offset_unshifted, _uto
								>(signless);
								return F::_make(sign, new_mant);
							}
						}

						auto bits = static_cast<_uto>(std::bit_width(signless)); // <= MantissaBits

						_uto new_exp;
						// can't rely on overflow behavior here since the numbers may not be of the same type
						if constexpr (_exponent_offset_unshifted >= MantissaBits) {
							new_exp = bits + (_exponent_offset_unshifted - MantissaBits);
						} else {
							new_exp = bits - (MantissaBits - _exponent_offset_unshifted);
						}

						_uto new_mant;
						if constexpr (_mant_to + 1 >= MantissaBits) {
							new_mant = static_cast<_uto>(signless) << ((_mant_to + 1) - bits);
						} else { // we need to decide whether to shift left or right
							// no danger of shifting too much in both cases
							if (bits < _mant_to + 1) {
								new_mant = static_cast<_uto>(signless) << ((_mant_to + 1) - bits);
							} else {
								new_mant = static_cast<_uto>(signless >> (bits - (_mant_to + 1)));
							}
						}

						return F::_make(sign, _details::shl_safe<_mant_to>(new_exp), new_mant);
					}
				}

				// TODO handle rounding?
				auto exp_mant = _details::adjust_position<MantissaBits, _mant_to, _uto>(signless);
				return F::_make(sign, exp_mant + _exponent_offset);
			}
		}

		/// Returns the sign bit.
		[[nodiscard]] constexpr bool is_negative() const {
			return _binary & _sign_mask;
		}

		/// Checks if this value is finite.
		[[nodiscard]] constexpr bool is_finite() const {
			return (_binary & _exponent_mask) != _exponent_mask;
		}
		/// Checks if this value is positive or negative infinity.
		[[nodiscard]] constexpr bool is_inf() const {
			return (_binary & (_exponent_mask | _mantissa_mask)) == _exponent_mask;
		}
		/// Checks if this value is NaN.
		[[nodiscard]] constexpr bool is_nan() const {
			return (_binary & _exponent_mask) == _exponent_mask && (_binary & _mantissa_mask) != 0;
		}
		/// Checks if this is a denormalized number.
		[[nodiscard]] constexpr bool is_denorm() const {
			return (_binary & _exponent_mask) == 0;
		}
	private:
		constexpr static Storage _zero = static_cast<Storage>(0); ///< Zero.
		constexpr static Storage _one = static_cast<Storage>(1); ///< One.

		/// Mask for the sign bit.
		constexpr static Storage _sign_mask = static_cast<Storage>(_one << (ExponentBits + MantissaBits));
		/// Mask for the exponent.
		constexpr static Storage _exponent_mask =
			static_cast<Storage>(((_one << ExponentBits) - 1) << MantissaBits);
		/// Mask for the mantissa.
		constexpr static Storage _mantissa_mask = static_cast<Storage>((_one << MantissaBits) - 1);
		/// Mantissa that indicates that a NaN is quiet.
		constexpr static Storage _quiet_mask = static_cast<Storage>(_one << (MantissaBits - 1));

		constexpr static Storage _min_value = _one; ///< Minimum nonnegative value of the float. This is a denorm.
		/// Minimum nonnegative non-denorm value of the float.
		constexpr static Storage _min_value_nondenorm = _details::shl_safe<MantissaBits>(_one);
		/// Maximum non-infinite value of the float.
		constexpr static Storage _max_value =
			~_details::shl_safe<MantissaBits>(_one) & (_exponent_mask | _mantissa_mask);

		/// Initializes the binary storage.
		constexpr explicit basic_custom_float(Storage b) : _binary(b) {
		}
		/// Constructs a new float from its components. \p exp is assumed to have been shifted to the correct
		/// position.
		[[nodiscard]] constexpr inline static basic_custom_float _make(bool sign, Storage exp, Storage mant) {
			return basic_custom_float(static_cast<Storage>(
				(sign ? _sign_mask : Storage(0)) | (exp & _exponent_mask) | (mant & _mantissa_mask)
			));
		}
		/// \overload
		[[nodiscard]] constexpr inline static basic_custom_float _make(bool sign, Storage exp_mant) {
			return basic_custom_float(static_cast<Storage>(
				(sign ? _sign_mask : Storage(0)) | (exp_mant & (_exponent_mask | _mantissa_mask))
			));
		}

		Storage _binary; ///< Binary representation of the number.
	};

	inline namespace custom_float_types {
		using float16 = basic_custom_float<5, 10, u16>; ///< IEEE 754 16-bit floating point numbers.
		using float32 = basic_custom_float<8, 23, u32>; ///< IEEE 754 32-bit floating point numbers.
		using float64 = basic_custom_float<11, 52, u64>; ///< IEEE 754 64-bit floating point numbers.
	}
}
