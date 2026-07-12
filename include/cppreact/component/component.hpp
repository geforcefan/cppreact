#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <vector>

#include "flags.hpp"
#include "../vnode/create.hpp"

namespace cppreact {

class Host;
struct ComponentInstance;

using GlobalContext = std::map<std::size_t, std::shared_ptr<ComponentInstance>>;

struct RenderCallback {
  std::function<void()> callback{};
  std::int32_t hook_index = -1;
};

struct ComponentInstance : std::enable_shared_from_this<ComponentInstance> {
  FunctionComponent render_function{};
  Object props{};
  GlobalContext global_context{};
  std::uint8_t flags = 0;
  bool rendering_self = false;

  std::vector<RenderCallback> render_callbacks{};
  std::function<bool(const Object& next_props, const std::vector<VNode>& next_children)>
      should_component_update{};
  std::function<void(const Object& next_props)> component_will_update{};
  std::function<void()> component_will_unmount{};
  std::function<GlobalContext()> get_child_context{};
  std::function<void(const std::shared_ptr<ComponentInstance>&)> sub{};
  bool has_scu_from_hooks = false;
  std::shared_ptr<void> hooks{};

  std::vector<VNode> rendered{};
  std::vector<VNode> children{};
  Host* host = nullptr;
  DomNode parent_dom = null_dom_node;
  std::int32_t depth = 0;

  void set_state();
  void force_update();
};

void enqueue_render(const std::shared_ptr<ComponentInstance>& component);

void process();

void render_component(const std::shared_ptr<ComponentInstance>& component);

}
