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
  using namespace juce::gl;
  glGenVertexArrays(1, &m_vao);
  glGenBuffers(1, &m_vbo);
  glBindVertexArray(m_vao);
  glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
  m_vertex_buffer = {                 // x y r g b
                     -1, -1, 1, 0, 0, //
                     3,  -1, 0, 1, 0, //
                     -1, 3,  0, 0, 1};
  glBufferData(GL_ARRAY_BUFFER,
               static_cast<GLsizeiptr>(m_vertex_buffer.size() * sizeof(float)),
               m_vertex_buffer.data(), GL_STATIC_DRAW);

  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), nullptr);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float),
                        reinterpret_cast<GLvoid*>(sizeof(float) * 2));
  glEnableVertexAttribArray(0);
  glEnableVertexAttribArray(1);
  m_shader_manager.addProgram("bg", "vert", "frag");
}

void GlynthEditor::renderOpenGL() {
  m_shader_manager.tryUpdateDirty();

  using namespace juce::gl;
  juce::OpenGLHelpers::clear(juce::Colours::black);
  m_shader_manager.useProgram("bg");
  glBindVertexArray(m_vao);
  glDrawArrays(GL_TRIANGLES, 0, 3);
}

void GlynthEditor::openGLContextClosing() {}
