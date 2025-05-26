#include "lotus/system/platforms/macos/details.h"

/// \file
/// Implementation of miscellaneous MacOS functions.

#include <Carbon/Carbon.h>
#include <AppKit/NSEvent.h>

namespace lotus::system::platforms::macos::_details {
	namespace conversions {
		/// Mapping from virtual key codes to \ref key enums.
		static constexpr std::pair<u16, key> _key_mapping[] = {
			{ kVK_ANSI_A,              key::a             },
			{ kVK_ANSI_S,              key::s             },
			{ kVK_ANSI_D,              key::d             },
			{ kVK_ANSI_F,              key::f             },
			{ kVK_ANSI_H,              key::h             },
			{ kVK_ANSI_G,              key::g             },
			{ kVK_ANSI_Z,              key::z             },
			{ kVK_ANSI_X,              key::x             },
			{ kVK_ANSI_C,              key::c             },
			{ kVK_ANSI_V,              key::v             },
			{ kVK_ANSI_B,              key::b             },
			{ kVK_ANSI_Q,              key::q             },
			{ kVK_ANSI_W,              key::w             },
			{ kVK_ANSI_E,              key::e             },
			{ kVK_ANSI_R,              key::r             },
			{ kVK_ANSI_Y,              key::y             },
			{ kVK_ANSI_T,              key::t             },
			{ kVK_ANSI_1,              key::num1          },
			{ kVK_ANSI_2,              key::num2          },
			{ kVK_ANSI_3,              key::num3          },
			{ kVK_ANSI_4,              key::num4          },
			{ kVK_ANSI_6,              key::num6          },
			{ kVK_ANSI_5,              key::num5          },
			{ kVK_ANSI_Equal,          key::unknown       }, // TODO
			{ kVK_ANSI_9,              key::num9          },
			{ kVK_ANSI_7,              key::num7          },
			{ kVK_ANSI_Minus,          key::unknown       }, // TODO
			{ kVK_ANSI_8,              key::num8          },
			{ kVK_ANSI_0,              key::num0          },
			{ kVK_ANSI_RightBracket,   key::unknown       }, // TODO
			{ kVK_ANSI_O,              key::o             },
			{ kVK_ANSI_U,              key::u             },
			{ kVK_ANSI_LeftBracket,    key::unknown       }, // TODO
			{ kVK_ANSI_I,              key::i             },
			{ kVK_ANSI_P,              key::p             },
			{ kVK_ANSI_L,              key::l             },
			{ kVK_ANSI_J,              key::j             },
			{ kVK_ANSI_Quote,          key::unknown       }, // TODO
			{ kVK_ANSI_K,              key::k             },
			{ kVK_ANSI_Semicolon,      key::unknown       }, // TODO
			{ kVK_ANSI_Backslash,      key::unknown       }, // TODO
			{ kVK_ANSI_Comma,          key::unknown       }, // TODO
			{ kVK_ANSI_Slash,          key::unknown       }, // TODO
			{ kVK_ANSI_N,              key::n             },
			{ kVK_ANSI_M,              key::m             },
			{ kVK_ANSI_Period,         key::unknown       }, // TODO
			{ kVK_ANSI_Grave,          key::unknown       }, // TODO
			{ kVK_ANSI_KeypadDecimal,  key::unknown       }, // TODO
			{ kVK_ANSI_KeypadMultiply, key::unknown       }, // TODO
			{ kVK_ANSI_KeypadPlus,     key::unknown       }, // TODO
			{ kVK_ANSI_KeypadClear,    key::unknown       }, // TODO
			{ kVK_ANSI_KeypadDivide,   key::unknown       }, // TODO
			{ kVK_ANSI_KeypadEnter,    key::unknown       }, // TODO
			{ kVK_ANSI_KeypadMinus,    key::unknown       }, // TODO
			{ kVK_ANSI_KeypadEquals,   key::unknown       }, // TODO
			{ kVK_ANSI_Keypad0,        key::numpad0       },
			{ kVK_ANSI_Keypad1,        key::numpad1       },
			{ kVK_ANSI_Keypad2,        key::numpad2       },
			{ kVK_ANSI_Keypad3,        key::numpad3       },
			{ kVK_ANSI_Keypad4,        key::numpad4       },
			{ kVK_ANSI_Keypad5,        key::numpad5       },
			{ kVK_ANSI_Keypad6,        key::numpad6       },
			{ kVK_ANSI_Keypad7,        key::numpad7       },
			{ kVK_ANSI_Keypad8,        key::numpad8       },
			{ kVK_ANSI_Keypad9,        key::numpad9       },

			{ kVK_Return,              key::enter         },
			{ kVK_Tab,                 key::tab           },
			{ kVK_Space,               key::space         },
			{ kVK_Delete,              key::del           },
			{ kVK_Escape,              key::escape        },
			{ kVK_Command,             key::left_control  },
			{ kVK_Shift,               key::left_shift    },
			{ kVK_CapsLock,            key::caps_lock     },
			{ kVK_Option,              key::left_alt      },
			{ kVK_Control,             key::left_super    },
			{ kVK_RightCommand,        key::right_control },
			{ kVK_RightShift,          key::right_shift   },
			{ kVK_RightOption,         key::right_alt     },
			{ kVK_RightControl,        key::right_super   },
			{ kVK_Function,            key::unknown       }, // TODO
			{ kVK_F17,                 key::f17           },
			{ kVK_VolumeUp,            key::unknown       }, // TODO
			{ kVK_VolumeDown,          key::unknown       }, // TODO
			{ kVK_Mute,                key::unknown       }, // TODO
			{ kVK_F18,                 key::f18           },
			{ kVK_F19,                 key::f19           },
			{ kVK_F20,                 key::f20           },
			{ kVK_F5,                  key::f5            },
			{ kVK_F6,                  key::f6            },
			{ kVK_F7,                  key::f7            },
			{ kVK_F3,                  key::f3            },
			{ kVK_F8,                  key::f8            },
			{ kVK_F9,                  key::f9            },
			{ kVK_F11,                 key::f11           },
			{ kVK_F13,                 key::f13           },
			{ kVK_F16,                 key::f16           },
			{ kVK_F14,                 key::f14           },
			{ kVK_F10,                 key::f10           },
			{ kVK_ContextualMenu,      key::unknown       }, // TODO
			{ kVK_F12,                 key::f12           },
			{ kVK_F15,                 key::f15           },
			{ kVK_Help,                key::unknown       }, // TODO
			{ kVK_Home,                key::home          },
			{ kVK_PageUp,              key::page_up       },
			{ kVK_ForwardDelete,       key::del           },
			{ kVK_F4,                  key::f4            },
			{ kVK_End,                 key::end           },
			{ kVK_F2,                  key::f2            },
			{ kVK_PageDown,            key::page_down     },
			{ kVK_F1,                  key::f1            },
			{ kVK_LeftArrow,           key::left          },
			{ kVK_RightArrow,          key::right         },
			{ kVK_DownArrow,           key::down          },
			{ kVK_UpArrow,             key::up            },
		};
		/// Returns the maximum visual key code value.
		[[nodiscard]] static consteval u16 _get_maximum_virtual_key_code() {
			u16 result = 0;
			for (std::pair<u16, key> p : _key_mapping) {
				result = std::max(result, p.first);
			}
			return result;
		}
		/// Type used to store a mapping from virtual key codes to \ref key enums.
		using _key_mapping_table_t = std::array<key, _get_maximum_virtual_key_code() + 1>;
		/// Returns a table storing mapping from virtual key codes to \ref key enums.
		[[nodiscard]] static consteval _key_mapping_table_t _get_key_mapping_table() {
			_key_mapping_table_t result;
			std::fill(result.begin(), result.end(), key::unknown);
			for (std::pair<u16, key> p : _key_mapping) {
				result[p.first] = p.second;
			}
			return result;
		}
		/// Mapping from virtual key codes to \ref key enums.
		static constexpr _key_mapping_table_t _key_mapping_table = _get_key_mapping_table();
		key to_key(u16 code) {
			if (code <= _get_maximum_virtual_key_code()) {
				return _key_mapping_table[code];
			}
			return key::unknown;
		}
	}
}
