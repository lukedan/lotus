#pragma once

/// \file
/// Axis aligned bounding box trees.

#include "lotus/memory/common.h"
#include "lotus/profiler.h"
#include "lotus/math/aab.h"
#include "lotus/collision/common.h"

namespace lotus::collision {
	/// Axis aligned bounding box trees.
	template <typename T, u32 Order = 4, typename Allocator = memory::raw::allocator> class aabb_tree {
	public:
		using index_t = minimum_unsigned_type_t<Order>; ///< Used for representing child indices within nodes.

		/// Base class of tree nodes.
		struct node {
		};
		/// An intermediate tree node.
		struct intermediate_node : node {
			friend aabb_tree;
		public:
			/// Returns the parent node.
			[[nodiscard]] intermediate_node *get_parent() const {
				return _parent;
			}
			/// Returns the i-th child.
			[[nodiscard]] node *get_child(index_t i) const {
				return _children[i];
			}
			/// Returns the bounding box of the i-th child.
			[[nodiscard]] aab3s get_child_bounding_box(index_t i) const {
				return _children_aabb[i];
			}
			/// Returns the number of primitives in the i-th child.
			[[nodiscard]] u32 get_child_num_primitives(index_t i) const {
				return _children_num_primitives[i];
			}
			/// Returns whether the i-th child is a leaf.
			[[nodiscard]] bool is_child_leaf(index_t i) const {
				return _is_leaf & (1u << i);
			}

			/// Computes the AABB of this node as the union of all children's AABBs.
			[[nodiscard]] aab3s compute_aabb() const {
				aab3s result = aab3s::create_infinity().negated();
				for (index_t i = 0; i < Order; ++i) {
					if (_children[i]) {
						result = aab3s::minimum_containing({ result, _children_aabb[i] });
					}
				}
				return result;
			}
			/// Computes the size of this subtree as the sum of all children's sizes.
			[[nodiscard]] u32 compute_size() const {
				u32 result = 1;
				for (index_t i = 0; i < Order; ++i) {
					if (!_children[i]) {
						continue;
					}
					result += is_child_leaf(i) ? 1 : static_cast<intermediate_node*>(_children[i])->_size;
				}
				return result;
			}
			/// Returns the number of primitives in this subtree.
			[[nodiscard]] u32 compute_num_primitives() const {
				u32 result = 0;
				for (const u32 n : _children_num_primitives) {
					result += n;
				}
				return result;
			}
		private:
			std::array<aab3s, Order> _children_aabb = {}; ///< The bounding box of all children.
			std::array<node*, Order> _children = {}; ///< Children of this node.
			/// The total number of primitives in each child.
			std::array<u32, Order> _children_num_primitives = {};
			intermediate_node *_parent = nullptr; ///< The parent node.
			index_t _parent_index = 0; ///< Index of this node in the parent.
			minimum_unsigned_type_bits_t<Order> _is_leaf = 0; ///< Bits indicating whether each child is a leaf.

			/// Fetches AABB information from the parent.
			[[nodiscard]] aab3s &_aabb_in_parent() const {
				return _parent->_children_aabb[_parent_index];
			}
			/// Fetches the number of primitives from the parent.
			[[nodiscard]] u32 &_num_primitives_in_parent() const {
				return _parent->_children_num_primitives[_parent_index];
			}

			/// Sets that the i-th child is a leaf.
			void _set_child_leaf(index_t i) {
				_is_leaf |= 1u << i;
			}
			/// Sets that the i-th child is not a leaf.
			void _set_child_non_leaf(index_t i) {
				_is_leaf &= ~(1u << i);
			}
		};
		/// A leaf node.
		struct leaf_node : node {
			friend aabb_tree;
		public:
			/// Initializes \ref value.
			explicit leaf_node(T v) : value(std::move(v)) {
			}

			/// Returns the parent node.
			[[nodiscard]] intermediate_node *get_parent() const {
				return _parent;
			}
			/// Returns the index of this node within the parent node.
			[[nodiscard]] index_t get_index_in_parent() const {
				return _parent_index;
			}

			T value; ///< Value.
		private:
			intermediate_node *_parent = nullptr; ///< The parent node.
			index_t _parent_index = 0; ///< Index of this node in the parent.

			/// Fetches AABB information from the parent.
			[[nodiscard]] aab3s &_aabb_in_parent() const {
				return _parent->_children_aabb[_parent_index];
			}
			/// Fetches the number of primitives from the parent.
			[[nodiscard]] u32 &_num_primitives_in_parent() const {
				return _parent->_children_num_primitives[_parent_index];
			}
		};

		/// Initializes the AABB tree.
		explicit aabb_tree(const Allocator &allocator = Allocator()) : _allocator(allocator) {
		}
		/// Move constructor.
		aabb_tree(aabb_tree &&src) noexcept :
			_root(std::exchange(src._root, nullptr)), _allocator(std::move(src._allocator)) {
		}
		/// No copy construction.
		aabb_tree(const aabb_tree&) = delete;
		/// Move assignment.
		aabb_tree &operator=(aabb_tree &&src) {
			clear();
			_root = std::exchange(src._root, nullptr);
			_allocator = std::move(src._allocator);
			return *this;
		}
		/// No copy assignment.
		aabb_tree &operator=(const aabb_tree&) = delete;
		/// Calls \ref clear().
		~aabb_tree() {
			clear();
		}

		/// Creates a new detached node.
		[[nodiscard]] leaf_node *create_node(T value) {
			return new (_allocator.allocate(memory::size_alignment::of<leaf_node>())) leaf_node(std::move(value));
		}
		/// Inserts a node into this tree.
		void insert(leaf_node *node, aab3s bb, u32 num_primitives = 1) {
			crash_if(node->_parent != nullptr);
			if (!_root) {
				_root = new (
					_allocator.allocate(memory::size_alignment::of<intermediate_node>())
				) intermediate_node();
				return _insert_at(_root, 0, node, bb, num_primitives);
			}
			for (intermediate_node *cur = _root; cur; ) {
				scalar best_cost = std::numeric_limits<scalar>::max();
				index_t best_child = 0;
				for (index_t i = 0; i < Order; ++i) {
					if (!cur->_children[i]) {
						return _insert_at(cur, i, node, bb, num_primitives);
					}
					const aab3s cur_bb = cur->_children_aabb[i];
					const scalar cur_heuristic = heuristic(cur_bb, cur->get_child_num_primitives(i));
					const scalar new_heuristic = heuristic(
						aab3s::minimum_containing({ cur_bb, bb }), cur->get_child_num_primitives(i) + num_primitives
					);
					const scalar estimated_cost = new_heuristic - cur_heuristic;
					if (estimated_cost < best_cost) {
						best_cost = estimated_cost;
						best_child = i;
					}
				}

				if (cur->is_child_leaf(best_child)) {
					return _insert_at(cur, best_child, node, bb, num_primitives);
				} else {
					cur = static_cast<intermediate_node*>(cur->_children[best_child]);
				}
			}
			std::unreachable();
		}
		/// Allocates and inserts a node into this tree.
		[[nodiscard]] leaf_node *insert(aab3s bb, T value) {
			leaf_node *result = create_node(std::move(value));
			insert(result, bb);
			return result;
		}
		/// Updates the given node to have a new bounding box.
		void update(leaf_node *n, aab3s new_bb) {
			n->_aabb_in_parent() = new_bb;
			for (intermediate_node *cur = n->_parent; cur->_parent; cur = cur->_parent) {
				cur->_aabb_in_parent() = cur->compute_aabb();
			}

			// optimize the tree
			for (intermediate_node *cur = n->_parent; cur->_parent; ) {
				const std::optional<index_t> promotion = _find_best_promotion(cur);
				if (!promotion) {
					break;
				}
				_promote(cur, promotion.value()); // cur replaces its parent - no need to update
			}
		}
		/// Detaches the leaf node from this tree without freeing it. The node will need to be disposed of manually.
		void detach(leaf_node *leaf) {
			if (!leaf->_parent) {
				return;
			}

			intermediate_node *old_parent = std::exchange(leaf->_parent, nullptr);

			// update parent
			old_parent->_children[leaf->_parent_index] = nullptr;
			old_parent->_set_child_non_leaf(leaf->_parent_index);

			// update path to root
			u32 nodes_erased = 1;
			for (intermediate_node *cur = old_parent; cur->_parent; ) {
				intermediate_node *parent = cur->_parent;

				cur->_size -= nodes_erased;
				if (cur->_size == 1) { // erase empty node
					// update parent
					parent->_children[cur->_parent_index] = nullptr;

					// destroy cur
					std::destroy_at(cur);
					_allocator.free(cur);
					++nodes_erased;
				} else { // update parent with new abb
					cur->_aabb_in_parent() = cur->compute_aabb();
				}

				cur = parent;
			}
			crash_if(_root->_size <= nodes_erased);
			_root->_size -= nodes_erased;
		}
		/// Erases the given leaf from this tree and frees it.
		void erase(leaf_node *leaf) {
			detach(leaf);

			// destroy & free leaf
			std::destroy_at(leaf);
			_allocator.free(leaf);
		}

		/// Calls \p callback with all \ref leaf_node objects that intersect the given box.
		template <typename Cb> void query_aab(aab3s box, Cb &&callback) const {
			if (!_root) {
				return;
			}

			auto bookmark = get_scratch_bookmark();
			auto stack = bookmark.create_vector_array<intermediate_node*>(1u, _root);
			while (!stack.empty()) {
				intermediate_node *const cur = stack.back();
				stack.pop_back();

				for (index_t i = 0; i < Order; ++i) {
					if (!cur->_children[i] || !aab3s::intersects(cur->_children_aabb[i], box)) {
						continue;
					}
					if (cur->is_child_leaf(i)) {
						callback(static_cast<leaf_node*>(cur->_children[i]));
					} else {
						stack.emplace_back(static_cast<intermediate_node*>(cur->_children[i]));
					}
				}
			}
		}
		/// Calls \p intersect_only1 with all leaves that intersect \p b1 but not \p b2, calls \p intersect_only2
		/// with all leaves that intersect \p b2 but not \p b1, and calls \p intersect_both with all leaves that
		/// intersect both \p b1 and \p b2.
		template <typename Cb1, typename Cb2, typename CbBoth> void query_dual_aab(
			aab3s b1, aab3s b2, Cb1 intersect_only1, Cb2 intersect_only2, CbBoth intersect_both
		) const {
			if (!_root) {
				return;
			}

			auto bookmark = get_scratch_bookmark();
			auto stack = bookmark.create_vector_array<intermediate_node*>(1u, _root);
			while (!stack.empty()) {
				intermediate_node *const cur = stack.back();
				stack.pop_back();

				for (index_t i = 0; i < Order; ++i) {
					if (!cur->_children[i]) {
						continue;
					}
					const bool intersect1 = aab3s::intersects(cur->_children_aabb[i], b1);
					const bool intersect2 = aab3s::intersects(cur->_children_aabb[i], b2);
					if (!intersect1 && !intersect2) {
						continue;
					}
					if (cur->is_child_leaf(i)) {
						auto *leaf = static_cast<leaf_node*>(cur->_children[i]);
						if (!intersect1) {
							intersect_only2(leaf);
						} else if (!intersect2) {
							intersect_only1(leaf);
						} else {
							intersect_both(leaf);
						}
					} else {
						stack.emplace_back(static_cast<intermediate_node*>(cur->_children[i]));
					}
				}
			}
		}

		/// Destroys all nodes in this tree.
		void clear() {
			if (!_root) {
				return;
			}

			auto bookmark = get_scratch_bookmark();

			auto nodes_to_destroy = bookmark.create_vector_array<intermediate_node*>(1u, _root);
			while (!nodes_to_destroy.empty()) {
				intermediate_node *node = nodes_to_destroy.back();
				nodes_to_destroy.pop_back();
				for (index_t i = 0; i < Order; ++i) {
					if (node->_children[i]) {
						if (node->is_child_leaf(i)) {
							std::destroy_at(static_cast<leaf_node*>(node->_children[i]));
							_allocator.free(node->_children[i]);
						} else {
							nodes_to_destroy.emplace_back(static_cast<intermediate_node*>(node->_children[i]));
						}
					}
				}
				std::destroy_at(node);
				_allocator.free(node);
			}
			_root = nullptr;
		}

		/// Returns the root node.
		[[nodiscard]] intermediate_node *get_root() const {
			return _root;
		}

		/// For debugging only: Validates all links and metrics in this tree are valid.
		template <typename Cb> void validate(Cb error_callback) const {
			if (!_root) {
				return;
			}

			auto bookmark = get_scratch_bookmark();

			if (_root->_parent != nullptr) {
				error_callback(_root, u8"Root parent non-null");
			}
			auto stack = bookmark.create_vector_array<const intermediate_node*>(1, _root);
			while (!stack.empty()) {
				const intermediate_node *cur = stack.back();
				stack.pop_back();

				// check children
				for (index_t i = 0; i < Order; ++i) {
					if (!cur->_children[i]) {
						continue;
					}
					if (cur->is_child_leaf(i)) {
						const auto* child = static_cast<const leaf_node*>(cur->_children[i]);
						if (child->_parent != cur) {
							error_callback(child, u8"Incorrect leaf node parent");
						}
						if (child->_parent_index != i) {
							error_callback(child, u8"Incorrect leaf node parent index");
						}
					} else {
						const auto *child = static_cast<const intermediate_node*>(cur->_children[i]);
						if (child->_parent != cur) {
							error_callback(child, u8"Incorrect node parent");
						}
						if (child->_parent_index != i) {
							error_callback(child, u8"Incorrect node parent index");
						}
						if (child->compute_aabb() != cur->_children_aabb[i]) {
							error_callback(child, u8"Incorrect aabb");
						}
						if (child->compute_num_primitives() != cur->_children_num_primitives[i]) {
							error_callback(child, u8"Incorrect size");
						}
						stack.emplace_back(child);
					}
				}
			}
		}

		/// Surface area heuristic.
		[[nodiscard]] constexpr static scalar heuristic(aab3s bb, u32 num_primitives) {
			const vec3 sz = bb.signed_size();
			const scalar area = sz[0] * sz[1] + sz[0] * sz[2] + sz[1] * sz[2];
			return static_cast<scalar>(num_primitives) * area;
		}
	private:
		intermediate_node *_root = nullptr; ///< The root node.
		[[no_unique_address]] Allocator _allocator; ///< Allocator.

		/// Inserts the given node at the given location.
		void _insert_at(intermediate_node *parent, index_t index, leaf_node *node, aab3s bb, u32 num_primitives) {
			if (parent->is_child_leaf(index)) {
				leaf_node *old_child = static_cast<leaf_node*>(parent->_children[index]);
				auto *new_child =
					new (_allocator.allocate(memory::size_alignment::of<intermediate_node>())) intermediate_node();

				parent->_children[index] = new_child;
				parent->_set_child_non_leaf(index);

				new_child->_parent = parent;
				new_child->_parent_index = index;
				new_child->_children[0] = old_child;
				new_child->_children_aabb[0] = parent->_children_aabb[index];
				new_child->_children_num_primitives[0] = parent->_children_num_primitives[index];
				new_child->_set_child_leaf(0);

				old_child->_parent = new_child;
				old_child->_parent_index = 0;

				parent = new_child;
				index = 1;
			} else {
				crash_if(parent->_children[index] != nullptr);
			}

			node->_parent = parent;
			node->_parent_index = index;

			// update parent
			parent->_children[index] = node;
			parent->_children_aabb[index] = bb;
			parent->_children_num_primitives[index] = num_primitives;
			parent->_set_child_leaf(index);

			// update path to root
			for (intermediate_node *cur = parent; cur->_parent; cur = cur->_parent) {
				cur->_num_primitives_in_parent() += num_primitives;
				aab3s &cur_bb = cur->_aabb_in_parent();
				cur_bb = aab3s::minimum_containing({ cur_bb, bb });
			}
		}

		/// Finds the promotion that would result in the most heuristic reduction for the tree.
		[[nodiscard]] static std::optional<index_t> _find_best_promotion(intermediate_node *node) {
			// the heuristic of the parent node will not change - we only need to focus on the child node
			const scalar original_heuristic = heuristic(node->_aabb_in_parent(), node->_num_primitives_in_parent());

			aab3s parent_bb_no_this = aab3s::create_infinity().negated();
			u32 parent_num_prims_no_this = 0;
			for (index_t i = 0; i < Order; ++i) {
				if (i == node->_parent_index) {
					continue;
				}
				parent_bb_no_this =
					aab3s::minimum_containing({ parent_bb_no_this, node->_parent->_children_aabb[i] });
				parent_num_prims_no_this += node->_parent->_children_num_primitives[i];
			}

			std::optional<index_t> best_promotion;
			scalar best_heuristic = std::numeric_limits<scalar>::max();
			for (index_t i = 0; i < Order; ++i) {
				const aab3s new_bb =
					node->_children[i] ?
					aab3s::minimum_containing({ parent_bb_no_this, node->_children_aabb[i] }) :
					parent_bb_no_this;
				const u32 new_num_primitives = parent_num_prims_no_this + node->_children_num_primitives[i];
				const scalar new_heuristic = heuristic(new_bb, new_num_primitives);
				if (new_heuristic < best_heuristic) {
					best_promotion = i;
					best_heuristic = new_heuristic;
				}
			}
			return best_heuristic < original_heuristic ? best_promotion : std::nullopt;
		}
		/// Rotates a given node with its parent, replacing the children at the given index with the parent node.
		void _promote(intermediate_node *node, index_t replace_id) {
			intermediate_node *const parent = node->_parent;
			const index_t old_parent_index = node->_parent_index;

			// link node with parent->parent
			node->_parent = parent->_parent;
			node->_parent_index = parent->_parent_index;
			if (node->_parent) {
				node->_parent->_children[node->_parent_index] = node;
			} else {
				_root = node;
			}
			// aabb unchanged

			// link child with parent
			parent->_children[old_parent_index] = node->_children[replace_id];
			if (node->_children[replace_id]) {
				if (node->is_child_leaf(replace_id)) {
					auto *const child = static_cast<leaf_node*>(node->_children[replace_id]);
					child->_parent = parent;
					child->_parent_index = old_parent_index;
					parent->_set_child_leaf(old_parent_index);
				} else {
					auto *const child = static_cast<intermediate_node*>(node->_children[replace_id]);
					child->_parent = parent;
					child->_parent_index = old_parent_index;
				}
			}

			// link parent with node
			node->_children[replace_id] = parent;
			node->_set_child_non_leaf(replace_id);
			parent->_parent = node;
			parent->_parent_index = replace_id;

			// update AABB
			parent->_children_aabb[old_parent_index] = node->_children_aabb[replace_id];
			node->_children_aabb[replace_id] = parent->compute_aabb();
			// update num primitives
			parent->_children_num_primitives[old_parent_index] = node->_children_num_primitives[replace_id];
			node->_children_num_primitives[replace_id] = parent->compute_num_primitives();
		}
	};
}
