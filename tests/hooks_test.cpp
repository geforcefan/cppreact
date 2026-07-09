#include <cassert>
#include <cstdio>
#include <string>
#include <vector>

#include "cppreact/hooks.hpp"
#include "cppreact/render.hpp"
#include "cppreact/vnode.hpp"
#include "cppreact/renderers/test.hpp"

using namespace cppreact;

static VNode Counter(const Props&) {
    auto [count, set_count] = use_state<int>(0);
    return h("button",
             {{"on_click", EventHandler{[set_count, count](const Event&) { set_count(count + 1); }}}},
             std::to_string(count));
}

static int effect_runs = 0;
static int cleanup_runs = 0;
static int effect_dep = 0;
static int memo_computes = 0;
static int memo_dep = 0;

static int lazy_state_calls = 0;
static std::function<void()> bump_lazy;

static VNode LazyState(const Props&) {
    auto [value, set_value] = use_state<int>([] {
        ++lazy_state_calls;
        return 7;
    });
    bump_lazy = [set_value, value = value] { set_value(value + 1); };
    return h("span", {}, std::to_string(value));
}

static VNode RenderCounter(const Props& props) {
    int& renders = use_ref<int>(0);
    renders += 1;
    auto tick = props.get<double>("tick").value_or(0);
    (void)tick;
    return h("span", {}, std::to_string(renders));
}

static VNode EffectComponent(const Props&) {
    use_effect(
        []() {
            ++effect_runs;
            return []() { ++cleanup_runs; };
        },
        deps(effect_dep));

    int doubled = use_memo<int>([]() { ++memo_computes; return memo_dep * 2; },
                               deps(memo_dep));
    return h("span", {}, std::to_string(doubled));
}

int main() {
    {
        renderers::TestRenderer renderer;
        NodeHandle container = renderer.document();
        Container root = renderer.create_container();

        render(h(Counter, {}), root);
        assert(renderer.serialize(container) == "<button>0</button>");

        NodeHandle button = renderer.find_first("button");
        renderer.fire(button, "click");
        flush();
        assert(renderer.serialize(container) == "<button>1</button>");

        renderer.fire(button, "click");
        flush();
        assert(renderer.serialize(container) == "<button>2</button>");
        std::printf("PASS use_state re-renders on event\n");
    }

    {
        effect_runs = cleanup_runs = 0;
        effect_dep = 0;
        memo_computes = 0;
        memo_dep = 0;

        renderers::TestRenderer renderer;
        NodeHandle container = renderer.document();
        Container root = renderer.create_container();

        render(h(EffectComponent, {}), root);
        assert(effect_runs == 1);
        assert(cleanup_runs == 0);
        assert(memo_computes == 1);

        render(h(EffectComponent, {}), root);
        assert(effect_runs == 1);
        assert(memo_computes == 1);

        effect_dep = 1;
        render(h(EffectComponent, {}), root);
        assert(cleanup_runs == 1);
        assert(effect_runs == 2);
        assert(memo_computes == 1);

        memo_dep = 5;
        render(h(EffectComponent, {}), root);
        assert(memo_computes == 2);
        assert(effect_runs == 2);
        assert(renderer.serialize(container) == "<span>10</span>");

        render(h("div", {}), root);
        assert(cleanup_runs == 2);
        assert(renderer.serialize(container) == "<div></div>");
        std::printf("PASS use_effect lifecycle and use_memo memoization\n");
    }

    {
        renderers::TestRenderer renderer;
        Container root = renderer.create_container();
        render(h(LazyState, {}), root);
        assert(renderer.serialize() == "<span>7</span>");
        assert(lazy_state_calls == 1);

        bump_lazy();
        flush();
        assert(renderer.serialize() == "<span>8</span>");
        assert(lazy_state_calls == 1);
        std::printf("PASS the lazy use_state initializer runs once\n");
    }

    {
        renderers::TestRenderer renderer;
        Container root = renderer.create_container();
        render(h(RenderCounter, {{"tick", 1.0}}), root);
        assert(renderer.serialize() == "<span>1</span>");

        render(h(RenderCounter, {{"tick", 2.0}}), root);
        render(h(RenderCounter, {{"tick", 3.0}}), root);
        assert(renderer.serialize() == "<span>3</span>");
        std::printf("PASS use_ref keeps its value across renders without triggering one\n");
    }

    std::printf("ALL hooks tests passed\n");
    return 0;
}
