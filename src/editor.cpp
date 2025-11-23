#include "editor.h"
#include "processor.h"

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

void OpenGlComponent::newOpenGLContextCreated() {}

void OpenGlComponent::renderOpenGL() {}

void OpenGlComponent::openGLContextClosing() {}
