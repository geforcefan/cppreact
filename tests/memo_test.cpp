#include <cassert>
#include <cstdio>
#include <functional>
#include <string>

#include "cppreact/hooks.hpp"
#include "cppreact/render.hpp"
#include "cppreact/vnode.hpp"
#include "cppreact/renderers/test.hpp"

using namespace cppreact;

static int stable_renders = 0;
static int changing_renders = 0;
static int plain_renders = 0;
static std::function<void()> bump_parent;

static VNode StableBody(const Props&) {
  ++stable_renders;
  return h("span", {}, "s");
}

static VNode ChangingBody(const Props& props) {
  ++changing_renders;
  return h("span", {}, std::to_string(static_cast<int>(props.get<double>("value").value_or(0))));
}

static VNode PlainBody(const Props&) {
  ++plain_renders;
  return h("span", {}, "p");
}

static const Component StableMemo = memo(StableBody);
static const Component ChangingMemo = memo(ChangingBody);

static VNode Parent(const Props&) {
  auto [tick, set_tick] = use_state<int>(0);
  bump_parent = [set_tick, tick] { set_tick(tick + 1); };
  return h("div", {}, StableMemo({{"label", std::string("x")}}),
           ChangingMemo({{"value", static_cast<double>(tick)}}), h(PlainBody, {}));
}

int main() {
  renderers::TestRenderer renderer;
  Container root = renderer.create_container();
  render(h(Parent, {}), root);
  assert(stable_renders == 1 && changing_renders == 1 && plain_renders == 1);
  std::printf("PASS initial render mounts every child once\n");

  bump_parent();
  flush();
  assert(stable_renders == 1);
  assert(changing_renders == 2);
  assert(plain_renders == 2);
  assert(renderer.serialize() == "<div><span>s</span><span>1</span><span>p</span></div>");
  std::printf("PASS memo bails on unchanged props, re-renders on a changed prop, plain always re-renders\n");

  bump_parent();
  flush();
  assert(stable_renders == 1 && changing_renders == 3 && plain_renders == 3);
  assert(renderer.serialize() == "<div><span>s</span><span>2</span><span>p</span></div>");
  std::printf("PASS memo stays bailed while the changed child keeps updating in place\n");

  std::printf("ALL memo tests passed\n");
  return 0;
}
