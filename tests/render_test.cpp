#include <cmath>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "cppreact/hooks/hooks.hpp"
#include "cppreact/render.hpp"
#include "cppreact/hosts/html_string.hpp"
#include "cppreact/vnode/tags.hpp"
#include "cppreact/vnode/create.hpp"
#include "cppreact/vnode/helpers.hpp"

using namespace cppreact;
using namespace cppreact::tags;

namespace {

struct HarnessProps {};

struct RenderedItemProps {
    bool render_as_null = false;
    std::string item_id{};
    std::string key{};
};

struct RetainProps {
    std::string name{};
};

struct RetainFooProps {
    bool condition = false;
};

struct StaleChildProps {
    int count = 0;
};

struct FalsyItemProps {
    int index = 0;
};

struct FalsyAppProps {
    std::string activation{};
};

struct RemoveClassProps {
    std::string class_name{};
};

}

static VNode reconcile_list(const std::vector<std::string>& values) {
    std::vector<VNode> items;
    for (const std::string& value : values) items.push_back(View({.key = value, .children = {value}}));
    return View({.children = std::move(items)});
}

static std::string reconcile_list_html(const std::vector<std::string>& values) {
    std::string html = "<view>";
    for (const std::string& value : values) html += "<view>" + value + "</view>";
    return html + "</view>";
}

static VNode shuffled_list(const std::vector<std::string>& keys) {
    std::vector<VNode> items;
    for (const std::string& key : keys) items.push_back(View({.key = key, .children = {key}}));
    return View({.children = std::move(items)});
}

static std::string shuffled_list_html(const std::vector<std::string>& keys) {
    std::string html = "<view>";
    for (const std::string& key : keys) html += "<view>" + key + "</view>";
    return html + "</view>";
}

static const FunctionComponent MixedFoo = [](const HarnessProps&) -> VNode { return "d"; };

static const FunctionComponent RenderedItem = [](const RenderedItemProps& props) -> VNode {
    if (props.render_as_null) return fragment();
    return View({.children = {props.item_id}});
};

static VNode shrink_list(const std::vector<std::pair<std::string, bool>>& items) {
    std::vector<VNode> nodes;
    for (const auto& [item_id, render_as_null] : items) {
        nodes.push_back(RenderedItem({.render_as_null = render_as_null,
                                      .item_id = item_id,
                                      .key = item_id}));
    }
    return View({.children = std::move(nodes)});
}

static std::function<void()> update_reuse_x;
static std::function<void()> update_reuse_app;

static const FunctionComponent ReuseX = [](const HarnessProps&) -> VNode {
    auto [i, set_state] = use_state<int>(0);
    update_reuse_x = [set_state = set_state, i = i] { set_state(i + 1); };
    return View({.children = {i}});
};

static const FunctionComponent ReuseApp = [](const HarnessProps&) -> VNode {
    auto [i, set_state] = use_state<int>(0);
    update_reuse_app = [set_state = set_state, i = i] { set_state(i ^ 1); };
    return View({.children = {when(i == 0, [] { return ReuseX({}); }), ReuseX({})}});
};

static const FunctionComponent RetainX = [](const RetainProps& props) -> VNode {
    std::string& name = use_ref<std::string>(props.name);
    return Text({.children = {name}});
};

static const FunctionComponent RetainFoo = [](const RetainFooProps& props) -> VNode {
    if (props.condition) return View({.children = {Text({}), RetainX({.name = "B"})}});
    return View({.children = {RetainX({.name = "A"})}});
};

static int stale_render_count = 0;
static std::function<void()> update_stale_app;
static std::function<void()> update_stale_parent;

static const FunctionComponent StaleChild = [](const StaleChildProps& props) -> VNode {
    return when(props.count >= 3, [] { return View({.children = {"foo"}}); });
};

static const FunctionComponent StaleParent = [](const HarnessProps&) -> VNode {
    auto [tick, set_tick] = use_state<int>(0);
    update_stale_parent = [set_tick = set_tick, tick = tick] { set_tick(tick + 1); };
    ++stale_render_count;
    return StaleChild({.count = stale_render_count});
};

