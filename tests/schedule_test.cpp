#include <cassert>
#include <cstdio>
#include <functional>
#include <string>

#include "cppreact/hooks.hpp"
#include "cppreact/render.hpp"
#include "cppreact/vnode.hpp"
#include "cppreact/renderers/test.hpp"

using namespace cppreact;

static std::function<void(int)> counter_set;

static VNode Counter(const Props&) {
    auto [count, set_count] = use_state<int>(0);
    counter_set = [set_count](int value) { set_count(value); };
    return h("button", {}, std::to_string(count));
}

static int parent_renders = 0;
static int child_renders = 0;
static std::function<void(int)> parent_set;
static std::function<void(int)> child_set;

static VNode Child(const Props&) {
    auto [value, set_value] = use_state<int>(0);
    ++child_renders;
    child_set = [set_value](int next) { set_value(next); };
    return h("span", {}, std::to_string(value));
}

static VNode Parent(const Props&) {
    auto [value, set_value] = use_state<int>(0);
    ++parent_renders;
    parent_set = [set_value](int next) { set_value(next); };
    return h("div", {}, h(Child, {}));
}

int main() {
    {
        renderers::TestRenderer renderer;
        NodeHandle container = renderer.document();
        Container root = renderer.create_container();
        render(h(Counter, {}), root);
        assert(renderer.serialize(container) == "<button>0</button>");

        counter_set(7);
        assert(renderer.serialize(container) == "<button>0</button>");
        flush();
        assert(renderer.serialize(container) == "<button>7</button>");
        std::printf("PASS set_state queues and flush() drains it\n");
    }

    {
        parent_renders = child_renders = 0;

        renderers::TestRenderer renderer;
        Container root = renderer.create_container();
        render(h(Parent, {}), root);
        assert(parent_renders == 1);
        assert(child_renders == 1);

        child_set(1);
        parent_set(1);
        flush();
        assert(parent_renders == 2);
        assert(child_renders == 2);
        std::printf("PASS parent renders before child and absorbs it\n");
    }

    {
        parent_renders = child_renders = 0;

        renderers::TestRenderer renderer;
        Container root = renderer.create_container();
        render(h(Parent, {}), root);
        assert(parent_renders == 1);

        parent_set(0);
        flush();
        assert(parent_renders == 1);
        parent_set(9);
        flush();
        assert(parent_renders == 2);
        std::printf("PASS set_state with an unchanged value bails out\n");
    }

    std::printf("ALL schedule tests passed\n");
    return 0;
}
