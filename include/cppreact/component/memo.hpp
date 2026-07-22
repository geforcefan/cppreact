#pragma once

#include <concepts>
#include <functional>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>

#include "../diff/diff.hpp"

namespace cppreact {

namespace detail {

template <class Properties>
ComponentRender memo_render(ComponentRender inner,
                            std::function<bool(const Properties&, const Properties&)> comparer) {
  return std::make_shared<std::function<VNode(const Payload&)>>(
      [inner, comparer](const Payload& props) -> VNode {
        ComponentInstance* memoed_instance = current_rendering;
        if (memoed_instance && !memoed_instance->should_component_update) {
          std::weak_ptr<ComponentInstance> self = memoed_instance->weak_from_this();
          memoed_instance->should_component_update =
              [self, comparer](const Payload& next_props) -> bool {
            std::shared_ptr<ComponentInstance> memoed = self.lock();
            if (!memoed) return true;

            if (comparer) {
              std::shared_ptr<const Properties> stored = payload_as<Properties>(memoed->props);
              std::shared_ptr<const Properties> incoming = payload_as<Properties>(next_props);
              if (!stored || !incoming) return true;
              return !comparer(*stored, *incoming);
            }

            return !(memoed->props == next_props);
          };
        }

        return create_vnode(ComponentTag{inner, props}, std::string(), Reference{}, {});
      });
}

}

template <class Properties>
  requires std::equality_comparable<Properties>
FunctionComponent<Properties> memo(FunctionComponent<Properties> component) {
  return FunctionComponent<Properties>(
      detail::memo_render<Properties>(component.render_function(), {}));
}

template <class Properties, class Comparer>
  requires std::is_invocable_r_v<bool, const Comparer&, const Properties&, const Properties&>
FunctionComponent<Properties> memo(FunctionComponent<Properties> component, Comparer comparer) {
  return FunctionComponent<Properties>(detail::memo_render<Properties>(
      component.render_function(),
      std::function<bool(const Properties&, const Properties&)>(std::move(comparer))));
}

}
