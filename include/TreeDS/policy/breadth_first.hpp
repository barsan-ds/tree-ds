#pragma once

#include <deque>
#include <memory> // std::allocator_traits

#include <TreeDS/policy/policy_base.hpp>
#include <TreeDS/utility.hpp>

namespace md::detail {

/**
 * Traversal policy that returns nodes in a line by line fashion. In forward order the nodes will be retrieved from left
 * to right, and from top to bottom.  Please note that this iterator is intended to be usedforward only (incremented
 * only). Reverse order iteration is possible (and tested) though it will imply severe performance drop.
 */
template <typename NodePtr, typename NodeNavigator, typename Allocator>
class breadth_first_impl final
        : public policy_base<breadth_first_impl<NodePtr, NodeNavigator, Allocator>, NodePtr, NodeNavigator, Allocator> {

    public:
    using typename policy_base<breadth_first_impl, NodePtr, NodeNavigator, Allocator>::allocator_type;

    private:
    std::deque<NodePtr, allocator_type> open_nodes = manage_initial_status();

    public:
    using policy_base<breadth_first_impl, NodePtr, NodeNavigator, Allocator>::policy_base;

    // Formward puhes into open back and pops front
    NodePtr increment_impl() {
        if (this->open_nodes.empty()) {
            return nullptr;
        }
        // Get element to be returned
        NodePtr result = this->open_nodes.front();
        // Manage next sibling replacement in queue
        NodePtr sibling = this->navigator.get_next_sibling(result);
        if (sibling) {
            this->open_nodes.front() = sibling;
        } else {
            this->open_nodes.pop_front();
        }
        // Push back its first child
        NodePtr first_child = this->navigator.get_first_child(result);
        if (first_child) {
            this->open_nodes.push_back(first_child);
        }
        return result;
    }

    NodePtr decrement_impl() {
        // Delete the child of current node from open_nodes
        NodePtr first_child = this->navigator.get_first_child(this->current);
        if (first_child) {
            assert(this->navigator.get_parent(this->open_nodes.back()) == this->current);
            // Delete the child of the previous node from open_nodes (invariants garantee that it is the last element)
            this->open_nodes.pop_back();
        }
        // Delete next sibling of the current node from open_nodes
        if (this->navigator.get_next_sibling(this->current)) {
            assert(this->navigator.get_prev_sibling(this->open_nodes.front()) == this->current);
            this->open_nodes.pop_front();
        }
        // Calculate the previous element
        NodePtr result = this->navigator.get_left_branch(this->current);
        if (result == nullptr) {
            result = this->navigator.get_same_row_rightmost(this->navigator.get_parent(this->current));
        }
        // Update queue
        this->open_nodes.push_front(this->current);
        return result;
    }

    NodePtr go_first_impl() {
        this->open_nodes.clear();
        this->open_nodes.push_back(this->navigator.get_root());
        return this->increment_impl();
    }

    NodePtr go_last_impl() {
        this->open_nodes.clear();
        return this->navigator.get_deepest_rightmost_leaf();
    }

    std::deque<NodePtr, allocator_type> manage_initial_status() {
        std::deque<NodePtr, allocator_type> result;
        if (this->current == nullptr) {
            return result;
        }
        auto process_child = [&](NodePtr node) {
            NodePtr first_child = this->navigator.get_first_child(node);
            if (first_child) {
                result.push_back(first_child);
            }
        };
        // Manage next_sibling insertion
        NodePtr node = this->navigator.get_next_sibling(this->current);
        if (node) {
            result.push_back(node);
        }
        // Manage right elements
        node = this->navigator.is_root(this->current)
            ? nullptr
            : this->navigator.get_right_branch(this->navigator.get_parent(this->current));
        while (node) {
            process_child(node);
            node = this->navigator.get_right_branch(node);
        }
        // Manage lower row, left elements
        node = this->navigator.get_same_row_leftmost(this->current);
        while (node && node != this->current) {
            process_child(node);
            node = this->navigator.get_right_branch(node);
        }
        // Manage current node child
        node = this->current;
        process_child(node);
        return std::move(result);
    }

    void update(NodePtr current, NodePtr replacement) {
        // Delete child of the previous nodes from open_nodes
        if (this->navigator.get_first_child(current)) {
            assert(this->navigator.get_parent(this->open_nodes.back()) == current);
            this->open_nodes.pop_back();
        }
        // Push back the children of first child
        NodePtr child = replacement
            ? this->navigator.get_first_child(replacement)
            : nullptr;
        if (child != nullptr) {
            this->open_nodes.push_back(child);
        }
        this->policy_base<breadth_first_impl, NodePtr, NodeNavigator, Allocator>::update(
            current,
            replacement);
    }
};

} // namespace md::detail

namespace md::policy {

struct breadth_first : detail::policy_tag<detail::breadth_first_impl> {
    // What needed is inherited
};
} // namespace md::policy
