#pragma once

#include <cstddef>
#include <functional>
#include <memory>

namespace cppreact {

template <class T>
T use_memo(std::function<T()> factory, Dependencies dependencies) {
  const std::size_t index = detail::current_index++;
  detail::HookState& state = detail::get_hook_state(index, 7);
  if (detail::arguments_changed(state.arguments, dependencies.list())) {
    state.value = std::static_pointer_cast<void>(std::make_shared<T>(factory()));
    if (dependencies.list()) {
      state.arguments = *dependencies.list();
    } else {
      state.arguments.reset();
    }
  }

  return *std::static_pointer_cast<T>(state.value);
}

}
