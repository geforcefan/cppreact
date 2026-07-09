#pragma once

#include <functional>
#include <vector>

namespace cppreact {

struct VNode;
struct ComponentInstance;

struct Options {
  std::function<void(VNode&)> vnode{};
  std::function<void(VNode&)> before_diff{};
  std::function<void(ComponentInstance&)> before_render{};
  std::function<void(ComponentInstance&)> diffed{};
  std::function<void(const std::vector<ComponentInstance*>&)> commit{};
  std::function<void(ComponentInstance&)> unmount{};
};

inline Options& options() {
  static Options instance{};
  return instance;
}

}
