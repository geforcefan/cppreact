#include <cassert>
#include <cstdio>
#include <functional>

#include "cppreact/hooks.hpp"
#include "cppreact/render.hpp"
#include "cppreact/vnode.hpp"
#include "cppreact/renderers/test.hpp"

using namespace cppreact;

static NodeRef probe_ref{};
static Box measured{};
static std::function<void()> rerender;

static VNode Measured(const Props&) {
  NodeRef ref = use_node_ref();
  probe_ref = ref;
  auto [tick, set_tick] = use_state<int>(0);
  rerender = [set_tick, tick] { set_tick(tick + 1); };
  measured = use_measure(ref);
  return h("div", {{"class", "box"}, {"ref", ref.prop()}});
}

int main() {
  renderers::TestRenderer renderer;
  Container root = renderer.create_container();
  render(h(Measured, {}), root);

  NodeHandle node = renderer.find_first("div");
  assert(probe_ref.current() == node);
  std::printf("PASS ref receives the mounted node\n");

  assert(measured == Box{});
  std::printf("PASS unmeasured node reports an empty box without looping\n");

  renderer.set_box(node, Box{10, 20, 120, 24});
  rerender();
  flush();
  assert((measured == Box{10, 20, 120, 24}));
  std::printf("PASS use_measure reports the node's box and converges\n");

  std::printf("ALL ref/measure tests passed\n");
  return 0;
}
