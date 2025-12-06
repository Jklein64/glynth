#pragma once

#include <efsw/efsw.hpp>
#include <glm/glm.hpp>
#include <juce_opengl/juce_opengl.h>
#include <vector>

#ifdef GLYNTH_HSR
#include <filesystem>
#endif

class ShaderManager : public efsw::FileWatchListener {
public:
  using ProgramId = std::string;
  using ShaderName = std::string;
  using Uniform = std::variant<int, size_t, float, glm::vec2, glm::mat4>;

  ShaderManager(juce::OpenGLContext& context);
  bool addProgram(const ProgramId& id, const ShaderName& vert_name,
                  const ShaderName& frag_name);
  bool useProgram(const ProgramId& id);
  bool setUniform(const ProgramId& id, const std::string& name, Uniform value);
  void markDirty(const ProgramId& id);
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
  std::unordered_map<ProgramId, std::unique_ptr<juce::OpenGLShaderProgram>>
      m_programs;
  std::unordered_map<ProgramId, ProgramMetadata> m_metadata;
  std::unordered_map<ProgramId, std::vector<std::function<bool()>>>
      m_uniform_refreshers;

#ifdef GLYNTH_HSR
  // For file watch synchronization
  std::string readFile(const std::string& filename) const;
  efsw::FileWatcher m_file_watcher;
  std::vector<ProgramId> m_dirty;
  std::mutex m_mutex;
#endif
};
