#pragma once

/// \file
/// Implementation of a pooled hash table.

#include <vector>

#include "lotus/logging.h"
#include "pool.h"

namespace lotus {
	// TODO allocator
	/// A hash table where the nodes are stored in a pool.
	template <typename Value, typename Hash, typename Index = std::uint32_t> class pooled_hash_table {
	private:
		struct _node;
		using _pool_type = pool<_node, Index>; ///< Type of the node pool.
	public:
		using value_type = Value; ///< Value type.
		using index_type = Index; ///< Index type.

		/// A reference to an element in this table.
		struct reference {
			friend pooled_hash_table;
		public:
			/// Initializes this reference to empty.
			reference(std::nullptr_t) : _ref(nullptr) {
			}

			/// Returns the raw index.
			[[nodiscard]] Index get_index() const {
				return _ref.get().get_value();
			}

			/// Returns whether this reference is valid.
			[[nodiscard]] bool is_valid() const {
				return _ref.is_valid();
			}
			/// Returns whether this reference is valid.
			[[nodiscard]] explicit operator bool() const {
				return is_valid();
			}
		private:
			/// Initializes \ref _ref.
			explicit reference(_pool_type::optional_index_type r) : _ref(r) {
			}

			_pool_type::optional_index_type _ref; ///< The reference.
		};

		/// No default constructor.
		pooled_hash_table() = delete;
		/// Creates a new hash table.
		[[nodiscard]] inline static pooled_hash_table create(Index head_size, Index pool_size = 0) {
			return pooled_hash_table(head_size, pool_size > 0 ? pool_size : head_size);
		}

		/// Inserts a value into this table. No checking of whether an object that compares equal is already in the
		/// table will be performed.
		template <typename ...Args> reference emplace(Args &&...args) {
			auto ref = _node_pool.allocate(std::forward<Args>(args)...);
			_node &n = _node_pool.at(ref);
			auto hash_value = static_cast<Index>(Hash{}(n.object));
			Index slot = hash_value % _head.size();
			if (_head[slot]) {
				log().info("Hash collision with hash value {} at slot {}", hash_value, slot);
			}
			n.next = _head[slot];
			_head[slot] = ref;
			return reference(ref);
		}
		/// Erases the given element from this table.
		void erase(reference ref) {
			const _node &n = ref.get(_node_pool);
			auto hash_value = static_cast<Index>(Hash{}(n.object));
			Index slot = hash_value & _head.size();
			auto *pcur = &_head[slot];
			for (; *pcur && *pcur != ref; pcur = &pcur->get(_node_pool).next) {
			}
			assert(*pcur);
			*pcur = pcur->get(_node_pool).next;
			_node_pool.free(ref);
		}
		/// Clears this hash table.
		void clear() {
			for (auto &head_ref : _head) {
				if (head_ref) {
					for (auto cur = head_ref; cur; ) {
						auto next = _node_pool.get(cur).next;
						_node_pool.free(cur);
						cur = next;
					}
					head_ref = nullptr;
				}
			}
		}

		/// Retrieves the object at the given location.
		[[nodiscard]] Value &at(reference ref) {
			return _node_pool.at(ref._ref.get()).object;
		}
		/// Retrieves the object at the given location.
		[[nodiscard]] const Value &at(reference ref) const {
			return _node_pool.at(ref._ref.get()).object;
		}

		/// Finds an element in this hash table with the given hash and predicate.
		template <typename Pred> [[nodiscard]] reference find(Index hash, Pred pred) const {
			Index slot = hash % _head.size();
			for (auto ref = _head[slot]; ref; ref = _node_pool.at(ref.get()).next) {
				if (pred(_node_pool.at(ref.get()).object)) {
					return reference(ref);
				}
			}
			return nullptr;
		}
		/// \overload
		template <
			typename Equal = std::equal_to<void>
		> [[nodiscard]] reference find(const Value &val, const Equal &equal = Equal{}) const {
			return find(Hash{}(val), [&val, &equal](const Value &entry) {
				return equal(entry, val);
			});
		}

		/// Returns the capacity of the pool for objects (not to be confused with the number of bins).
		[[nodiscard]] Index get_pool_capacity() const {
			return _node_pool.get_capacity();
		}
		/// Returns the number of bins used for hashing.
		[[nodiscard]] Index get_num_bins() const {
			return _head.size();
		}
	private:
		/// A node in the linked list.
		struct _node {
			/// Initializes \ref object.
			template <typename ...Args> explicit _node(Args &&...args) :
				object(std::forward<Args>(args)...), next(nullptr) {
			}

			Value object; ///< Object contained in this node.
			typename _pool_type::optional_index_type next; ///< Reference to the next node.
		};

		/// Allocates memory for this hash table.
		pooled_hash_table(Index head_size, Index pool_size) :
			_node_pool(_pool_type::create(pool_size)), _head(head_size, nullptr) {
		}

		_pool_type _node_pool; ///< A pool of nodes.
		std::vector<typename _pool_type::optional_index_type> _head; ///< Slots for objects with different hashes.
	};
}
