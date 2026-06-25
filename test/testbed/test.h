#pragma once

#include <imgui.h>

#include <lotus/renderer/shader_types_include_wrapper.h>
namespace shader_types {
#include "shaders/shader_types.hlsli"
}
#include <lotus/renderer/shader_types_include_wrapper.h>

#include "utils.h"

enum class test_category {
	invalid,

	rigid_body_physics,
	soft_body_physics,
	fundamentals,
	miscellaneous,
};
constexpr const char *const test_category_names[] = {
	"INVALID",
	"Rigid Body Physics",
	"Soft Body Physics",
	"Fundamentals",
	"Miscellaneous",
};

/// A test.
class test {
public:
	/// Initializes \ref _test_context.
	explicit test(test_context &test_ctx) : _test_context(&test_ctx) {
	}
	/// Default virtual destructor.
	virtual ~test() = default;


	/// Updates the simulation.
	virtual void timestep(scalar dt, u32 iterations) = 0;
	/// Resets the simulation without resetting the parameters. This function is also an opportunity to update any
	/// parameters that cannot be easily updated mid-simulation.
	virtual void soft_reset() = 0;

	/// Renders the scene.
	virtual void render(
		lotus::renderer::context&, lotus::renderer::context::queue&, lotus::renderer::constant_uploader&,
		lotus::renderer::recorded_resources::swap_chain, cvec2u32 size
	) = 0;
	/// Displays the test-specific GUI.
	virtual void gui() {
	}
	/// Key press callback.
	virtual void on_key_down(lotus::system::window_events::key_down&) {
	}
	/// Key release callback.
	virtual void on_key_up(lotus::system::window_events::key_up&) {
	}
protected:
	/// Retrieves the test context.
	[[nodiscard]] test_context &_get_test_context() const {
		return *_test_context;
	}
private:
	test_context *_test_context; ///< The test context.
};
