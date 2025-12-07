#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <random>

class GlynthProcessor;
class SubProcessor {
public:
  SubProcessor(GlynthProcessor& processor_ref);
  virtual ~SubProcessor() = default;
  virtual void processBlock(juce::AudioBuffer<float>& buffer,
                            juce::MidiBuffer& midi_messages) = 0;

  inline virtual void prepareToPlay(double sample_rate, int samples_per_block) {
    // Default prepareToPlay is a no-op
    juce::ignoreUnused(sample_rate, samples_per_block);
  }

protected:
  GlynthProcessor& m_processor_ref;
};

class GlynthProcessor final : public juce::AudioProcessor, public juce::Timer {
public:
  GlynthProcessor();
  ~GlynthProcessor() override;

  void prepareToPlay(double sample_rate, int samples_per_block) override;
  void releaseResources() override;

  bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

  void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
  using AudioProcessor::processBlock;

  juce::AudioProcessorEditor* createEditor() override;
  inline bool hasEditor() const override { return true; }

  inline const juce::String getName() const override { return JucePlugin_Name; }

  inline bool acceptsMidi() const override { return true; }
  inline bool producesMidi() const override { return true; }
  inline bool isMidiEffect() const override { return false; }
  inline double getTailLengthSeconds() const override { return 0.0; }

  inline int getNumPrograms() override { return 1; }
  inline int getCurrentProgram() override { return 0; }
  inline void setCurrentProgram(int) override {}
  inline const juce::String getProgramName(int) override { return {}; }
  inline void changeProgramName(int, const juce::String&) override {}

  void getStateInformation(juce::MemoryBlock& dest_data) override;
  void setStateInformation(const void* data, int size) override;

  void timerCallback() override;
  // All parameters are float values
  juce::AudioParameterFloat& getParamById(std::string_view id);

private:
  inline static auto s_io_layouts = BusesProperties().withOutput(
      "Output", juce::AudioChannelSet::stereo(), true);

  juce::AudioParameterFloat& m_hpf_freq;
  juce::AudioParameterFloat& m_hpf_res;
  juce::AudioParameterFloat& m_lpf_freq;
  juce::AudioParameterFloat& m_lpf_res;
  juce::AudioParameterFloat& m_attack_ms;
  juce::AudioParameterFloat& m_decay_ms;
  std::vector<std::unique_ptr<SubProcessor>> m_processors;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GlynthProcessor)
};

class CorruptionSilencer : public SubProcessor {
public:
  CorruptionSilencer(GlynthProcessor& processor_ref);
  void processBlock(juce::AudioBuffer<float>& buffer,
                    juce::MidiBuffer& midi_messages) override;

private:
  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CorruptionSilencer)
};

class NoiseGenerator : public SubProcessor {
public:
  NoiseGenerator(GlynthProcessor& processor_ref);
  void processBlock(juce::AudioBuffer<float>& buffer,
                    juce::MidiBuffer& midi_messages) override;

private:
  std::random_device m_rd;
  std::mt19937 m_gen;
  std::uniform_real_distribution<float> m_dist;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NoiseGenerator)
};

class BiquadFilter : public SubProcessor {
public:
  BiquadFilter(GlynthProcessor& processor_ref,
               juce::AudioParameterFloat* freq_param,
               juce::AudioParameterFloat* res_param);
  void prepareToPlay(double sample_rate, int samples_per_block) override;
  void processBlock(juce::AudioBuffer<float>& buffer,
                    juce::MidiBuffer& midi_messages) override;

protected:
  virtual void configure(float freq, float res) = 0;

  std::array<double, 3> b;
  std::array<double, 3> a;
  double m_sample_rate;
  float m_freq;
  float m_res;
  juce::AudioParameterFloat* m_freq_param;
  juce::AudioParameterFloat* m_res_param;

  struct FilterState {
    double x_prev = 0, x_prevprev = 0, y_prev = 0, y_prevprev = 0;
  };

  std::vector<FilterState> m_states;

private:
  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BiquadFilter)
};

class LowPassFilter : public BiquadFilter {
public:
  // Inherit the constructor
  using BiquadFilter::BiquadFilter;

protected:
  void configure(float freq, float res) override;

private:
  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LowPassFilter)
};

class HighPassFilter : public BiquadFilter {
public:
  // Inherit the constructor
  using BiquadFilter::BiquadFilter;

protected:
  void configure(float freq, float res) override;

private:
  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HighPassFilter)
};

struct SynthVoice {
  enum class State { Inactive, Active, Decay };

  SynthVoice(float attack_ms, float decay_ms);
  void configure(int note_number, double sample_rate);
  inline double sample();
  void release();
  void setAttack(float attack_ms);
  void setDecay(float decay_ms);
  inline bool isActive() { return m_state == State::Active; }
  inline bool isInactive() { return m_state == State::Inactive; }
  inline bool isDecaying() { return m_state == State::Decay; }
  int id;
  int note;
  const State& state;

private:
  inline static int s_next_id = 0;
  std::vector<double> m_wavetable;
  double m_sample_rate;
  // Goes from 0 -> 1
  double m_angle;
  // Increment to maintain desired frequency
  double m_inc;
  // Envelope attack in milliseconds
  float m_attack_ms;
  // Envelope decay in milliseconds
  float m_decay_ms;
  // Coefficient used to amplify gain
  double m_attack_coeff;
  // Coefficient used to attenuate gain
  double m_decay_coeff;
  // Gain multiplier for output
  double m_gain = 1;
  // Current state
  State m_state = State::Inactive;
};
class Synth : public SubProcessor {
public:
  Synth(GlynthProcessor& processor_ref);
  void prepareToPlay(double sample_rate, int samples_per_block) override;
  void processBlock(juce::AudioBuffer<float>& buffer,
                    juce::MidiBuffer& midi_messages) override;

private:
  std::optional<size_t> getOldestVoiceWithState(SynthVoice::State state);

  std::vector<SynthVoice> m_voices;
  double m_sample_rate;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Synth)
};
