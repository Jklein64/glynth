#include "editor.h"
#include "processor.h"

#include <fmt/base.h>
#include <fmt/format.h>

GlynthEditor::GlynthEditor(GlynthProcessor& p)
    : AudioProcessorEditor(&p), processor_ref(p), m_shader_manager(m_context) {
  // Must set size for window to show properly
  setSize(400, 300);
  setOpaque(true);
  m_context.setOpenGLVersionRequired(juce::OpenGLContext::openGL3_2);
  m_context.setRenderer(this);
  m_context.setContinuousRepainting(true);
  m_context.attachTo(*this);
}

GlynthEditor::~GlynthEditor() { m_context.detach(); }

void GlynthEditor::paint(juce::Graphics&) {}

void GlynthEditor::newOpenGLContextCreated() {
  m_shader_manager.addProgram("background", "vert", "frag");
  m_shader_components.push_back(
      std::make_unique<BackgroundComponent>(m_shader_manager, "background"));
}

void GlynthEditor::renderOpenGL() {
  m_shader_manager.tryUpdateDirty();

  using namespace juce::gl;
  juce::OpenGLHelpers::clear(juce::Colours::black);
  for (auto& component : m_shader_components) {
    component->renderOpenGL();
  }
}

void GlynthEditor::openGLContextClosing() {}

ShaderComponent::ShaderComponent(ShaderManager& shader_manager,
                                 const std::string& program_id)
    : m_shader_manager(shader_manager), m_program_id(program_id) {
  using namespace juce::gl;
  glGenBuffers(1, &m_vbo);
  glGenBuffers(1, &m_ebo);
  glGenVertexArrays(1, &m_vao);
  // Guarantee the correct bindings are used during construction
  glBindVertexArray(m_vao);
  glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
}

ShaderComponent::~ShaderComponent() {
  using namespace juce::gl;
  glDeleteBuffers(1, &m_vbo);
  glDeleteBuffers(1, &m_ebo);
  glDeleteVertexArrays(1, &m_vao);
}

void ShaderComponent::renderOpenGL() {
  using namespace juce::gl;
  // Guarantee the correct bindings are used during rendering
  glBindVertexArray(m_vao); // Also binds m_ebo
  glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
  m_shader_manager.useProgram(m_program_id);
  render();
}

BackgroundComponent::BackgroundComponent(ShaderManager& shader_manager,
                                         const std::string& program_id)
    : ShaderComponent(shader_manager, program_id) {
  using namespace juce::gl;
  std::array<GLfloat, 15> vertices = {                 // x y r g b
                                      -1, -1, 1, 0, 0, //
                                      3,  -1, 0, 1, 0, //
                                      -1, 3,  0, 0, 1};
  std::array<GLuint, 3> indices = {0, 1, 2};
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices.data(),
               GL_STATIC_DRAW);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices.data(),
               GL_STATIC_DRAW);
  // Set position attribute
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), nullptr);
  glEnableVertexAttribArray(0);
  // Set color attribute
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float),
                        reinterpret_cast<GLvoid*>(sizeof(float) * 2));
  glEnableVertexAttribArray(1);
}

void BackgroundComponent::render() {
  using namespace juce::gl;
  glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_INT, nullptr);
}
