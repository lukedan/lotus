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
	namespace _details::short_vector {
		/// Adjusts the size to be at least the given value.
		template <bool Exact, typename Size> [[nodiscard]] Size enlarge_size(
			Size original, Size target, Size base, f32 factor
		) {
			if constexpr (Exact) {
				return target;
			}
			Size result = std::max(base, original);
			while (result < target) {
				result = static_cast<Size>(result * factor);
			}
			return result;
		}
	}

	/// A short vector where no allocation is necessary if the number of elements is small. Note that unlike standard
	/// vectors, there's no guarantee that objects will not be moved when the vector itself is moved. This is
	/// violated when the vector is in `short' mode.
	template <typename T, usize ShortSize, typename Allocator = memory::raw::allocator> class short_vector {
		template <typename, usize, typename> friend class short_vector;
	public:
		constexpr static bool pedantic_internal_checks = false; ///< Whether to perform pedantic internal checks.
		constexpr static bool pedantic_usage_checks = true; ///< Whether to perform pedantic usage checks.
		using value_type     = T;           ///< Value type.
		using iterator       = T*;          ///< Iterator type.
		using const_iterator = const T*;    ///< Const iterator type.
		using size_type      = usize; ///< Type for representing sizes.
	private:
		/// Performs a pedantic internal check.
		static void _pedantic_internal_crash_if([[maybe_unused]] bool x) {
			if constexpr (pedantic_internal_checks) {
				crash_if(x);
			}
		}
		/// Performs a pedantic usage check.
		static void _pedantic_usage_crash_if([[maybe_unused]] bool x) {
			if constexpr (pedantic_usage_checks) {
				crash_if(x);
			}
		}

		/// Array that stores values externally.
		struct _external_array {
			/// Initializes this array to empty.
			_external_array(std::nullptr_t) {
			}
			/// Move constructor.
			_external_array(_external_array &&src) noexcept :
				data(std::exchange(src.data, nullptr)),
				count(std::exchange(src.count, 0)),
				capacity(std::exchange(src.capacity, 0)) {
			}
			/// No copy construction.
			_external_array(const _external_array&) = delete;
			/// Move assignment. Swaps the two objects.
			_external_array &operator=(_external_array &&src) noexcept {
				if (&src != this) {
					std::swap(data, src.data);
					std::swap(count, src.count);
					std::swap(capacity, src.capacity);
				}
				return *this;
			}
			/// No copy assignment.
			_external_array &operator=(const _external_array&) = delete;
			/// Performs checks when enabled.
			~_external_array() {
				_pedantic_internal_crash_if(data);
			}

			/// Allocates storage with the given capacity.
			void allocate_storage(Allocator &alloc, size_type cap) {
				count = 0;
				capacity = cap;
				data = static_cast<T*>(static_cast<void*>(
					alloc.allocate(memory::size_alignment::of_array<T>(capacity))
				));
			}
			/// Frees the storage.
			void free_storage(Allocator &alloc) {
				alloc.free(static_cast<std::byte*>(static_cast<void*>(data)));
				data = nullptr;
				count = 0;
				capacity = 0;
			}

			T *data = nullptr; ///< Array data.
			size_type count = 0; ///< Number of elements.
			size_type capacity = 0; ///< The capacity of this array.
		};
		/// Marks that an object should be default initialized.
		struct _default_construct_tag {
		};
	public:
		/// Type used for storing the number of elements stored internally.
		using short_size_type = minimum_unsigned_type_t<static_cast<u64>(ShortSize + 1)>;
		/// The actual length of the short vector.
		constexpr static size_type actual_short_size = std::max<size_type>(
			sizeof(_external_array) / sizeof(T), static_cast<size_type>(ShortSize)
		);
	private:
		constexpr static u32 _growth_factor = 2; ///< Factor used when growing the vector.
		/// Value of \ref _short_count that indicates this vector is using an external array.
		constexpr static short_size_type _external_array_marker = std::numeric_limits<short_size_type>::max();

		/// Array that stores values internally.
		struct _internal_array {
			/// Leaves the storage uninitialized.
			_internal_array(uninitialized_t) noexcept {
			}

			/// Returns the data of this array.
			[[nodiscard]] T *data() {
				return static_cast<T*>(static_cast<void*>(storage.data()));
			}
			/// \overload
			[[nodiscard]] const T *data() const {
				return static_cast<const T*>(static_cast<const void*>(storage.data()));
			}

			alignas(T) std::array<std::byte, sizeof(T) * actual_short_size> storage; ///< Storage for the objects.
		};
	public:
		/// Initializes this array to empty.
		explicit short_vector(const Allocator &alloc) : _allocator(alloc) {
			std::construct_at(&_short_storage, uninitialized);
		}
		/// Default initialization.
		short_vector() : short_vector(Allocator()) {
		}
		/// Move constructor.
		short_vector(short_vector &&src) : _allocator(src._allocator) {
			_short_count = src._short_count;
			if (src._using_external_array()) {
				std::construct_at(&_long_storage, std::move(src._long_storage));
				std::destroy_at(&src._long_storage);
				std::construct_at(&src._short_storage, uninitialized);
				src._short_count = 0;
			} else {
				auto [src_data, src_size] = src._data_size();
				std::construct_at(&_short_storage, uninitialized);
				std::uninitialized_move(src_data, src_data + src_size, _short_storage.data());
			}
		}
		/// Initializes this vector with the given range of elements.
		template <typename It> short_vector(It begin, It end, const Allocator &alloc = Allocator()) :
			short_vector(alloc) {

			assign(begin, end);
		}
		/// Copy construction.
		short_vector(const short_vector &src, const Allocator &alloc = Allocator()) :
			short_vector(src.begin(), src.end(), alloc) {
		}
		/// Copy assignment.
		short_vector &operator=(const short_vector &src) {
			if (&src == this) {
				return *this;
			}
			assign(src.begin(), src.end());
			return *this;
		}
		/// Initializes this vector with the given range of objects.
		short_vector(std::initializer_list<T> values, const Allocator &alloc = Allocator()) :
			short_vector(values.begin(), values.end(), alloc) {
		}
		/// Creates a vector with \p count copies of the given value.
		short_vector(size_type count, const T &value, const Allocator &alloc = Allocator()) : short_vector(alloc) {
			assign(count, value);
		}
		/// Creates a vector with \p count value-initialized objects.
		explicit short_vector(size_type count, const Allocator &alloc = Allocator()) : short_vector(alloc) {
			T *storage = _assign_impl(count);
			std::uninitialized_value_construct(storage, storage + count);
		}
		/// Destructor.
		~short_vector() {
			auto [d, sz] = _data_size();
			std::destroy(d, d + sz);
			if (_using_external_array()) {
				_long_storage.free_storage(_allocator);
				std::destroy_at(&_long_storage);
			} else {
				std::destroy_at(&_short_storage);
			}
		}

		/// Constructs a new element at the end of this vector.
		template <typename ...Args> T &emplace_back(Args &&...args) {
			auto [new_vec, old_ptr, old_size] = _begin_allocate_more(1);
			T *res;
			if (new_vec) {
				res = std::construct_at(new_vec->data + old_size, std::forward<Args>(args)...);
				std::uninitialized_move(old_ptr, old_ptr + old_size, new_vec->data);
			} else {
				res = std::construct_at(old_ptr + old_size, std::forward<Args>(args)...);
			}
			_end_allocate_more(std::move(new_vec));
			return *res;
		}
		/// \overload
		T &push_back(const T &obj) {
			return emplace_back(obj);
		}

		/// Removes the last element of this vector.
		void pop_back() {
			auto [d, sz] = _data_size();
			_pedantic_usage_crash_if(sz == 0);
			auto new_size = sz - 1;
			std::destroy_at(d + new_size);
			_set_size(new_size);
		}

		/// Replaces the contents of this container with the given range of objects.
		template <typename It> void assign(It begin, It end) {
			T *storage = _assign_impl(static_cast<size_type>(std::distance(begin, end)));
			std::uninitialized_copy(begin, end, storage);
		}
		/// Replaces the contents of this container with \p count copies of the given object.
		void assign(size_type count, const T &value) {
			T *storage = _assign_impl(count);
			for (size_type i = 0; i < count; ++i) {
				std::construct_at(storage + i, value);
			}
		}

		/// Inserts the given elements at the given position.
		template <typename It> iterator insert(const_iterator pos, It first, It last) {
			if (first == last) {
				return const_cast<iterator>(pos);
			}

			const auto insert_count = static_cast<size_type>(std::distance(first, last));
			auto [new_vec, old_ptr, old_size] = _begin_allocate_more(insert_count);
			const std::ptrdiff_t insert_index = pos - old_ptr;
			T *result;
			if (new_vec) {
				std::uninitialized_move(old_ptr, old_ptr + insert_index, new_vec->data);
				std::uninitialized_move(
					old_ptr + insert_index,
					old_ptr + old_size,
					new_vec->data + insert_index + insert_count
				);
				// copy new data
				result = new_vec->data + insert_index;
				std::uninitialized_copy(first, last, result);
			} else {
				const size_type move_count = old_size - static_cast<size_type>(insert_index);
				if (move_count > insert_count) {
					T *old_end = old_ptr + old_size;
					T *src = old_end;
					T *dst = src + insert_count;
					while (dst != old_end) {
						--src;
						--dst;
						std::uninitialized_move_n(src, 1, dst);
					}
					while (src != pos) {
						--src;
						--dst;
						*dst = std::move(*src);
					}
					// copy new data
					std::copy(first, last, old_ptr + insert_index);
				} else {
					std::uninitialized_move(
						old_ptr + insert_index,
						old_ptr + old_size,
						old_ptr + insert_index + insert_count
					);
					// copy new data
					It src = first;
					T *split = old_ptr + old_size;
					for (T *dst = old_ptr + insert_index; dst != split; ++src, ++dst) {
						*dst = *src;
					}
					std::uninitialized_copy(src, last, split);
				}
				result = old_ptr + insert_index;
			}
			_end_allocate_more(std::move(new_vec));

			return result;
		}

		/// Erases the given range of objects.
		void erase(const_iterator range_beg, const_iterator range_end) {
			if (range_beg == range_end) {
				return;
			}

			auto [d, sz] = _data_size();
			auto arr_end = d + sz;
			auto dst = const_cast<T*>(range_beg);
			auto src = const_cast<T*>(range_end);
			for (; src != arr_end; ++src, ++dst) {
				*dst = std::move(*src);
			}
			std::destroy(dst, arr_end);

			_set_size(sz - static_cast<usize>(range_end - range_beg));
		}

		/// Resizes this array.
		void resize(size_type size) {
			_resize_impl(size, _default_construct_tag());
		}
		/// \overload
		void resize(size_type size, const T &value) {
			_resize_impl(size, value);
		}

		/// Clears the array.
		void clear() {
			auto [d, sz] = _data_size();
			std::destroy(d, d + sz);
			if (_using_external_array()) {
				_long_storage.count = 0;
			} else {
				_short_count = 0;
			}
		}

		/// Shrinks the storage of this vector to fit its contents.
		void shrink_to_fit() {
			if (!_using_external_array()) {
				return;
			}
			if (_long_storage.count > actual_short_size) {
				_external_array new_arr = nullptr;
				new_arr.allocate_storage(_allocator, _long_storage.count);
				new_arr.count = _long_storage.count;
				std::uninitialized_move(_long_storage.data, _long_storage.data + _long_storage.count, new_arr.data);
				std::destroy(_long_storage.data, _long_storage.data + _long_storage.count);
				_long_storage.free_storage(_allocator);
				_long_storage = std::move(new_arr);
			} else {
				_external_array old_arr = std::move(_long_storage);
				std::destroy_at(&_long_storage);
				std::construct_at(&_short_storage, uninitialized);
				std::uninitialized_move(old_arr.data, old_arr.data + old_arr.count, _short_storage.data());
				std::destroy(old_arr.data, old_arr.data + old_arr.count);
				_short_count = static_cast<short_size_type>(old_arr.count);
				old_arr.free_storage(_allocator);
			}
		}

		/// Retrieves the element at the given index.
		[[nodiscard]] T &at(size_type i) {
			_pedantic_usage_crash_if(i >= size());
			return data()[i];
		}
		/// \overload
		[[nodiscard]] const T &at(size_type i) const {
			_pedantic_usage_crash_if(i >= size());
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

		/// Returns a reference to the first element.
		[[nodiscard]] T &front() {
			_pedantic_usage_crash_if(empty());
			return *data();
		}
		/// \overload
		[[nodiscard]] const T &front() const {
			_pedantic_usage_crash_if(empty());
			return *data();
		}
		/// Returns a reference to the last element.
		[[nodiscard]] T &back() {
			auto [d, sz] = _data_size();
			_pedantic_usage_crash_if(sz == 0);
			return *(d + (sz - 1));
		}
		/// \overload
		[[nodiscard]] const T &back() const {
			auto [d, sz] = _data_size();
			_pedantic_usage_crash_if(sz == 0);
			return *(d + (sz - 1));
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
			auto [d, sz] = _data_size();
			return d + sz;
		}
		/// \overload
		[[nodiscard]] const_iterator end() const {
			auto [d, sz] = _data_size();
			return d + sz;
		}
		/// \overload
		[[nodiscard]] const_iterator cend() const {
			return end();
		}

		/// Returns the data pointer.
		[[nodiscard]] T *data() {
			return _data_size().first;
		}
		/// \overload
		[[nodiscard]] const T *data() const {
			return _data_size().first;
		}

		/// Returns the number of elements in this array.
		[[nodiscard]] size_type size() const {
			return _data_size().second;
		}
		/// Returns the capacity of this array.
		[[nodiscard]] size_type capacity() const {
			auto [d, sz, cap] = _data_size_capacity();
			return cap;
		}
		/// Returns whether this vector is empty.
		[[nodiscard]] bool empty() const {
			return size() == 0;
		}

	private:
		/// Returns whether this container is currently using an external array.
		[[nodiscard]] bool _using_external_array() const {
			return _short_count == _external_array_marker;
		}
		/// Returns the data pointer, the number of elements, and the current capacity.
		[[nodiscard]] std::tuple<T*, size_type, size_type> _data_size_capacity() {
			if (_using_external_array()) {
				return { _long_storage.data, _long_storage.count, _long_storage.capacity };
			}
			return { _short_storage.data(), static_cast<size_type>(_short_count), actual_short_size };
		}
		/// \overload
		[[nodiscard]] std::tuple<const T*, size_type, size_type> _data_size_capacity() const {
			if (_using_external_array()) {
				return { _long_storage.data, _long_storage.count, _long_storage.capacity };
			}
			return { _short_storage.data(), static_cast<size_type>(_short_count), actual_short_size };
		}
		/// Returns the data pointer and the number of elements.
		[[nodiscard]] std::pair<T*, size_type> _data_size() {
			auto [ptr, sz, cap] = _data_size_capacity();
			return { ptr, sz };
		}
		/// \overload
		[[nodiscard]] std::pair<const T*, size_type> _data_size() const {
			auto [ptr, sz, cap] = _data_size_capacity();
			return { ptr, sz };
		}
		/// Sets the number of elements in this array.
		void _set_size(size_type sz) {
			if (_using_external_array()) {
				_long_storage.count = sz;
			} else {
				_short_count = static_cast<short_size_type>(sz);
			}
		}

		/// Makes sure that the storage is large enough for \p count additional objects. If a new array is needed,
		/// this function allocates it but does not initialize it or move any elements. The caller should manually
		/// copy contents over and free the old array. Otherwise, \p nullopt is returned. In either case, the size
		/// of this array is also updated.
		///
		/// \return Newly allocated array if any, pointer to the old array, and size of the old vector.
		template <bool Exact = false> [[nodiscard]] std::tuple<
			std::optional<_external_array>, T*, size_type
		> _begin_allocate_more(size_type count) {
			auto [old_data, old_size, old_cap] = _data_size_capacity();
			size_type new_size = old_size + count;
			if (new_size > old_cap) {
				size_type new_cap = _details::short_vector::enlarge_size<Exact>(
					old_size, new_size, actual_short_size, _growth_factor
				);
				_external_array result = nullptr;
				result.allocate_storage(_allocator, new_cap);
				result.count = new_size;
				return { std::make_optional(std::move(result)), old_data, old_size };
			}
			_set_size(new_size);
			return { std::nullopt, old_data, old_size };
		}
		/// Finishes allocating more elements in the array and deallocates any old storages. Also destroys all
		/// elements in the old array.
		void _end_allocate_more(std::optional<_external_array> new_arr) {
			if (new_arr) {
				if (_using_external_array()) {
					std::destroy(_long_storage.data, _long_storage.data + _long_storage.count);
					_long_storage.free_storage(_allocator);
					_long_storage = std::move(new_arr.value());
				} else {
					std::destroy(_short_storage.data(), _short_storage.data() + _short_count);
					std::destroy_at(&_short_storage);
					std::construct_at(&_long_storage, std::move(new_arr.value()));
					_short_count = _external_array_marker;
				}
			}
		}

		/// Allocates space for the given number of elements in this array.
		///
		/// \return Pointer to uninitialized storage for \p count objects in this array.
		[[nodiscard]] T *_assign_impl(size_type count) {
			{ // destroy old elements
				auto [old_data, old_size] = _data_size();
				std::destroy(old_data, old_data + old_size);
			}

			if (_using_external_array()) {
				if (count > _long_storage.capacity) {
					size_type new_cap = _details::short_vector::enlarge_size<false>(
						_long_storage.capacity, count, actual_short_size, _growth_factor
					);
					_long_storage.free_storage(_allocator);
					_long_storage.allocate_storage(_allocator, new_cap);
				}
				_long_storage.count = count;
				return _long_storage.data;
			} else if (count > actual_short_size) {
				std::destroy_at(&_short_storage);
				std::construct_at(&_long_storage, nullptr);
				size_type cap = _details::short_vector::enlarge_size<false, size_type>(
					0, count, actual_short_size, _growth_factor
				);
				_long_storage.allocate_storage(_allocator, cap);
				_short_count = _external_array_marker;
				_long_storage.count = count;
				return _long_storage.data;
			} else {
				_short_count = static_cast<short_size_type>(count);
				return _short_storage.data();
			}
		}

		/// Resizes the array.
		template <typename U> void _resize_impl(size_type new_size, const U &value) {
			auto [d, cur_sz] = _data_size();
			if (new_size < cur_sz) {
				std::destroy(d + new_size, d + cur_sz);
				_set_size(new_size);
			} else if (new_size > cur_sz) {
				auto [new_vec, old_ptr, old_size] = _begin_allocate_more(new_size - cur_sz);
				T *dest;
				if (new_vec) {
					std::uninitialized_move(old_ptr, old_ptr + old_size, new_vec->data);
					dest = new_vec->data;
				} else {
					dest = old_ptr;
				}
				// construct new elements
				if constexpr (std::is_same_v<U, _default_construct_tag>) {
					std::uninitialized_default_construct(dest + old_size, dest + new_size);
				} else {
					std::uninitialized_fill(dest + old_size, dest + new_size, value);
				}
				_end_allocate_more(std::move(new_vec));
			}
		}

		union {
			[[no_unique_address]] _internal_array _short_storage; ///< Storage for short arrays.
			_external_array _long_storage; ///< Storage for long arrays.
		};
		short_size_type _short_count = 0; ///< The number of elements in this conatiner.
		[[no_unique_address]] Allocator _allocator; ///< The allocator.
	};

	/// Equality comparison for short vectors.
	template <
		typename T, usize Size1, usize Size2, typename Alloc1, typename Alloc2
	> [[nodiscard]] constexpr bool operator==(
		const short_vector<T, Size1, Alloc1> &lhs, const short_vector<T, Size2, Alloc2> &rhs
	) {
		return std::equal(lhs.begin(), lhs.end(), rhs.begin(), rhs.end());
	}
	/// Comparison function for short vectors.
	template <
		typename T, usize Size1, usize Size2, typename Alloc1, typename Alloc2
	> [[nodiscard]] constexpr auto operator<=>(
		const short_vector<T, Size1, Alloc1> &lhs, const short_vector<T, Size2, Alloc2> &rhs
	) {
		return std::lexicographical_compare_three_way(lhs.begin(), lhs.end(), rhs.begin(), rhs.end());
	}
}
