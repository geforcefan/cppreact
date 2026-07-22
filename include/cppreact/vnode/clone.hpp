#pragma once

#include <vector>

#include "create.hpp"

namespace cppreact {

inline std::vector<VNode> clone_vnodes(const std::vector<VNode>& children) {
  std::vector<VNode> copies;
  copies.reserve(children.size());
  for (const VNode& child : children) copies.push_back(clone_vnode(child));
  return copies;
}

inline VNode clone_vnode(const VNode& node) {
  VNode copy;
  copy.type = node.type;
  copy.key = node.key;
  copy.ref = node.ref;
  copy.children = clone_vnodes(node.children);
  return copy;
}

}
