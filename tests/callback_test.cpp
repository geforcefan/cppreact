#include <cassert>
#include <cstdio>
#include <functional>

#include "cppreact/hooks.hpp"
#include "cppreact/render.hpp"
#include "cppreact/vnode.hpp"
#include "cppreact/renderers/test.hpp"

using namespace cppreact;

static std::function<int()> stored;

static VNode CallbackHost(const Props& props) {
  int value = static_cast<int>(props.get<double>("value").value_or(0));
  int gate = static_cast<int>(props.get<double>("gate").value_or(0));
  stored = use_callback<std::function<int()>>([value] { return value; }, deps(gate));
  return h("span", {});
}

int main() {
  renderers::TestRenderer renderer;
  Container root = renderer.create_container();

  render(h(CallbackHost, {{"value", 1.0}, {"gate", 0.0}}), root);
  assert(stored() == 1);

  render(h(CallbackHost, {{"value", 2.0}, {"gate", 0.0}}), root);
  assert(stored() == 1);
  std::printf("PASS unchanged deps keep the first callback\n");

  render(h(CallbackHost, {{"value", 3.0}, {"gate", 1.0}}), root);
  assert(stored() == 3);
  std::printf("PASS changed deps swap in the new callback\n");

  std::printf("ALL callback tests passed\n");
  return 0;
}
