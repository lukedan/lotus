#pragma once

/// \file
/// Short vectors.

#include <cinttypes>
#include <vector>
#include <memory>
#include <initializer_list>

#include "lotus/memory/common.h"
#include "maybe_uninitialized.h"

namespace lotus {
	/// A short vector where no allocation is necessary if the number of elements is small. Note that unlike standard
	/// vectors, there's no guarantee that objects will not be moved when the vector itself is moved. This is
	/// violated when the vector is in `short' mode.
	template <typename T, std::size_t ShortSize, typename Allocator = memory::raw::allocator> class short_vector {
		template <typename, std::size_t, typename> friend class short_vector;
	public:
		using iterator = T*; ///< Iterator type.
		using const_iterator = const T*; ///< Const iterator type.
		using size_type = std::size_t; ///< Type for representing sizes.

		/// Initializes this array to empty.
		explicit short_vector(Allocator alloc = Allocator()) : _allocator(std::move(alloc)) {
			std::construct_at(&_short_storage, uninitialized);
		}
		/// Move constructor.
		template <std::size_t OtherSize> short_vector(short_vector<T, OtherSize, Allocator> &&src) :
			_count(src._count), _allocator(src._allocator) {

			if (_using_external_array()) {
				if (src._using_external_array()) {
					std::construct_at(&_long_storage, src._long_storage);
					std::destroy_at(&src._long_storage);
					std::construct_at(&src._short_storage, uninitialized);
					src._count = 0;
				} else {
					std::construct_at(&_long_storage, nullptr)->allocate_storage(_allocator, _count);
					std::uninitialized_move(src.data(), src.data() + src.size(), _long_storage.data);
				}
			} else {
				std::construct_at(&_short_storage, uninitialized);
				std::uninitialized_move(src.data(), src.data() + src.size(), _short_storage.data());
			}
		}
		/// Initializes this vector with the given range of elements.
		template <typename It> short_vector(It begin, It end, Allocator alloc = Allocator()) :
			_allocator(std::move(alloc)) {

			_count = static_cast<size_type>(std::distance(begin, end));
			if (_using_external_array()) {
				std::construct_at(&_long_storage, nullptr)->allocate_storage(_allocator, _count);
			} else {
				std::construct_at(&_short_storage, uninitialized);
			}
			std::uninitialized_copy(begin, end, data());
		}
		/// Initializes this vector with the given range of objects.
		short_vector(std::initializer_list<T> values, Allocator alloc = Allocator()) :
			short_vector(values.begin(), values.end(), std::move(alloc)) {
		}
		/// Destructor.
		~short_vector() {
			std::destroy(data(), data() + size());
			if (_using_external_array()) {
				_long_storage.free_storage(_allocator);
				std::destroy_at(&_long_storage);
			} else {
				std::destroy_at(&_short_storage);
			}
		}

		/// Constructs a new element at the end of this vector.
		template <typename ...Args> T &emplace_back(Args &&...args) {
			if (_count == ShortSize) { // switch to long storage
				auto new_arr = _move_to(_growth_factor * _count);
				std::destroy_at(&_short_storage);
				std::construct_at(&_long_storage, new_arr);
			} else if (_using_external_array() && _count == _long_storage.capacity) {
				auto new_arr = _move_to(_growth_factor * _count);
				_long_storage.free_storage(_allocator);
				_long_storage = new_arr;
			}
			T *result = std::construct_at(data() + _count, std::forward<Args>(args)...);
			++_count;
			return *result;
		}

		/// Retrieves the element at the given index.
		[[nodiscard]] T &at(size_type i) {
			crash_if(i >= size());
			return data()[i];
		}
		/// \overload
		[[nodiscard]] const T &at(size_type i) const {
			crash_if(i >= size());
			return data()[i];
		}
		/// \overload
		[[nodiscard]] T &operator[](size_type i) {
			return at(i);
		}
		/// \overload
		[[nodiscard]] const T &operator[](size_type i) const {
			return at(i);
		}

		/// Moves all elements into a \p std::vector and returns it.
		[[nodiscard]] std::vector<T> move_into_vector() && {
			std::vector<T> result;
			result.reserve(size());
			for (size_type i = 0; i < size(); ++i) {
				result.emplace_back(std::move(at(i)));
			}
			return result;
		}
		/// Copies all elements into a \p std::vector and returns it.
		[[nodiscard]] std::vector<T> into_vector() const {
			return std::vector<T>(begin(), end());
		}

		/// Returns an iterator to the first element of this array.
		[[nodiscard]] iterator begin() {
			return data();
		}
		/// \overload
		[[nodiscard]] const_iterator begin() const {
			return data();
		}
		/// \overload
		[[nodiscard]] const_iterator cbegin() const {
			return begin();
		}

		/// Returns an iterator past the end of this array.
		[[nodiscard]] iterator end() {
			return data() + size();
		}
		/// \overload
		[[nodiscard]] const_iterator end() const {
			return data() + size();
		}
		/// \overload
		[[nodiscard]] const_iterator cend() const {
			return end();
		}

		/// Returns the data pointer.
		[[nodiscard]] T *data() {
			if (_using_external_array()) {
				return _long_storage.data;
			}
			return _short_storage.data();
		}
		/// \overload
		[[nodiscard]] const T *data() const {
			if (_using_external_array()) {
				return _long_storage.data;
			}
			return _short_storage.data();
		}

		/// Returns the number of elements in this array.
		[[nodiscard]] size_type size() const {
			return _count;
		}
		/// Returns whether this vector is empty.
		[[nodiscard]] bool empty() const {
			return size() == 0;
		}
	private:
		constexpr static std::uint32_t _growth_factor = 2; ///< Factor used when growing the vector.

		/// Array that stores values internally.
		struct _internal_array {
			/// Leaves the storage uninitialized.
			_internal_array(uninitialized_t) {
			}

			/// Returns the data of this array.
			[[nodiscard]] T *data() {
				return static_cast<T*>(static_cast<void*>(storage.data()));
			}
			/// \overload
			[[nodiscard]] const T *data() const {
				return static_cast<const T*>(static_cast<const void*>(storage.data()));
			}

			alignas(T) std::array<std::byte, sizeof(T) * ShortSize> storage; ///< Storage for the objects.
		};
		/// Array that stores values externally.
		struct _external_array {
			/// Initializes this array to empty.
			_external_array(std::nullptr_t) {
			}

			/// Allocates storage with the given capacity.
			void allocate_storage(Allocator &alloc, size_type cap) {
				capacity = cap;
				data = static_cast<T*>(static_cast<void*>(
					alloc.allocate(memory::size_alignment::of_array<T>(capacity))
				));
			}
			/// Frees the storage.
			void free_storage(Allocator &alloc) {
				alloc.free(static_cast<std::byte*>(static_cast<void*>(data)));
				data = nullptr;
				capacity = 0;
			}

			T *data = nullptr; ///< Array data.
			size_type capacity = 0; ///< The capacity of this array.
		};

		/// Returns whether this container is currently using an external array.
		[[nodiscard]] bool _using_external_array() const {
			return _count > ShortSize;
		}

		/// Creates a \ref _external_array object with the given capacity, moves all contents of this array to that
		/// array, destroys all elements in this vector, and returns the new array.
		[[nodiscard]] _external_array _move_to(size_type cap) {
			_external_array result = nullptr;
			result.allocate_storage(_allocator, cap);
			T *old_data = data();
			std::uninitialized_move(old_data, old_data + _count, result.data);
			std::destroy(old_data, old_data + _count);
			return result;
		}

		union {
			_internal_array _short_storage; ///< Storage for short arrays.
			_external_array _long_storage; ///< Storage for long arrays.
		};
		size_type _count = 0; ///< The number of elements in this conatiner.
		[[no_unique_address]] Allocator _allocator; ///< The allocator.
	};

	/// Equality comparison for short vectors.
	template <
		typename T, std::size_t Size1, std::size_t Size2, typename Alloc1, typename Alloc2
	> [[nodiscard]] constexpr bool operator==(
		const short_vector<T, Size1, Alloc1> &lhs, const short_vector<T, Size2, Alloc2> &rhs
	) {
		return std::equal(lhs.begin(), lhs.end(), rhs.begin(), rhs.end());
	}
	/// Comparison function for short vectors.
	template <
		typename T, std::size_t Size1, std::size_t Size2, typename Alloc1, typename Alloc2
	> [[nodiscard]] constexpr auto operator<=>(
		const short_vector<T, Size1, Alloc1> &lhs, const short_vector<T, Size2, Alloc2> &rhs
	) {
		return std::lexicographical_compare_three_way(lhs.begin(), lhs.end(), rhs.begin(), rhs.end());
	}
}
