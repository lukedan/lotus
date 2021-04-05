#pragma once

#include <pbd/math/vector.h>

void set_matrix(pbd::mat44d);

/// Draws a sphere of radius 0.5 centered at the origin. The caller should set its color via \p glColor() and its
/// transformation using a matrix.
void draw_sphere();
