#pragma once

#include <utility>

#include "use_memo.hpp"

namespace cppreact {

template <class T>
T use_callback(T callback, Dependencies dependencies) {
  detail::current_hook = 8;
  return use_memo<T>([callback]() { return callback; }, std::move(dependencies));
}

}
