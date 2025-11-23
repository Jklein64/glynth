#pragma once

#include "processor.h"

#include <juce_opengl/juce_opengl.h>

class OpenGlComponent final : public juce::Component,
                              public juce::OpenGLRenderer {
public:
  explicit OpenGlComponent();
  ~OpenGlComponent() override;

  void paint(juce::Graphics&) override;
  void newOpenGLContextCreated() override;
  void renderOpenGL() override;
  void openGLContextClosing() override;

private:
  juce::OpenGLContext m_context;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OpenGlComponent)
};

//==============================================================================
class GlynthEditor final : public juce::AudioProcessorEditor {
public:
  explicit GlynthEditor(GlynthProcessor&);
  ~GlynthEditor() override;

  //==============================================================================
  void paint(juce::Graphics&) override;
  void resized() override;

private:
  GlynthProcessor& processor_ref;
  OpenGlComponent m_opengl_component;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GlynthEditor)
};
