#pragma once

/// \file
/// An intrusive linked list.

#include <optional>

#include "lotus/common.h"

namespace lotus {
	/// An intrusive doubly-linked list.
	///
	/// \tparam Ref References to objects containing other nodes. It must support copying and equality tests.
	template <typename Ref> class intrusive_linked_list {
	public:
		/// This class is only used as a pseudo-namespace for the node and head types.
		intrusive_linked_list() = delete;

		/// A node that contains a pair of references.
		///
		/// \tparam Getter Used to dereference references stored in the node. It must contain a field \p is_opaque:
		///                - If it's \p true, the linked list will have no information about the object containing
		///                  the node itself, and the \p Getter::dereference() function of the getter will return
		///                  another node.
		///                - If it's \p false, the \p Getter::dereference() function will return the object
		///                  containing the node, and \p Getter::get_node() will return the node inside that
		///                  object.
		struct node {
			friend intrusive_linked_list;
		public:
			/// Creates a node with invalid references to other nodes. \ref update_this() must be used to initialize
			/// the node properly.
			[[nodiscard]] inline static node create_uninitialized(Ref bad_ref = uninitialized) {
				return node(bad_ref);
			}
			/// Used to initialize this node after \ref create_uninitialized().
			void update_this(Ref self) {
				crash_if(!is_isolated());
				_prev = _next = self;
			}
			/// Creates a new node whose previous and next nodes are the node itself.
			[[nodiscard]] inline static node create_from_this(Ref ref) {
				return node(ref); // guaranteed copy elision
			}
			/// No copy and move construction.
			node(const node&) = delete;
			/// No copy and move assignment.
			node &operator=(const node&) = delete;
			/// Checks that this node has been properly detached.
			~node() {
				crash_if(!is_isolated());
			}

			/// Returns the reference to the next node.
			[[nodiscard]] Ref next_ref() const {
				return _next;
			}
			/// Dereferences and retrieves the next node or object that contains this node.
			template <typename Getter> [[nodiscard]] auto &next(const Getter &getter = Getter()) const {
				return getter.dereference(next_ref());
			}
			/// Dereferences and retrieves the next node.
			template <typename Getter> [[nodiscard]] node &next_node(const Getter &getter = Getter()) const {
				return _deref(next_ref(), getter);
			}
			/// Returns the reference to the previous node.
			[[nodiscard]] Ref previous_ref() const {
				return _prev;
			}
			/// Dereferences and retrieves the previous node or object that contains the node.
			template <typename Getter> [[nodiscard]] auto &previous(const Getter &getter = Getter()) const {
				return getter.dereference(previous_ref());
			}
			/// Dereferences and retrieves the previous node.
			template <typename Getter> [[nodiscard]] node &previous_node(const Getter &getter = Getter()) const {
				return _deref(previous_ref(), getter);
			}

			/// Returns whether this node is not in a linked list - i.e., whether both the references are null.
			[[nodiscard]] bool is_isolated() const {
				return next_ref() == previous_ref();
			}

			/// Links this node after the given one. This node must be isolated before the call.
			template <typename Getter> void link_after(node &prev_n, const Getter &getter = Getter()) {
				// for an isolated node, _next and _prev will point to this node itself
				crash_if(!is_isolated() || &next_node(getter) != this);
				node &next_n = prev_n.next_node(getter);
				std::swap(prev_n._next, _next);
				std::swap(next_n._prev, _prev);
			}
			/// \overload
			template <typename Getter> void link_after(Ref prev_n, const Getter &getter = Getter()) {
				link_after(_deref(prev_n, getter), getter);
			}
			/// Detaches this node from the linked list it's currently in.
			///
			/// \return A reference to the next node in the list, if this node is not the only node in the list.
			template <typename Getter> std::optional<Ref> detach(const Getter &getter = Getter()) {
				if (is_isolated()) {
					return std::nullopt;
				}
				Ref result = _next;
				node &next_n = next_node(getter);
				node &prev_n = previous_node(getter);
				std::swap(prev_n._next, _next);
				std::swap(next_n._prev, _prev);
				crash_if_debug(!is_isolated() || &previous_node(getter) != this); // integrity check
				return result;
			}
		private:
			Ref _next; ///< Pointer to the next node.
			Ref _prev; ///< Pointer to the previous node.

			/// Initializes this node by setting both pointers to point to itself (which the caller must guarantee,
			/// since there's no way for a node to obtain a reference to itself).
			explicit node(Ref self) : _next(self), _prev(self) {
			}

			/// Dereferences the given reference and returns the node.
			template <typename Getter> [[nodiscard]] inline static node &_deref(Ref r, const Getter &getter) {
				auto &deref = getter.dereference(r);
				if constexpr (Getter::is_opaque) {
					return deref;
				} else {
					return getter.get_node(deref);
				}
			}
		};

		/// Breaks both linked lists after the given nodes, then inserts the second list after the first one, with
		/// the node after the given node being the first one inserted and the given node being the last.
		template <typename Getter> void splice_after(
			node &first_prev, node &second_prev, const Getter &getter = Getter()
		) {
			node &first_next = first_prev.next_node(getter);
			node &second_next = second_prev.next_node(getter);
			std::swap(first_prev._next, second_prev._next);
			std::swap(first_next._prev, second_next._prev);
		}

		/// Getter when the references are pointers. \p get_node() needs to be implemented manually.
		template <typename ValueType> struct default_getter {
		public:
			/// Not an opaque getter.
			constexpr static bool is_opaque = false;
			/// Type that contains the linked list nodes.
			using value_type = ValueType;

			/// Dereferences the pointer.
			[[nodiscard]] inline static value_type &dereference(value_type *ref) {
				return *ref;
			}
		};
		/// Getter when the references are indices. \p get_node() needs to be implemented manually.
		template <typename Value, typename Container> struct indexed_getter {
		public:
			/// Not an opaque getter.
			constexpr static bool is_opaque = false;
			/// Type that contains the linked nodes.
			using value_type = Value;

			/// Dereferencing.
			[[nodiscard]] value_type &dereference(Ref ref) const {
				return _container.at(ref);
			}
		protected:
			/// Initializes \ref _container.
			explicit indexed_getter(Container &con) : _container(con) {
			}
		private:
			Container &_container; ///< The container.
		};
	};
}
