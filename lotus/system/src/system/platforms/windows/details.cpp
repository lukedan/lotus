#include "lotus/system/platforms/windows/details.h"

/// \file
/// Implementation of Windows-specific utilities.

namespace lotus::system::platforms::windows::_details {
	/// Mapping between virtual key codes and key enum values.
	static constexpr std::pair<int, key> _key_mapping[] = {
		{ VK_BACK,     key::backspace     },
		{ VK_TAB,      key::tab           },
		{ VK_RETURN,   key::enter         },
		{ VK_CAPITAL,  key::caps_lock     },
		{ VK_ESCAPE,   key::escape        },
		{ VK_SPACE,    key::space         },
		{ VK_PRIOR,    key::page_up       },
		{ VK_NEXT,     key::page_down     },
		{ VK_HOME,     key::home          },
		{ VK_END,      key::end           },
		{ VK_LEFT,     key::left          },
		{ VK_UP,       key::up            },
		{ VK_RIGHT,    key::right         },
		{ VK_DOWN,     key::down          },
		{ VK_INSERT,   key::insert        },
		{ VK_DELETE,   key::del           },
		{ VK_LWIN,     key::left_super    },
		{ VK_RWIN,     key::right_super   },
		{ VK_LSHIFT,   key::left_shift    },
		{ VK_RSHIFT,   key::right_shift   },
		{ VK_LCONTROL, key::left_control  },
		{ VK_RCONTROL, key::right_control },
		{ VK_LMENU,    key::left_alt      },
		{ VK_RMENU,    key::right_alt     },
	};
	/// Retrieves the maximum virtual key code.
	[[nodiscard]] static consteval int _get_maximum_virtual_key_code() {
		int result = 0;
		for (auto p : _key_mapping) {
			result = std::max(result, p.first);
		}
		result = std::max<int>(result, '9');
		result = std::max<int>(result, 'Z');
		result = std::max<int>(result, VK_NUMPAD9);
		result = std::max<int>(result, VK_F24);
		return result;
	}
	/// Type used to store a mapping from virtual key codes to \ref key enums.
	using _key_mapping_table_t = std::array<key, _get_maximum_virtual_key_code() + 1>;
	/// Returns a table storing mapping from virtual key codes to \ref key enums.
	[[nodiscard]] static consteval _key_mapping_table_t _get_key_mapping_table() {
		_key_mapping_table_t result;
		std::fill(result.begin(), result.end(), key::unknown);
		for (auto p : _key_mapping) {
			result[p.first] = p.second;
		}
		for (int i = 0; i < 10; ++i) {
			result[i + '0'] = static_cast<key>(std::to_underlying(key::num0) + i);
			result[i + VK_NUMPAD0] = static_cast<key>(std::to_underlying(key::numpad0) + i);
		}
		for (int i = 0; i < 26; ++i) {
			result[i + 'A'] = static_cast<key>(std::to_underlying(key::a) + i);
		}
		for (int i = 0; i < 24; ++i) {
			result[i + VK_F1] = static_cast<key>(std::to_underlying(key::f1) + i);
		}
		return result;
	}
	/// Mapping from virtual key codes to \ref key enums.
	static constexpr _key_mapping_table_t _key_mapping_table = _get_key_mapping_table();
	key virtual_keycode_to_key(int k) {
		if (k >= 0 && static_cast<usize>(k) < _key_mapping_table.size()) {
			return _key_mapping_table[k];
		}
		return key::unknown;
	}
}
