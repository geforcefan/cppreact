#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "renderer.hpp"
#include "context.hpp"
#include "value.hpp"
#include "hook_slot.hpp"
#include "component.hpp"

namespace cppreact {

std::vector<VNode> children();

void install_hooks();

void commit_effects();

ComponentInstance* current_component_pointer();
Renderer* current_component_renderer();
ContextChain current_context_chain();
std::size_t reserve_hook_index();
HookSlot& hook_slot_at(ComponentInstance* owner, std::size_t index);
void enqueue_component_render(ComponentInstance* owner);
void schedule_effect(ComponentInstance* owner, std::size_t index, EffectFunction effect,
                     const std::vector<Value>* deps, bool layout);
bool memo_deps_changed(ComponentInstance* owner, std::size_t index,
                       const std::vector<Value>& deps);

template <class T, class... Args>
T& hook_state(std::size_t index, Args&&... arguments) {
  HookSlot& slot = hook_slot_at(current_component_pointer(), index);
  if (!slot.state) {
    T* raw = new T(std::forward<Args>(arguments)...);
    slot.state = std::unique_ptr<void, void (*)(void*)>(
        raw, [](void* pointer) { delete static_cast<T*>(pointer); });
  }
  return *static_cast<T*>(slot.state.get());
}

template <class T, class Factory>
T& hook_state_from(std::size_t index, Factory&& initialize) {
  HookSlot& slot = hook_slot_at(current_component_pointer(), index);
  if (!slot.state) {
    T* raw = new T(initialize());
    slot.state = std::unique_ptr<void, void (*)(void*)>(
        raw, [](void* pointer) { delete static_cast<T*>(pointer); });
  }
  return *static_cast<T*>(slot.state.get());
}

template <class T>
struct StateHook {
  T value;
};

template <class T>
bool state_values_equal(const T& left, const T& right) {
  if constexpr (requires(const T& a, const T& b) { a == b; }) {
    return left == right;
  } else {
    return false;
  }
}

template <class T>
struct MemoHook {
  std::optional<T> value{};
};

template <class T>
struct RefHook {
  T value;
};

template <class T>
class StateSetter {
public:
  StateSetter() = default;
  StateSetter(std::weak_ptr<ComponentInstance> owner, std::size_t slot)
      : owner(std::move(owner)), index(slot) {}

