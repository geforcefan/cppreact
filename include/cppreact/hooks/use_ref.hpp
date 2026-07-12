#pragma once

#include <cstddef>
#include <memory>
#include <utility>

namespace cppreact {

template <class T>
T& use_ref(T initial_value) {
  detail::current_hook = 5;
  const std::size_t index = detail::current_index++;
  detail::HookState& state = detail::get_hook_state(index, 7);
  if (!state.value) {
    state.value = std::static_pointer_cast<void>(std::make_shared<T>(std::move(initial_value)));
  }
  return *std::static_pointer_cast<T>(state.value);
}

}
