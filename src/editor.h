#pragma once

#include "processor.h"

#include <efsw/efsw.hpp>
#include <filesystem>
#include <glm/glm.hpp>
#include <juce_opengl/juce_opengl.h>
#include <optional>
#include <vector>

struct ShaderManager {
  using ProgramId = std::string;
  using ShaderName = std::string;

  ShaderManager(juce::OpenGLContext& context);
  bool addProgram(const ProgramId& id, const ShaderName& vert_name,
                  const ShaderName& frag_name);
  bool useProgram(const ProgramId& id);
  void markDirty(const ShaderName& name);
  // Call this in render loop to update dirty shaders
  void tryUpdateDirty();

private:
  inline static auto s_shader_dir =
      std::filesystem::path(__FILE__).parent_path().parent_path().append(
          "shader");

  struct ProgramMetadata {
    std::string vert_name;     // e.g. vert
    std::string vert_res_name; // e.g. vert_glsl
    std::string vert_filename; // e.g. vert.glsl
    std::string frag_name;
    std::string frag_res_name;
    std::string frag_filename;

    ProgramMetadata(const std::string& vname, const std::string& fname);
  };

  std::unique_ptr<juce::OpenGLShaderProgram>
  createProgram(const ProgramMetadata& metadata, const char* vert_source,
                const char* frag_source);
  std::string readFile(const std::string& filename) const;

  juce::OpenGLContext& m_context;
  std::unordered_map<ShaderName, std::string> m_vert_sources;
  std::unordered_map<ShaderName, std::string> m_frag_sources;
  std::unordered_map<ShaderName, ProgramId> m_name_to_id;
  // std::unordered_map<ProgramId, ShaderName> m_id_to_vert_name;
  // std::unordered_map<ProgramId, ShaderName> m_id_to_frag_name;
  std::unordered_map<ProgramId, std::unique_ptr<juce::OpenGLShaderProgram>>
      m_programs;
  std::unordered_map<ProgramId, ProgramMetadata> m_metadata;

  // Accessed across threads; protected by `m_mutex`
  std::vector<ProgramId> m_dirty;
  std::mutex m_mutex;
};

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
  ShaderManager m_shader_manager;
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
