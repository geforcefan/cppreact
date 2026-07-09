#pragma once

#include <functional>
#include <memory>
#include <vector>

#include "value.hpp"

namespace cppreact {

using CleanupFunction = std::function<void()>;
using EffectFunction = std::function<CleanupFunction()>;

struct HookSlot {
  std::unique_ptr<void, void (*)(void*)> state{nullptr, +[](void*) {}};
  std::vector<Value> deps{};
  bool has_deps = false;
  EffectFunction pending{};
  CleanupFunction cleanup{};
};

}
