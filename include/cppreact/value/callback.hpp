#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <type_traits>
#include <utility>

namespace cppreact {

namespace detail {

template <class Signature>
const void* signature_marker() {
  static const char marker = 0;
  return &marker;
}

template <class CallOperator>
struct callable_signature;

template <class Result, class Owner, class... Arguments>
struct callable_signature<Result (Owner::*)(Arguments...) const> {
  using type = Result(Arguments...);
};

template <class Result, class Owner, class... Arguments>
struct callable_signature<Result (Owner::*)(Arguments...)> {
  using type = Result(Arguments...);
};

template <class Callable>
using signature_for = typename callable_signature<decltype(&Callable::operator())>::type;

template <class T>
struct callback_signature {
  using type = T;
};

template <class Signature>
struct callback_signature<std::function<Signature>> {
  using type = Signature;
};

}

class Callback {
public:
  Callback() = default;
  Callback(std::nullptr_t) {}

  template <class Callable, class Signature = detail::signature_for<std::decay_t<Callable>>,
            std::enable_if_t<!std::is_same_v<std::decay_t<Callable>, Callback>, int> = 0>
  Callback(Callable callable)
      : function(std::make_shared<std::function<Signature>>(std::move(callable))),
        signature(detail::signature_marker<Signature>()) {}

  template <class T>
  const std::function<typename detail::callback_signature<T>::type>* as() const {
    using Signature = typename detail::callback_signature<T>::type;
    if (signature != detail::signature_marker<Signature>()) return nullptr;
    return static_cast<const std::function<Signature>*>(function.get());
  }

  explicit operator bool() const { return function != nullptr; }

  bool operator==(const Callback& other) const { return function == other.function; }

  const void* identity() const { return function.get(); }

private:
  std::shared_ptr<void> function{};
  const void* signature = nullptr;
};

}
