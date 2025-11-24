#include "editor.h"
#include "processor.h"

#include <fmt/base.h>
#include <fstream>

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
  m_context.extensions.glGenBuffers(1, &m_vbo);
  m_context.extensions.glGenBuffers(1, &m_ibo);

  m_vertex_buffer = {// Vertex 0
                     {
                         {-0.5f, 0.5f},       // (-0.5, 0.5)
                         {1.f, 0.f, 0.f, 1.f} // Red
                     },
                     // Vertex 1
                     {
                         {0.5f, 0.5f},         // (0.5, 0.5)
                         {1.f, 0.5f, 0.f, 1.f} // Orange
                     },
                     // Vertex 2
                     {
                         {0.5f, -0.5f},       // (0.5, -0.5)
                         {1.f, 1.f, 0.f, 1.f} // Yellow
                     },
                     // Vertex 3
                     {
                         {-0.5f, -0.5f},      // (-0.5, -0.5)
                         {1.f, 0.f, 1.f, 1.f} // Purple
                     }};

  // We need 6 indices, 1 for each corner of the two triangles.
  m_index_buffer = {0, 1, 2, 0, 2, 3};

  m_context.extensions.glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
  m_context.extensions.glBufferData(
      GL_ARRAY_BUFFER,
      static_cast<GLsizeiptr>(sizeof(Vertex) * m_vertex_buffer.size()),
      m_vertex_buffer.data(), GL_STATIC_DRAW);

  m_context.extensions.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ibo);
  m_context.extensions.glBufferData(
      GL_ELEMENT_ARRAY_BUFFER,
      static_cast<GLsizeiptr>(sizeof(uint32_t) * m_index_buffer.size()),
      m_index_buffer.data(), GL_STATIC_DRAW);

  auto vertex_shader = readFile(s_vert_file.c_str());
  auto fragment_shader = readFile(s_frag_file.c_str());
  if (vertex_shader.has_value() && fragment_shader.has_value()) {
    m_shader_program.reset(new juce::OpenGLShaderProgram(m_context));
    if (m_shader_program->addVertexShader(*vertex_shader) &&
        m_shader_program->addFragmentShader(*fragment_shader) &&
        m_shader_program->link()) {
      m_shader_program->use();
    } else {
      fmt::println(stderr, "Failed to load shaders");
    }
  }
}

void OpenGlComponent::renderOpenGL() {
  using namespace juce::gl;
  juce::OpenGLHelpers::clear(juce::Colours::black);
  m_shader_program->use();

  m_context.extensions.glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
  m_context.extensions.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ibo);

  m_context.extensions.glVertexAttribPointer(
      0,              // The attribute's index (AKA location).
      2,              // How many values this attribute contains.
      GL_FLOAT,       // The attribute's type (float).
      GL_FALSE,       // Tells OpenGL NOT to normalise the values.
      sizeof(Vertex), // How many bytes to move to find the attribute with
                      // the same index in the next vertex.
      nullptr         // How many bytes to move from the start of this vertex
                      // to find this attribute (the default is 0 so we just
                      // pass nullptr here).
  );
  m_context.extensions.glEnableVertexAttribArray(0);

  // Enable to colour attribute.
  m_context.extensions.glVertexAttribPointer(
      1, // This attribute has an index of 1
      4, // This time we have four values for the
         // attribute (r, g, b, a)
      GL_FLOAT, GL_FALSE, sizeof(Vertex),
      (GLvoid*)(sizeof(float) * 2) // This attribute comes after the
                                   // position attribute in the Vertex
                                   // struct, so we need to skip over the
                                   // size of the position array to find
                                   // the start of this attribute.
  );
  m_context.extensions.glEnableVertexAttribArray(1);

  glDrawElements(
      GL_TRIANGLES, // Tell OpenGL to render triangles.
      static_cast<GLsizei>(m_index_buffer.size()), // How many indices we have.
      GL_UNSIGNED_INT,                             // What type our indices are.
      nullptr // We already gave OpenGL our indices so we don't
              // need to pass that again here, so pass nullptr.
  );

  m_context.extensions.glDisableVertexAttribArray(0);
  m_context.extensions.glDisableVertexAttribArray(1);
}

void OpenGlComponent::openGLContextClosing() {}

std::optional<std::string>
OpenGlComponent::readFile(std::string_view filename) const {
  // See https://codereview.stackexchange.com/a/22907
  std::vector<char> chars;
  std::ifstream ifs(filename, std::ios::ate);
  std::ifstream::pos_type pos = ifs.tellg();
  if (pos <= 0) {
    fmt::println("error reading {}, pos = {}", filename.data(),
                 static_cast<size_t>(pos));
    return std::nullopt;
  } else {
    chars.resize(static_cast<size_t>(pos) + 1);
    ifs.seekg(0, std::ios::beg);
    ifs.read(chars.data(), pos);
    // Need to null-terminate the string
    chars[chars.size() - 1] = '\0';
    return std::make_optional(std::string(chars.data()));
  }
}
