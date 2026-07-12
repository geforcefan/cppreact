#pragma once

#include <functional>
#include <memory>
#include <string>
#include <type_traits>
#include <variant>

#include "callback.hpp"
#include "../vnode/reference.hpp"
#include "raw_payload.hpp"

namespace cppreact {

class Object;

using Value = std::variant<std::monostate, bool, double, std::string, Callback,
                           ReferenceObject, RawPayload, std::shared_ptr<Object>>;

namespace detail {

template <class T>
using typed_value = std::conditional_t<is_callback_signature<T>::value,
                                       std::function<typename callback_signature<T>::type>, T>;

}

template <class T>
const detail::typed_value<T>* value_as(const Value& value) {
  if constexpr (detail::is_callback_signature<T>::value) {
    const Callback* callback = std::get_if<Callback>(&value);
    return callback ? callback->as<T>() : nullptr;
  } else {
    return std::get_if<T>(&value);
  }
}

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
