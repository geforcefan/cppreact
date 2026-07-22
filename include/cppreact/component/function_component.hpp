#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <type_traits>
#include <utility>

#include "../value/payload.hpp"

namespace cppreact {

struct VNode;

namespace detail {

using ComponentRender = std::shared_ptr<std::function<VNode(const Payload&)>>;

template <class Render>
struct render_properties {};

template <class Lambda, class Properties>
struct render_properties<VNode (Lambda::*)(const Properties&) const> {
  using type = Properties;
};

}

template <class Properties>
class FunctionComponent {
public:
  FunctionComponent() = default;
  FunctionComponent(std::nullptr_t) {}

  template <class Render>
    requires(!std::is_same_v<std::decay_t<Render>, FunctionComponent<Properties>> &&
             !std::is_same_v<std::decay_t<Render>, std::nullptr_t> &&
             std::is_invocable_r_v<VNode, const std::decay_t<Render>&, const Properties&>)
  FunctionComponent(Render render);

  explicit FunctionComponent(detail::ComponentRender wrapped)
      : component_render(std::move(wrapped)) {}

  VNode operator()(Properties properties = {}) const;

  const detail::ComponentRender& render_function() const { return component_render; }

  explicit operator bool() const { return component_render != nullptr; }

  bool operator==(const FunctionComponent& other) const {
    return component_render == other.component_render;
  }

private:
  detail::ComponentRender component_render{};
};

template <class Render>
FunctionComponent(Render)
    -> FunctionComponent<typename detail::render_properties<decltype(&Render::operator())>::type>;

}
