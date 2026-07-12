#include <algorithm>
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

static int serial = 0;
static std::vector<std::function<int()>> callbacks;
static int handler_child_renders = 0;
static FunctionComponent handler_child = nullptr;
static Callback stable_handler{};

static long count_handler_sets(const std::vector<std::string>& log) {
    return std::count_if(log.begin(), log.end(), [](const std::string& entry) {
        return entry.rfind("set handler", 0) == 0;
    });
}

TEST_CASE("useCallback") {
    hosts::HtmlStringHost renderer;
    Container scratch = renderer.create_container();

    SECTION("only recomputes the callback when inputs change") {
        serial = 0;
        callbacks.clear();

        const FunctionComponent TestComponent = [](const Object& props) -> VNode {
            double a = props.get<double>("a",0);
            double b = props.get<double>("b",0);
            int created = ++serial;
            std::function<int()> callback = use_callback<std::function<int()>>(
                [created] { return created; }, {a, b});
            callbacks.push_back(callback);
            return fragment();
        };

        render(TestComponent({{"a", 1.0}, {"b", 1.0}}), scratch);
        render(TestComponent({{"a", 1.0}, {"b", 1.0}}), scratch);

        REQUIRE(callbacks[0]() == callbacks[1]());
        REQUIRE(callbacks[1]() == 1);

        render(TestComponent({{"a", 1.0}, {"b", 2.0}}), scratch);
        render(TestComponent({{"a", 1.0}, {"b", 2.0}}), scratch);

        REQUIRE(callbacks[1]() != callbacks[2]());
        REQUIRE(callbacks[2]() == callbacks[3]());
        REQUIRE(callbacks[2]() == 3);
    }

    SECTION("an unchanged handler is not re-attached to the node") {
        stable_handler = Callback{[](const Event&) {}};

        const FunctionComponent App = [](const Object& props) -> VNode {
            return View({{"class", props.get<std::string>("class","")},
                        {"on_click", stable_handler}});
        };

        render(App({{"class", "first"}}), scratch);
        const long after_mount = count_handler_sets(renderer.log);

        render(App({{"class", "second"}}), scratch);
        REQUIRE(count_handler_sets(renderer.log) == after_mount);

        stable_handler = Callback{[](const Event&) {}};
        render(App({{"class", "third"}}), scratch);
        REQUIRE(count_handler_sets(renderer.log) == after_mount + 1);
    }

    SECTION("memo bails on a stable use_callback handler prop") {
        handler_child_renders = 0;

        const FunctionComponent Child = [](const Object&) -> VNode {
            ++handler_child_renders;
            return View({});
        };
        handler_child = memo(Child);

        const FunctionComponent App = [](const Object& props) -> VNode {
            auto on_click = use_callback(Callback{[](const Event&) {}}, {});
            return View({{"class", props.get<std::string>("class","")}},
                     handler_child({{"on_click", on_click}}));
        };

        render(App({{"class", "first"}}), scratch);
        REQUIRE(handler_child_renders == 1);

        render(App({{"class", "second"}}), scratch);
        REQUIRE(handler_child_renders == 1);
    }

    SECTION("memo re-renders on a fresh inline handler prop") {
        handler_child_renders = 0;

        const FunctionComponent Child = [](const Object&) -> VNode {
            ++handler_child_renders;
            return View({});
        };
        handler_child = memo(Child);

        const FunctionComponent App = [](const Object& props) -> VNode {
            return View({{"class", props.get<std::string>("class","")}},
                     handler_child({{"on_click", Callback{[](const Event&) {}}}}));
        };

        render(App({{"class", "first"}}), scratch);
        REQUIRE(handler_child_renders == 1);

        render(App({{"class", "second"}}), scratch);
        REQUIRE(handler_child_renders == 2);
    }
}