  void operator()(T next) const;
  void operator()(const std::function<T(const T&)>& update) const;

private:
  std::weak_ptr<ComponentInstance> owner{};
  std::size_t index = 0;
};

std::weak_ptr<ComponentInstance> current_component_weak();

template <class T>
std::pair<T, StateSetter<T>> use_state(T initial) {
  std::size_t index = reserve_hook_index();
  StateHook<T>& hook = hook_state<StateHook<T>>(index, std::move(initial));
  return {hook.value, StateSetter<T>(current_component_weak(), index)};
}

template <class T, class Initialize,
          std::enable_if_t<std::is_invocable_r_v<T, Initialize&>, int> = 0>
std::pair<T, StateSetter<T>> use_state(Initialize initialize) {
  std::size_t index = reserve_hook_index();
  StateHook<T>& hook =
      hook_state_from<StateHook<T>>(index, [&] { return StateHook<T>{initialize()}; });
  return {hook.value, StateSetter<T>(current_component_weak(), index)};
}

template <class T, class Action>
struct ReducerHook {
  T value;
  std::function<T(const T&, const Action&)> reducer{};
};

namespace detail {

template <class T, class Action>
std::pair<T, std::function<void(Action)>> reducer_pair(
    std::size_t index, ReducerHook<T, Action>& hook,
    std::function<T(const T&, const Action&)> reducer) {
  hook.reducer = std::move(reducer);
  std::weak_ptr<ComponentInstance> owner = current_component_weak();
  auto dispatch = [owner, index](Action action) {
    std::shared_ptr<ComponentInstance> instance = owner.lock();
    if (!instance) return;
    HookSlot& slot = hook_slot_at(instance.get(), index);
    ReducerHook<T, Action>* state = static_cast<ReducerHook<T, Action>*>(slot.state.get());
    T next = state->reducer(state->value, action);
    if (state_values_equal(state->value, next)) return;
    state->value = std::move(next);
    enqueue_component_render(instance.get());
  };
  return {hook.value, std::function<void(Action)>(dispatch)};
}

}

template <class T, class Action>
std::pair<T, std::function<void(Action)>> use_reducer(
    std::function<T(const T&, const Action&)> reducer, T initial) {
  std::size_t index = reserve_hook_index();
  ReducerHook<T, Action>& hook = hook_state_from<ReducerHook<T, Action>>(
      index, [&] { return ReducerHook<T, Action>{std::move(initial)}; });
  return detail::reducer_pair<T, Action>(index, hook, std::move(reducer));
}

template <class T, class Action, class Initialize,
          std::enable_if_t<std::is_invocable_r_v<T, Initialize&>, int> = 0>
std::pair<T, std::function<void(Action)>> use_reducer(
    std::function<T(const T&, const Action&)> reducer, Initialize initialize) {
  std::size_t index = reserve_hook_index();
  ReducerHook<T, Action>& hook = hook_state_from<ReducerHook<T, Action>>(
      index, [&] { return ReducerHook<T, Action>{initialize()}; });
  return detail::reducer_pair<T, Action>(index, hook, std::move(reducer));
}

using MaybeDeps = std::optional<std::vector<Value>>;

namespace detail {
inline Value to_dep(bool value) { return value; }
inline Value to_dep(const char* value) { return std::string(value); }
inline Value to_dep(std::string value) { return value; }
inline Value to_dep(Value value) { return value; }
template <class T, std::enable_if_t<std::is_arithmetic_v<T> && !std::is_same_v<T, bool>, int> = 0>
Value to_dep(T value) {
  return static_cast<double>(value);
}
}

template <class... Ts>
std::vector<Value> deps(Ts&&... values) {
  return std::vector<Value>{detail::to_dep(std::forward<Ts>(values))...};
}

void use_effect(EffectFunction effect, MaybeDeps deps = std::nullopt);
void use_layout_effect(EffectFunction effect, MaybeDeps deps = std::nullopt);

template <class T>
T use_memo(std::function<T()> factory, std::vector<Value> deps) {
  std::size_t index = reserve_hook_index();
  MemoHook<T>& hook = hook_state<MemoHook<T>>(index);
  if (memo_deps_changed(current_component_pointer(), index, deps) || !hook.value.has_value()) {
    hook.value = factory();
  }
  return *hook.value;
}

template <class T>
T use_callback(T callback, std::vector<Value> deps) {
  return use_memo<T>([callback]() { return callback; }, std::move(deps));
}

template <class T>
T& use_ref(T initial) {
  std::size_t index = reserve_hook_index();
  RefHook<T>& hook = hook_state<RefHook<T>>(index, std::move(initial));
  return hook.value;
}

template <class T>
const T& use_context(const Context<T>& context) {
  std::size_t index = reserve_hook_index();
  std::shared_ptr<ContextCell> cell = lookup_context_cell(current_context_chain(), context.id);
  HookSlot& slot = hook_slot_at(current_component_pointer(), index);
  if (cell && !slot.state) {
    hook_state<bool>(index, true);
    std::weak_ptr<ComponentInstance> owner = current_component_weak();
    std::uint32_t token = subscribe_context(*cell, [owner] {
      std::shared_ptr<ComponentInstance> instance = owner.lock();
      if (instance) enqueue_component_render(instance.get());
    });
    slot.cleanup = [held = std::weak_ptr<ContextCell>(cell), token] {
      std::shared_ptr<ContextCell> subscribed = held.lock();
      if (subscribed) unsubscribe_context(*subscribed, token);
    };
  }
  if (!cell || !cell->value.data) return *context.default_value;
  return *static_cast<const T*>(cell->value.data.get());
}

template <class Subscribe, class GetSnapshot>
auto use_sync_external_store(Subscribe subscribe, GetSnapshot get_snapshot) {
  using Snapshot = decltype(get_snapshot());
  Snapshot current = get_snapshot();
  Snapshot* latest = &use_ref<Snapshot>(current);
  *latest = current;
  auto [version, set_version] = use_state<int>(0);
  (void)version;
  use_effect(
      [subscribe, get_snapshot, latest, set_version]() -> CleanupFunction {
        return subscribe([get_snapshot, latest, set_version] {
          Snapshot next = get_snapshot();
          if (!(next == *latest)) {
            *latest = next;
            set_version([](int value) { return value + 1; });
          }
        });
      },
      deps());
  return current;
}

template <class T>
void StateSetter<T>::operator()(T next) const {
  std::shared_ptr<ComponentInstance> instance = owner.lock();
  if (!instance) return;
  HookSlot& slot = hook_slot_at(instance.get(), index);
  StateHook<T>* state = static_cast<StateHook<T>*>(slot.state.get());
  if (state_values_equal(state->value, next)) return;
  state->value = std::move(next);
  enqueue_component_render(instance.get());
}

template <class T>
void StateSetter<T>::operator()(const std::function<T(const T&)>& update) const {
  std::shared_ptr<ComponentInstance> instance = owner.lock();
  if (!instance) return;
  HookSlot& slot = hook_slot_at(instance.get(), index);
  StateHook<T>* state = static_cast<StateHook<T>*>(slot.state.get());
  T next = update(state->value);
  if (state_values_equal(state->value, next)) return;
  state->value = std::move(next);
  enqueue_component_render(instance.get());
}

struct NodeRef {
  std::shared_ptr<NodeHandle> slot = std::make_shared<NodeHandle>(kNullNode);

