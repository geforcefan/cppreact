#pragma once

#include <utility>
#include <vector>

#include "renderer.hpp"
#include "vnode.hpp"
#include "hooks.hpp"
#include "render.hpp"

namespace cppreact {

VNode portal(NodeHandle target, std::vector<VNode> children);

template <class... Children>
VNode portal(NodeHandle target, Children&&... children) {
  std::vector<VNode> collected;
  collected.reserve(sizeof...(children));
  (collected.emplace_back(std::forward<Children>(children)), ...);
  return portal(target, std::move(collected));
}

}

#include <memory>

#include "context.hpp"
#include "hooks.hpp"
#include "render.hpp"

namespace cppreact {

namespace detail {

inline VNode with_context(ContextChain chain, std::vector<VNode> child_nodes) {
  VNode node = fragment(std::move(child_nodes));
  node.context_scope = std::move(chain);
  return node;
}

inline VNode PortalComponent(const Props& props) {
  Renderer* renderer = current_component_renderer();
  NodeHandle target = static_cast<NodeHandle>(props.get<double>("container").value_or(0.0));
  ContextChain context = current_context_chain();

  std::shared_ptr<std::shared_ptr<Container>> holder =
      use_ref<std::shared_ptr<std::shared_ptr<Container>>>(std::make_shared<std::shared_ptr<Container>>());

  use_layout_effect(
      [renderer, target, holder]() -> CleanupFunction {
        if (renderer && target != kNullNode)
          *holder = std::make_shared<Container>(Container{renderer, target, {}});
        return [holder] {
          if (*holder) {
            render(fragment(), **holder);
            holder->reset();
          }
        };
      },
      deps(target));

  std::shared_ptr<std::vector<VNode>> child_nodes = std::make_shared<std::vector<VNode>>(children());
  use_layout_effect([holder, context, child_nodes]() -> CleanupFunction {
    if (*holder) render(with_context(context, std::move(*child_nodes)), **holder);
    return {};
  });

  return fragment();
}

}

inline VNode portal(NodeHandle target, std::vector<VNode> children) {
  return h(&detail::PortalComponent, Props{{"container", static_cast<double>(target)}},
           std::move(children));
}

}
