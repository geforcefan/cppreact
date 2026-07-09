#pragma once

#include <array>
#include <cmath>

#include "RmlUi_Include_GL3.h"

namespace demo {

using Matrix = std::array<float, 16>;

inline Matrix multiply(const Matrix& left, const Matrix& right) {
  Matrix result{};
  for (int row = 0; row < 4; ++row)
    for (int column = 0; column < 4; ++column)
      for (int inner = 0; inner < 4; ++inner)
        result[row * 4 + column] += left[row * 4 + inner] * right[inner * 4 + column];
  return result;
}

inline Matrix perspective(float vertical_field_of_view, float aspect, float near, float far) {
  float focal = 1.0f / std::tan(vertical_field_of_view * 0.5f);
  return {focal / aspect, 0, 0, 0,
          0, focal, 0, 0,
          0, 0, (far + near) / (near - far), (2 * far * near) / (near - far),
          0, 0, -1, 0};
}

inline Matrix translation(float x, float y, float z) {
  return {1, 0, 0, x,
          0, 1, 0, y,
          0, 0, 1, z,
          0, 0, 0, 1};
}

inline Matrix rotation_x(float angle) {
  float cosine = std::cos(angle);
  float sine = std::sin(angle);
  return {1, 0, 0, 0,
          0, cosine, -sine, 0,
          0, sine, cosine, 0,
          0, 0, 0, 1};
}

inline Matrix rotation_y(float angle) {
  float cosine = std::cos(angle);
  float sine = std::sin(angle);
  return {cosine, 0, sine, 0,
          0, 1, 0, 0,
          -sine, 0, cosine, 0,
          0, 0, 0, 1};
}

inline unsigned int compile_shader(unsigned int type, const char* source) {
  unsigned int shader = glCreateShader(type);
  glShaderSource(shader, 1, &source, nullptr);
  glCompileShader(shader);
  return shader;
}

class Cube {
public:
  Cube() {
    const char* vertex_source =
        "#version 330 core\n"
        "layout(location=0) in vec3 position;\n"
        "layout(location=1) in vec3 color;\n"
        "uniform mat4 model_view_projection;\n"
        "out vec3 vertex_color;\n"
        "void main() {\n"
        "  gl_Position = model_view_projection * vec4(position, 1.0);\n"
        "  vertex_color = color;\n"
        "}\n";
    const char* fragment_source =
        "#version 330 core\n"
        "in vec3 vertex_color;\n"
        "out vec4 fragment_color;\n"
        "void main() { fragment_color = vec4(vertex_color, 1.0); }\n";

    unsigned int vertex_shader = compile_shader(GL_VERTEX_SHADER, vertex_source);
    unsigned int fragment_shader = compile_shader(GL_FRAGMENT_SHADER, fragment_source);
    program = glCreateProgram();
    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);
    glLinkProgram(program);
    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);
    model_view_projection_location = glGetUniformLocation(program, "model_view_projection");

    glGenVertexArrays(1, &vertex_array);
    glGenBuffers(1, &vertex_buffer);
    glBindVertexArray(vertex_array);
    glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float),
                          reinterpret_cast<void*>(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);
  }

  void draw(int width, int height, float elapsed) const {
    if (width <= 0 || height <= 0) return;
    float aspect = static_cast<float>(width) / static_cast<float>(height);
    Matrix model_view_projection =
        multiply(perspective(0.9f, aspect, 0.1f, 100.0f),
                 multiply(translation(0, 0, -6.0f),
                          multiply(rotation_x(elapsed * 0.4f), rotation_y(elapsed * 0.6f))));

    glViewport(0, 0, width, height);
    glClearColor(0.078f, 0.086f, 0.106f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    glUseProgram(program);
    glUniformMatrix4fv(model_view_projection_location, 1, GL_TRUE, model_view_projection.data());
    glBindVertexArray(vertex_array);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);

    glDisable(GL_CULL_FACE);
  }

private:
  unsigned int program = 0;
  unsigned int vertex_array = 0;
  unsigned int vertex_buffer = 0;
  int model_view_projection_location = -1;

  static constexpr float vertices[36 * 6] = {
      -1, -1, 1,  0.27f, 0.64f, 0.47f,  1, -1, 1,  0.27f, 0.64f, 0.47f,  1, 1, 1,  0.27f, 0.64f, 0.47f,
      -1, -1, 1,  0.27f, 0.64f, 0.47f,  1, 1, 1,  0.27f, 0.64f, 0.47f,  -1, 1, 1,  0.27f, 0.64f, 0.47f,
      1, -1, -1,  0.16f, 0.40f, 0.29f,  -1, -1, -1,  0.16f, 0.40f, 0.29f,  -1, 1, -1,  0.16f, 0.40f, 0.29f,
      1, -1, -1,  0.16f, 0.40f, 0.29f,  -1, 1, -1,  0.16f, 0.40f, 0.29f,  1, 1, -1,  0.16f, 0.40f, 0.29f,
      -1, -1, -1,  0.16f, 0.40f, 0.29f,  -1, -1, 1,  0.16f, 0.40f, 0.29f,  -1, 1, 1,  0.16f, 0.40f, 0.29f,
      -1, -1, -1,  0.16f, 0.40f, 0.29f,  -1, 1, 1,  0.16f, 0.40f, 0.29f,  -1, 1, -1,  0.16f, 0.40f, 0.29f,
      1, -1, 1,  0.27f, 0.64f, 0.47f,  1, -1, -1,  0.27f, 0.64f, 0.47f,  1, 1, -1,  0.27f, 0.64f, 0.47f,
      1, -1, 1,  0.27f, 0.64f, 0.47f,  1, 1, -1,  0.27f, 0.64f, 0.47f,  1, 1, 1,  0.27f, 0.64f, 0.47f,
      -1, 1, 1,  0.27f, 0.64f, 0.47f,  1, 1, 1,  0.27f, 0.64f, 0.47f,  1, 1, -1,  0.27f, 0.64f, 0.47f,
      -1, 1, 1,  0.27f, 0.64f, 0.47f,  1, 1, -1,  0.27f, 0.64f, 0.47f,  -1, 1, -1,  0.27f, 0.64f, 0.47f,
      -1, -1, -1,  0.16f, 0.40f, 0.29f,  1, -1, -1,  0.16f, 0.40f, 0.29f,  1, -1, 1,  0.16f, 0.40f, 0.29f,
      -1, -1, -1,  0.16f, 0.40f, 0.29f,  1, -1, 1,  0.16f, 0.40f, 0.29f,  -1, -1, 1,  0.16f, 0.40f, 0.29f};
};

}
