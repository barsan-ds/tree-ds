#pragma once

#include <functional> // std::reference_wrapper
#include <optional>

#include <TreeDS/allocator_utility.hpp>
#include <TreeDS/matcher/node/matcher.hpp>
#include <TreeDS/matcher/pattern.hpp>
#include <TreeDS/matcher/value/true_matcher.hpp>
#include <TreeDS/policy/siblings.hpp>

namespace md {

template <quantifier Quantifier, typename ValueMatcher, typename FirstChild, typename NextSibling>
class opt_matcher : public matcher<
                        opt_matcher<Quantifier, ValueMatcher, FirstChild, NextSibling>,
                        ValueMatcher,
                        FirstChild,
                        NextSibling> {

    /*   ---   FRIENDS   ---   */
    template <typename, typename, typename, typename>
    friend class matcher;

    /*   ---   ATTRIBUTES   ---   */
    public:
    static constexpr matcher_info_t info {
        // Matches null
        opt_matcher::foldl_children_types(
            [](auto&& accumulated, auto&& element) {
                using element_type = typename std::decay_t<decltype(element)>::type;
                return accumulated && element_type::info.matches_null;
            },
            true),
        // It matches null if we consider just this node
        true,
        // Reluctant behavior
        Quantifier == quantifier::RELUCTANT,
        Quantifier == quantifier::POSSESSIVE};

    /*   ---   CONSTRUCTOR   ---   */
    using matcher<opt_matcher, ValueMatcher, FirstChild, NextSibling>::matcher;

    /*   ---   METHODS   ---   */
    template <typename NodeAllocator>
    bool search_node_impl(allocator_value_type<NodeAllocator>& node, NodeAllocator& allocator) {
        if constexpr (opt_matcher::info.reluctant && opt_matcher::child_may_steal_target()) {
            if (this->let_child_steal(node, allocator)) {
                return true;
            }
        }
        if (!this->match_value(node.get_value())) {
            if constexpr (opt_matcher::info.possessive) {
                return false;
            } else {
                return this->let_child_steal(node, allocator);
            }
        }
        auto target = policy::siblings().get_instance(
            node.get_first_child(),
            node_navigator<allocator_value_type<NodeAllocator>*>(),
            allocator);
        // Match children of the pattern
        auto do_search_child = [&](auto& it, auto& child) -> bool {
            return child.search_node(it.get_current_node(), allocator);
        };
        bool children_matched = this->search_children(allocator, std::move(target), do_search_child);
        if (!opt_matcher::info.possessive && opt_matcher::child_may_steal_target() && !children_matched) {
            return this->let_child_steal(node, allocator);
        }
        return children_matched;
    }

    template <typename NodeAllocator>
    unique_node_ptr<NodeAllocator> result_impl(NodeAllocator& allocator) {
        unique_node_ptr<NodeAllocator> result;
        if constexpr (!opt_matcher::info.possessive) {
            if (this->did_child_steal_target(result, allocator)) {
                return std::move(result);
            }
        }
        result            = this->clone_node(allocator);
        auto attach_child = [&](auto& child) {
            if (!child.empty()) {
                result->assign_child_like(
                    child.result(allocator),
                    *child.get_node(allocator));
            }
        };
        std::apply(
            [&](auto&... children) {
                (..., attach_child(children));
            },
            this->get_children());
        return std::move(result);
    }

    template <typename... Nodes>
    constexpr opt_matcher<Quantifier, ValueMatcher, Nodes...> replace_children(Nodes&... nodes) const {
        return {this->value, nodes...};
    }

    template <typename Child>
    constexpr opt_matcher<Quantifier, ValueMatcher, std::remove_reference_t<Child>, NextSibling>
    with_first_child(Child&& child) const {
        return {this->value, child, this->next_sibling};
    }

    template <typename Sibling>
    constexpr opt_matcher<Quantifier, ValueMatcher, FirstChild, std::remove_reference_t<Sibling>>
    with_next_sibling(Sibling&& sibling) const {
        return {this->value, this->first_child, sibling};
    }
};

template <quantifier Quantifier = quantifier::DEFAULT, typename ValueMatcher>
opt_matcher<Quantifier, ValueMatcher, detail::empty_t, detail::empty_t> opt(const ValueMatcher& value_matcher) {
    return {value_matcher, detail::empty_t(), detail::empty_t()};
}
template <quantifier Quantifier = quantifier::DEFAULT>
opt_matcher<Quantifier, true_matcher, detail::empty_t, detail::empty_t> opt() {
    return {true_matcher(), detail::empty_t(), detail::empty_t()};
}

} // namespace md