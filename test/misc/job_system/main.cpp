#include "lotus/types.h"
#include "lotus/utils/job_system.h"
#include "lotus/logging.h"

using namespace lotus;

int main() {
	auto jman = job_system::manager::spawn_workers(4);
	job_system::resource_handle h1 = jman.create_resource_with_value<i32>(1);
	job_system::resource_handle h2 = jman.create_resource<f32>();
	jman.schedule_job<f32>(static_cast<std::tuple<f32>(*)(i32)>([](i32 x) -> std::tuple<f32> {
		log().debug("Value: {}", x);
		return { static_cast<f32>(x) + 1.5f };
	}), { h1 }, { h2 });
	log().debug("Processed Value: {}", jman.get_resource_value_blocking<f32>(h2));
	return 0;
}
