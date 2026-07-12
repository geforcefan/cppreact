#pragma once

#include <cstdint>

namespace cppreact {

namespace vnode_flag {
constexpr std::uint8_t insert_vnode = 1 << 2;
constexpr std::uint8_t matched = 1 << 1;
}

}
