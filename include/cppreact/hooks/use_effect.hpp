#pragma once

#include <cstddef>
#include <utility>

namespace cppreact {

inline void use_effect(EffectFunction effect,
                       Dependencies dependencies = Dependencies::every_render()) {
  const std::size_t index = detail::current_index++;
  detail::HookState& state = detail::get_hook_state(index, 3);
  if (!options().skip_effects && detail::arguments_changed(state.arguments, dependencies.list())) {
    state.effect = std::move(effect);
    if (dependencies.list()) {
      state.pending_arguments = *dependencies.list();
    } else {
      state.pending_arguments.reset();
      state.arguments.reset();
    }

    detail::hooks_for(*detail::current_component).pending_effects.push_back(index);
  }
}

}
