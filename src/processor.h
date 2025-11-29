#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <random>

class SubProcessor {
public:
  virtual ~SubProcessor() = default;
  virtual void processBlock(juce::AudioBuffer<float>& buffer) = 0;

  inline virtual void prepareToPlay(double sample_rate, int samples_per_block) {
    // Default prepareToPlay is a no-op
    juce::ignoreUnused(sample_rate, samples_per_block);
  }
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
  inline double getTailLengthSeconds() const override { return 0.0; }

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

  std::vector<std::unique_ptr<SubProcessor>> m_processors;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GlynthProcessor)
};

class CorruptionSilencer : public SubProcessor {
public:
  CorruptionSilencer() = default;
  void processBlock(juce::AudioBuffer<float>& buffer) override;

private:
  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CorruptionSilencer)
};

class NoiseGenerator : public SubProcessor {
public:
  NoiseGenerator();
  void processBlock(juce::AudioBuffer<float>& buffer) override;

private:
  std::random_device m_rd;
  std::mt19937 m_gen;
  std::uniform_real_distribution<float> m_dist;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NoiseGenerator)
};

class BiquadFilter : public SubProcessor {
public:
  BiquadFilter(int num_channels, juce::AudioParameterFloat* freq_param,
               juce::AudioParameterFloat* res_param);
  void prepareToPlay(double sample_rate, int samples_per_block) override;
  void processBlock(juce::AudioBuffer<float>& buffer) override;

private:
  void configure(double f0, double Q);

  std::array<double, 3> b;
  std::array<double, 3> a;
  double m_sample_rate;
  double m_freq;
  double m_res;
  juce::AudioParameterFloat* m_freq_param;
  juce::AudioParameterFloat* m_res_param;

  struct FilterState {
    double x_prev = 0, x_prevprev = 0, y_prev = 0, y_prevprev = 0;
  };

  std::vector<FilterState> m_states;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BiquadFilter)
};
