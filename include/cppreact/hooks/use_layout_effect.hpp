#pragma once

#include <cstddef>
#include <cstdint>
#include <utility>

namespace cppreact {

inline void use_layout_effect(EffectFunction effect,
                              Dependencies dependencies = Dependencies::every_render()) {
  const std::size_t index = detail::current_index++;
  detail::HookState& state = detail::get_hook_state(index, 4);
  if (!options().skip_effects && detail::arguments_changed(state.arguments, dependencies.list())) {
    state.effect = std::move(effect);
    if (dependencies.list()) {
      state.pending_arguments = *dependencies.list();
    } else {
      state.pending_arguments.reset();
      state.arguments.reset();
    }

    detail::current_component->render_callbacks.push_back(
        RenderCallback{{}, static_cast<std::int32_t>(index)});
  }
}

}
