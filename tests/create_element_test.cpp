#include <string>
#include <variant>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "cppreact/vnode/tags.hpp"
#include "cppreact/vnode/create.hpp"
#include "cppreact/vnode/clone.hpp"

using namespace cppreact;
using namespace cppreact::tags;

TEST_CASE("createElement(jsx)") {
    SECTION("should return a VNode") {
        VNode node = h("foo", {});

        REQUIRE(is_element(node));
        REQUIRE(std::get<ElementTag>(node.type).tag == "foo");
        REQUIRE(node.props.size() == 0);
    }

    SECTION("should set VNode#type property") {
        VNode element_node = h("view", {});
        REQUIRE(is_element(element_node));
        REQUIRE(std::get<ElementTag>(element_node.type).tag == "view");

        const FunctionComponent TestComponent = [](const Object&) -> VNode { return View({}); };
        VNode component_node = h(TestComponent, {});
        REQUIRE(is_component(component_node));
        REQUIRE(std::get<FunctionComponent>(component_node.type) == TestComponent);
    }

    SECTION("should set VNode#props property") {
        VNode node = h("view", {{"foo", "bar"}});

        REQUIRE(node.props.get("foo"));
        REQUIRE(node.props.get<std::string>("foo","") == "bar");
    }

    SECTION("should set VNode#key property") {
        VNode node = h("view", {});
        REQUIRE(node.key.empty());

        VNode node_with_attribute = h("view", {{"class", "panel"}});
        REQUIRE(node_with_attribute.key.empty());

        VNode node_with_key = h("view", {{"key", "1"}});
        REQUIRE(node_with_key.key == "1");
    }

    SECTION("should not set VNode#props.key property") {
        REQUIRE_FALSE(h("view", {}).props.get("key"));
        REQUIRE_FALSE(h("view", {{"key", "1"}}).props.get("key"));
    }

    SECTION("should set VNode#ref property") {
        VNode node = h("view", {});
        REQUIRE_FALSE(has_reference(node.ref));

        VNode node_with_attribute = h("view", {{"class", "panel"}});
        REQUIRE_FALSE(has_reference(node_with_attribute.ref));

        VNode node_with_callback = h("view", {{"ref", Callback{[](DomNode) {}}}});
        REQUIRE(has_reference(node_with_callback.ref));

        VNode node_with_object = h("view", {{"ref", ReferenceObject{}}});
        REQUIRE(has_reference(node_with_object.ref));
    }

    SECTION("should not set VNode#props.ref property") {
        REQUIRE_FALSE(h("view", {}).props.get("ref"));
        REQUIRE_FALSE(h("view", {{"ref", Callback{[](DomNode) {}}}}).props.get("ref"));
        REQUIRE_FALSE(h("view", {{"ref", ReferenceObject{}}}).props.get("ref"));
    }

    SECTION("should support element children") {
        VNode child_one = h("bar", {});
        VNode child_two = h("baz", {});
        VNode node = h("foo", {}, std::move(child_one), std::move(child_two));

        REQUIRE(node.children.size() == 2);
        REQUIRE(is_element(node.children[0]));
        REQUIRE(std::get<ElementTag>(node.children[0].type).tag == "bar");
        REQUIRE(is_element(node.children[1]));
        REQUIRE(std::get<ElementTag>(node.children[1].type).tag == "baz");
    }

    SECTION("should support multiple element children, given as arg list") {
        VNode child_one = h("bar", {});
        VNode child_three = h("test", {});
        VNode child_two = h("baz", {}, std::move(child_three));

        VNode node = h("foo", {}, std::move(child_one), std::move(child_two));

        REQUIRE(node.children.size() == 2);
        REQUIRE(std::get<ElementTag>(node.children[0].type).tag == "bar");
        REQUIRE(std::get<ElementTag>(node.children[1].type).tag == "baz");
        REQUIRE(node.children[1].children.size() == 1);
        REQUIRE(std::get<ElementTag>(node.children[1].children[0].type).tag == "test");
    }

    SECTION("should handle multiple element children, given as an array") {
        VNode child_one = h("bar", {});
        VNode child_three = h("test", {});
        VNode child_two = h("baz", {}, std::move(child_three));

        std::vector<VNode> children;
        children.push_back(std::move(child_one));
        children.push_back(std::move(child_two));
        VNode node = h("foo", {}, std::move(children));

        REQUIRE(node.children.size() == 2);
        REQUIRE(std::get<ElementTag>(node.children[0].type).tag == "bar");
        REQUIRE(std::get<ElementTag>(node.children[1].type).tag == "baz");
        REQUIRE(node.children[1].children.size() == 1);
        REQUIRE(std::get<ElementTag>(node.children[1].children[0].type).tag == "test");
    }

    SECTION("should support nested children") {
        VNode node = h("foo", {}, h("a", {}), h("b", {}, h("c", {})));

        REQUIRE(node.children.size() == 2);
        REQUIRE(std::get<ElementTag>(node.children[0].type).tag == "a");
        REQUIRE(std::get<ElementTag>(node.children[1].type).tag == "b");
        REQUIRE(node.children[1].children.size() == 1);
        REQUIRE(std::get<ElementTag>(node.children[1].children[0].type).tag == "c");
    }

    SECTION("should support text children") {
        VNode node = h("foo", {}, "textstuff");

        REQUIRE(node.children.size() == 1);
        REQUIRE(is_text(node.children[0]));
        REQUIRE(std::get<TextTag>(node.children[0].type).text == "textstuff");
    }

    SECTION("should NOT merge adjacent text children") {
        VNode bar_node = h("bar", {});
        VNode baz_node = h("baz", {});
        VNode baz2_node = h("baz", {});

        VNode node = h("foo", {}, "one", "two", std::move(bar_node), "three", std::move(baz_node),
                       std::move(baz2_node), "four", "five", "six");

        REQUIRE(node.children.size() == 9);
        REQUIRE(std::get<TextTag>(node.children[0].type).text == "one");
        REQUIRE(std::get<TextTag>(node.children[1].type).text == "two");
        REQUIRE(is_element(node.children[2]));
        REQUIRE(std::get<ElementTag>(node.children[2].type).tag == "bar");
        REQUIRE(std::get<TextTag>(node.children[3].type).text == "three");
        REQUIRE(is_element(node.children[4]));
        REQUIRE(std::get<ElementTag>(node.children[4].type).tag == "baz");
        REQUIRE(is_element(node.children[5]));
        REQUIRE(std::get<ElementTag>(node.children[5].type).tag == "baz");
        REQUIRE(std::get<TextTag>(node.children[6].type).text == "four");
        REQUIRE(std::get<TextTag>(node.children[7].type).text == "five");
        REQUIRE(std::get<TextTag>(node.children[8].type).text == "six");
    }
}
