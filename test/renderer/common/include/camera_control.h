#pragma once

#include "lotus.h"

#include <lotus/utils/camera.h>

template <typename T> class camera_control {
public:
	explicit camera_control(lotus::camera_parameters<T> &t) : _target(&t) {
	}

	bool on_mouse_move(cvec2i new_position) {
		bool changed = false;

		cvec2f offset = (new_position - _prev_mouse).into<float>();
		offset[0] = -offset[0];
		if (_is_rotating) {
			_target->rotate_around_world_up(offset * rotation_speed);
			changed = true;
		}
		if (_is_zooming) {
			cvec3f cam_offset = _target->position - _target->look_at;
			cam_offset *= std::exp(-zooming_speed * offset[1]);
			_target->position = _target->look_at + cam_offset;
			changed = true;
		}
		if (_is_moving) {
			auto cam = _target->into_camera();
			cvec3f x = cam.unit_right * offset[0];
			cvec3f y = cam.unit_up * offset[1];
			float distance = (_target->position - _target->look_at).norm() * moving_speed;
			cvec3f cam_off = (x + y) * distance;
			_target->position += cam_off;
			_target->look_at += cam_off;
			changed = true;
		}

		_prev_mouse = new_position;
		return changed;
	}
	bool on_mouse_down(lsys::mouse_button button) {
		switch (button) {
		case lsys::mouse_button::primary:
			_is_rotating = true;
			return true;
		case lsys::mouse_button::secondary:
			_is_zooming = true;
			return true;
		case lsys::mouse_button::middle:
			_is_moving = true;
			return true;
		}
		return false;
	}
	bool on_mouse_up(lsys::mouse_button button) {
		switch (button) {
		case lsys::mouse_button::primary:
			_is_rotating = false;
			break;
		case lsys::mouse_button::secondary:
			_is_zooming = false;
			break;
		case lsys::mouse_button::middle:
			_is_moving = false;
			break;
		}
		return !_is_rotating && !_is_zooming && !_is_moving;
	}
	void on_capture_broken() {
		_is_rotating = _is_zooming = _is_moving = false;
	}

	float rotation_speed = 0.004f;
	float zooming_speed = 0.005f;
	float moving_speed = 0.001f;
private:
	lotus::camera_parameters<T> *_target = nullptr;
	bool _is_rotating = false;
	bool _is_zooming = false;
	bool _is_moving = false;
	cvec2i _prev_mouse = zero;
};
