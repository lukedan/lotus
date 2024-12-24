#pragma once

#include <lotus/math/vector.h>
#include <lotus/utils/camera.h>

namespace lotus {
	/// Camera controls.
	template <typename T> class camera_control {
	public:
		/// Initializes the object to empty.
		camera_control(std::nullptr_t) {
		}
		/// Initializes this object to control the given camera.
		explicit camera_control(camera_parameters<T> &t) : _target(&t) {
		}

		/// Called when the mouse moves.
		///
		/// \return whether the camera has been updated.
		bool on_mouse_move(cvec2i new_position) {
			bool changed = false;

			cvec2<T> offset = (new_position - _prev_mouse).into<T>();
			offset[0] = -offset[0];
			if (_is_rotating) {
				_target->rotate_around_world_up(offset * rotation_speed);
				changed = true;
			}
			if (_is_zooming) {
				cvec3<T> cam_offset = _target->position - _target->look_at;
				cam_offset *= std::exp(-zooming_speed * offset[1]);
				_target->position = _target->look_at + cam_offset;
				changed = true;
			}
			if (_is_moving) {
				auto cam = _target->into_camera();
				cvec3<T> x = cam.unit_right * offset[0];
				cvec3<T> y = cam.unit_up * offset[1];
				T distance = (_target->position - _target->look_at).norm() * moving_speed;
				cvec3<T> cam_off = (x + y) * distance;
				_target->position += cam_off;
				_target->look_at += cam_off;
				changed = true;
			}

			_prev_mouse = new_position;
			return changed;
		}
		/// Called when a mouse button is pressed.
		///
		/// \return \p true if this action causes camera manipulation to start.
		bool on_mouse_down(system::mouse_button button, system::modifier_key_mask mods) {
			if (_is_rotating || _is_zooming || _is_moving) {
				return false;
			}
			switch (button) {
			case system::mouse_button::primary:
				if (bit_mask::contains<system::modifier_key_mask::control>(mods)) {
					_is_zooming = true;
				} else if (bit_mask::contains<system::modifier_key_mask::alt>(mods)) {
					_is_moving = true;
				} else {
					_is_rotating = true;
				}
				break;
			case system::mouse_button::secondary:
				_is_zooming = true;
				break;
			case system::mouse_button::middle:
				_is_moving = true;
				break;
			default:
				return false;
			}
			_trigger_button = button;
			return true;
		}
		/// Called when a mouse button is released.
		///
		/// \return \p true if the camera is *not* being manipulated.
		bool on_mouse_up(system::mouse_button button) {
			if (!_is_rotating && !_is_zooming && !_is_moving) {
				return false;
			}
			if (button == _trigger_button) {
				_is_rotating = _is_zooming = _is_moving = false;
				return true;
			}
			return false;
		}
		/// Called when mouse capture is broken.
		void on_capture_broken() {
			_is_rotating = _is_zooming = _is_moving = false;
		}

		T rotation_speed = 0.004f; ///< Camera rotation speed.
		T zooming_speed = 0.005f; ///< Camera zooming speed.
		T moving_speed = 0.001f; ///< Camera movement speed.
	private:
		camera_parameters<T> *_target = nullptr; ///< Target camera parameters.
		bool _is_rotating = false; ///< Whether this camera is being rotated.
		bool _is_zooming = false; ///< Whether this camera is being zoomed in and out.
		bool _is_moving = false; ///< Whether the camera is being moved.
		cvec2i _prev_mouse = zero; ///< Previous mouse position.
		/// The mouse button that triggered the last action.
		system::mouse_button _trigger_button = system::mouse_button::num_enumerators;
	};
}