  NodeHandle current() const { return *slot; }

  Value prop() const {
    std::shared_ptr<NodeHandle> cell = slot;
    Ref assign = [cell](NodeHandle handle) { *cell = handle; };
    return Payload{std::make_shared<Ref>(std::move(assign))};
  }
};

inline NodeRef use_node_ref() { return use_ref<NodeRef>(NodeRef{}); }

Box use_measure(const NodeRef& ref);

std::function<Box()> use_measure_callback(const NodeRef& ref);

void use_document_event(const std::string& type, EventHandler handler);

}

#include "component.hpp"
#include "options.hpp"

namespace cppreact {

namespace detail {

inline thread_local ComponentInstance* current_component = nullptr;
inline thread_local std::size_t current_hook_index = 0;
inline thread_local ContextChain current_chain{};
inline thread_local std::vector<ComponentInstance*> commit_queue{};

inline void run_effect_list(ComponentInstance* instance, std::vector<std::size_t>& indices) {
  for (std::size_t index : indices) {
    HookSlot& slot = instance->hooks[index];
    if (slot.cleanup) {
      slot.cleanup();
      slot.cleanup = nullptr;
    }
    if (slot.pending) {
      ComponentInstance* previous = current_component;
      current_component = instance;
      slot.cleanup = slot.pending();
      slot.pending = nullptr;
      current_component = previous;
    }
  }
  indices.clear();
}

inline void run_effects(const std::vector<ComponentInstance*>& queue) {
  for (ComponentInstance* instance : queue) {
    run_effect_list(instance, instance->pending_layout_effects);
  }
  for (ComponentInstance* instance : queue) {
    run_effect_list(instance, instance->pending_effects);
    instance->in_commit_queue = false;
  }
}

}

inline void install_hooks() {
  Options& option = options();
  option.before_diff = [](VNode&) { detail::current_component = nullptr; };
  option.before_render = [](ComponentInstance& instance) {
    detail::current_component = &instance;
    detail::current_hook_index = 0;
    detail::current_chain = instance.context;
    instance.pending_effects.clear();
    instance.pending_layout_effects.clear();
  };
  option.diffed = [](ComponentInstance&) { detail::current_component = nullptr; };
  option.commit = [](const std::vector<ComponentInstance*>& queue) { detail::run_effects(queue); };
}

inline void commit_effects() {
  std::vector<ComponentInstance*> batch;
  batch.swap(detail::commit_queue);
  if (options().commit) options().commit(batch);
}

inline ComponentInstance* current_component_pointer() {
  return detail::current_component;
}

inline Renderer* current_component_renderer() {
  return detail::current_component ? detail::current_component->renderer : nullptr;
}

inline std::vector<VNode> children() {
  std::vector<VNode> copy;
  if (detail::current_component) {
    copy.reserve(detail::current_component->children.size());
    for (const VNode& child : detail::current_component->children) copy.push_back(clone(child));
  }
  return copy;
}

inline ContextChain current_context_chain() {
  return detail::current_chain;
}

inline std::weak_ptr<ComponentInstance> current_component_weak() {
  if (!detail::current_component) return {};
  return detail::current_component->weak_from_this();
}

inline std::size_t reserve_hook_index() {
  std::size_t index = detail::current_hook_index++;
  if (index >= detail::current_component->hooks.size()) {
    detail::current_component->hooks.resize(index + 1);
  }
  return index;
}

inline HookSlot& hook_slot_at(ComponentInstance* owner, std::size_t index) {
  return owner->hooks[index];
}

inline void enqueue_component_render(ComponentInstance* owner) {
  enqueue_render(owner);
}

inline void schedule_effect(ComponentInstance* owner, std::size_t index, EffectFunction effect,
                            const std::vector<Value>* deps, bool layout) {
  HookSlot& slot = owner->hooks[index];
  bool changed = true;
  if (deps) {
    changed = !slot.has_deps || !value_lists_equal(slot.deps, *deps);
    slot.deps = *deps;
    slot.has_deps = true;
  } else {
    slot.has_deps = false;
  }
  if (!changed) return;

  slot.pending = std::move(effect);
  if (layout) {
    owner->pending_layout_effects.push_back(index);
  } else {
    owner->pending_effects.push_back(index);
  }
  if (!owner->in_commit_queue) {
    owner->in_commit_queue = true;
    detail::commit_queue.push_back(owner);
  }
}

inline bool memo_deps_changed(ComponentInstance* owner, std::size_t index,
                              const std::vector<Value>& deps) {
  HookSlot& slot = owner->hooks[index];
  bool changed = !slot.has_deps || !value_lists_equal(slot.deps, deps);
  slot.deps = deps;
  slot.has_deps = true;
  return changed;
}

inline void use_effect(EffectFunction effect, MaybeDeps deps) {
  std::size_t index = reserve_hook_index();
  const std::vector<Value>* deps_pointer = deps ? &*deps : nullptr;
  schedule_effect(current_component_pointer(), index, std::move(effect), deps_pointer, false);
}

inline void use_layout_effect(EffectFunction effect, MaybeDeps deps) {
  std::size_t index = reserve_hook_index();
  const std::vector<Value>* deps_pointer = deps ? &*deps : nullptr;
  schedule_effect(current_component_pointer(), index, std::move(effect), deps_pointer, true);
}

inline Box use_measure(const NodeRef& ref) {
  auto [box, set_box] = use_state<Box>(Box{});
  Renderer* renderer = current_component_renderer();
  std::shared_ptr<NodeHandle> slot = ref.slot;
  use_layout_effect([renderer, slot, set_box, box]() -> CleanupFunction {
    if (renderer && *slot != kNullNode) {
      Box next = renderer->measure(*slot);
      if (!(next == box)) set_box(next);
    }
    return {};
  });
  return box;
}

inline std::function<Box()> use_measure_callback(const NodeRef& ref) {
  Renderer* renderer = current_component_renderer();
  std::shared_ptr<NodeHandle> slot = ref.slot;
  return [renderer, slot]() -> Box {
    return renderer && *slot != kNullNode ? renderer->measure(*slot) : Box{};
  };
}

inline void use_document_event(const std::string& type, EventHandler handler) {
  Renderer* renderer = current_component_renderer();
  EventHandler& latest = use_ref<EventHandler>(EventHandler{});
  latest = std::move(handler);
  EventHandler* latest_pointer = &latest;
  use_effect(
      [renderer, type, latest_pointer]() -> CleanupFunction {
        if (!renderer) return {};
        NodeHandle document = renderer->document();
        ListenerToken token = renderer->add_event_listener(
            document, type,
            [latest_pointer](const Event& event) {
              EventHandler current = *latest_pointer;
              if (current) current(event);
            },
            false);
        return [renderer, document, type, token] {
          renderer->remove_event_listener(document, type, token);
        };
      },
      deps());
}

}
