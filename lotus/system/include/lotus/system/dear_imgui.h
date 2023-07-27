#pragma once

/// \file
/// Header-only support for Dear ImGui. This is not included by other parts of the library. Include all necessary
/// Dear ImGui headers before this file.

#include "lotus/common.h"
#include "common.h"

namespace lotus::system::dear_imgui {
	/// System support for Dear ImGui.
	class context {
	public:
		/// Creates a new Dear ImGui system context.
		[[nodiscard]] inline static context create() {
			auto &io = ImGui::GetIO();
			io.BackendPlatformName = "imgui_impl_lotus_system";
			io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;
			return context();
		}

		/// Called when the window is resized.
		void on_resize(window_events::resize &e) {
			auto &io = ImGui::GetIO();
			io.DisplaySize = ImVec2(e.new_size[0], e.new_size[1]);
		}
		/// Called when a mouse move event is detected.
		void on_mouse_move(window_events::mouse::move &e) {
			auto &io = ImGui::GetIO();
			io.AddMousePosEvent(e.new_position[0], e.new_position[1]);
		}
		/// Called when the mouse leaves the window.
		void on_mouse_leave() {
			auto &io = ImGui::GetIO();
			io.AddMousePosEvent(-std::numeric_limits<float>::max(), -std::numeric_limits<float>::max());
		}
		/// Called when a mouse down event is detected.
		void on_mouse_down(window &wnd, window_events::mouse::button_down &e) {
			if (!wnd.has_mouse_capture()) {
				wnd.acquire_mouse_capture();
			}
			_mouse_buttons |= 1u << std::to_underlying(e.button);

			auto &io = ImGui::GetIO();
			io.AddMousePosEvent(e.position[0], e.position[1]);
			io.AddMouseButtonEvent(_mouse_button_mapping[e.button], true);
		}
		/// Called when a mouse up event is detected.
		void on_mouse_up(window &wnd, window_events::mouse::button_up &e) {
			_mouse_buttons &= ~(1u << std::to_underlying(e.button));
			if (_mouse_buttons == 0 && wnd.has_mouse_capture()) {
				wnd.release_mouse_capture();
			}

			auto &io = ImGui::GetIO();
			io.AddMousePosEvent(e.position[0], e.position[1]);
			io.AddMouseButtonEvent(_mouse_button_mapping[e.button], false);
		}
		/// Called when a mouse scroll event is detected.
		void on_mouse_scroll(window_events::mouse::scroll &e) {
			auto &io = ImGui::GetIO();
			io.AddMousePosEvent(e.position[0], e.position[1]);
			io.AddMouseWheelEvent(e.offset[0], e.offset[1]);
		}
		/// Called when mouse capture is broken externally.
		void on_capture_broken() {
			_mouse_buttons = 0;
		}
		/// Called when a key down event is detected.
		void on_key_down(window_events::key_down &e) {
			auto &io = ImGui::GetIO();
			_update_modifier_keys(e.modifiers);
			io.AddKeyEvent(_key_mapping[e.key_code], true);
		}
		/// Called when a key up event is detected.
		void on_key_up(window_events::key_up &e) {
			auto &io = ImGui::GetIO();
			_update_modifier_keys(e.modifiers);
			io.AddKeyEvent(_key_mapping[e.key_code], false);
		}
		/// Called when a text input event is detected.
		void on_text_input(window_events::text_input &e) {
			auto &io = ImGui::GetIO();
			io.AddInputCharactersUTF8(reinterpret_cast<const char*>(e.text.c_str()));
		}
	private:
		/// Does nothing.
		context() {
		}

		std::uint32_t _mouse_buttons = 0; ///< Bit mask of all mouse buttons that are being held.

