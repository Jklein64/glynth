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
  // This reference is provided as a quick way for your editor to
  // access the processor object that created it.
  GlynthProcessor& processorRef;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GlynthEditor)
};
