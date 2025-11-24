#include "editor.h"
#include "processor.h"
#include "shaders.h"

#include <fmt/base.h>

//==============================================================================
GlynthEditor::GlynthEditor(GlynthProcessor& p)
    : AudioProcessorEditor(&p), processor_ref(p) {
  // Must set size for window to show properly
  setSize(400, 300);
  addAndMakeVisible(m_opengl_component);
}

GlynthEditor::~GlynthEditor() {}

//==============================================================================
void GlynthEditor::paint(juce::Graphics&) {
  // Only the OpenGlComponent for now, but this will have controls later
}

void GlynthEditor::resized() { m_opengl_component.setBounds(getLocalBounds()); }

//==============================================================================
OpenGlComponent::OpenGlComponent() {
  setOpaque(true);
  m_context.setOpenGLVersionRequired(juce::OpenGLContext::openGL3_2);
  m_context.setRenderer(this);
  m_context.setContinuousRepainting(true);
  m_context.attachTo(*this);
}

OpenGlComponent::~OpenGlComponent() { m_context.detach(); }

void OpenGlComponent::paint(juce::Graphics&) {}

void OpenGlComponent::newOpenGLContextCreated() {
  using namespace juce::gl;
  glGenVertexArrays(1, &m_vao);
  glGenBuffers(1, &m_vbo);
  glBindVertexArray(m_vao);
  glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
  m_vertex_buffer = {                   // x y r g b
                     0,   0,   1, 0, 0, //
                     1,   0,   0, 1, 0, //
                     0.5, 0.5, 0, 0, 1};
  glBufferData(GL_ARRAY_BUFFER,
               static_cast<GLsizeiptr>(m_vertex_buffer.size() * sizeof(float)),
               m_vertex_buffer.data(), GL_STATIC_DRAW);

  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), nullptr);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float),
                        reinterpret_cast<GLvoid*>(sizeof(float) * 2));
  glEnableVertexAttribArray(0);
  glEnableVertexAttribArray(1);

  m_shader_program.reset(new juce::OpenGLShaderProgram(m_context));
  if (m_shader_program->addVertexShader(shaders::vert_glsl) &&
      m_shader_program->addFragmentShader(shaders::frag_glsl) &&
      m_shader_program->link()) {
    m_shader_program->use();
  } else {
    fmt::println(stderr, "Failed to load shaders");
  }
}

void OpenGlComponent::renderOpenGL() {
  using namespace juce::gl;
  juce::OpenGLHelpers::clear(juce::Colours::black);
  m_shader_program->use();
  glBindVertexArray(m_vao);
  glDrawArrays(GL_TRIANGLES, 0, 3);
}

void OpenGlComponent::openGLContextClosing() {}
