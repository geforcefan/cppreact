#include <cassert>
#include <cstdio>
#include <functional>
#include <string>

#include "cppreact/hooks.hpp"
#include "cppreact/render.hpp"
#include "cppreact/vnode.hpp"
#include "cppreact/renderers/test.hpp"

using namespace cppreact;

static const auto theme = create_context<std::string>("light");

static int consumer_renders = 0;
static int middle_renders = 0;

static VNode ThemeText(const Props&) {
  const std::string& value = use_context(theme);
  ++consumer_renders;
  return h("span", {}, value);
}

static VNode Middle(const Props&) {
  ++middle_renders;
  return h(ThemeText, {});
}
static const Component MemoMiddle = memo(Middle);

static VNode Root(const Props& props) {
  auto value = props.get<std::string>("value").value_or("light");
  return provide(theme, value, h("div", {}, MemoMiddle({})));
}

static VNode Plain(const Props&) {
  return h("p", {}, use_context(theme));
}

int main() {
  {
    renderers::TestRenderer renderer;
    Container root = renderer.create_container();
    render(h(Root, {{"value", std::string("dark")}}), root);
    assert(renderer.serialize() == "<div><span>dark</span></div>");
    int middle_after_mount = middle_renders;

    render(h(Root, {{"value", std::string("solar")}}), root);
    flush();
    assert(renderer.serialize() == "<div><span>solar</span></div>");
    assert(middle_renders == middle_after_mount);
    std::printf("PASS a context update reaches the consumer through a memoized component\n");
  }
  {
    consumer_renders = 0;
    renderers::TestRenderer renderer;
    Container root = renderer.create_container();
    render(h(Root, {{"value", std::string("dark")}}), root);
    int renders_after_mount = consumer_renders;

    render(h(Root, {{"value", std::string("dark")}}), root);
    flush();
    assert(consumer_renders == renders_after_mount);
    std::printf("PASS an equal context value notifies nobody\n");
  }
  {
    renderers::TestRenderer renderer;
    Container root = renderer.create_container();
    render(h(Plain, {}), root);
    assert(renderer.serialize() == "<p>light</p>");
    std::printf("PASS a consumer without a provider reads the default\n");
  }
  {
    renderers::TestRenderer renderer;
    Container root = renderer.create_container();
    render(h(Root, {{"value", std::string("dark")}}), root);
    render(fragment(), root);
    render(h(Root, {{"value", std::string("solar")}}), root);
    flush();
    assert(renderer.serialize() == "<div><span>solar</span></div>");
    std::printf("PASS unmounting consumers unsubscribes them\n");
  }
  std::printf("ALL context update tests passed\n");
  return 0;
}
