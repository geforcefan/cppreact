#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <vector>

namespace cppreact {

struct VNode;
struct ComponentInstance;
struct SyntheticEvent;
struct Container;

struct Options {
  std::function<void(VNode&)> vnode{};
  std::function<void(SyntheticEvent&)> event{};
  std::function<void(VNode&)> diff{};
  std::function<void(VNode&)> render{};
  std::function<void(VNode&)> diffed{};
  std::function<void(VNode&, const std::vector<std::weak_ptr<ComponentInstance>>&)> commit{};
  std::function<void(VNode&)> unmount{};
  std::function<void(const VNode&, const Container&)> root{};
  std::function<void(ComponentInstance&, std::size_t, int)> hook{};
  bool skip_effects = false;
};

inline Options& options() {
  static Options instance{};
  return instance;
}

}
