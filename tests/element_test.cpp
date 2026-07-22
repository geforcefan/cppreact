#include <string>
#include <variant>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "cppreact/vnode/tags.hpp"
#include "cppreact/vnode/create.hpp"
#include "cppreact/vnode/clone.hpp"

using namespace cppreact;
using namespace cppreact::tags;

namespace {

struct WidgetProps {
  double count = 0;

  bool operator==(const WidgetProps&) const = default;
};

const FunctionComponent Widget = [](const WidgetProps&) -> VNode { return View({}); };

}

TEST_CASE("elements") {
    SECTION("an element call returns an element vnode with its tag") {
        VNode node = View({});

        REQUIRE(is_element(node));
        REQUIRE(std::get<ElementTag>(node.type).tag == "view");
        REQUIRE(std::get<ElementTag>(node.type).apply != nullptr);
    }

    SECTION("a component call returns a component vnode with its render function") {
        VNode node = Widget({.count = 1});
        REQUIRE(is_component(node));
        REQUIRE(std::get<ComponentTag>(node.type).render == Widget.render_function());
    }

    SECTION("typed props land as the payload of the vnode") {
        VNode node = Widget({.count = 3});
        std::shared_ptr<const WidgetProps> stored =
            payload_as<WidgetProps>(std::get<ComponentTag>(node.type).props);
        REQUIRE(stored);
        REQUIRE(stored->count == 3);
    }

    SECTION("the key field lifts onto the vnode") {
        REQUIRE(View({}).key.empty());
        REQUIRE(View({.class_name = "panel"}).key.empty());
        REQUIRE(View({.key = "1"}).key == "1");
    }

    SECTION("the ref field lifts onto the vnode") {
        REQUIRE_FALSE(has_reference(View({}).ref));
        REQUIRE(has_reference(View({.ref = ReferenceObject{}}).ref));
        REQUIRE(has_reference(View({.ref = Callback{[](DomNode) {}}}).ref));
    }

    SECTION("braced children become vnode children in order") {
        VNode node = View({.children = {Text({.class_name = "a"}), Text({.class_name = "b"})}});

        REQUIRE(node.children.size() == 2);
        REQUIRE(is_element(node.children[0]));
        REQUIRE(std::get<ElementTag>(node.children[0].type).tag == "text");
    }

    SECTION("nested children nest") {
        VNode node = View({.children = {View({.class_name = "a"}),
                                        View({.class_name = "b",
                                              .children = {View({.class_name = "c"})}})}});

        REQUIRE(node.children.size() == 2);
        REQUIRE(node.children[1].children.size() == 1);
    }

    SECTION("text and number children become text vnodes, never merged") {
        VNode node = View({.children = {"one", "two", View({}), 3}});

        REQUIRE(node.children.size() == 4);
        REQUIRE(std::get<TextTag>(node.children[0].type).text == "one");
        REQUIRE(std::get<TextTag>(node.children[1].type).text == "two");
        REQUIRE(is_element(node.children[2]));
        REQUIRE(std::get<TextTag>(node.children[3].type).text == "3");
    }

    SECTION("children built as a vector move in unchanged") {
        std::vector<VNode> collected;
        collected.push_back(View({}));
        collected.push_back(Text({}));
        VNode node = View({.children = std::move(collected)});

        REQUIRE(node.children.size() == 2);
        REQUIRE(std::get<ElementTag>(node.children[1].type).tag == "text");
    }

    SECTION("clone copies type, key, ref, and children deep") {
        VNode original = View({.class_name = "panel",
                               .key = "k",
                               .children = {Text({.children = {"hi"}})}});
        VNode copy = clone_vnode(original);

        REQUIRE(std::get<ElementTag>(copy.type).tag == "view");
        REQUIRE(copy.key == "k");
        REQUIRE(copy.children.size() == 1);
        REQUIRE(copy.children[0].children.size() == 1);
    }
}
