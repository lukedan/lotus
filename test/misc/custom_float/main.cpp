#include <cmath>
#include <cfenv>
#include <random>

#include <lotus/utils/custom_float.h>
#include <lotus/logging.h>

[[nodiscard]] bool test_f32_to_f64(float f32) {
	auto myf32 = lotus::float32::reinterpret(f32);

	auto myf64 = myf32.into<lotus::float64>();
	auto myi64 = myf64.reinterpret_as<std::uint64_t>();
	auto sysi64 = std::bit_cast<std::uint64_t>(static_cast<double>(f32));
	if (std::isnan(f32) != myf64.is_nan()) {
		return false;
	}
	if (!myf64.is_nan()) {
		return myi64 == sysi64;
	}
	return true;
}

[[nodiscard]] bool test_f64_to_f32(double f64) {
	auto myf64 = lotus::float64::reinterpret(f64);

	auto myf32 = myf64.into<lotus::float32>();
	auto myi32 = myf32.reinterpret_as<std::uint32_t>();
	auto sysi32 = std::bit_cast<std::uint32_t>(static_cast<float>(f64));
	if (std::isnan(f64) != myf32.is_nan()) {
		return false;
	}
	if (!myf32.is_nan()) {
		return myi32 == sysi32;
	}
	return true;
}

int main() {
	std::fesetround(FE_TOWARDZERO);

	for (std::uint32_t i = 0; ; ++i) {
		if (i % 10000000 == 0) {
			lotus::log().debug("f32 -> f64  {:.1f}%", 100.0 * i / static_cast<double>(std::numeric_limits<std::uint32_t>::max()));
		}

		lotus::crash_if(!test_f32_to_f64(std::bit_cast<float>(i)));

		if (i == std::numeric_limits<std::uint32_t>::max()) {
			break;
		}
	}

	constexpr std::uint64_t f64_interval = 2100000000;
	for (std::uint64_t i = 0; ; ) {
		if (i % (f64_interval * 10000000) == 0) {
			lotus::log().debug("f64 -> f32  {:.1f}%", 100.0 * i / static_cast<double>(std::numeric_limits<std::uint64_t>::max()));
		}

		lotus::crash_if(!test_f64_to_f32(std::bit_cast<double>(i)));

		auto new_i = i + f64_interval;
		if (new_i < i) {
			break;
		}
		i = new_i;
	}

	std::default_random_engine rng;
	std::uniform_int_distribution<std::uint64_t> range(0);

	lotus::log().debug("Now testing random f64 -> f32");
	for (std::uint64_t i = 0; ; ++i) {
		if (i % 10000000 == 0) {
			lotus::log().debug("Tested {} numbers", i);
		}

		std::uint64_t x = range(rng);
		bool ok = test_f64_to_f32(std::bit_cast<double>(x));
		if (!ok) {
			lotus::log().error("Bad value: {}", x);
			std::abort();
		}
	}

	return 0;
}
