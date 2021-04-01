#pragma once

/// A test.
class test {
public:
	/// Default virtual destructor.
	virtual ~test() = default;

	/// Updates the simulation.
	virtual void update(double dt, double step) = 0;
	/// Renders the scene.
	virtual void render() = 0;
};
