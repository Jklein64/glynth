#pragma once

#include "processor.h"

#include <efsw/efsw.hpp>
#include <filesystem>
#include <glm/glm.hpp>
#include <juce_opengl/juce_opengl.h>
#include <optional>
#include <vector>

class GlynthEditor final : public juce::AudioProcessorEditor,
                           public juce::OpenGLRenderer,
                           public efsw::FileWatchListener {
public:
  explicit GlynthEditor(GlynthProcessor&);
  ~GlynthEditor() override;

  void paint(juce::Graphics&) override;
  void newOpenGLContextCreated() override;
  void renderOpenGL() override;
  void openGLContextClosing() override;
  void handleFileAction(efsw::WatchID watchid, const std::string& dir,
                        const std::string& filename, efsw::Action action,
                        std::string oldFilename) override;

private:
  // TODO disable hot swapping and filesystem stuff for release builds
  inline static auto s_shader_dir =
      std::filesystem::path(__FILE__).parent_path().parent_path().append(
          "shader");
  inline static auto s_vert_path =
      std::filesystem::path(s_shader_dir).append("vert.glsl");
  inline static auto s_frag_path =
      std::filesystem::path(s_shader_dir).append("frag.glsl");

  void reloadShaders();

  GlynthProcessor& processor_ref;
  juce::OpenGLContext m_context;
  std::unique_ptr<juce::OpenGLShaderProgram> m_program;

  GLuint m_vbo;
  GLuint m_vao;
  std::vector<float> m_vertex_buffer;
  std::vector<uint32_t> m_index_buffer;

  efsw::FileWatcher m_file_watcher;
  std::optional<std::string> m_dirty_shader;
  std::string m_vert_source;
  std::string m_frag_source;
  std::mutex m_mutex;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GlynthEditor)
};
