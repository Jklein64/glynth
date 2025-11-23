#pragma once

#include "processor.h"

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

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GlynthEditor)
};
