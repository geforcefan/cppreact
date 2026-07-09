#include <cassert>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

#include "cppreact/hooks.hpp"
#include "cppreact/render.hpp"
#include "cppreact/vnode.hpp"
#include "cppreact/renderers/test.hpp"

using namespace cppreact;

namespace {

struct Store {
  int selected = 0;
  int other = 0;
  std::vector<std::function<void()>> listeners;
  void notify() {
    for (const auto& listener : listeners) listener();
  }
};

Store store;
int render_count = 0;

VNode SelectedView(const Props&) {
  ++render_count;
  int value = use_sync_external_store(
      [](std::function<void()> on_change) -> CleanupFunction {
        store.listeners.push_back(std::move(on_change));
        return [] {};
      },
      [] { return store.selected; });
  return h("div", {}, std::to_string(value));
}

}

int main() {
  renderers::TestRenderer renderer;
  Container root = renderer.create_container();
  render(h(SelectedView, {}), root);
  assert(renderer.serialize() == "<div>0</div>");
  const int mounted_renders = render_count;

  store.other = 5;
  store.notify();
  flush();
  assert(render_count == mounted_renders);
  assert(renderer.serialize() == "<div>0</div>");
  std::printf("PASS an unselected change does not re-render\n");

  store.selected = 7;
  store.notify();
  flush();
  assert(render_count == mounted_renders + 1);
  assert(renderer.serialize() == "<div>7</div>");
  std::printf("PASS a selected change re-renders with the new value\n");

  std::printf("ALL use_sync_external_store tests passed\n");
  return 0;
}
