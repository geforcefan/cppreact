#include <functional>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "cppreact/hooks/hooks.hpp"
#include "cppreact/render.hpp"
#include "cppreact/hosts/html_string.hpp"
#include "cppreact/vnode/tags.hpp"
#include "cppreact/vnode/create.hpp"
#include "cppreact/component/memo.hpp"

using namespace cppreact;
using namespace cppreact::tags;

namespace {

struct PairProps {
    double a = 0;
    double b = 0;
};

struct ClassProps {
    std::string class_name{};
};

struct HandlerChildProps {
    Callback on_click{};

    bool operator==(const HandlerChildProps&) const = default;
};

}

static int serial = 0;
static std::vector<std::function<int()>> callbacks;
static int handler_child_renders = 0;
static FunctionComponent<HandlerChildProps> handler_child = nullptr;

TEST_CASE("useCallback") {
    hosts::HtmlStringHost renderer;
    Container scratch = renderer.create_container();

    SECTION("only recomputes the callback when inputs change") {
        serial = 0;
        callbacks.clear();

        const FunctionComponent TestComponent = [](const PairProps& props) -> VNode {
            int created = ++serial;
            std::function<int()> callback = use_callback<std::function<int()>>(
                [created] { return created; }, {props.a, props.b});
            callbacks.push_back(callback);
            return fragment();
        };

        render(TestComponent({.a = 1, .b = 1}), scratch);
        render(TestComponent({.a = 1, .b = 1}), scratch);

        REQUIRE(callbacks[0]() == callbacks[1]());
        REQUIRE(callbacks[1]() == 1);

        render(TestComponent({.a = 1, .b = 2}), scratch);
        render(TestComponent({.a = 1, .b = 2}), scratch);

        REQUIRE(callbacks[1]() != callbacks[2]());
        REQUIRE(callbacks[2]() == callbacks[3]());
        REQUIRE(callbacks[2]() == 3);
    }

    SECTION("memo bails on a stable use_callback handler prop") {
        handler_child_renders = 0;

        const FunctionComponent Child = [](const HandlerChildProps&) -> VNode {
            ++handler_child_renders;
            return View({});
        };
        handler_child = memo(FunctionComponent{Child});

        const FunctionComponent App = [](const ClassProps& props) -> VNode {
            Callback on_click = use_callback(Callback{[](const Event&) {}}, {});
            return View({.class_name = props.class_name,
                         .children = {handler_child({.on_click = on_click})}});
        };

        render(App({.class_name = "first"}), scratch);
        REQUIRE(handler_child_renders == 1);

        render(App({.class_name = "second"}), scratch);
        REQUIRE(handler_child_renders == 1);
    }

    SECTION("memo re-renders on a fresh inline handler prop") {
        handler_child_renders = 0;

        const FunctionComponent Child = [](const HandlerChildProps&) -> VNode {
            ++handler_child_renders;
            return View({});
        };
        handler_child = memo(FunctionComponent{Child});

        const FunctionComponent App = [](const ClassProps& props) -> VNode {
            return View({.class_name = props.class_name,
                         .children = {handler_child({.on_click = Callback{[](const Event&) {}}})}});
        };

        render(App({.class_name = "first"}), scratch);
        REQUIRE(handler_child_renders == 1);

        render(App({.class_name = "second"}), scratch);
        REQUIRE(handler_child_renders == 2);
    }
}
