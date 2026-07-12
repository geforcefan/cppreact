#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "../context/create_context.hpp"
#include "../diff/diff.hpp"
#include "../render.hpp"

namespace cppreact {

class Dependencies {
public:
  Dependencies() : values(std::in_place) {}
  Dependencies(std::initializer_list<Value> list) : values(std::in_place, list) {}

  static Dependencies every_render() {
    Dependencies dependencies;
    dependencies.values.reset();
    return dependencies;
  }

  const std::vector<Value>* list() const { return values ? &*values : nullptr; }

private:
  std::optional<std::vector<Value>> values{};
};

using CleanupFunction = std::function<void()>;
using EffectFunction = std::function<CleanupFunction()>;

template <class T>
bool state_values_equal(const T& left, const T& right) {
  if constexpr (requires(const T& a, const T& b) { a == b; }) {
    return left == right;
  } else {
    return false;
  }
}

namespace detail {

struct HookState {
  std::shared_ptr<void> value{};
  std::shared_ptr<void> next_value{};
  std::shared_ptr<void> reducer{};
  bool (*promote)(HookState&) = nullptr;
  std::optional<std::vector<Value>> arguments{};
  std::optional<std::vector<Value>> pending_arguments{};
  EffectFunction effect{};
  CleanupFunction cleanup{};
  std::weak_ptr<ComponentInstance> component{};
  bool subscribed = false;
};

struct HookList {
  std::vector<HookState> list{};
  std::vector<std::size_t> pending_effects{};
};

inline thread_local std::size_t current_index = 0;
inline thread_local ComponentInstance* current_component = nullptr;
inline thread_local ComponentInstance* previous_component = nullptr;
inline thread_local int current_hook = 0;
inline thread_local std::vector<std::weak_ptr<ComponentInstance>> passive_effects{};

inline HookList& hooks_for(ComponentInstance& component) {
  if (!component.hooks) {
    component.hooks = std::static_pointer_cast<void>(std::make_shared<HookList>());
  }
  return *std::static_pointer_cast<HookList>(component.hooks);
}

inline bool arguments_changed(const std::optional<std::vector<Value>>& old_arguments,
                              const std::vector<Value>* new_arguments) {
  if (!new_arguments || !old_arguments) return true;
  if (old_arguments->size() != new_arguments->size()) return true;
  for (std::size_t position = 0; position < new_arguments->size(); ++position) {
    if (!values_equal((*old_arguments)[position], (*new_arguments)[position])) return true;
  }
  return false;
}

inline void invoke_cleanup(ComponentInstance& component, std::size_t index) {
  ComponentInstance* previous = current_component;
  HookList& hooks = hooks_for(component);
  if (index < hooks.list.size() && hooks.list[index].cleanup) {
    CleanupFunction cleanup = std::move(hooks.list[index].cleanup);
    hooks.list[index].cleanup = nullptr;
    cleanup();
  }
  current_component = previous;
}

inline void invoke_effect(ComponentInstance& component, std::size_t index) {
  ComponentInstance* previous = current_component;
  HookList& hooks = hooks_for(component);
  if (index >= hooks.list.size()) {
    current_component = previous;
    return;
  }
  EffectFunction effect = hooks.list[index].effect;
  CleanupFunction cleanup = effect ? effect() : CleanupFunction{};
  if (component.hooks) {
    HookList& settled = hooks_for(component);
    if (index < settled.list.size()) {
      settled.list[index].cleanup = std::move(cleanup);
      current_component = previous;
      return;
    }
  }
  if (cleanup) cleanup();
  current_component = previous;
}

inline void flush_passive_effects() {
  while (!passive_effects.empty()) {
    std::shared_ptr<ComponentInstance> component = passive_effects.front().lock();
    passive_effects.erase(passive_effects.begin());
    if (!component || !component->host || !component->hooks) continue;
    std::shared_ptr<void> hooks_handle = component->hooks;
    std::vector<std::size_t> pending = std::move(hooks_for(*component).pending_effects);
    for (std::size_t index : pending) {
      if (component->hooks != hooks_handle) break;
      invoke_cleanup(*component, index);
    }
    for (std::size_t index : pending) {
      if (component->hooks != hooks_handle) break;
      invoke_effect(*component, index);
    }
  }
}

inline void schedule_passive_effects() {
  passive_effects_flush = flush_passive_effects;
}

inline void install_hooks() {
  Options& hooked = options();

  auto old_before_diff = hooked.diff;
  auto old_before_render = hooked.render;
  auto old_after_diff = hooked.diffed;
  auto old_commit = hooked.commit;
  auto old_before_unmount = hooked.unmount;

  hooked.diff = [old_before_diff](VNode& vnode) {
    current_component = nullptr;
    if (old_before_diff) old_before_diff(vnode);
  };

  hooked.render = [old_before_render](VNode& vnode) {
    if (old_before_render) old_before_render(vnode);

    current_component = vnode.component.get();
    current_index = 0;

    if (current_component && current_component->hooks) {
      HookList& hooks = hooks_for(*current_component);
      if (previous_component == current_component) {
        hooks.pending_effects.clear();
        current_component->render_callbacks.clear();
        for (HookState& hook_item : hooks.list) {
          if (hook_item.next_value && hook_item.promote) {
            hook_item.promote(hook_item);
          }
          hook_item.pending_arguments.reset();
          hook_item.next_value.reset();
        }
      } else {
        std::shared_ptr<ComponentInstance> component = vnode.component;
        std::shared_ptr<void> hooks_handle = component->hooks;
        std::vector<std::size_t> pending = std::move(hooks.pending_effects);
        for (std::size_t index : pending) {
          if (component->hooks != hooks_handle) break;
          invoke_cleanup(*component, index);
        }
        for (std::size_t index : pending) {
          if (component->hooks != hooks_handle) break;
          invoke_effect(*component, index);
        }
        current_index = 0;
      }
    }
    previous_component = current_component;
  };

  hooked.diffed = [old_after_diff](VNode& vnode) {
    if (old_after_diff) old_after_diff(vnode);

    ComponentInstance* component = vnode.component.get();
    if (component && component->hooks) {
      HookList& hooks = hooks_for(*component);
      if (!hooks.pending_effects.empty()) {
        passive_effects.push_back(component->weak_from_this());
        schedule_passive_effects();
      }
      for (HookState& hook_item : hooks.list) {
        if (hook_item.pending_arguments) {
          hook_item.arguments = std::move(hook_item.pending_arguments);
          hook_item.pending_arguments.reset();
        }
      }
    }
    previous_component = current_component = nullptr;
  };

  hooked.commit = [old_commit](VNode& root, const CommitQueue& commit_queue) {
    std::vector<Host*> layout_ready;
    for (const std::weak_ptr<ComponentInstance>& entry : commit_queue) {
      std::shared_ptr<ComponentInstance> component = entry.lock();
      if (!component || !component->host) continue;
      bool has_layout_effects = false;
      for (const RenderCallback& render_callback : component->render_callbacks) {
        if (render_callback.hook_index >= 0) {
          has_layout_effects = true;
          break;
        }
      }
      if (!has_layout_effects) continue;
      if (std::find(layout_ready.begin(), layout_ready.end(), component->host) !=
          layout_ready.end())
        continue;
      component->host->update_layout();
      layout_ready.push_back(component->host);
    }

    for (const std::weak_ptr<ComponentInstance>& entry : commit_queue) {
      std::shared_ptr<ComponentInstance> component = entry.lock();
      if (!component || !component->hooks) continue;
      std::shared_ptr<void> hooks_handle = component->hooks;

      for (std::size_t position = 0; position < component->render_callbacks.size(); ++position) {
        if (component->hooks != hooks_handle) break;
        const RenderCallback& render_callback = component->render_callbacks[position];
        if (render_callback.hook_index >= 0) {
          invoke_cleanup(*component, static_cast<std::size_t>(render_callback.hook_index));
        }
      }

      std::vector<RenderCallback> remaining;
      for (std::size_t position = 0; position < component->render_callbacks.size(); ++position) {
        RenderCallback& render_callback = component->render_callbacks[position];
        if (render_callback.hook_index >= 0) {
          if (component->hooks == hooks_handle) {
            invoke_effect(*component, static_cast<std::size_t>(render_callback.hook_index));
          }
        } else {
          remaining.push_back(std::move(render_callback));
        }
      }
      component->render_callbacks = std::move(remaining);
    }

    if (old_commit) old_commit(root, commit_queue);
  };

  hooked.unmount = [old_before_unmount](VNode& vnode) {
    if (old_before_unmount) old_before_unmount(vnode);

    std::shared_ptr<ComponentInstance> component = vnode.component;
    if (component && component->hooks) {
      std::shared_ptr<void> hooks_handle = component->hooks;
      for (std::size_t index = 0; index < hooks_for(*component).list.size(); ++index) {
        if (component->hooks != hooks_handle) break;
        invoke_cleanup(*component, index);
      }
      component->hooks.reset();
    }
  };
}

inline const bool hooks_installed = (install_hooks(), true);

inline HookState& get_hook_state(std::size_t index, int type) {
  if (options().hook) {
    options().hook(*current_component, index, current_hook ? current_hook : type);
  }
  current_hook = 0;

  HookList& hooks = hooks_for(*current_component);

  if (index >= hooks.list.size()) {
    hooks.list.push_back({});
  }

  return hooks.list[index];
}

}

}

#include "use_reducer.hpp"
#include "use_state.hpp"
#include "use_effect.hpp"
#include "use_layout_effect.hpp"
#include "use_memo.hpp"
#include "use_callback.hpp"
#include "use_ref.hpp"
#include "use_context.hpp"
#include "use_sync_external_store.hpp"
#include "use_document_event.hpp"
