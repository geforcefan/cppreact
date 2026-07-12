#pragma once

#include <optional>
#include <string>
#include <utility>
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
  copy.props = node.props;
  copy.key = node.key;
  copy.ref = node.ref;
  copy.children = clone_vnodes(node.children);
  return copy;
}

inline VNode clone_element(const VNode& vnode, Object props,
                           std::optional<std::vector<VNode>> children = std::nullopt) {
  Object normalized_props = vnode.props;
  std::string key = vnode.key;
  Reference ref = vnode.ref;

  for (const auto& [name, value] : props) {
    if (name == "key") {
      Object key_props{{"key", value}};
      key = detail::extract_key(key_props);
    } else if (name == "ref" && !is_component(vnode)) {
      Object reference_props{{"ref", value}};
      ref = detail::extract_reference(reference_props);
    } else {
      normalized_props.set(name, value);
    }
  }

  std::vector<VNode> next_children =
      children ? std::move(*children) : clone_vnodes(vnode.children);

  return create_vnode(vnode.type, std::move(normalized_props), std::move(key), std::move(ref),
                      std::move(next_children));
}

}
