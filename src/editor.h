#pragma once

#include "processor.h"

#include <efsw/efsw.hpp>
#include <glm/glm.hpp>
#include <juce_opengl/juce_opengl.h>
#include <optional>
#include <vector>
#ifdef GLYNTH_HSR
#include <filesystem>
#endif

struct ShaderManager : public efsw::FileWatchListener {
  using ProgramId = std::string;
  using ShaderName = std::string;

  ShaderManager(juce::OpenGLContext& context);
  bool addProgram(const ProgramId& id, const ShaderName& vert_name,
                  const ShaderName& frag_name);
  bool useProgram(const ProgramId& id);
  void markDirty(const ShaderName& name);
  void tryUpdateDirty();
  void handleFileAction(efsw::WatchID watchid, const std::string& dir,
                        const std::string& filename, efsw::Action action,
                        std::string old_filename) override;

private:
#ifdef GLYNTH_HSR
  // Needs to be macro'd out because the shader dir won't exist when distributed
  inline static auto s_shader_dir = std::filesystem::path(GLYNTH_SHADER_DIR);
#endif

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

  juce::OpenGLContext& m_context;
  std::unordered_map<ShaderName, std::string> m_vert_sources;
  std::unordered_map<ShaderName, std::string> m_frag_sources;
  std::unordered_map<ShaderName, ProgramId> m_name_to_id;
  std::unordered_map<ProgramId, std::unique_ptr<juce::OpenGLShaderProgram>>
      m_programs;
  std::unordered_map<ProgramId, ProgramMetadata> m_metadata;

#ifdef GLYNTH_HSR
  // For file watch synchronization
  std::string readFile(const std::string& filename) const;
  efsw::FileWatcher m_file_watcher;
  std::vector<ProgramId> m_dirty;
  std::mutex m_mutex;
#endif
};

class GlynthEditor final : public juce::AudioProcessorEditor,
                           public juce::OpenGLRenderer {
public:
  explicit GlynthEditor(GlynthProcessor&);
  ~GlynthEditor() override;

  void paint(juce::Graphics&) override;
  void newOpenGLContextCreated() override;
  void renderOpenGL() override;
  void openGLContextClosing() override;

private:
  GlynthProcessor& processor_ref;
  juce::OpenGLContext m_context;
  // TODO disable hot swapping and filesystem stuff for release builds
  ShaderManager m_shader_manager;
  std::unique_ptr<juce::OpenGLShaderProgram> m_program;

  GLuint m_vbo;
  GLuint m_vao;
  std::vector<float> m_vertex_buffer;
  std::vector<uint32_t> m_index_buffer;

  std::optional<std::string> m_dirty_shader;
  std::string m_vert_source;
  std::string m_frag_source;
  std::mutex m_mutex;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GlynthEditor)
};
