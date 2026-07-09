#include <cassert>
#include <cstdio>
#include <functional>
#include <vector>

#include "cppreact/hooks.hpp"
#include "cppreact/render.hpp"
#include "cppreact/vnode.hpp"
#include "cppreact/renderers/test.hpp"

using namespace cppreact;

static NodeRef shared{};
static int placement = 0;
static std::function<void()> rerender;
static int callback_fires = 0;

static VNode Movable(const Props&) {
  NodeRef ref = use_node_ref();
  shared = ref;
  auto [tick, set_tick] = use_state<int>(0);
  rerender = [set_tick, tick] { set_tick(tick + 1); };

  int mode = placement;
  Props alpha{{"class", "alpha"}};
  Props beta{{"class", "beta"}};
  if (mode == 0) alpha.set("ref", ref.prop());
  if (mode == 1) beta.set("ref", ref.prop());

  std::vector<VNode> children_list;
  children_list.push_back(h("div", std::move(alpha)));
  children_list.push_back(h("div", std::move(beta)));
  return h("div", {{"class", "root"}}, std::move(children_list));
}

static VNode Counted(const Props&) {
  auto [tick, set_tick] = use_state<int>(0);
  rerender = [set_tick, tick] { set_tick(tick + 1); };
  Ref counter = [](NodeHandle) { ++callback_fires; };
  return h("div", {{"class", "counted"}, {"ref", Payload{std::make_shared<Ref>(counter)}}});
}

int main() {
  renderers::TestRenderer renderer;
  Container root = renderer.create_container();

  placement = 0;
  render(h(Movable, {}), root);
  assert(shared.current() != kNullNode);
  NodeHandle mounted = shared.current();
  std::printf("PASS ref attaches to the element that carries it on mount\n");

  placement = 0;
  rerender();
  flush();
  assert(shared.current() == mounted);
  std::printf("PASS a stable ref on a reused node keeps its node across re-render\n");

  placement = 1;
  rerender();
  flush();
  NodeHandle moved = shared.current();
  assert(moved != kNullNode && moved != mounted);
  std::printf("PASS ref moving between two living siblings detaches then attaches (final target wins)\n");

  placement = 2;
  rerender();
  flush();
  assert(shared.current() == kNullNode);
  std::printf("PASS ref removed from a reused node detaches to null\n");

  renderers::TestRenderer other;
  Container second = other.create_container();
  callback_fires = 0;
  render(h(Counted, {}), second);
  int after_mount = callback_fires;
  assert(after_mount == 1);
  rerender();
  flush();
  assert(callback_fires == after_mount);
  std::printf("PASS a stable ref does not re-fire on a plain re-render\n");

  std::printf("ALL ref reuse tests passed\n");
  return 0;
}
