#pragma once

#include <cstddef>
#include <memory>

#include "../context/create_context.hpp"

namespace cppreact {

template <class T>
const T& use_context(const Context<T>& context) {
  std::shared_ptr<ComponentInstance> provider;
  const auto provider_entry = detail::current_component->global_context.find(context.id);
  if (provider_entry != detail::current_component->global_context.end()) {
    provider = provider_entry->second;
  }

  const std::size_t index = detail::current_index++;
  detail::HookState& state = detail::get_hook_state(index, 9);
  if (!provider) return *context.default_value;
  if (!state.subscribed) {
    state.subscribed = true;
    if (provider->sub) provider->sub(detail::current_component->shared_from_this());
  }

  std::shared_ptr<const ProviderProps<T>> stored =
      payload_as<ProviderProps<T>>(provider->props);
  if (!stored) return *context.default_value;
  return stored->value;
}

}
