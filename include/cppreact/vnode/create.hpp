#pragma once

#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "vnode.hpp"
#include "children.hpp"
#include "options.hpp"
#include "../component/function_component.hpp"

namespace cppreact {

inline VNode create_vnode(VNodeType type, std::string key, Reference ref,
                          std::vector<VNode> children) {
  VNode vnode;
  vnode.type = std::move(type);
  vnode.key = std::move(key);
  vnode.ref = std::move(ref);
  vnode.children = std::move(children);
  if (options().vnode) options().vnode(vnode);
  return vnode;
}

template <class Properties>
template <class Render>
  requires(!std::is_same_v<std::decay_t<Render>, FunctionComponent<Properties>> &&
           !std::is_same_v<std::decay_t<Render>, std::nullptr_t> &&
           std::is_invocable_r_v<VNode, const std::decay_t<Render>&, const Properties&>)
FunctionComponent<Properties>::FunctionComponent(Render render)
    : component_render(std::make_shared<std::function<VNode(const Payload&)>>(
          [render = std::move(render)](const Payload& properties) -> VNode {
            std::shared_ptr<const Properties> stored = payload_as<Properties>(properties);
            if (stored) return render(*stored);
            return render(Properties{});
          })) {}

template <class Properties>
VNode FunctionComponent<Properties>::operator()(Properties properties) const {
  std::string key{};
  Reference ref{};
  if constexpr (requires { properties.key; }) key = properties.key;
  if constexpr (requires { properties.ref; }) ref = properties.ref;
  return create_vnode(ComponentTag{component_render, make_payload(std::move(properties))},
                      std::move(key), std::move(ref), {});
}

inline VNode fragment(std::vector<VNode> children) {
  VNode node;
  node.type = FragmentTag{};
  node.children = std::move(children);
  return node;
}

template <class... Nodes>
VNode fragment(Nodes&&... children) {
  std::vector<VNode> child_nodes;
  child_nodes.reserve(sizeof...(children));
  (child_nodes.push_back(VNode(std::forward<Nodes>(children))), ...);
  return fragment(std::move(child_nodes));
}

inline Children::operator VNode() const { return fragment(clone_vnodes(nodes)); }

}
