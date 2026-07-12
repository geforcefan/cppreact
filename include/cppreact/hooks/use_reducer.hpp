#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

namespace cppreact {

namespace detail {

template <class T, class Action>
struct ReducerHookValue {
  T value;
  std::function<void(Action)> dispatch{};
};

template <class T, class Action>
std::pair<T, std::function<void(Action)>> use_reducer_with(
    std::function<T(const T&, const Action&)> reducer, std::function<T()> first_value_factory) {
  using StoredReducer = std::function<T(const T&, const Action&)>;
  using StoredValue = detail::ReducerHookValue<T, Action>;

  const std::size_t index = detail::current_index++;
  detail::HookState& hook_state = detail::get_hook_state(index, 2);
  hook_state.reducer =
      std::static_pointer_cast<void>(std::make_shared<StoredReducer>(std::move(reducer)));

  if (hook_state.component.expired()) {
    T first_value = first_value_factory();

    std::weak_ptr<ComponentInstance> component_reference =
        detail::current_component->weak_from_this();
    std::function<void(Action)> dispatch = [component_reference, index](Action action) {
      std::shared_ptr<ComponentInstance> component = component_reference.lock();
      if (!component || !component->hooks) return;
      detail::HookList& hooks = detail::hooks_for(*component);
      detail::HookState& hook_item = hooks.list[index];

      auto stored = std::static_pointer_cast<StoredValue>(hook_item.value);
      auto pending = hook_item.next_value
                         ? std::static_pointer_cast<StoredValue>(hook_item.next_value)
                         : nullptr;
      const T& current_value = pending ? pending->value : stored->value;
      auto stored_reducer = std::static_pointer_cast<StoredReducer>(hook_item.reducer);
      T next_value = (*stored_reducer)(current_value, action);

      if (!state_values_equal(current_value, next_value)) {
        hook_item.next_value = std::static_pointer_cast<void>(
            std::make_shared<StoredValue>(StoredValue{std::move(next_value), stored->dispatch}));
        component->set_state();
      }
    };

    hook_state.value = std::static_pointer_cast<void>(
        std::make_shared<StoredValue>(StoredValue{std::move(first_value), std::move(dispatch)}));
    hook_state.component = detail::current_component->weak_from_this();
    hook_state.promote = [](detail::HookState& self) -> bool {
      auto stored = std::static_pointer_cast<StoredValue>(self.value);
      auto next = std::static_pointer_cast<StoredValue>(self.next_value);
      const bool changed = !state_values_equal(stored->value, next->value);
      self.value = self.next_value;
      self.next_value.reset();
      return changed;
    };

    ComponentInstance* component = detail::current_component;
    if (!component->has_scu_from_hooks) {
      component->has_scu_from_hooks = true;

      auto previous_scu =
          std::make_shared<std::function<bool(const Object&, const std::vector<VNode>&)>>(
              component->should_component_update);
      std::function<void(const Object&)> previous_cwu = component->component_will_update;
      std::weak_ptr<ComponentInstance> self_reference = component->weak_from_this();

      auto update_hook_state = [self_reference, previous_scu](
                                   const Object& next_props,
                                   const std::vector<VNode>& next_children) -> bool {
        std::shared_ptr<ComponentInstance> self = self_reference.lock();
        if (!self || !self->hooks) return true;

        bool updated_hook = false;
        bool should_update = !self->rendering_self;
        detail::HookList& hooks = detail::hooks_for(*self);
        for (detail::HookState& hook_item : hooks.list) {
          if (hook_item.next_value && hook_item.promote) {
            updated_hook = true;
            if (hook_item.promote(hook_item)) should_update = true;
          }
        }

        if (*previous_scu) {
          const bool result = (*previous_scu)(next_props, next_children);
          return updated_hook ? (result || should_update) : result;
        }

        return !updated_hook || should_update;
      };

      component->component_will_update = [self_reference, previous_scu, previous_cwu,
                                          update_hook_state](const Object& next_props) {
        std::shared_ptr<ComponentInstance> self = self_reference.lock();
        if (self && (self->flags & component_flag::force)) {
          std::function<bool(const Object&, const std::vector<VNode>&)> saved = *previous_scu;
          *previous_scu = nullptr;
          update_hook_state(next_props, {});
          *previous_scu = saved;
        }

        if (previous_cwu) previous_cwu(next_props);
      };

      component->should_component_update = update_hook_state;
    }
  }

  auto stored = std::static_pointer_cast<StoredValue>(
      detail::hooks_for(*detail::current_component).list[index].value);
  return {stored->value, stored->dispatch};
}

}

template <class T, class Action>
std::pair<T, std::function<void(Action)>> use_reducer(
    std::function<T(const T&, const Action&)> reducer, T initial_state,
    std::function<T(const T&)> initialize) {
  return detail::use_reducer_with<T, Action>(
      std::move(reducer), [initialize, initial_state]() -> T {
        return initialize ? initialize(initial_state) : initial_state;
      });
}

template <class T, class Action>
std::pair<T, std::function<void(Action)>> use_reducer(
    std::function<T(const T&, const Action&)> reducer, T initial_state) {
  return detail::use_reducer_with<T, Action>(std::move(reducer),
                                             [initial_state]() -> T { return initial_state; });
}

template <class T, class Action, class Initialize,
          std::enable_if_t<std::is_invocable_r_v<T, Initialize&>, int> = 0>
std::pair<T, std::function<void(Action)>> use_reducer(
    std::function<T(const T&, const Action&)> reducer, Initialize initialize) {
  return detail::use_reducer_with<T, Action>(std::move(reducer), std::function<T()>(initialize));
}

}
