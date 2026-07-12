#pragma once

#include <initializer_list>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "vnode.hpp"
#include "../component/function_component.hpp"
#include "options.hpp"
#include "../value/object.hpp"

namespace cppreact {

inline VNode create_vnode(VNodeType type, Object props, std::string key, Reference ref,
                          std::vector<VNode> children) {
  VNode vnode;
  vnode.type = std::move(type);
  vnode.props = std::move(props);
  vnode.key = std::move(key);
  vnode.ref = std::move(ref);
  vnode.children = std::move(children);
  if (options().vnode) options().vnode(vnode);
  return vnode;
}

namespace detail {

inline std::string extract_key(Object& props) {
  Value key = props.take("key");
  if (const std::string* text = value_as<std::string>(key)) return *text;
  if (const double* number = value_as<double>(key)) return number_to_text(*number);
  return {};
}

inline Reference extract_reference(Object& props) {
  Value value = props.take("ref");
  if (ReferenceObject* object = std::get_if<ReferenceObject>(&value)) {
    return std::move(*object);
  }
  if (Callback* callback = std::get_if<Callback>(&value)) return std::move(*callback);
  return {};
}

}

inline VNode h(std::string tag, Object props, std::vector<VNode> children) {
  std::string key = detail::extract_key(props);
  Reference ref = detail::extract_reference(props);
  return create_vnode(ElementTag{std::move(tag)}, std::move(props), std::move(key),
                      std::move(ref), std::move(children));
}

inline VNode h(FunctionComponent component, Object props, std::vector<VNode> children) {
  std::string key = detail::extract_key(props);
  return create_vnode(std::move(component), std::move(props), std::move(key), Reference{},
                      std::move(children));
}

template <class... Children>
VNode h(std::string tag, Object props = {}, Children&&... children) {
  std::vector<VNode> child_nodes;
  child_nodes.reserve(sizeof...(children));
  (child_nodes.push_back(VNode(std::forward<Children>(children))), ...);
  return h(std::move(tag), std::move(props), std::move(child_nodes));
}

template <class... Children>
VNode h(FunctionComponent component, Object props = {}, Children&&... children) {
  std::vector<VNode> child_nodes;
  child_nodes.reserve(sizeof...(children));
  (child_nodes.push_back(VNode(std::forward<Children>(children))), ...);
  return h(std::move(component), std::move(props), std::move(child_nodes));
}

template <class... Children>
VNode FunctionComponent::operator()(Object props, Children&&... children) const {
  return h(*this, std::move(props), std::forward<Children>(children)...);
}

inline VNode FunctionComponent::render(const Object& properties) const {
  return (*function)(properties);
}

inline ReferenceObject create_ref() { return ReferenceObject{}; }

inline VNode fragment(std::vector<VNode> children) {
  VNode node;
  node.type = FragmentTag{};
  node.children = std::move(children);
  return node;
}

template <class... Children>
VNode fragment(Children&&... children) {
  std::vector<VNode> child_nodes;
  child_nodes.reserve(sizeof...(children));
  (child_nodes.push_back(VNode(std::forward<Children>(children))), ...);
  return fragment(std::move(child_nodes));
}

namespace detail {

inline thread_local std::vector<VNode>* current_children = nullptr;

}

VNode clone_vnode(const VNode& node);

std::vector<VNode> clone_vnodes(const std::vector<VNode>& children);

inline std::vector<VNode> children() {
  if (!detail::current_children) return {};
  return clone_vnodes(*detail::current_children);
}

inline const FunctionComponent Fragment = [](const Object&) -> VNode {
  return fragment(children());
};

}
