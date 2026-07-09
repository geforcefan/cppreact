#include <cassert>
#include <cstdio>
#include <functional>
#include <string>

#include "cppreact/hooks.hpp"
#include "cppreact/render.hpp"
#include "cppreact/vnode.hpp"
#include "cppreact/renderers/test.hpp"

using namespace cppreact;

enum class CounterAction { increment, decrement, reset };

static int counter_reduce(const int& count, const CounterAction& action) {
  switch (action) {
  case CounterAction::increment: return count + 1;
  case CounterAction::decrement: return count - 1;
  case CounterAction::reset: return 0;
  }
  return count;
}

static int counter_renders = 0;
static std::function<void(CounterAction)> counter_dispatch;

static VNode Counter(const Props&) {
  ++counter_renders;
  auto [count, dispatch] = use_reducer<int, CounterAction>(counter_reduce, 0);
  counter_dispatch = dispatch;
  return h("span", {}, std::to_string(count));
}

static int initialize_calls = 0;
static std::function<void(CounterAction)> lazy_dispatch;

static VNode LazyCounter(const Props&) {
  auto [count, dispatch] = use_reducer<int, CounterAction>(counter_reduce, [] {
    ++initialize_calls;
    return 40;
  });
  lazy_dispatch = dispatch;
  return h("span", {}, std::to_string(count));
}

int main() {
  {
    renderers::TestRenderer renderer;
    Container root = renderer.create_container();
    render(h(Counter, {}), root);
    assert(renderer.serialize() == "<span>0</span>");

    counter_dispatch(CounterAction::increment);
    counter_dispatch(CounterAction::increment);
    flush();
    assert(renderer.serialize() == "<span>2</span>");

    counter_dispatch(CounterAction::decrement);
    flush();
    assert(renderer.serialize() == "<span>1</span>");

    counter_dispatch(CounterAction::reset);
    flush();
    assert(renderer.serialize() == "<span>0</span>");
    std::printf("PASS dispatch runs the reducer over the current state\n");
  }
  {
    counter_renders = 0;
    renderers::TestRenderer renderer;
    Container root = renderer.create_container();
    render(h(Counter, {}), root);
    int renders_after_mount = counter_renders;

    counter_dispatch(CounterAction::reset);
    flush();
    assert(counter_renders == renders_after_mount);
    std::printf("PASS dispatch skips the render when the reducer returns an equal state\n");
  }
  {
    renderers::TestRenderer renderer;
    Container root = renderer.create_container();
    render(h(Counter, {}), root);
    render(fragment(), root);
    counter_dispatch(CounterAction::increment);
    flush();
    std::printf("PASS dispatch after unmount is ignored\n");
  }
  {
    renderers::TestRenderer renderer;
    Container root = renderer.create_container();
    render(h(LazyCounter, {}), root);
    assert(renderer.serialize() == "<span>40</span>");
    assert(initialize_calls == 1);

    lazy_dispatch(CounterAction::increment);
    flush();
    assert(renderer.serialize() == "<span>41</span>");
    assert(initialize_calls == 1);
    std::printf("PASS the lazy initializer runs once\n");
  }
  std::printf("ALL reducer tests passed\n");
  return 0;
}
