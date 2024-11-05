#pragma once

#include <imgui.h>

#include <lotus/renderer/shader_types_include_wrapper.h>
namespace shader_types {
#include "../shaders/shader_types.hlsli"
}
#include <lotus/renderer/shader_types_include_wrapper.h>

#include "../utils.h"

/// A test.
class test {
public:
	/// Initializes \ref _test_context.
	explicit test(const test_context &test_ctx) : _test_context(&test_ctx) {
	}
	/// Default virtual destructor.
	virtual ~test() = default;


	/// Updates the simulation.
	virtual void timestep(double dt, std::size_t iterations) = 0;
	/// Resets the simulation without resetting the parameters. This function is also an opportunity to update any
	/// parameters that cannot be easily updated mid-simulation.
	virtual void soft_reset() = 0;

	/// Renders the scene.
	virtual void render(
		lotus::renderer::context&, lotus::renderer::context::queue&, lotus::renderer::constant_uploader&,
		lotus::renderer::image2d_color, lotus::renderer::image2d_depth_stencil, lotus::cvec2u32 size
	) = 0;
	/// Displays the test-specific GUI.
	virtual void gui() {
		if (ImGui::Button("Soft Reset")) {
			soft_reset();
		}
	}
protected:
	/// Retrieves the test context.
	const test_context &_get_test_context() const {
		return *_test_context;
	}
private:
	const test_context *_test_context; ///< The test context.
};
