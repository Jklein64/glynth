#include "editor.h"
#include "processor.h"

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
  m_context.extensions.glBufferData(GL_ARRAY_BUFFER,
                                    sizeof(Vertex) * m_vertex_buffer.size(),
                                    m_vertex_buffer.data(), GL_STATIC_DRAW);

  m_context.extensions.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ibo);
  m_context.extensions.glBufferData(
      GL_ELEMENT_ARRAY_BUFFER, sizeof(unsigned int) * m_index_buffer.size(),
      m_index_buffer.data(), GL_STATIC_DRAW);

  // std::string vertex_shader =
  //     R"(
  //           #version 330 core

  //           // Input attributes.
  //           in vec4 position;
  //           in vec4 sourceColour;

  //           // Output to fragment shader.
  //           out vec4 fragColour;

  //           void main()
  //           {
  //               // Set the position to the attribute we passed in.
  //               gl_Position = position;

  //               // Set the frag colour to the attribute we passed in.
  //               // This value will be interpolated for us before the fragment
  //               // shader so the colours we defined ateach vertex will create
  //               a
  //               // gradient across the shape.
  //               fragColour = sourceColour;
  //           }
  //       )";

  // std::string fragment_shader =
  //     R"(
  //           #version 330 core

  //           // The value that was output by the vertex shader.
  //           // This MUST have the exact same name that we used in the vertex
  //           shader. in vec4 fragColour;

  //           void main()
  //           {
  //               // Set the fragment's colour to the attribute passed in by
  //               the
  //               // vertex shader.
  //               gl_FragColor = fragColour;
  //           }
  //       )";

  const char* vertex_shader = R"(
                   attribute vec4 position;
                   void main()
                   {
                       gl_Position = position;
                   }
               )";

  const char* fragment_shader = R"(
                   void main()
                   {
                       gl_FragColor = vec4(1.0, 0.0, 0.0, 1.0); // Red color
                   }
               )";

  auto translated_vertex_shader =
      juce::OpenGLHelpers::translateVertexShaderToV3(vertex_shader);
  fmt::println("Translated vertex shader: {}",
               translated_vertex_shader.toStdString());
  auto translated_fragment_shader =
      juce::OpenGLHelpers::translateFragmentShaderToV3(fragment_shader);
  fmt::println("Translated fragment shader: {}",
               translated_fragment_shader.toStdString());

  m_shader_program.reset(new juce::OpenGLShaderProgram(m_context));
  if (m_shader_program->addVertexShader(translated_vertex_shader) &&
      m_shader_program->addFragmentShader(translated_fragment_shader) &&
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

  glDrawElements(GL_TRIANGLES,          // Tell OpenGL to render triangles.
                 m_index_buffer.size(), // How many indices we have.
                 GL_UNSIGNED_INT,       // What type our indices are.
                 nullptr // We already gave OpenGL our indices so we don't
                         // need to pass that again here, so pass nullptr.
  );

  m_context.extensions.glDisableVertexAttribArray(0);
  m_context.extensions.glDisableVertexAttribArray(1);
}

void OpenGlComponent::openGLContextClosing() {}
