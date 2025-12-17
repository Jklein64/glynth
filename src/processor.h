#pragma once

#include "error.h"
#include "font_manager.h"
#include "logger.h"
#include "outliner.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <random>
#include <readerwriterqueue.h>

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

class Synth;

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

  void setOutlineFace(std::string_view face_name);
  void setOutlineText(std::string_view outline_text);
  const Outline& getOutline();
  FT_Face getOutlineFace();
  std::string_view getOutlineText();

private:
  inline static auto s_io_layouts = BusesProperties().withOutput(
      "Output", juce::AudioChannelSet::stereo(), true);

  juce::AudioParameterFloat& m_hpf_freq;
  juce::AudioParameterFloat& m_hpf_res;
  juce::AudioParameterFloat& m_lpf_freq;
  juce::AudioParameterFloat& m_lpf_res;
  juce::AudioParameterFloat& m_attack_ms;
  juce::AudioParameterFloat& m_decay_ms;

  Synth& m_synth;
  std::string m_outline_text = "Glynth";
  std::string m_outline_face = "SplineSansMono-Medium";
  Outline m_outline;
  FontManager m_font_manager;
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

class TriggerHandler : public SubProcessor, juce::Timer {
public:
  // A rising edge across this value causes a trigger
  static constexpr float s_trigger_threshold = 0.01f;
  // Length (in seconds) of the cooldown after a trigger
  static constexpr float s_trigger_cooldown = 0.075f;

  TriggerHandler(GlynthProcessor& processor_ref, const int channel);
  void prepareToPlay(double sample_rate, int samples_per_block) override;
  void processBlock(juce::AudioBuffer<float>& buffer,
                    juce::MidiBuffer& midi_messages) override;
  void timerCallback() override;

private:
  struct Block {
  public:
    inline void add(float x, bool trigger = false) {
      assert(m_size < m_samples.size());
      m_samples[m_size] = x;
      if (trigger) {
        m_trigger = m_size;
      }
      m_size++;
    }

    inline size_t size() { return m_size; }
    inline bool full() { return m_size == m_samples.size(); }
    inline std::optional<size_t> trigger() { return m_trigger; }
    inline float operator[](size_t i) { return m_samples[i]; }

  private:
    std::array<float, 64> m_samples;
    std::optional<size_t> m_trigger = std::nullopt;
    size_t m_size = 0;
  };

  const int m_channel;
  // For getting samples off of the audio thread
  moodycamel::ReaderWriterQueue<Block> m_blocks{1024};
  float m_prev_sample = 0;
  size_t m_size = 0;
  Block m_block;
  // Number of samples to store after a trigger
  size_t m_burst_length;
  std::vector<float> m_burst_buffer;
  bool m_triggered = false;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TriggerHandler)
};

struct Wavetable {
  static constexpr size_t s_num_samples = 512;

  std::array<float, s_num_samples> ch0;
  std::array<float, s_num_samples> ch1;
  // For cross-fading
  std::array<float, s_num_samples> ch0_old;
  std::array<float, s_num_samples> ch1_old;

  inline std::span<float, s_num_samples> channel(size_t ch, bool old = false) {
    if (ch == 0 && old == false) {
      return ch0;
    } else if (ch == 1 && old == false) {
      return ch1;
    } else if (ch == 0 && old == true) {
      return ch0_old;
    } else if (ch == 1 && old == true) {
      return ch1_old;
    } else {
      throw GlynthError("Bad channel index");
    }
  }

  template <typename Index>
  inline float sample(size_t ch, Index i, bool old = false) {
    return this->channel(ch, old)[static_cast<size_t>(i)];
  }
};

struct SynthVoice {
  enum class State { Inactive, Active, Decay };

  SynthVoice(Wavetable& wavetable_ref, float attack_ms, float decay_ms);

  void configure(int note_number, double sample_rate);
  inline float sample(size_t channel);
  void release();
  void crossfade();
  void setAttack(float attack_ms, double sample_rate);
  void setDecay(float decay_ms, double sample_rate);
  inline bool isActive() { return m_state == State::Active; }
  inline bool isInactive() { return m_state == State::Inactive; }
  inline bool isDecaying() { return m_state == State::Decay; }
  int id;
  int note;
  const State& state;

private:
  inline static int s_next_id = 0;
  Wavetable& m_wavetable_ref;
  // std::vector<double> m_wavetable;
  double m_sample_rate;
  // Goes from 0 -> 1
  std::array<double, 2> m_angle;
  // Increment to maintain desired frequency
  double m_inc;
  // Envelope attack in milliseconds
  float m_attack_ms;
  // Envelope decay in milliseconds
  float m_decay_ms;
  // Coefficient used to amplify gain
  float m_attack_coeff;
  // Coefficient used to attenuate gain
  float m_decay_coeff;
  // Gain multiplier for output
  std::array<float, 2> m_gain = {1, 1};
  // Amount of old wavetable to mix with new
  std::array<float, 2> m_crossfade = {0, 0};
  // Current state
  State m_state = State::Inactive;
};
class Synth : public SubProcessor,
              public juce::AudioProcessorParameter::Listener {
public:
  Synth(GlynthProcessor& processor_ref, juce::AudioParameterFloat& attack_ms,
        juce::AudioParameterFloat& decay_ms);

  void prepareToPlay(double sample_rate, int samples_per_block) override;
  void processBlock(juce::AudioBuffer<float>& buffer,
                    juce::MidiBuffer& midi_messages) override;
  void parameterValueChanged(int index, float new_value) override;
  void parameterGestureChanged(int index, bool gesture_is_starting) override;
  void updateWavetable(const Outline& outline);

private:
  std::optional<size_t> getOldestVoiceWithState(SynthVoice::State state);
  // Stereo wavetable referenced by all voices
  Wavetable m_wavetable;
  std::vector<SynthVoice> m_voices;
  double m_sample_rate;
  float m_gain = 0.1f;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Synth)
};
