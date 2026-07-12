#include <string>

#include <catch2/catch_test_macros.hpp>

#include "cppreact/render.hpp"
#include "cppreact/hosts/html_string.hpp"
#include "cppreact/vnode/tags.hpp"
#include "cppreact/vnode/create.hpp"

using namespace cppreact;
using namespace cppreact::tags;

static int click_calls = 0;
static int other_calls = 0;
static int click1_calls = 0;
static int click2_calls = 0;
static int mouse_down_calls = 0;
static int capture_calls = 0;
static int bubble_calls = 0;
static Event seen{};

TEST_CASE("events") {
    hosts::HtmlStringHost renderer;
    Container scratch = renderer.create_container();

    SECTION("should only register on prefixed functions as handlers") {
        click_calls = 0;
        other_calls = 0;

        render(View({{"click", Callback{[](const Event&) { ++other_calls; }}},
                         {"on_click", Callback{[](const Event&) { ++click_calls; }}}}),
               scratch);

        REQUIRE(renderer.inner_html() == "<view></view>");

        renderer.dispatch_event(renderer.find_first("view"), "click");
        REQUIRE(click_calls == 1);
        REQUIRE(other_calls == 0);
    }

    SECTION("should only register truthy values as handlers") {
        click_calls = 0;
        other_calls = 0;

        render(View({{"on_click", Callback{}},
                         {"on_other_click", Callback{[](const Event&) { ++other_calls; }}}}),
               scratch);

        DomNode target = renderer.find_first("view");
        renderer.dispatch_event(target, "click");
        REQUIRE(click_calls == 0);

        renderer.dispatch_event(target, "other_click");
        REQUIRE(other_calls == 1);
    }

    SECTION("should update event handlers") {
        click1_calls = 0;
        click2_calls = 0;

        render(View({{"on_click", Callback{[](const Event&) { ++click1_calls; }}}}),
               scratch);

        renderer.dispatch_event(renderer.find_first("view"), "click");
        REQUIRE(click1_calls == 1);
        REQUIRE(click2_calls == 0);

        click1_calls = 0;

        render(View({{"on_click", Callback{[](const Event&) { ++click2_calls; }}}}),
               scratch);

        renderer.dispatch_event(renderer.find_first("view"), "click");
        REQUIRE(click1_calls == 0);
        REQUIRE(click2_calls == 1);
    }

    SECTION("should remove event handlers") {
        click_calls = 0;
        mouse_down_calls = 0;

        render(View({{"on_click", Callback{[](const Event&) { ++click_calls; }}},
                         {"on_mouse_down", Callback{[](const Event&) { ++mouse_down_calls; }}}}),
               scratch);
        render(View({{"on_click", Callback{[](const Event&) { ++click_calls; }}}}),
               scratch);

        DomNode target = renderer.find_first("view");
        renderer.dispatch_event(target, "mouse_down");
        REQUIRE(mouse_down_calls == 0);

        render(View({}), scratch);

        renderer.dispatch_event(renderer.find_first("view"), "click");
        REQUIRE(click_calls == 0);
    }

    SECTION("should register events not appearing on dom nodes") {
        other_calls = 0;

        render(View({{"on_animation_end", Callback{[](const Event&) { ++other_calls; }}}}),
               scratch);

        renderer.dispatch_event(renderer.find_first("view"), "animation_end");
        REQUIRE(other_calls == 1);
    }

    SECTION("should use capturing for event props ending with capture") {
        capture_calls = 0;

        render(View({{"on_click_capture", Callback{[](const Event&) { ++capture_calls; }}}},
                 Text({{"type", "button"}}, "Click me")),
               scratch);

        renderer.dispatch_event(renderer.find_first("text"), "click");
        REQUIRE(capture_calls == 1);
    }

    SECTION("should support both capturing and non-capturing events on the same element") {
        capture_calls = 0;
        bubble_calls = 0;

        render(View({{"on_click", Callback{[](const Event&) { ++bubble_calls; }}},
                         {"on_click_capture", Callback{[](const Event&) { ++capture_calls; }}}},
                 Text({})),
               scratch);

        renderer.dispatch_event(renderer.find_first("text"), "click");
        REQUIRE(capture_calls == 1);
        REQUIRE(bubble_calls == 1);
    }
}

TEST_CASE("event payload") {
    hosts::HtmlStringHost renderer;
    Container scratch = renderer.create_container();

    SECTION("delivers position, modifiers, key and wheel data to the handler") {
        render(View({{"class", "target"},
                         {"on_drag", Callback{[](const Event& event) { seen = event; }}},
                         {"on_key_down", Callback{[](const Event& event) { seen = event; }}},
                         {"on_wheel", Callback{[](const Event& event) { seen = event; }}}}),
               scratch);
        DomNode target = renderer.find_first("view");

        renderer.dispatch_event(target, "drag", Event{.client_x = 12.5, .client_y = -4, .shift_key = true});
        REQUIRE(seen.type == "drag");
        REQUIRE(seen.target == target);
        REQUIRE(seen.client_x == 12.5);
        REQUIRE(seen.client_y == -4);
        REQUIRE((seen.shift_key && !seen.ctrl_key && !seen.meta_key));

        renderer.dispatch_event(target, "key_down", Event{.ctrl_key = true, .key = "escape"});
        REQUIRE((seen.type == "key_down" && seen.key == "escape" && seen.ctrl_key));

        renderer.dispatch_event(target, "wheel", Event{.delta_y = 120});
        REQUIRE((seen.type == "wheel" && seen.delta_y == 120));
    }

    SECTION("stop_propagation and prevent_default invoke the renderer capabilities") {
        render(View({{"on_click", Callback{[](const Event& event) {
                    event.stop_propagation();
                    event.prevent_default();
                }}}}),
               scratch);

        REQUIRE((!renderer.propagation_stopped && !renderer.default_prevented));
        renderer.dispatch_event(renderer.find_first("view"), "click");
        REQUIRE((renderer.propagation_stopped && renderer.default_prevented));
    }

    SECTION("stop_propagation halts bubbling to ancestors") {
        bubble_calls = 0;

        render(View({{"on_click", Callback{[](const Event&) { ++bubble_calls; }}}},
                 Text({{"on_click", Callback{[](const Event& event) {
                     event.stop_propagation();
                 }}}})),
               scratch);

        renderer.dispatch_event(renderer.find_first("text"), "click");
        REQUIRE(bubble_calls == 0);
    }
}
