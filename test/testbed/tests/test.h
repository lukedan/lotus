#pragma once

#include <imgui.h>

#include "../utils.h"

/// A test.
class test {
public:
	/// Default virtual destructor.
	virtual ~test() = default;


	/// Updates the simulation.
	virtual void timestep(double dt, std::size_t iterations) = 0;
	/// Resets the simulation without resetting the parameters. This function is also an opportunity to update any
	/// parameters that cannot be easily updated mid-simulation.
	virtual void soft_reset() = 0;

	/// Renders the scene.
	virtual void render(const draw_options&) = 0;
	/// Displays the test-specific GUI.
	virtual void gui() {
		if (ImGui::Button("Soft Reset")) {
			soft_reset();
		}
	}
};
