#include <algorithm>
#include <string>
#include <vector>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <RmlUi/Core.h>
#include <RmlUi_Backend.h>

#include "cppreact/cppreact.hpp"
#include "cppreact/hosts/rml.hpp"

#include "cube.hpp"

using namespace cppreact;
using namespace cppreact::tags;

struct Todo {
  int id = 0;
  std::string title{};
  bool done = false;

  bool operator==(const Todo&) const = default;
};

auto create_todo(std::string title) {
  return [=](std::vector<Todo> next) {
    int id = next.empty() ? 1 : next.back().id + 1;
    next.push_back({.id = id, .title = title});
    return next;
  };
}

auto toggle_todo(int id) {
  return [=](std::vector<Todo> next) {
    for (Todo& todo : next)
      if (todo.id == id) todo.done = !todo.done;
    return next;
  };
}

auto remove_todo(int id) {
  return [=](std::vector<Todo> next) {
    std::erase_if(next, [id](const Todo& todo) { return todo.id == id; });
    return next;
  };
}

struct TodoItemProps {
  std::string title{};
  bool done = false;
  EventCallback on_toggle{};
  EventCallback on_remove{};
  std::string key{};
};

const FunctionComponent TodoItem = [](const TodoItemProps& props) -> VNode {
  return View({.class_name = props.done ? "todo done" : "todo",
               .children = {Text({.class_name = "status", .on_click = props.on_toggle}),
                            Text({.class_name = "title",
                                  .on_click = props.on_toggle,
                                  .children = {props.title}}),
                            Text({.class_name = "remove",
                                  .on_click = props.on_remove,
                                  .children = {"x"}})}});
};

struct AppProps {};

const FunctionComponent App = [](const AppProps&) -> VNode {
  auto [todos, set_todos] = use_state<std::vector<Todo>>({});
  auto [draft, set_draft] = use_state<std::string>("");

  auto open = std::count_if(todos.begin(), todos.end(),
                            [](const Todo& todo) { return !todo.done; });

  return View({.class_name = "card",
               .children = {
    View({.class_name = "heading", .children = {"todos"}}),
    Input({.type = "text",
           .value = draft,
           .on_key_down = [=, todos = todos, draft = draft](const Event& event) {
             if (event.key != "enter" || draft.empty()) return;
             set_todos(create_todo(draft));
             set_draft("");
           },
           .on_change = [=](const Event& event) { set_draft(event.value); }}),
    View({.class_name = "todos",
          .children = map(todos, [=](const Todo& todo) {
            return TodoItem({.title = todo.title,
                             .done = todo.done,
                             .on_toggle = [=](const Event&) { set_todos(toggle_todo(todo.id)); },
                             .on_remove = [=](const Event&) { set_todos(remove_todo(todo.id)); },
                             .key = std::to_string(todo.id)});
          })}),
    when(todos.empty(),
         View({.class_name = "footer", .children = {"nothing to do, add one above"}})),
    when(!todos.empty(), [open] {
      return View({.class_name = "footer", .children = {open, " open"}});
    })}});
};

static Rml::Context* refresh_context = nullptr;
static demo::Cube* background_cube = nullptr;

static void render_frame(Rml::Context* context) {
  flush();
  context->Update();
  Backend::BeginFrame();
  if (background_cube) {
    int width = 0;
    int height = 0;
    glfwGetFramebufferSize(glfwGetCurrentContext(), &width, &height);
    background_cube->draw(width, height, static_cast<float>(glfwGetTime()));
  }
  context->Render();
  Backend::PresentFrame();
}

int main() {
  if (!Backend::Initialize("c++react todos", 520, 640, true)) return 1;

  Rml::SetSystemInterface(Backend::GetSystemInterface());
  Rml::SetRenderInterface(Backend::GetRenderInterface());
  Rml::Initialise();

  Rml::Context* context = Rml::CreateContext("main", Rml::Vector2i(520, 640));
  Rml::LoadFontFace(APP_FONT);
  Rml::LoadFontFace(APP_FONT_BOLD);

  Rml::ElementDocument* document = context->LoadDocument(APP_DOCUMENT);
  document->Show();

  hosts::RmlHost host;
  Container container = host.create_container(document->GetElementById("root"));
  render(App({}), container);

  demo::Cube cube;
  background_cube = &cube;

  refresh_context = context;
  glfwSetWindowRefreshCallback(glfwGetCurrentContext(), [](GLFWwindow*) {
    if (refresh_context) render_frame(refresh_context);
  });

  while (Backend::ProcessEvents(context, nullptr, false)) {
    render_frame(context);
  }

  render(fragment(), container);
  Rml::Shutdown();
  Backend::Shutdown();
  return 0;
}
