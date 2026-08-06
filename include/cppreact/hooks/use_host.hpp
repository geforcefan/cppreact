#pragma once

#include <cstddef>

namespace cppreact {

inline Host* use_host() {
  return detail::current_component ? detail::current_component->host : nullptr;
}

}
