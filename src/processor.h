#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <random>

class CorruptionSilencer {
public:
  void processBlock(juce::AudioBuffer<float>& buffer);
};

//==============================================================================
class GlynthProcessor final : public juce::AudioProcessor {
public:
  //==============================================================================
  GlynthProcessor();
  ~GlynthProcessor() override;

  //==============================================================================
  void prepareToPlay(double sample_rate, int samples_per_block) override;
  void releaseResources() override;

  bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

  void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
  using AudioProcessor::processBlock;

  //==============================================================================
  juce::AudioProcessorEditor* createEditor() override;
  inline bool hasEditor() const override { return true; }

  //==============================================================================
  inline const juce::String getName() const override { return JucePlugin_Name; }

  inline bool acceptsMidi() const override { return true; }
  inline bool producesMidi() const override { return true; }
  inline bool isMidiEffect() const override { return false; }
  double getTailLengthSeconds() const override;

  //==============================================================================
  inline int getNumPrograms() override { return 1; }
  inline int getCurrentProgram() override { return 0; }
  inline void setCurrentProgram(int) override {}
  inline const juce::String getProgramName(int) override { return {}; }
  inline void changeProgramName(int, const juce::String&) override {}

  //==============================================================================
  void getStateInformation(juce::MemoryBlock& dest_data) override;
  void setStateInformation(const void* data, int size) override;

private:
  //==============================================================================
  inline static auto s_io_layouts = BusesProperties().withOutput(
      "Output", juce::AudioChannelSet::stereo(), true);

  juce::AudioParameterFloat* m_hpf_freq;
  juce::AudioParameterFloat* m_hpf_res;
  juce::AudioParameterFloat* m_lpf_freq;
  juce::AudioParameterFloat* m_lpf_res;

  CorruptionSilencer m_silencer;
  std::random_device m_rd;
  std::mt19937 m_gen;
  std::uniform_real_distribution<float> m_dist;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GlynthProcessor)
};
