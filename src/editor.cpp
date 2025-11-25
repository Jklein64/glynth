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
  // using namespace juce::gl;
  // glGenVertexArrays(1, &m_vao);
  // glGenBuffers(1, &m_vbo);
  // glBindVertexArray(m_vao);
  // glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
  // m_vertex_buffer = {                 // x y r g b
  //                    -1, -1, 1, 0, 0, //
  //                    3,  -1, 0, 1, 0, //
  //                    -1, 3,  0, 0, 1};
  // glBufferData(GL_ARRAY_BUFFER,
  //              static_cast<GLsizeiptr>(m_vertex_buffer.size() *
  //              sizeof(float)), m_vertex_buffer.data(), GL_STATIC_DRAW);

  // glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float),
  // nullptr); glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 5 *
  // sizeof(float),
  //                       reinterpret_cast<GLvoid*>(sizeof(float) * 2));
  // glEnableVertexAttribArray(0);
  // glEnableVertexAttribArray(1);
  m_shader_manager.addProgram("background", "vert", "frag");
  m_shader_components.push_back(
      std::make_unique<BackgroundComponent>(m_shader_manager, "background"));
  // m_shader_components.emplace_back(m_shader_manager, "bg");
}

void GlynthEditor::renderOpenGL() {
  m_shader_manager.tryUpdateDirty();

  using namespace juce::gl;
  juce::OpenGLHelpers::clear(juce::Colours::black);
  for (auto& component : m_shader_components) {
    component->renderOpenGL();
  }
  // m_shader_manager.useProgram("background");
  // glBindVertexArray(m_vao);
  // glDrawArrays(GL_TRIANGLES, 0, 3);
}

void GlynthEditor::openGLContextClosing() {}

ShaderComponent::ShaderComponent(ShaderManager& shader_manager,
                                 const std::string& program_id)
    : m_shader_manager(shader_manager), m_program_id(program_id) {
  using namespace juce::gl;
  glGenBuffers(1, &m_vbo);
  glGenBuffers(1, &m_ebo);
  glGenVertexArrays(1, &m_vao);
  // glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
  // glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
  // glBindVertexArray(m_vao);
}

ShaderComponent::~ShaderComponent() {
  using namespace juce::gl;
  glDeleteBuffers(1, &m_vbo);
  glDeleteBuffers(1, &m_ebo);
  glDeleteVertexArrays(1, &m_vao);
}

void ShaderComponent::renderOpenGL() {
  using namespace juce::gl;
  // // Assertions fail when buffers are zero or have not yet been bound
  // assert(glIsBuffer(m_vbo));
  // assert(glIsBuffer(m_ebo));
  // // Guarantee the correct bindings are used during rendering
  // glBindVertexArray(m_vao);
  // glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
  // glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
  // m_shader_manager.useProgram(m_program_id);
  render();
}

BackgroundComponent::BackgroundComponent(ShaderManager& shader_manager,
                                         const std::string& program_id)
    : ShaderComponent(shader_manager, program_id) {
  using namespace juce::gl;
  glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
  glBindVertexArray(m_vao);
  std::vector<GLfloat> vertices = {                 // x y r g b
                                   -1, -1, 1, 0, 0, //
                                   3,  -1, 0, 1, 0, //
                                   -1, 3,  0, 0, 1};
  std::vector<GLuint> indices = {0, 1, 2};
  glBufferData(GL_ARRAY_BUFFER,
               static_cast<GLsizeiptr>(5 * sizeof(GLfloat) * vertices.size()),
               vertices.data(), GL_STATIC_DRAW);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER,
               static_cast<GLsizeiptr>(sizeof(GLuint) * indices.size()),
               indices.data(), GL_STATIC_DRAW);
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
  glBindVertexArray(m_vao);
  glDrawArrays(GL_TRIANGLES, 0, 3);
  // glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_INT, nullptr);
}
