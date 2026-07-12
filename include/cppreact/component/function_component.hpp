#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <type_traits>

#include "../value/object.hpp"

namespace cppreact {

struct VNode;

class FunctionComponent {
public:
  FunctionComponent() = default;
  FunctionComponent(std::nullptr_t) {}

  template <class Render,
            std::enable_if_t<std::conjunction_v<
                                 std::negation<std::is_same<std::decay_t<Render>, FunctionComponent>>,
                                 std::negation<std::is_same<std::decay_t<Render>, std::nullptr_t>>,
                                 std::is_invocable<std::decay_t<Render>&, const Object&>>,
                             int> = 0>
  FunctionComponent(Render render)
      : function(std::make_shared<std::function<VNode(const Object&)>>(std::move(render))) {}

  template <class... Children>
  VNode operator()(Object props = {}, Children&&... children) const;

  VNode render(const Object& properties) const;

  explicit operator bool() const { return function != nullptr; }

  bool operator==(const FunctionComponent& other) const { return function == other.function; }

private:
  std::shared_ptr<std::function<VNode(const Object&)>> function{};
};

}
