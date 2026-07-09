#pragma once

#include "component.hpp"
#include "options.hpp"

namespace cppreact {

inline ComponentInstance::~ComponentInstance() {
  if (options().unmount) options().unmount(*this);
  for (HookSlot& slot : hooks) {
    if (slot.cleanup) slot.cleanup();
  }
}

inline bool ComponentInstance::has(std::uint8_t bit) const {
  return (bits & bit) != 0;
}

inline void ComponentInstance::set(std::uint8_t bit, bool on) {
  if (on) {
    bits |= bit;
  } else {
    bits &= static_cast<std::uint8_t>(~bit);
  }
}

}
