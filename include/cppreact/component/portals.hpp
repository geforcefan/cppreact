#pragma once

#include <memory>
#include <utility>

#include "../hooks/hooks.hpp"

namespace cppreact {

namespace detail {

struct ContextForwardProps {
  GlobalContext context{};
  Children children{};
};

inline const FunctionComponent ContextForward =
    [](const ContextForwardProps& props) -> VNode {
  ComponentInstance* provider = current_rendering;
  if (provider && !provider->get_child_context) {
    std::weak_ptr<ComponentInstance> self = provider->weak_from_this();
    provider->get_child_context = [self]() -> GlobalContext {
      std::shared_ptr<ComponentInstance> locked = self.lock();
      if (!locked) return {};
      std::shared_ptr<const ContextForwardProps> stored =
          payload_as<ContextForwardProps>(locked->props);
      return stored ? stored->context : GlobalContext{};
    };
  }
  return props.children;
};

}

struct PortalProps {
  DomNode container = null_dom_node;
  Children children{};
};

inline const FunctionComponent Portal = [](const PortalProps& props) -> VNode {
  Host* host = current_component_host();
  GlobalContext context =
      detail::current_rendering ? detail::current_rendering->global_context : GlobalContext{};

  std::shared_ptr<Container>& mounted_container =
      use_ref<std::shared_ptr<Container>>(nullptr);
  std::shared_ptr<Container>* mounted_container_pointer = &mounted_container;

  use_layout_effect(
      [mounted_container_pointer]() -> CleanupFunction {
        return [mounted_container_pointer] {
          if (*mounted_container_pointer) {
            render(fragment(), **mounted_container_pointer);
            mounted_container_pointer->reset();
          }
        };
      },
      {});

  if (mounted_container && mounted_container->mount != props.container) {
    render(fragment(), *mounted_container);
    mounted_container.reset();
  }

  if (!mounted_container && host && props.container != null_dom_node) {
    mounted_container = std::make_shared<Container>(Container{host, props.container, {}});
  }

  if (mounted_container) {
    render(detail::ContextForward(detail::ContextForwardProps{
               .context = std::move(context), .children = props.children}),
           *mounted_container);
  }

  return fragment();
};

}
