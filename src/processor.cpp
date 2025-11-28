#include "processor.h"
#include "editor.h"

#include <fmt/base.h>

//==============================================================================
GlynthProcessor::GlynthProcessor()
    : AudioProcessor(s_io_layouts), m_gen(m_rd()), m_dist(-1.f, 1) {
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
}

GlynthProcessor::~GlynthProcessor() {}

//==============================================================================

double GlynthProcessor::getTailLengthSeconds() const { return 0.0; }

//==============================================================================
void GlynthProcessor::prepareToPlay(double sample_rate, int samples_per_block) {
  fmt::println("prepareToPlay: sample_rate = {}, samples_per_block = {}",
               sample_rate, samples_per_block);
  fmt::println("num_inputs = {}", getTotalNumInputChannels());
  fmt::println("num_outputs = {}", getTotalNumOutputChannels());
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
    for (int i = 0; i < buffer.getNumSamples(); i++) {
      auto rand_value = m_dist(m_gen);
      buffer.setSample(ch, i, rand_value);
    }
    // Clear unused output buffers to avoid garbage data blasting speakers
    // buffer.clear(ch, 0, buffer.getNumSamples());
  }

  for (auto&& msg_metadata : midi_messages) {
    auto sample = msg_metadata.samplePosition;
    auto&& msg = msg_metadata.getMessage();
    auto&& description = msg.getDescription().toStdString();
    fmt::println("MIDI message at buffer sample {}: {}", sample, description);
  }

  // Check output for earrape and silence buffer if present
  m_silencer.processBlock(buffer);
}

//==============================================================================

juce::AudioProcessorEditor* GlynthProcessor::createEditor() {
  return new juce::GenericAudioProcessorEditor(*this);
  // return new GlynthEditor(*this);
}

//==============================================================================
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

void CorruptionSilencer::processBlock(juce::AudioBuffer<float>& buffer) {
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
