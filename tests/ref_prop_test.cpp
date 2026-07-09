#include <cassert>
#include <cstdio>
#include <optional>
#include <utility>

#include "cppreact/hooks.hpp"
#include "cppreact/render.hpp"
#include "cppreact/vnode.hpp"
#include "cppreact/renderers/test.hpp"

using namespace cppreact;

static NodeRef passed_slot{};
static NodeRef ignored_slot{};
static bool saw_ref_prop = false;

static VNode PassesRef(const Props& props) {
  Props element{{"class", "inner"}};
  if (std::optional<Payload> ref = props.get<Payload>("ref")) element.set("ref", *ref);
  return h("div", std::move(element));
}
static const Component PassesRefComponent = PassesRef;

static VNode IgnoresRef(const Props& props) {
  saw_ref_prop = props.has("ref");
  return h("div", {{"class", "inner"}});
}
static const Component IgnoresRefComponent = IgnoresRef;

int main() {
  {
    renderers::TestRenderer renderer;
    Container root = renderer.create_container();
    render(PassesRefComponent({{"ref", passed_slot.prop()}}), root);
    NodeHandle inner = renderer.find_first("div");
    assert(inner != kNullNode && passed_slot.current() == inner);
    std::printf("PASS a component receives ref in its props and passes it to its host element\n");
  }
  {
    renderers::TestRenderer renderer;
    Container root = renderer.create_container();
    render(IgnoresRefComponent({{"ref", ignored_slot.prop()}}), root);
    assert(saw_ref_prop);
    assert(ignored_slot.current() == kNullNode);
    std::printf("PASS a component that ignores its ref prop attaches nothing\n");
  }
  {
    renderers::TestRenderer renderer;
    Container root = renderer.create_container();
    NodeRef element_slot{};
    render(h("div", {{"class", "direct"}, {"ref", element_slot.prop()}}), root);
    NodeHandle direct = renderer.find_first("div");
    assert(direct != kNullNode && element_slot.current() == direct);
    std::printf("PASS an element extracts ref out of props and attaches it\n");
  }
  std::printf("ALL ref prop tests passed\n");
  return 0;
}
