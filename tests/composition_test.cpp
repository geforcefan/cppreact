#include <cassert>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

#include "cppreact/context.hpp"
#include "cppreact/hooks.hpp"
#include "cppreact/render.hpp"
#include "cppreact/vnode.hpp"
#include "cppreact/renderers/test.hpp"

using namespace cppreact;

static VNode ToggleChild(const Props& props) {
    std::string name = props.get<std::string>("name").value_or("?");
    auto [on, set_on] = use_state<bool>(false);
    return h("div",
             {{"class", "child"},
              {"on_click", EventHandler{[set_on, on](const Event&) { set_on(!on); }}}},
             name + (on ? ":on" : ":off"));
}

static std::function<void(std::vector<std::string>)> set_names_external;
static VNode ParentList(const Props&) {
    auto [names, set_names] = use_state(std::vector<std::string>{"a", "b"});
    set_names_external = [set_names](std::vector<std::string> next) { set_names(std::move(next)); };
    std::vector<VNode> child_nodes;
    for (const std::string& name : names) child_nodes.push_back(h(ToggleChild, {{"key", name}, {"name", name}}));
    return h("div", {{"class", "list"}}, std::move(child_nodes));
}

static const Context<std::string>& theme() {
    static Context<std::string> context = create_context<std::string>("default");
    return context;
}

static VNode Consumer(const Props&) {
    return h("span", {}, use_context(theme()));
}

static VNode ProviderApp(const Props&) {
    std::vector<VNode> child_nodes;
    child_nodes.push_back(h(Consumer, {}));
    return provide(theme(), std::string("amber"), std::move(child_nodes));
}

static VNode Boxed(const Props&) {
    return h("div", {{"class", "box"}}, children());
}

int main() {
    {
        renderers::TestRenderer renderer;
        NodeHandle container = renderer.document();
        Container root = renderer.create_container();
        render(h(ParentList, {}), root);
        assert(renderer.serialize(container) ==
               "<div class=\"list\"><div class=\"child\">a:off</div>"
               "<div class=\"child\">b:off</div></div>");
        std::printf("PASS parent composes child components\n");

        NodeHandle text_a = renderer.find_by_text("a:off");
        NodeHandle child_a = renderer.parent_node(text_a);
        renderer.fire(child_a, "click");
        flush();
        assert(renderer.serialize(container) ==
               "<div class=\"list\"><div class=\"child\">a:on</div>"
               "<div class=\"child\">b:off</div></div>");
        std::printf("PASS child-local state toggles independently\n");

        set_names_external({"a", "b", "c"});
        flush();
        assert(renderer.serialize(container) ==
               "<div class=\"list\"><div class=\"child\">a:on</div>"
               "<div class=\"child\">b:off</div>"
               "<div class=\"child\">c:off</div></div>");
        std::printf("PASS child state survives a parent re-render (keyed instance reuse)\n");

        set_names_external({"b", "c"});
        flush();
        assert(renderer.serialize(container) ==
               "<div class=\"list\"><div class=\"child\">b:off</div>"
               "<div class=\"child\">c:off</div></div>");
        std::printf("PASS removing a keyed child unmounts exactly it\n");
    }

    {
        renderers::TestRenderer renderer;
        NodeHandle container = renderer.document();
        Container root = renderer.create_container();
        render(h(ProviderApp, {}), root);
        assert(renderer.serialize(container) == "<span>amber</span>");
        std::printf("PASS use_context reads the provided value\n");
    }

    {
        renderers::TestRenderer renderer;
        NodeHandle container = renderer.document();
        Container root = renderer.create_container();
        render(h(Consumer, {}), root);
        assert(renderer.serialize(container) == "<span>default</span>");
        std::printf("PASS use_context falls back to the default value\n");
    }

    {
        renderers::TestRenderer renderer;
        NodeHandle container = renderer.document();
        Container root = renderer.create_container();
        render(h(Boxed, {}, h("span", {}, std::string{"inner"}), h("b", {}, std::string{"x"})), root);
        assert(renderer.serialize(container) == "<div class=\"box\"><span>inner</span><b>x</b></div>");
        render(h(Boxed, {}, h("span", {}, std::string{"again"})), root);
        assert(renderer.serialize(container) == "<div class=\"box\"><span>again</span></div>");
        std::printf("PASS a component renders the children passed to it\n");
    }

    std::printf("ALL composition tests passed\n");
    return 0;
}
