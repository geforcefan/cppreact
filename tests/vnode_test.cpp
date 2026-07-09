#include <cassert>
#include <cstdio>
#include <string>
#include <variant>

#include "cppreact/tags.hpp"
#include "cppreact/vnode.hpp"

using namespace cppreact;

static VNode Label(const Props&) {
    return h("span", {});
}

int main() {
    {
        VNode node = h("div", {{"class", std::string{"panel"}}});
        assert(is_element(node));
        assert(std::get<ElementTag>(node.type).tag == "div");
        assert(node.props.has("class"));
        std::printf("PASS h builds an element with props\n");
    }

    {
        VNode node = h("li", {{"key", std::string{"row-7"}}, {"class", std::string{"row"}}});
        assert(node.key == "row-7");
        assert(!node.props.has("key"));
        assert(node.props.has("class"));
        std::printf("PASS key is extracted from props\n");
    }

    {
        VNode node = h("p", {}, std::string{"count: "}, 42);
        assert(node.children.size() == 2);
        assert(is_text(node.children[0]));
        assert(std::get<TextTag>(node.children[0].type).text == "count: ");
        assert(is_text(node.children[1]));
        assert(std::get<TextTag>(node.children[1].type).text == "42");
        std::printf("PASS variadic children coerce to text nodes\n");
    }

    {
        VNode node = h("div", {{"label", std::string{"hits"}}, {"count", 3.0}, {"on", true}});
        assert(node.props.get<std::string>("label").value_or("count") == "hits");
        assert(!node.props.get<std::string>("missing").has_value());
        assert(node.props.get<double>("count").value_or(0.0) == 3.0);
        assert(node.props.get<bool>("on").value_or(false) == true);
        assert(!node.props.get<double>("label").has_value());
        std::printf("PASS Props::get returns an optional you default with value_or\n");
    }

    {
        VNode node = h(Label, {});
        assert(is_component(node));
        assert(std::get<ComponentFunction>(node.type) == &Label);
        std::printf("PASS h builds a component node keyed by function identity\n");
    }

    {
        using namespace cppreact::tags;
        VNode node = div({{"class", std::string{"x"}}}, span({}, std::string{"hi"}));
        assert(is_element(node));
        assert(std::get<ElementTag>(node.type).tag == "div");
        assert(node.children.size() == 1);
        assert(is_element(node.children[0]));
        assert(std::get<ElementTag>(node.children[0].type).tag == "span");

        Component Widget = [](const Props&) -> VNode { return span({}, std::string{"w"}); };
        VNode used = Widget({{"count", 1.0}});
        assert(is_component(used));
        std::printf("PASS tag helpers and Component objects build the right nodes\n");
    }

    {
        std::vector<std::string> names{"a", "b", "c"};
        std::vector<VNode> nodes = map(names, [](const std::string& name) { return h("li", {}, name); });
        assert(nodes.size() == 3);
        assert(is_element(nodes[1]));
        assert(std::get<ElementTag>(nodes[1].type).tag == "li");

        std::vector<VNode> indexed =
            map(names, [](const std::string&, std::size_t position) { return h("li", {}, static_cast<double>(position)); });
        assert(std::get<TextTag>(indexed[2].children[0].type).text == "2");
        std::printf("PASS map builds a node per item, with an optional index\n");
    }

    {
        VNode shown = when(true, h("li", {}, std::string{"x"}));
        assert(is_element(shown));
        VNode hidden = when(false, [] { return h("li", {}, std::string{"x"}); });
        assert(is_fragment(hidden));
        assert(hidden.children.empty());
        std::printf("PASS when renders a node only when the condition holds\n");
    }

    {
        VNode node = fragment(h("a", {}), h("b", {}));
        assert(is_fragment(node));
        assert(node.children.size() == 2);
        std::printf("PASS fragment wraps children\n");
    }

    std::printf("ALL vnode tests passed\n");
    return 0;
}
