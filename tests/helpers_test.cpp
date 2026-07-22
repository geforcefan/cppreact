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
    SECTION("map builds a node per item, with an optional index") {
        std::vector<std::string> names{"a", "b", "c"};
        std::vector<VNode> nodes = map(names, [](const std::string& name) {
            return View({.children = {name}});
        });

        REQUIRE(nodes.size() == 3);
        REQUIRE(is_element(nodes[1]));
        REQUIRE(std::get<ElementTag>(nodes[1].type).tag == "view");

        std::vector<VNode> indexed = map(names, [](const std::string&, std::size_t position) {
            return View({.children = {static_cast<double>(position)}});
        });
        REQUIRE(std::get<TextTag>(indexed[2].children[0].type).text == "2");
    }

    SECTION("when renders a node only when the condition holds") {
        VNode shown = when(true, View({.children = {"x"}}));
        REQUIRE(is_element(shown));

        VNode hidden = when(false, [] { return View({.children = {"x"}}); });
        REQUIRE(is_null(hidden));
    }

    SECTION("fragment wraps children") {
        VNode node = fragment(Text({}), Text({}));
        REQUIRE(is_fragment(node));
        REQUIRE(node.children.size() == 2);
    }
}
