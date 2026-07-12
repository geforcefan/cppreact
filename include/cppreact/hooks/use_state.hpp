#pragma once

#include <functional>
#include <type_traits>
#include <utility>
#include <variant>

#include "use_reducer.hpp"

namespace cppreact {

template <class T>
using StateUpdate = std::variant<T, std::function<T(const T&)>>;

template <class T>
class StateSetter {
public:
  StateSetter() = default;
  StateSetter(std::function<void(StateUpdate<T>)> dispatch) : dispatch(std::move(dispatch)) {}

  void operator()(T next) const {
    if (dispatch) dispatch(StateUpdate<T>{std::move(next)});
  }
  void operator()(const std::function<T(const T&)>& update) const {
    if (dispatch) dispatch(StateUpdate<T>{update});
  }

private:
  std::function<void(StateUpdate<T>)> dispatch{};
};

namespace detail {

template <class T>
T invoke_or_return(const T& current_value, const StateUpdate<T>& update) {
  if (const auto* update_function = std::get_if<std::function<T(const T&)>>(&update)) {
    return (*update_function)(current_value);
  }
  return std::get<T>(update);
}

}

template <class T>
std::pair<T, StateSetter<T>> use_state(T initial_state) {
  detail::current_hook = 1;
  auto [value, dispatch] =
      use_reducer<T, StateUpdate<T>>(&detail::invoke_or_return<T>, std::move(initial_state));
  return {std::move(value), StateSetter<T>(std::move(dispatch))};
}

template <class T, class Initialize,
          std::enable_if_t<std::is_invocable_r_v<T, Initialize&>, int> = 0>
std::pair<T, StateSetter<T>> use_state(Initialize initialize) {
  detail::current_hook = 1;
  auto [value, dispatch] = use_reducer<T, StateUpdate<T>>(&detail::invoke_or_return<T>,
                                                          std::function<T()>(initialize));
  return {std::move(value), StateSetter<T>(std::move(dispatch))};
}

}
