#include <functional>
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

struct Store {
    int selected = 0;
    int other = 0;
    std::vector<std::function<void()>> listeners;
    void notify() {
        for (const auto& listener : listeners) listener();
    }
};

static Store store;
static int render_count = 0;

static const FunctionComponent SelectedView = [](const Object&) -> VNode {
    ++render_count;
    int value = use_sync_external_store(
        [](std::function<void()> on_change) -> CleanupFunction {
            store.listeners.push_back(std::move(on_change));
            return [] {};
        },
        [] { return store.selected; });
    return View({}, value);
};

TEST_CASE("useSyncExternalStore") {
    hosts::HtmlStringHost renderer;
    Container scratch = renderer.create_container();
    store = Store{};
    render_count = 0;

    SECTION("an unselected change does not re-render") {
        render(SelectedView({}), scratch);
        REQUIRE(renderer.inner_html() == "<view>0</view>");
        const int mounted_renders = render_count;

        store.other = 5;
        store.notify();
        flush();

        REQUIRE(render_count == mounted_renders);
        REQUIRE(renderer.inner_html() == "<view>0</view>");
    }

    SECTION("a selected change re-renders with the new value") {
        render(SelectedView({}), scratch);
        const int mounted_renders = render_count;

        store.selected = 7;
        store.notify();
        flush();

        REQUIRE(render_count == mounted_renders + 1);
        REQUIRE(renderer.inner_html() == "<view>7</view>");
    }
}
