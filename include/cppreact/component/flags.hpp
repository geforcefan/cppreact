#pragma once

#include <cstdint>

namespace cppreact {

namespace component_flag {
constexpr std::uint8_t force = 1 << 2;
constexpr std::uint8_t dirty = 1 << 3;
}

}
