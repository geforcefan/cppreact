#include <cassert>
#include <cstdio>
#include <string>
#include <vector>

#include "cppreact/hooks.hpp"
#include "cppreact/render.hpp"
#include "cppreact/vnode.hpp"
#include "cppreact/renderers/test.hpp"

using namespace cppreact;

static std::vector<std::string> order;

static VNode Effects(const Props& props) {
  auto tick = props.get<double>("tick").value_or(0);
  use_effect(
      [] {
        order.push_back("passive");
        return [] { order.push_back("passive cleanup"); };
      },
      deps(tick));
  use_layout_effect(
      [] {
        order.push_back("layout");
        return [] { order.push_back("layout cleanup"); };
      },
      deps(tick));
  return h("span", {});
}

int main() {
  renderers::TestRenderer renderer;
  Container root = renderer.create_container();

  render(h(Effects, {{"tick", 1.0}}), root);
  assert(order == (std::vector<std::string>{"layout", "passive"}));
  std::printf("PASS layout effects run before passive effects\n");

  order.clear();
  render(h(Effects, {{"tick", 2.0}}), root);
  assert(order ==
         (std::vector<std::string>{"layout cleanup", "layout", "passive cleanup", "passive"}));
  std::printf("PASS cleanups run before the next effect\n");

  order.clear();
  render(fragment(), root);
  assert(order ==
         (std::vector<std::string>{"layout cleanup", "passive cleanup"}) ||
         order == (std::vector<std::string>{"passive cleanup", "layout cleanup"}));
  std::printf("PASS cleanups run on unmount\n");

  std::printf("ALL layout effect tests passed\n");
  return 0;
}
