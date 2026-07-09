#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "renderer.hpp"
#include "context.hpp"
#include "hook_slot.hpp"
#include "vnode.hpp"

namespace cppreact {

namespace component_bit {
constexpr std::uint8_t dirty = 1 << 0;
constexpr std::uint8_t rendering = 1 << 1;
}

struct ComponentInstance : std::enable_shared_from_this<ComponentInstance> {
  ComponentFunction render_function = nullptr;
  Props props{};
  ContextChain context{};

  Renderer* renderer = nullptr;
  NodeHandle parent_dom = kNullNode;
  std::int32_t depth = 0;

  std::vector<VNode> rendered{};
  std::vector<VNode> children{};
  std::vector<HookSlot> hooks{};
  std::vector<std::size_t> pending_effects{};
  std::vector<std::size_t> pending_layout_effects{};
  bool in_commit_queue = false;

  std::uint8_t bits = 0;

  ComponentInstance() = default;
  ~ComponentInstance();

  bool has(std::uint8_t bit) const;
  void set(std::uint8_t bit, bool on);
};

void enqueue_render(ComponentInstance* instance);

}

#include "component.inl"
#include "vnode.inl"
