#include <cstddef>
#include <string>
#include <variant>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "cppreact/vnode/tags.hpp"
#include "cppreact/vnode/create.hpp"
#include "cppreact/vnode/clone.hpp"
#include "cppreact/vnode/helpers.hpp"

using namespace cppreact;
using namespace cppreact::tags;

TEST_CASE("helpers") {
    SECTION("tag helpers and component objects build the right nodes") {
        VNode node = View({{"class", "x"}}, Text({}, "hi"));
        REQUIRE(is_element(node));
        REQUIRE(std::get<ElementTag>(node.type).tag == "view");
        REQUIRE(node.children.size() == 1);
        REQUIRE(is_element(node.children[0]));
        REQUIRE(std::get<ElementTag>(node.children[0].type).tag == "text");

        const FunctionComponent Widget = [](const Object&) -> VNode { return Text({}, "w"); };
        VNode used = Widget({{"count", 1.0}});
        REQUIRE(is_component(used));
    }

    SECTION("Object::get returns an optional you default with value_or") {
        VNode node = View({{"label", "hits"}, {"count", 3.0}, {"on", true}});

        REQUIRE(node.props.get<std::string>("label","count") == "hits");
        REQUIRE_FALSE(node.props.get<std::string>("missing").has_value());
        REQUIRE(node.props.get<double>("count",0.0) == 3.0);
        REQUIRE(node.props.get<bool>("on",false) == true);
        REQUIRE_FALSE(node.props.get<double>("label").has_value());
    }

    SECTION("map builds a node per item, with an optional index") {
        std::vector<std::string> names{"a", "b", "c"};
        std::vector<VNode> nodes =
            map(names, [](const std::string& name) { return View({}, name); });

        REQUIRE(nodes.size() == 3);
        REQUIRE(is_element(nodes[1]));
        REQUIRE(std::get<ElementTag>(nodes[1].type).tag == "view");

        std::vector<VNode> indexed = map(names, [](const std::string&, std::size_t position) {
            return View({}, static_cast<double>(position));
        });
        REQUIRE(std::get<TextTag>(indexed[2].children[0].type).text == "2");
    }

    SECTION("when renders a node only when the condition holds") {
        VNode shown = when(true, View({}, "x"));
        REQUIRE(is_element(shown));

        VNode hidden = when(false, [] { return View({}, "x"); });
        REQUIRE(is_null(hidden));
    }

    SECTION("fragment wraps children") {
        VNode node = fragment(Text({}), Text({}));
        REQUIRE(is_fragment(node));
        REQUIRE(node.children.size() == 2);
    }
}
