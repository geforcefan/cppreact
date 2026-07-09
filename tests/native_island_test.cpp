#include <cassert>
#include <cstdio>
#include <memory>
#include <string>

#include "cppreact/native.hpp"
#include "cppreact/render.hpp"
#include "cppreact/vnode.hpp"
#include "cppreact/renderers/test.hpp"

using namespace cppreact;

struct FakeIsland : NativeElement {
  int prop_calls = 0;
  std::shared_ptr<std::string> last_model{};
  std::shared_ptr<std::string> handle = std::make_shared<std::string>("island-handle");

  void set_native_prop(std::string_view name, const Payload& value) override {
    if (name != "model") return;
    prop_calls++;
    last_model = std::static_pointer_cast<std::string>(value.data);
  }

  Payload native_reference() override { return Payload{handle}; }
};

static Payload model_a{std::make_shared<std::string>("alpha")};
static const Payload* current_model = &model_a;

static VNode App(const Props&) { return h("island", {{"model", *current_model}}); }

int main() {
  renderers::TestRenderer renderer;
  FakeIsland island;
  renderer.register_native("island", &island);

  Container root = renderer.create_container();
  render(h(App, {}), root);

  NodeHandle node = renderer.find_first("island");
  assert(node != kNullNode);
  assert(island.prop_calls == 1);
  assert(island.last_model && *island.last_model == "alpha");
  std::printf("PASS Payload prop routes to the native element, not an attribute\n");

  Payload handle = renderer.native_reference(node);
  assert(handle.data && *std::static_pointer_cast<std::string>(handle.data) == "island-handle");
  std::printf("PASS native_reference returns the native element's imperative handle\n");

  render(h(App, {}), root);
  assert(island.prop_calls == 1);
  std::printf("PASS an unchanged payload is not re-delivered\n");

  Payload model_b{std::make_shared<std::string>("beta")};
  current_model = &model_b;
  render(h(App, {}), root);
  assert(island.prop_calls == 2 && *island.last_model == "beta");
  std::printf("PASS a changed payload is delivered again\n");

  std::printf("ALL native-island tests passed\n");
  return 0;
}
