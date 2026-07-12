#pragma once

#include <memory>
#include <utility>
#include <variant>
#include <vector>

#include "../hooks/hooks.hpp"

namespace cppreact {

namespace detail {

inline const FunctionComponent ContextProvider = [](const Object&) -> VNode {
  ComponentInstance* provider = current_rendering;
  if (provider && !provider->get_child_context) {
    std::weak_ptr<ComponentInstance> self = provider->weak_from_this();
    provider->get_child_context = [self]() -> GlobalContext {
      std::shared_ptr<ComponentInstance> locked = self.lock();
      if (!locked) return {};
      const Value* context_value = locked->props.get("context");
      const RawPayload* payload =
          context_value ? std::get_if<RawPayload>(context_value) : nullptr;
      if (!payload || !payload->data) return {};
      return *std::static_pointer_cast<GlobalContext>(payload->data);
    };
  }
  return fragment(children());
};

inline const FunctionComponent Portal = [](const Object& props) -> VNode {
  Host* host = current_component_host();
  DomNode container = static_cast<DomNode>(props.get<double>("container",0.0));
  GlobalContext context =
      current_rendering ? current_rendering->global_context : GlobalContext{};
  std::vector<VNode> child_nodes = children();

  std::shared_ptr<Container>& temp = use_ref<std::shared_ptr<Container>>(nullptr);
  std::shared_ptr<Container>* temp_pointer = &temp;

  use_layout_effect(
      [temp_pointer]() -> CleanupFunction {
        return [temp_pointer] {
          if (*temp_pointer) {
            render(fragment(), **temp_pointer);
            temp_pointer->reset();
          }
        };
      },
      {});

  if (temp && temp->mount != container) {
    render(fragment(), *temp);
    temp.reset();
  }

  if (!temp && host && container != null_dom_node) {
    temp = std::make_shared<Container>(Container{host, container, {}});
  }

  if (temp) {
    render(h(ContextProvider,
             Object{{"context", RawPayload{std::make_shared<GlobalContext>(std::move(context))}}},
             std::move(child_nodes)),
           *temp);
  }

  return fragment();
};

}

inline VNode portal(DomNode target, std::vector<VNode> children) {
  return h(detail::Portal, Object{{"container", static_cast<double>(target)}},
           std::move(children));
}

template <class... Children>
VNode portal(DomNode target, Children&&... children) {
  std::vector<VNode> collected;
  collected.reserve(sizeof...(children));
  (collected.emplace_back(std::forward<Children>(children)), ...);
  return portal(target, std::move(collected));
}

}
