#pragma once

/// A test.
class test {
public:
	/// Default virtual destructor.
	virtual ~test() = default;

	/// Updates the simulation.
	virtual void timestep(double dt, std::size_t iterations) = 0;
	/// Renders the scene.
	virtual void render() = 0;
};
