#include "processor.h"
#include "editor.h"

#include <fmt/base.h>

GlynthProcessor::GlynthProcessor() : AudioProcessor(s_io_layouts) {
  m_hpf_freq = new juce::AudioParameterFloat(
      juce::ParameterID("hpf_freq", 1), "High Cutoff",
      juce::NormalisableRange(20.0f, 20000.0f), 5.0f);
  addParameter(m_hpf_freq);
  m_hpf_res = new juce::AudioParameterFloat(
      juce::ParameterID("hpf_res", 1), "Resonance",
      juce::NormalisableRange(0.1f, 100.0f), 0.71f);
  addParameter(m_hpf_res);
  m_lpf_freq = new juce::AudioParameterFloat(
      juce::ParameterID("lpf_freq", 1), "Low Cutoff",
      juce::NormalisableRange(20.0f, 20000.0f), 5.0f);
  addParameter(m_lpf_freq);
  m_lpf_res = new juce::AudioParameterFloat(
      juce::ParameterID("lpf_res", 1), "Resonance",
      juce::NormalisableRange(0.1f, 100.0f), 0.71f);
  addParameter(m_lpf_res);

  m_processors.emplace_back(new NoiseGenerator());
  m_processors.emplace_back(new BiquadFilter(2));
  m_processors.emplace_back(new CorruptionSilencer());
}

GlynthProcessor::~GlynthProcessor() {}

void GlynthProcessor::prepareToPlay(double sample_rate, int samples_per_block) {
  fmt::println("prepareToPlay: sample_rate = {}, samples_per_block = {}",
               sample_rate, samples_per_block);
  fmt::println("num_inputs = {}", getTotalNumInputChannels());
  fmt::println("num_outputs = {}", getTotalNumOutputChannels());

  for (auto& processor : m_processors) {
    processor->prepareToPlay(sample_rate, samples_per_block);
  }
}

void GlynthProcessor::releaseResources() {
  // Free up spare memory
}

bool GlynthProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
  auto&& output_channel_set = layouts.getMainOutputChannelSet();
  return output_channel_set == juce::AudioChannelSet::mono() ||
         output_channel_set == juce::AudioChannelSet::stereo();
}

void GlynthProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                   juce::MidiBuffer& midi_messages) {
  juce::ScopedNoDenormals noDenormals;
  for (int ch = 0; ch < getTotalNumOutputChannels(); ch++) {
    // Clear unused output buffers to avoid garbage data blasting speakers
    buffer.clear(ch, 0, buffer.getNumSamples());
  }

  for (auto&& msg_metadata : midi_messages) {
    auto sample = msg_metadata.samplePosition;
    auto&& msg = msg_metadata.getMessage();
    auto&& description = msg.getDescription().toStdString();
    fmt::println("MIDI message at buffer sample {}: {}", sample, description);
  }

  for (auto& processor : m_processors) {
    processor->processBlock(buffer);
  }
}

juce::AudioProcessorEditor* GlynthProcessor::createEditor() {
  return new juce::GenericAudioProcessorEditor(*this);
  // return new GlynthEditor(*this);
}

void GlynthProcessor::getStateInformation(juce::MemoryBlock& dest_data) {
  // Stores parameters in the memory block, either as raw data or using the
  // XML or ValueTree classes as intermediaries to make it easy to save and load
  // complex data.
  juce::ignoreUnused(dest_data);
}

void GlynthProcessor::setStateInformation(const void* data, int size) {
  // Restore parameters from this memory block, whose contents will have been
  // created by the getStateInformation() call.
  juce::ignoreUnused(data, size);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
  return new GlynthProcessor();
}

//==============================================================================

void CorruptionSilencer::processBlock(juce::AudioBuffer<float>& buffer) {
  // Check output for earrape and silence buffer if present
  bool warned = false;
  for (int ch = 0; ch < buffer.getNumChannels(); ch++) {
    auto samples = buffer.getWritePointer(ch);
    for (int i = 0; i < buffer.getNumSamples(); i++) {
      float x = samples[i];
      bool should_silence = false;
      if (std::isnan(x) || std::isinf(x)) {
        should_silence = true;
        if (!warned) {
          fmt::println(stderr, "Warning: audio buffer contains inf or nan");
          warned = true;
        }
      } else if (std::abs(x) > 2.0f) {
        should_silence = true;
        if (!warned) {
          fmt::println(stderr, "Warning: sample significantly out of range");
          warned = true;
        }
      } else if (std::abs(x) > 1.0f) {
        samples[i] = std::clamp(x, -1.0f, 1.0f);
        if (!warned) {
          fmt::println(stderr, "Warning: clamped out of range sample");
          warned = true;
        }
      }

      if (should_silence) {
        buffer.clear();
        return;
      }
    }
  }
}

NoiseGenerator::NoiseGenerator() : m_gen(m_rd()), m_dist(-1.0f, 1.0f) {}

void NoiseGenerator::processBlock(juce::AudioBuffer<float>& buffer) {
  for (int ch = 0; ch < buffer.getNumChannels(); ch++) {
    for (int i = 0; i < buffer.getNumSamples(); i++) {
      buffer.setSample(ch, i, m_dist(m_gen));
    }
  }
}

BiquadFilter::BiquadFilter(int num_channels) {
  for (size_t ch = 0; ch < static_cast<size_t>(num_channels); ch++) {
    // Creates a default state
    m_states.emplace_back();
  }
}

void BiquadFilter::prepareToPlay(double sample_rate, int) {
  // low pass with cutoff at 2 kHz and Q = 1
  double f0 = 2000;
  double Q = 1;
  double fs = sample_rate;
  double w0 = juce::MathConstants<double>::twoPi * f0 / fs;
  double cos_w0 = cos(w0);
  double sin_w0 = sin(w0);
  double alpha = sin_w0 / (2 * Q);

  double a0 = 1 + alpha;
  b = {(1 - cos_w0) / (2 * a0), (1 - cos_w0) / a0, (1 - cos_w0) / (2 * a0)};
  a = {1, (-2 * cos_w0) / a0, (1 - alpha) / a0};
}

void BiquadFilter::processBlock(juce::AudioBuffer<float>& buffer) {
  for (size_t ch = 0; ch < static_cast<size_t>(buffer.getNumChannels()); ch++) {
    for (int i = 0; i < buffer.getNumSamples(); i++) {
      FilterState& state = m_states[ch];
      double x = buffer.getSample(static_cast<int>(ch), i);
      double y = (b[0] * x + b[1] * state.x_prev + b[2] * state.x_prevprev -
                  a[1] * state.y_prev - a[2] * state.y_prevprev);
      buffer.setSample(static_cast<int>(ch), i, static_cast<float>(y));
      // Advance state one sample
      state.x_prevprev = state.x_prev;
      state.x_prev = x;
      state.y_prevprev = state.y_prev;
      state.y_prev = y;
    }
  }
}
