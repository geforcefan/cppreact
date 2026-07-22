#pragma once

#include <type_traits>
#include <utility>
#include <vector>

#include "vnode.hpp"

namespace cppreact {

VNode clone_vnode(const VNode& node);

std::vector<VNode> clone_vnodes(const std::vector<VNode>& children);

struct Children {
  std::vector<VNode> nodes{};

  Children() = default;

  Children(std::vector<VNode> initial) : nodes(std::move(initial)) {}

  template <class... Nodes>
    requires(sizeof...(Nodes) >= 1 && (std::is_constructible_v<VNode, Nodes &&> && ...))
  Children(Nodes&&... child_nodes) {
    nodes.reserve(sizeof...(Nodes));
    (nodes.push_back(VNode(std::forward<Nodes>(child_nodes))), ...);
  }

  Children(const Children& other) : nodes(clone_vnodes(other.nodes)) {}
  Children(Children&&) = default;
  Children& operator=(const Children& other) {
    nodes = clone_vnodes(other.nodes);
    return *this;
  }
  Children& operator=(Children&&) = default;

  operator VNode() const;

  bool operator==(const Children& other) const {
    return nodes.empty() && other.nodes.empty();
  }
};

}
