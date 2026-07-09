#include <cassert>
#include <cstdio>
#include <string>

#include "cppreact/context.hpp"
#include "cppreact/hooks.hpp"
#include "cppreact/portal.hpp"
#include "cppreact/render.hpp"
#include "cppreact/vnode.hpp"
#include "cppreact/renderers/test.hpp"

using namespace cppreact;

static const Context<std::string>& label() {
  static Context<std::string> context = create_context<std::string>("default");
  return context;
}

static VNode Ported(const Props&) {
  return h("span", {{"class", "ported"}}, use_context(label()));
}

static NodeHandle layer_target = kNullNode;
static bool show = true;

static VNode App(const Props&) {
  return provide(label(), std::string("from-app"), h("div", {{"class", "inline"}}),
                 when(show, [] { return portal(layer_target, h(Ported, {})); }));
}

static std::string document_key{};
static VNode Listener(const Props&) {
  use_document_event("key_down", [](const Event& event) { document_key = event.key; });
  return h("div", {{"class", "listener"}});
}

int main() {
  {
    renderers::TestRenderer renderer;
    NodeHandle root_container = renderer.document();
    layer_target = renderer.create_element("layer", Props{});
    renderer.insert_before(root_container, layer_target, kNullNode);

    Container root = renderer.create_container();
    render(h(App, {}), root);

    assert(renderer.serialize(layer_target) == "<span class=\"ported\">from-app</span>");
    std::printf("PASS portal children mount into the target and read context across the boundary\n");

    show = false;
    render(h(App, {}), root);
    assert(renderer.serialize(layer_target).empty());
    std::printf("PASS unmounting the portal tears down its target subtree\n");
  }

  {
    renderers::TestRenderer renderer;
    NodeHandle document = renderer.document();
    Container root = renderer.create_container();
    render(h(Listener, {}), root);

    renderer.fire(document, "key_down", Event{.key = "Escape"});
    assert(document_key == "Escape");
    std::printf("PASS use_document_event delivers a document event to the handler\n");

    render(fragment(), root);
    document_key.clear();
    renderer.fire(document, "key_down", Event{.key = "Enter"});
    assert(document_key.empty());
    std::printf("PASS unmounting removes the document listener\n");
  }

  std::printf("ALL portal tests passed\n");
  return 0;
}