		/// Mapping from \ref key enum values to \p ImGuiKey enum values.
		constexpr static enums::sequential_mapping<key, ImGuiKey> _key_mapping{
			std::pair(key::f1,            ImGuiKey_F1        ),
			std::pair(key::f2,            ImGuiKey_F2        ),
			std::pair(key::f3,            ImGuiKey_F3        ),
			std::pair(key::f4,            ImGuiKey_F4        ),
			std::pair(key::f5,            ImGuiKey_F5        ),
			std::pair(key::f6,            ImGuiKey_F6        ),
			std::pair(key::f7,            ImGuiKey_F7        ),
			std::pair(key::f8,            ImGuiKey_F8        ),
			std::pair(key::f9,            ImGuiKey_F9        ),
			std::pair(key::f10,           ImGuiKey_F10       ),
			std::pair(key::f11,           ImGuiKey_F11       ),
			std::pair(key::f12,           ImGuiKey_F12       ),
			std::pair(key::f13,           ImGuiKey_None      ),
			std::pair(key::f14,           ImGuiKey_None      ),
			std::pair(key::f15,           ImGuiKey_None      ),
			std::pair(key::f16,           ImGuiKey_None      ),
			std::pair(key::f17,           ImGuiKey_None      ),
			std::pair(key::f18,           ImGuiKey_None      ),
			std::pair(key::f19,           ImGuiKey_None      ),
			std::pair(key::f20,           ImGuiKey_None      ),
			std::pair(key::f21,           ImGuiKey_None      ),
			std::pair(key::f22,           ImGuiKey_None      ),
			std::pair(key::f23,           ImGuiKey_None      ),
			std::pair(key::f24,           ImGuiKey_None      ),
			std::pair(key::num1,          ImGuiKey_1         ),
			std::pair(key::num2,          ImGuiKey_2         ),
			std::pair(key::num3,          ImGuiKey_3         ),
			std::pair(key::num4,          ImGuiKey_4         ),
			std::pair(key::num5,          ImGuiKey_5         ),
			std::pair(key::num6,          ImGuiKey_6         ),
			std::pair(key::num7,          ImGuiKey_7         ),
			std::pair(key::num8,          ImGuiKey_8         ),
			std::pair(key::num9,          ImGuiKey_9         ),
			std::pair(key::num0,          ImGuiKey_0         ),
			std::pair(key::a,             ImGuiKey_A         ),
			std::pair(key::b,             ImGuiKey_B         ),
			std::pair(key::c,             ImGuiKey_C         ),
			std::pair(key::d,             ImGuiKey_D         ),
			std::pair(key::e,             ImGuiKey_E         ),
			std::pair(key::f,             ImGuiKey_F         ),
			std::pair(key::g,             ImGuiKey_G         ),
			std::pair(key::h,             ImGuiKey_H         ),
			std::pair(key::i,             ImGuiKey_I         ),
			std::pair(key::j,             ImGuiKey_J         ),
			std::pair(key::k,             ImGuiKey_K         ),
			std::pair(key::l,             ImGuiKey_L         ),
			std::pair(key::m,             ImGuiKey_M         ),
			std::pair(key::n,             ImGuiKey_N         ),
			std::pair(key::o,             ImGuiKey_O         ),
			std::pair(key::p,             ImGuiKey_P         ),
			std::pair(key::q,             ImGuiKey_Q         ),
			std::pair(key::r,             ImGuiKey_R         ),
			std::pair(key::s,             ImGuiKey_S         ),
			std::pair(key::t,             ImGuiKey_T         ),
			std::pair(key::u,             ImGuiKey_U         ),
			std::pair(key::v,             ImGuiKey_V         ),
			std::pair(key::w,             ImGuiKey_W         ),
			std::pair(key::x,             ImGuiKey_X         ),
			std::pair(key::y,             ImGuiKey_Y         ),
			std::pair(key::z,             ImGuiKey_Z         ),
			std::pair(key::numpad1,       ImGuiKey_Keypad1   ),
			std::pair(key::numpad2,       ImGuiKey_Keypad2   ),
			std::pair(key::numpad3,       ImGuiKey_Keypad3   ),
			std::pair(key::numpad4,       ImGuiKey_Keypad4   ),
			std::pair(key::numpad5,       ImGuiKey_Keypad5   ),
			std::pair(key::numpad6,       ImGuiKey_Keypad6   ),
			std::pair(key::numpad7,       ImGuiKey_Keypad7   ),
			std::pair(key::numpad8,       ImGuiKey_Keypad8   ),
			std::pair(key::numpad9,       ImGuiKey_Keypad9   ),
			std::pair(key::numpad0,       ImGuiKey_Keypad0   ),
			std::pair(key::escape,        ImGuiKey_Escape    ),
			std::pair(key::backspace,     ImGuiKey_Backspace ),
			std::pair(key::tab,           ImGuiKey_Tab       ),
			std::pair(key::caps_lock,     ImGuiKey_CapsLock  ),
			std::pair(key::left_shift,    ImGuiKey_LeftShift ),
			std::pair(key::left_control,  ImGuiKey_LeftCtrl  ),
			std::pair(key::left_alt,      ImGuiKey_LeftAlt   ),
			std::pair(key::left_super,    ImGuiKey_LeftSuper ),
			std::pair(key::right_shift,   ImGuiKey_RightShift),
			std::pair(key::right_control, ImGuiKey_RightCtrl ),
			std::pair(key::right_alt,     ImGuiKey_RightAlt  ),
			std::pair(key::right_super,   ImGuiKey_RightSuper),
			std::pair(key::space,         ImGuiKey_Space     ),
			std::pair(key::enter,         ImGuiKey_Enter     ),
			std::pair(key::up,            ImGuiKey_UpArrow   ),
			std::pair(key::left,          ImGuiKey_LeftArrow ),
			std::pair(key::right,         ImGuiKey_RightArrow),
			std::pair(key::down,          ImGuiKey_DownArrow ),
			std::pair(key::home,          ImGuiKey_Home      ),
			std::pair(key::end,           ImGuiKey_End       ),
			std::pair(key::page_up,       ImGuiKey_PageUp    ),
			std::pair(key::page_down,     ImGuiKey_PageDown  ),
			std::pair(key::insert,        ImGuiKey_Insert    ),
			std::pair(key::del,           ImGuiKey_Delete    ),
			std::pair(key::unknown,       ImGuiKey_None      ),
		};
		/// Mapping from \ref mouse_button enum values to \p ImGuiMouseButton enum values.
		constexpr static enums::sequential_mapping<mouse_button, ImGuiMouseButton> _mouse_button_mapping{
			std::pair(mouse_button::primary,   ImGuiMouseButton_Left  ),
			std::pair(mouse_button::secondary, ImGuiMouseButton_Right ),
			std::pair(mouse_button::middle,    ImGuiMouseButton_Middle),
		};

		/// Updates modifier key state.
		void _update_modifier_keys(modifier_key_mask mods) {
			auto &io = ImGui::GetIO();
			io.AddKeyEvent(ImGuiKey_ModCtrl,  bit_mask::contains<modifier_key_mask::control>(mods));
			io.AddKeyEvent(ImGuiKey_ModShift, bit_mask::contains<modifier_key_mask::shift  >(mods));
			io.AddKeyEvent(ImGuiKey_ModAlt,   bit_mask::contains<modifier_key_mask::alt    >(mods));
		}
	};
}
