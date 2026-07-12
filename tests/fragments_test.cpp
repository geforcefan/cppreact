#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "cppreact/hooks/hooks.hpp"
#include "cppreact/render.hpp"
#include "cppreact/hosts/html_string.hpp"
#include "cppreact/vnode/tags.hpp"
#include "cppreact/vnode/create.hpp"

using namespace cppreact;
using namespace cppreact::tags;

static std::vector<std::string> ops;
static FunctionComponent stateful_component = nullptr;
static FunctionComponent x_component = nullptr;
static StateSetter<int> set_app_state;

static const FunctionComponent Stateful = [](const Object&) -> VNode {
    int& renders = use_ref<int>(0);
    ++renders;
    if (renders > 1) ops.push_back("Update Stateful");
    return View({}, "Hello");
};

TEST_CASE("Fragment") {
    hosts::HtmlStringHost renderer;
    Container scratch = renderer.create_container();
    ops.clear();
    stateful_component = Stateful;

    SECTION("should not render empty Fragment") {
        render(fragment(), scratch);
        REQUIRE(renderer.inner_html() == "");
    }

    SECTION("should render a single child") {
        render(fragment(Text({}, "foo")), scratch);

        REQUIRE(renderer.inner_html() == "<text>foo</text>");
    }

    SECTION("should render multiple children via noop renderer") {
        render(fragment("hello ", Text({}, "world")), scratch);

        REQUIRE(renderer.inner_html() == "hello <text>world</text>");
    }

    SECTION("should just render children for fragments") {
        const FunctionComponent TestComponent = [](const Object&) -> VNode {
            return fragment(View({}, "Child1"), View({}, "Child2"));
        };

        render(TestComponent({}), scratch);
        REQUIRE(renderer.inner_html() == "<view>Child1</view><view>Child2</view>");
    }

    SECTION("should handle reordering components that return Fragments #1325") {
        const FunctionComponent X = [](const Object&) -> VNode {
            return fragment(children());
        };
        x_component = X;

        const FunctionComponent App = [](const Object& props) -> VNode {
            double i = props.get<double>("i",0);
            if (i == 0) {
                return View({},
                         x_component({{"key", "1"}}, "1"),
                         x_component({{"key", "2"}}, "2"));
            }
            return View({},
                     x_component({{"key", "2"}}, "2"),
                     x_component({{"key", "1"}}, "1"));
        };

        render(App({{"i", 0.0}}), scratch);
        REQUIRE(renderer.inner_html() == "<view>12</view>");
        render(App({{"i", 1.0}}), scratch);
        REQUIRE(renderer.inner_html() == "<view>21</view>");
    }

    SECTION("should handle changing node type within a Component that returns a Fragment #1326") {
        const FunctionComponent X = [](const Object&) -> VNode {
            return fragment(children());
        };
        x_component = X;

        const FunctionComponent App = [](const Object&) -> VNode {
            auto [i, set_state] = use_state<int>(0);
            set_app_state = set_state;
            if (i == 0) {
                return View({},
                         x_component({}, Text({}, "1")),
                         x_component({}, Text({}, "2"), Text({}, "2")));
            }
            return View({},
                     x_component({}, View({}, "1")),
                     x_component({}, Text({}, "2"), Text({}, "2")));
        };

        render(App({}), scratch);
        REQUIRE(renderer.inner_html() ==
                "<view><text>1</text><text>2</text><text>2</text></view>");

        set_app_state(1);
        flush();

        REQUIRE(renderer.inner_html() ==
                "<view><view>1</view><text>2</text><text>2</text></view>");
    }

    SECTION("should preserve state of children with 1 level nesting") {
        const FunctionComponent Foo = [](const Object& props) -> VNode {
            bool condition = props.get<bool>("condition",false);
            if (condition) return stateful_component({{"key", "a"}});
            return fragment(stateful_component({{"key", "a"}}),
                            View({{"key", "b"}}, "World"));
        };

        render(Foo({{"condition", true}}), scratch);
        render(Foo({{"condition", false}}), scratch);

        REQUIRE(ops == std::vector<std::string>{"Update Stateful"});
        REQUIRE(renderer.inner_html() == "<view>Hello</view><view>World</view>");

        render(Foo({{"condition", true}}), scratch);

        REQUIRE(ops == std::vector<std::string>{"Update Stateful", "Update Stateful"});
        REQUIRE(renderer.inner_html() == "<view>Hello</view>");
    }

    SECTION("should preserve state between top-level fragments") {
        const FunctionComponent Foo = [](const Object& props) -> VNode {
            bool condition = props.get<bool>("condition",false);
            if (condition) return fragment(stateful_component({}));
            return fragment(stateful_component({}));
        };

        render(Foo({{"condition", true}}), scratch);
        render(Foo({{"condition", false}}), scratch);

        REQUIRE(ops == std::vector<std::string>{"Update Stateful"});
        REQUIRE(renderer.inner_html() == "<view>Hello</view>");

        render(Foo({{"condition", true}}), scratch);

        REQUIRE(ops == std::vector<std::string>{"Update Stateful", "Update Stateful"});
        REQUIRE(renderer.inner_html() == "<view>Hello</view>");
    }

    SECTION("should preserve state of children nested at same level") {
        const FunctionComponent Foo = [](const Object& props) -> VNode {
            bool condition = props.get<bool>("condition",false);
            if (condition) {
                return fragment(fragment(fragment(stateful_component({{"key", "a"}}))));
            }
            return fragment(
                fragment(fragment(View({}), stateful_component({{"key", "a"}}))));
        };

        render(Foo({{"condition", true}}), scratch);
        render(Foo({{"condition", false}}), scratch);

        REQUIRE(ops == std::vector<std::string>{"Update Stateful"});
        REQUIRE(renderer.inner_html() == "<view></view><view>Hello</view>");

        render(Foo({{"condition", true}}), scratch);

        REQUIRE(ops == std::vector<std::string>{"Update Stateful", "Update Stateful"});
        REQUIRE(renderer.inner_html() == "<view>Hello</view>");
    }

    SECTION("should not preserve state in non-top-level fragment nesting") {
        const FunctionComponent Foo = [](const Object& props) -> VNode {
            bool condition = props.get<bool>("condition",false);
            if (condition) return fragment(fragment(stateful_component({{"key", "a"}})));
            return fragment(stateful_component({{"key", "a"}}));
        };

        render(Foo({{"condition", true}}), scratch);
        render(Foo({{"condition", false}}), scratch);

        REQUIRE(ops == std::vector<std::string>{});
        REQUIRE(renderer.inner_html() == "<view>Hello</view>");

        render(Foo({{"condition", true}}), scratch);

        REQUIRE(ops == std::vector<std::string>{});
        REQUIRE(renderer.inner_html() == "<view>Hello</view>");
    }

    SECTION("should not preserve state of children if nested 2 levels without siblings") {
        const FunctionComponent Foo = [](const Object& props) -> VNode {
            bool condition = props.get<bool>("condition",false);
            if (condition) return stateful_component({{"key", "a"}});
            return fragment(fragment(stateful_component({{"key", "a"}})));
        };

        render(Foo({{"condition", true}}), scratch);
        render(Foo({{"condition", false}}), scratch);

        REQUIRE(ops == std::vector<std::string>{});
        REQUIRE(renderer.inner_html() == "<view>Hello</view>");

        render(Foo({{"condition", true}}), scratch);

        REQUIRE(ops == std::vector<std::string>{});
        REQUIRE(renderer.inner_html() == "<view>Hello</view>");
    }

    SECTION("should not preserve state of children if nested 2 levels with siblings") {
        const FunctionComponent Foo = [](const Object& props) -> VNode {
            bool condition = props.get<bool>("condition",false);
            if (condition) return stateful_component({{"key", "a"}});
            return fragment(fragment(stateful_component({{"key", "a"}})), View({}));
        };

        render(Foo({{"condition", true}}), scratch);
        render(Foo({{"condition", false}}), scratch);

        REQUIRE(ops == std::vector<std::string>{});
        REQUIRE(renderer.inner_html() == "<view>Hello</view><view></view>");

        render(Foo({{"condition", true}}), scratch);

        REQUIRE(ops == std::vector<std::string>{});
        REQUIRE(renderer.inner_html() == "<view>Hello</view>");
    }
}
