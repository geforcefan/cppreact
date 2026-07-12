#pragma once

#include <functional>
#include <memory>
#include <utility>
#include <variant>

#include "../diff/diff.hpp"

namespace cppreact {

inline bool shallow_differs(const Object& a, const Object& b) {
  for (const auto& [name, value] : a) {
    const Value* other = b.get(name);
    if (!other || !values_equal(value, *other)) return true;
  }
  for (const auto& [name, value] : b) {
    if (!a.get(name)) return true;
  }
  return false;
}

using MemoComparer = std::function<bool(const Object&, const Object&)>;

inline FunctionComponent memo(FunctionComponent component, MemoComparer comparer = {}) {
  return FunctionComponent([component, comparer](const Object& props) -> VNode {
    ComponentInstance* memoed_instance = detail::current_rendering;
    if (memoed_instance && !memoed_instance->should_component_update) {
      std::weak_ptr<ComponentInstance> self = memoed_instance->weak_from_this();
      memoed_instance->should_component_update =
          [self, comparer](const Object& next_props,
                           const std::vector<VNode>& next_children) -> bool {
        std::shared_ptr<ComponentInstance> memoed = self.lock();
        if (!memoed) return true;
        if (!next_children.empty() || !memoed->children.empty()) return true;

        const Value* reference = memoed->props.get("ref");
        const Value* next_reference = next_props.get("ref");
        const bool reference_changed =
            (reference != nullptr) != (next_reference != nullptr) ||
            (reference && next_reference && !values_equal(*reference, *next_reference));

        if (reference_changed && reference) {
          if (const Callback* callback = std::get_if<Callback>(reference)) {
            apply_reference(Reference{*callback}, null_dom_node);
          } else if (const ReferenceObject* object =
                         std::get_if<ReferenceObject>(reference)) {
            apply_reference(Reference{*object}, null_dom_node);
          }
        }

        if (comparer) {
          return !comparer(memoed->props, next_props) || reference_changed;
        }
        return shallow_differs(memoed->props, next_props);
      };
    }

    return h(component, props, children());
  });
}

}
