<h1 align="center">c++react</h1>

<p align="center">React in C++, probably for your games, CAD editors or other crazy stuff, like a GUI inside Unreal Engine 5 (I did). Function components, hooks, and a virtual DOM that renders into whatever renderer you give it. Because your CAD software also deserved a pretty GUI.</p>

<p align="center">
  <img src="https://img.shields.io/badge/status-alpha-orange" alt="alpha">
  <img src="https://img.shields.io/badge/C%2B%2B-20-blue" alt="C++20">
  <img src="https://img.shields.io/badge/deps-none-brightgreen" alt="no dependencies">
  <img src="https://img.shields.io/badge/exceptions%20%2F%20rtti-off-black" alt="no exceptions or rtti">
  <img src="https://img.shields.io/badge/license-MIT-black" alt="MIT license">
</p>

<p align="center">
  <img src="demo/demo.gif" alt="the todo demo app, driven by c++react through RmlUi" width="480">
</p>

> **Alpha.** c++react is experimental and not recommended for production use yet. Try it, break it,
> and help me sort out the bugs: issues and pull requests are very welcome.

c++react is a C++ library for building user interfaces, with the React model: components, hooks, and
a virtual DOM that updates only what changed. It does not render on its own. You point it at a
renderer and it drives that, so the same components can run on a browser, a game engine, or anything
else. The [demo](#demo) below uses [RmlUi](https://github.com/mikke89/RmlUi), the great HTML and CSS
like renderer for C++ that draws through OpenGL and many other targets (and yes, I implemented an
Unreal Engine 5 backend for RmlUi, stay tuned, that will be open sourced too).

What you get:

- the hooks you expect (`use_state`, `use_effect`, `use_memo` and friends, all listed below)
- keyed lists, context, portals, synthetic events
- native elements for the parts that draw their own pixels
- batched rendering: `set_state` queues, one `flush()` per frame drains it
- no dependencies, C++20, builds with `-fno-exceptions -fno-rtti`

## Using it

c++react is header only. Fetch it and link the target:

```cmake
include(FetchContent)
FetchContent_Declare(cppreact GIT_REPOSITORY https://github.com/geforcefan/cppreact.git)
FetchContent_MakeAvailable(cppreact)

target_link_libraries(your_app PRIVATE cppreact::cppreact)
```

Or copy the `include/` directory into your project, there is nothing to compile.

The library alone is useless: it renders nothing without a render target.

## Demo

A demo says more than a thousand words. `demo/` is the todo app from the animation at the top, a
complete desktop app:

- a GLFW window with OpenGL
- RmlUi on top as the renderer for c++react
- CMake fetches GLFW and RmlUi
- FreeType comes from the system (`brew install freetype` on macOS, `apt install libfreetype-dev` on Linux)

```
cmake -S demo -B demo/build
cmake --build demo/build
./demo/build/todo
```

The demo fetches [my RmlUi fork](https://github.com/geforcefan/RmlUi) instead of upstream: it
carries a couple of bug fixes of mine that are not merged yet (open PRs
[#962](https://github.com/mikke89/RmlUi/pull/962) and
[#965](https://github.com/mikke89/RmlUi/pull/965)).

## Components

A component is a function `VNode(const Props&)`. Wrap it in `Component` and it calls like a tag:

```cpp
#include "cppreact/hooks.hpp"
#include "cppreact/tags.hpp"

using namespace cppreact;
using namespace cppreact::tags;

const Component Counter = [](const Props& props) -> VNode {
  auto label = props.get<std::string>("label").value_or("count");
  auto [count, set_count] = use_state<int>(0);

  auto increment = [=](const Event&) { set_count(count + 1); };

  return div({{"class", "counter"}},
    span({}, label + ": " + std::to_string(count)),
    button({{"on_click", increment}}, "increment"));
};
```

A click runs the handler, `set_count` re-renders, and the diff touches only the text node that
changed. Components go inside components:

```cpp
const Component App = [](const Props&) -> VNode {
  return div({{"class", "app"}},
    Counter({{"label", "left"}}),
    Counter({{"label", "right"}}));
};
```

A component renders the children it was given with `children()`:

```cpp
const Component Card = [](const Props&) -> VNode {
  return div({{"class", "card"}}, children());
};

Card({}, span({}, "inside"));
```

## Elements

Build elements with the tag helpers from `tags.hpp`:

```cpp
div({{"class", "row"}},
  span({}, "hi"),
  button({{"on_click", handler}}, "ok"));
```

These helpers exist: `div`, `span`, `p`, `button`, `a`, `ul`, `ol`, `li`, `input`, `img`, `label`,
`section`, `header`, `footer`, `nav`, `h1`, `h2`, `h3`, `strong`, `em`, `pre`, `code`.

Children are variadic. Strings and numbers turn into text nodes. `key` and `ref` are read out of
`props`.

If a tag is dynamic, or the shortcuts are not your style, use `h()`, the primitive every helper
calls:

```cpp
h(std::string tag, Props props = {}, children...);
h(ComponentFunction component, Props props = {}, children...);
fragment(children...);
```

## Props

Props are an untyped bag so `h()` can carry anything. You read them with a typed accessor:

```cpp
props.get<std::string>("label");
props.get<std::string>("label").value_or("fallback");
props.has("on_click");
props.find("count");
```

`get<T>` returns a `std::optional<T>`, `value_or` gives it a default, `has` is a presence check, and
`find` hands back a `const Value*` or `nullptr`.

Literals just work: `{{"class", "row"}}` is a string, `{{"count", 3.0}}` a double. `style` is an
object, a `Style` list of property and value pairs, not a string:

```cpp
div({{"style", Style{{"width", "50%"}, {"color", "red"}}}});
```

`Payload` holds a `shared_ptr<void>` for values you pass through untouched, a context value or a
pointer to one of your own objects.

## Hooks

Call them at the top of a component, same order every render. A dependency list is built with
`deps(...)` and compared item by item. `deps()` means run once; leaving the argument out means run
every render.

### use_state

```cpp
auto [count, set_count] = use_state<int>(0);

set_count(count + 1);
set_count([](const int& value) { return value + 1; });

auto [rows, set_rows] = use_state<Rows>([] { return load_rows(); });
```

The setter takes a value or an updater lambda. It re-renders the component, and skips the render when
the new value compares equal to the old one (for types with `operator==`). A call after unmount is
ignored. Pass a function as the initial value and it runs once, on mount.

### use_reducer

A store: the reducer takes the current state and an action and returns the next state, `dispatch`
feeds it an action:

```cpp
enum class CounterAction { increment, decrement, reset };

int counter_reduce(const int& count, const CounterAction& action) {
  switch (action) {
    case CounterAction::increment: return count + 1;
    case CounterAction::decrement: return count - 1;
    case CounterAction::reset:     return 0;
  }
  return count;
}

const Component Counter = [](const Props&) -> VNode {
  auto [count, dispatch] = use_reducer<int, CounterAction>(counter_reduce, 0);

  return div({},
    button({{"on_click", [=](const Event&) { dispatch(CounterAction::decrement); }}}, "-"),
    span({}, std::to_string(count)),
    button({{"on_click", [=](const Event&) { dispatch(CounterAction::increment); }}}, "+"));
};
```

An action can carry data too, use a `struct` (or a `std::variant` of them) instead of an enum.
`dispatch` skips the render when the reducer returns an equal state, and always runs the reducer you
passed on the latest render. Pass a function instead of the initial value and it runs once, on mount:
`use_reducer<int, CounterAction>(counter_reduce, [] { return load_count(); })`.

### use_effect

```cpp
use_effect([id]() -> CleanupFunction {
  subscribe(id);
  return [id] { unsubscribe(id); };
}, deps(id));
```

Runs after commit when a dependency changed. The effect returns its cleanup, which runs before the
next run and on unmount; return an empty `CleanupFunction` when there is nothing to clean up.

### use_layout_effect

Same signature as `use_effect`, runs first in the commit, before the passive effects. For reading
layout right after the tree changed.

### use_memo

```cpp
auto total = use_memo<int>([rows] { return sum(rows); }, deps(rows.size()));
```

Recomputes only when a dependency changed.

### use_callback

```cpp
EventHandler select = use_callback<EventHandler>(
  [id](const Event&) { open(id); }, deps(id));
```

### use_ref

```cpp
int& render_count = use_ref<int>(0);
render_count += 1;
```

A mutable slot that survives re-renders and does not trigger one.

### use_context

```cpp
const std::string& value = use_context(theme);
```

Reads the nearest provided value, see [Context](#context).

### use_sync_external_store

```cpp
auto paused = use_sync_external_store(
  [](auto on_change) { return store.subscribe(on_change); },
  [] { return store.paused(); });
```

Subscribes to an external store and re-renders when the snapshot changes (compared with
`operator==`). `subscribe(on_change)` returns the unsubscribe.

### use_node_ref and use_measure

```cpp
NodeRef ref = use_node_ref();
Box box = use_measure(ref);

return div({{"ref", ref.prop()}, {"class", "panel"}}, "...");
```

`ref` reaches the created node (`ref.current()`), and `use_measure` tracks that node's box across
re-renders.

On an element, `ref` attaches to the created node. On a component, `ref` is just another prop: the
component reads it and passes it on to whichever element it wants to expose:

```cpp
const Component Field = [](const Props& props) -> VNode {
  Props inner{{"class", "field"}, {"type", "text"}};
  if (auto ref = props.get<Payload>("ref")) inner.set("ref", *ref);

  return input(std::move(inner));
};

NodeRef ref = use_node_ref();
Field({{"ref", ref.prop()}});
```

A component that ignores its `ref` prop attaches nothing, exactly like a component that ignores any
other prop.

### use_document_event

```cpp
use_document_event("key_down", [cancel](const Event& event) {
  if (event.key == "Escape") cancel();
});
```

An event listener on the document. Bound once, always calls the latest handler, removed on unmount.

## Context

```cpp
const auto theme = create_context<std::string>("light");

provide(theme, std::string("dark"), App({}));

const std::string& value = use_context(theme);
```

`provide` publishes a value for its subtree, `use_context` reads the nearest one. A consumer
subscribes to the provider, so when the value changes every consumer re-renders, even behind a
memoized component that skips its own render.

## Lists

Give items a `key`. Add, remove and reorder then keep the matching instances and their state, moving
only what actually moved. The `map` helper turns a range into a list of nodes:

```cpp
div({{"class", "list"}}, map(items, [](const auto& item) {
  return Row({{"key", item.id}, {"label", item.name}});
}));

map(items, [](const auto& item, std::size_t i) {
  return Row({{"key", item.id}, {"index", double(i)}});
});
```

## Conditionals

`when` renders a node only when the condition holds. Pass a node, or a lambda to build it lazily and
skip the work when it is false:

```cpp
div({{"class", "row"}},
  span({}, label),
  when(expanded, [] { return div({{"class", "detail"}}, "more"); }));
```

## Events

A handler is `void(const Event&)`, wired through a prop named for the event:

- `on_click`
- `on_mouse_down`, `on_mouse_up`, `on_mouse_move`, `on_mouse_over`, `on_mouse_out`
- `on_key_down`, `on_key_up`
- `on_wheel`
- `on_focus`, `on_blur`
- `on_change`

Append `_capture` to listen in the capture phase:

```cpp
div({{"on_click", bubbled}, {"on_click_capture", first}},
  button({{"on_click", pressed}}, "ok"));
```

Capture handlers run on the way down to the target, before it: a click on the button runs `first`,
then `pressed`, then `bubbled`.

The `Event` is synthetic and the same on every renderer:

```cpp
struct Event {
  NodeHandle target;
  NodeHandle current_target;
  std::string type;
  double client_x, client_y;
  int button;
  bool shift_key, ctrl_key, alt_key, meta_key;
  std::string key;
  std::string value;
  double delta_x, delta_y;
  std::function<void()> prevent_default;
  std::function<void()> stop_propagation;
  Payload native;
};
```

`key` is the pressed key (`"a"`, `"Enter"`, `"Escape"`, `"ArrowUp"`), `value` is a control's value
on change, and `native` carries the raw host event for anything the fields above do not cover.

```cpp
EventHandler zoom = [set_scale](const Event& event) {
  event.prevent_default();
  set_scale([delta = event.delta_y](double scale) { return scale - delta * 0.001; });
};

return div({{"class", "graph"}, {"on_wheel", zoom}}, children());
```

For a listener on the document instead of an element, see
[use_document_event](#use_document_event).

## Portals

`portal` renders a subtree into a different container:

```cpp
return div({{"class", "row"}},
  span({}, "row"),
  portal(overlay_layer, Tooltip({{"text", "on top of everything"}})));
```

Context still flows from where the `portal` call sits, not from the target.

## Renderers

c++react renders through a `Renderer`. One ships with the library, `renderers/test.hpp`: an in-memory
tree you can serialize and fire events at, so components can be tested without a UI toolkit.

```cpp
#include "cppreact/renderers/test.hpp"

renderers::TestRenderer renderer;
Container root = renderer.create_container();
render(div({{"class", "x"}}, "hi"), root);

renderer.serialize();   // <div class="x">hi</div>
```

The RmlUi renderer ships too, in `renderers/rml.hpp`. Construction is the only difference, it mounts
into a host element:

```cpp
#include "cppreact/renderers/rml.hpp"

renderers::RmlRenderer renderer;
Container root = renderer.create_container(rml_element);
render(App({}), root);
```

`set_state` queues a re-render; nothing happens until you call `flush()`, once per frame from your
loop, which batches the frame's updates into one render.

To write your own renderer, implement `Renderer`. `renderers/test.hpp` is the reference to read.

## Native elements

Some nodes draw their own pixels and have no children to diff: a canvas, a chart. Such an element
implements `NativeElement`, and the renderer registers its tag as native:

```cpp
struct NativeElement {
  virtual void set_native_prop(std::string_view name, const Payload& value);
  virtual Payload native_reference();
};
```

A component feeds it a `Payload` prop (curve samples, a model) and reads a handle back through a
`ref`. c++react only ever moves the `Payload`, never a toolkit type.

## License

MIT, see [LICENSE](LICENSE).
