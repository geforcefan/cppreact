#pragma once

#include <string>
#include <type_traits>
#include <variant>

#include "callback.hpp"
#include "../vnode/reference.hpp"
#include "payload.hpp"

namespace cppreact {

using Value = std::variant<std::monostate, bool, double, std::string, Callback,
                           ReferenceObject, Payload>;

inline bool values_equal(const Value& left, const Value& right) {
  if (left.index() != right.index()) return false;
  return std::visit(
      [&right](const auto& stored) -> bool {
        using Stored = std::decay_t<decltype(stored)>;
        if constexpr (std::is_same_v<Stored, double>) {
          const double other = std::get<double>(right);
          return stored == other || (stored != stored && other != other);
        } else {
          return stored == std::get<Stored>(right);
        }
      },
      left);
}

}