static const FunctionComponent StaleApp = [](const HarnessProps&) -> VNode {
    auto [tick, set_tick] = use_state<int>(0);
    update_stale_app = [set_tick = set_tick, tick = tick] { set_tick(tick + 1); };
    return StaleParent({});
};

static std::vector<std::string> falsy_actions;

static const FunctionComponent FalsyComponent = [](const FalsyItemProps& props) -> VNode {
    int index = props.index;
    use_effect(
        [index]() -> CleanupFunction {
            falsy_actions.push_back("mounted " + std::to_string(index));
            return {};
        },
        {});
    return View({.children = {"Hello"}});
};

static const FunctionComponent FalsyApp = [](const FalsyAppProps& props) -> VNode {
    VNode first = props.activation == "1" ? FalsyComponent({.index = 1}) : View({});
    return View({.children = {std::move(first), fragment(), FalsyComponent({.index = 2}),
                             FalsyComponent({.index = 3})}});
};

TEST_CASE("render()") {
    hosts::HtmlStringHost renderer;
    Container scratch = renderer.create_container();

    SECTION("should rerender when value from \"\" to 0") {
        render("", scratch);
        REQUIRE(renderer.inner_html() == "");

        render(0, scratch);
        REQUIRE(renderer.inner_html() == "0");
    }

    SECTION("should render an empty text node given an empty string") {
        render("", scratch);

        DomNode empty_text = renderer.find_by_text("");
        REQUIRE(empty_text != null_dom_node);
        REQUIRE(renderer.parent_node(empty_text) == scratch.mount);
        REQUIRE(renderer.inner_html() == "");
    }

    SECTION("should allow node type change with content") {
        render(Text({.children = {"Bad"}}), scratch);
        render(View({.children = {"Good"}}), scratch);
        REQUIRE(renderer.inner_html() == "<view>Good</view>");
    }

    SECTION("should create empty nodes (<* />)") {
        render(View({}), scratch);
        REQUIRE(renderer.inner_html() == "<view></view>");

        hosts::HtmlStringHost renderer2;
        Container scratch2 = renderer2.create_container();
        render(Text({}), scratch2);
        REQUIRE(renderer2.inner_html() == "<text></text>");
    }

    SECTION("should merge new elements when called multiple times") {
        render(View({}), scratch);
        REQUIRE(renderer.inner_html() == "<view></view>");

        render(Text({}), scratch);
        REQUIRE(renderer.inner_html() == "<text></text>");

        render(Text({.class_name = "hello", .children = {"Hello!"}}), scratch);
        REQUIRE(renderer.inner_html() == "<text class=\"hello\">Hello!</text>");
    }

    SECTION("should render NaN as text content") {
        render(NAN, scratch);
        REQUIRE(renderer.inner_html() == "NaN");
    }

    SECTION("should render numbers (0) as text content") {
        render(0, scratch);
        REQUIRE(renderer.inner_html() == "0");
    }

    SECTION("should render numbers (42) as text content") {
        render(42, scratch);
        REQUIRE(renderer.inner_html() == "42");
    }

    SECTION("should render strings as text content") {
        render(std::string("Testing, huh! How is it going?"), scratch);
        REQUIRE(renderer.inner_html() == "Testing, huh! How is it going?");
    }

    SECTION("should render arrays of mixed elements") {
        std::vector<VNode> nested;
        nested.push_back("e");
        nested.push_back("f");

        std::vector<VNode> mixed;
        mixed.push_back(0);
        mixed.push_back("a");
        mixed.push_back("b");
        mixed.push_back(Text({.children = {"c"}}));
        mixed.push_back(MixedFoo({}));
        mixed.push_back(fragment());
        mixed.push_back(fragment());
        mixed.push_back(fragment());
        mixed.push_back(fragment(std::move(nested)));
        mixed.push_back(1);

        render(fragment(std::move(mixed)), scratch);
        REQUIRE(renderer.inner_html() == "0ab<text>c</text>def1");
    }

    SECTION("should apply class as String") {
        render(View({.class_name = "foo"}), scratch);
        REQUIRE(renderer.inner_html() == "<view class=\"foo\"></view>");
    }

    SECTION("should remove class attributes") {
        const FunctionComponent RemoveClassApp = [](const RemoveClassProps& props) -> VNode {
            return View({.class_name = props.class_name, .children = {Text({.children = {"Bye"}})}});
        };

        render(RemoveClassApp({.class_name = "hi"}), scratch);
        REQUIRE(renderer.inner_html() == "<view class=\"hi\"><text>Bye</text></view>");

        render(RemoveClassApp({}), scratch);
        REQUIRE(renderer.inner_html() == "<view><text>Bye</text></view>");
    }

    SECTION("should reorder child pairs") {
        render(View({.children = {Text({.children = {"a"}}), Text({.children = {"b"}})}}), scratch);

        DomNode a = renderer.find_first("a");
        DomNode b = renderer.find_first("b");

        render(View({.children = {Text({.children = {"b"}}), Text({.children = {"a"}})}}), scratch);

        REQUIRE(renderer.find_first("a") == a);
        REQUIRE(renderer.find_first("b") == b);
    }

    SECTION("should not reuse unkeyed components") {
        render(ReuseApp({}), scratch);
        REQUIRE(renderer.inner_html() == "<view><view>0</view><view>0</view></view>");

        update_reuse_x();
        update_reuse_app();
        flush();
        REQUIRE(renderer.inner_html() == "<view><view>1</view></view>");

        update_reuse_app();
        flush();
        REQUIRE(renderer.inner_html() == "<view><view>0</view><view>1</view></view>");
    }

    SECTION("should reconcile children in right order") {
        render(reconcile_list({"A", "B", "C", "D", "E"}), scratch);
        REQUIRE(renderer.inner_html() == reconcile_list_html({"A", "B", "C", "D", "E"}));

        render(reconcile_list({"B", "E", "C", "D"}), scratch);
        REQUIRE(renderer.inner_html() == reconcile_list_html({"B", "E", "C", "D"}));
    }

    SECTION("should reconcile children in right order #2") {
        render(reconcile_list({"A", "B", "C", "D", "E"}), scratch);
        REQUIRE(renderer.inner_html() == reconcile_list_html({"A", "B", "C", "D", "E"}));

        render(reconcile_list({"B", "E", "D", "C"}), scratch);
        REQUIRE(renderer.inner_html() == reconcile_list_html({"B", "E", "D", "C"}));
    }

    SECTION("should reconcile children in right order #3") {
        render(View({.children = {Text({.children = {"_A1"}}), Text({.children = {"_A2"}}),
                                  View({.children = {"_A3"}}), Text({.children = {"_A4"}}),
                                  View({.children = {"_A5"}}), Text({.children = {"_A6"}}),
                                  View({.children = {"_A7"}}), Text({.children = {"_A8"}})}}),
               scratch);

        render(View({.children = {Text({.children = {"_B1"}}), Text({.children = {"_B2"}}),
                                  Text({.children = {"_B3"}}), View({.children = {"_B4"}}),
                                  Text({.children = {"_B5"}}), Text({.children = {"_B6"}}),
                                  View({.children = {"_B7"}}), Text({.children = {"_B8"}})}}),
               scratch);

        REQUIRE(renderer.inner_html() ==
                "<view><text>_B1</text><text>_B2</text><text>_B3</text><view>_B4</view><text>_B5</text><text>_B6</text>"
                "<view>_B7</view><text>_B8</text></view>");
    }

    SECTION("should reconcile children in right order #4") {
        render(View({.children = {Text({.children = {"_A1"}}), Text({.children = {"_A2"}}),
                                  View({.children = {"_A3"}}), View({.children = {"_A4"}}),
                                  Text({.children = {"_A5"}}), View({.children = {"_A6"}}),
                                  View({.children = {"_A7"}}), Text({.children = {"_A8"}}),
                                  View({.children = {"_A9"}}), View({.children = {"_A10"}}),
                                  Text({.children = {"_A11"}}), View({.children = {"_A12"}})}}),
               scratch);

        render(View({.children = {Text({.children = {"_B1"}}), Text({.children = {"_B2"}}),
                                  Text({.children = {"_B3"}}), View({.children = {"_B4"}}),
                                  Text({.children = {"_B5"}}), Text({.children = {"_B6"}}),
                                  Text({.children = {"_B7"}}), View({.children = {"_B8"}}),
                                  Text({.children = {"_B9"}}), Text({.children = {"_B10"}}),
                                  Text({.children = {"_B11"}}), Text({.children = {"_B12"}}),
                                  View({.children = {"_B13"}})}}),
               scratch);

        REQUIRE(renderer.inner_html() ==
                "<view><text>_B1</text><text>_B2</text><text>_B3</text><view>_B4</view><text>_B5</text><text>_B6</text><text>_B7</text>"
                "<view>_B8</view><text>_B9</text><text>_B10</text><text>_B11</text><text>_B12</text><view>_B13</view></view>");
    }

    SECTION("should shrink lists") {
        render(shrink_list({{"One", false}, {"Two", false}, {"Three", false}, {"Four", false}}),
               scratch);
        REQUIRE(renderer.inner_html() ==
                "<view><view>One</view><view>Two</view><view>Three</view><view>Four</view></view>");

        render(shrink_list({{"One", false}, {"Four", true}, {"Six", false}, {"Seven", false}}),
               scratch);
        REQUIRE(renderer.inner_html() == "<view><view>One</view><view>Six</view><view>Seven</view></view>");
    }

    SECTION("handles shuffled child-ordering") {
        std::vector<std::string> a{"0", "1", "2", "3", "4", "5", "6"};
        std::vector<std::string> b{"1", "3", "5", "2", "6", "4", "0"};
        std::vector<std::string> c{"11", "3", "1", "4", "6", "2", "5", "0", "9", "10"};

        render(shuffled_list(a), scratch);
        REQUIRE(renderer.inner_html() == shuffled_list_html(a));

        render(shuffled_list(b), scratch);
        REQUIRE(renderer.inner_html() == shuffled_list_html(b));

        render(shuffled_list(c), scratch);
        REQUIRE(renderer.inner_html() == shuffled_list_html(c));

        render(shuffled_list(a), scratch);
        REQUIRE(renderer.inner_html() == shuffled_list_html(a));
    }

    SECTION("handle shuffled array children (moving to the front)") {
        std::vector<std::string> a{"0", "2", "7", "6", "1", "3", "5", "4"};
        std::vector<std::string> b{"1", "0", "6", "7", "5", "2", "4", "3"};
        std::vector<std::string> c{"0", "7", "2", "1", "3", "5", "6", "4"};

        render(shuffled_list(a), scratch);
        REQUIRE(renderer.inner_html() == shuffled_list_html(a));

        render(shuffled_list(b), scratch);
        REQUIRE(renderer.inner_html() == shuffled_list_html(b));

        render(shuffled_list(c), scratch);
        REQUIRE(renderer.inner_html() == shuffled_list_html(c));
    }

    SECTION("should retain state for inserted children") {
        render(RetainFoo({}), scratch);
        REQUIRE(renderer.inner_html() == "<view><text>A</text></view>");

        render(RetainFoo({.condition = true}), scratch);
        REQUIRE(renderer.inner_html() == "<view><text></text><text>A</text></view>");

        render(RetainFoo({}), scratch);
        REQUIRE(renderer.inner_html() == "<view><text>A</text></view>");
    }

    SECTION("should not lead to stale DOM nodes") {
        stale_render_count = 0;

        render(StaleApp({}), scratch);

        update_stale_app();
        flush();
        update_stale_parent();
        flush();
        update_stale_app();
        flush();

        REQUIRE(renderer.inner_html() == "<view>foo</view>");
    }

    SECTION("should not remount components when replacing a component with a falsy value in-between") {
        falsy_actions.clear();

        render(FalsyApp({.activation = "1"}), scratch);
        REQUIRE(falsy_actions == std::vector<std::string>{"mounted 1", "mounted 2", "mounted 3"});

        render(FalsyApp({.activation = "2"}), scratch);
        REQUIRE(falsy_actions == std::vector<std::string>{"mounted 1", "mounted 2", "mounted 3"});
    }
}
