#include <cassert>
#include <cstdio>

#include "cppreact/render.hpp"
#include "cppreact/vnode.hpp"
#include "cppreact/renderers/test.hpp"

using namespace cppreact;

static Event seen{};
static int bubble_hits = 0;
static int capture_hits = 0;

static VNode CaptureProbe(const Props&) {
  EventHandler bubble = [](const Event&) { ++bubble_hits; };
  EventHandler capturing = [](const Event&) { ++capture_hits; };
  return h("div", {{"class", "probe"}, {"on_click", bubble}, {"on_click_capture", capturing}});
}

static VNode Target(const Props&) {
  EventHandler record = [](const Event& event) { seen = event; };
  EventHandler stopper = [](const Event& event) {
    event.stop_propagation();
    event.prevent_default();
  };
  return h("div",
           {{"class", "target"},
            {"on_drag", record},
            {"on_key_down", record},
            {"on_wheel", record},
            {"on_click", stopper}});
}

int main() {
  renderers::TestRenderer renderer;
  Container root = renderer.create_container();
  render(h(Target, {}), root);
  NodeHandle target = renderer.find_first("div");

  renderer.fire(target, "drag", Event{.client_x = 12.5, .client_y = -4, .shift_key = true});
  assert(seen.type == "drag");
  assert(seen.target == target);
  assert(seen.client_x == 12.5 && seen.client_y == -4);
  assert(seen.shift_key && !seen.ctrl_key && !seen.meta_key);
  std::printf("PASS pointer position and modifiers reach the handler\n");

  renderer.fire(target, "key_down", Event{.ctrl_key = true, .key = "Escape"});
  assert(seen.type == "key_down" && seen.key == "Escape" && seen.ctrl_key);
  std::printf("PASS key name and modifier reach the handler\n");

  renderer.fire(target, "wheel", Event{.delta_y = 120});
  assert(seen.type == "wheel" && seen.delta_y == 120);
  std::printf("PASS wheel delta reaches the handler\n");

  assert(!renderer.propagation_stopped && !renderer.default_prevented);
  renderer.fire(target, "click");
  assert(renderer.propagation_stopped && renderer.default_prevented);
  std::printf("PASS stop_propagation and prevent_default invoke the renderer capabilities\n");

  renderers::TestRenderer capture_renderer;
  Container capture_root = capture_renderer.create_container();
  render(h(CaptureProbe, {}), capture_root);
  NodeHandle probe = capture_renderer.find_first("div");
  capture_renderer.fire(probe, "click");
  assert(bubble_hits == 1 && capture_hits == 1);
  std::printf("PASS bubble and capture handlers on one node coexist and both fire\n");

  std::printf("ALL event tests passed\n");
  return 0;
}
