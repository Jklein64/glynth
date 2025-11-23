#include "processor.h"
#include "editor.h"

#include <fmt/base.h>

//==============================================================================
GlynthProcessor::GlynthProcessor() : AudioProcessor(s_io_layouts) {}

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
  for (auto i = 0; i < getTotalNumOutputChannels(); i++) {
    // Clear unused output buffers to avoid garbage data blasting speakers
    buffer.clear(i, 0, buffer.getNumSamples());
  }

  for (auto&& msg_metadata : midi_messages) {
    auto sample = msg_metadata.samplePosition;
    auto&& msg = msg_metadata.getMessage();
    auto&& description = msg.getDescription().toStdString();
    fmt::println("MIDI message at buffer sample {}: {}", sample, description);
  }
}

//==============================================================================

juce::AudioProcessorEditor* GlynthProcessor::createEditor() {
  return new GlynthEditor(*this);
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
