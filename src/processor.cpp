#include "processor.h"
#include "editor.h"
#include "logger.h"

#include <fmt/base.h>

GlynthProcessor::GlynthProcessor() : AudioProcessor(s_io_layouts) {
#ifdef GLYNTH_LOG_TO_FILE
  startTimerHz(1);
#endif
  m_hpf_freq = new juce::AudioParameterFloat(
      juce::ParameterID("hpf_freq", 1), "Cutoff Freq. (HPF)",
      juce::NormalisableRange(20.0f, 20000.0f), 20.0f);
  addParameter(m_hpf_freq);
  m_hpf_res = new juce::AudioParameterFloat(
      juce::ParameterID("hpf_res", 1), "Resonance (HPF)",
      juce::NormalisableRange(0.1f, 10.0f), 0.71f);
  addParameter(m_hpf_res);
  m_lpf_freq = new juce::AudioParameterFloat(
      juce::ParameterID("lpf_freq", 1), "Cutoff Freq. (LPF)",
      juce::NormalisableRange(20.0f, 20000.0f, 0.1f, 0.2f), 20000.0f);
  addParameter(m_lpf_freq);
  m_lpf_res = new juce::AudioParameterFloat(
      juce::ParameterID("lpf_res", 1), "Resonance (LPF)",
      juce::NormalisableRange(0.1f, 10.0f), 0.71f);
  addParameter(m_lpf_res);

  m_processors.emplace_back(new NoiseGenerator(*this));
  // m_processors.emplace_back(new Synth(*this));
  m_processors.emplace_back(new HighPassFilter(*this, m_hpf_freq, m_hpf_res));
  m_processors.emplace_back(new LowPassFilter(*this, m_lpf_freq, m_lpf_res));
  m_processors.emplace_back(new CorruptionSilencer(*this));
}

GlynthProcessor::~GlynthProcessor() { fclose(Logger::file); }

void GlynthProcessor::prepareToPlay(double sample_rate, int samples_per_block) {
  fmt::println(Logger::file,
               "prepareToPlay: sample_rate = {}, samples_per_block = {}",
               sample_rate, samples_per_block);
  fmt::println(Logger::file, "num_inputs = {}", getTotalNumInputChannels());
  fmt::println(Logger::file, "num_outputs = {}", getTotalNumOutputChannels());

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

  for (auto& processor : m_processors) {
    processor->processBlock(buffer, midi_messages);
  }
}

juce::AudioProcessorEditor* GlynthProcessor::createEditor() {
  // return new juce::GenericAudioProcessorEditor(*this);
  return new GlynthEditor(*this);
}

void GlynthProcessor::getStateInformation(juce::MemoryBlock& dest_data) {
  // Stores parameters in the memory block, either as raw data or using the
  // XML or ValueTree classes as intermediaries to make it easy to save and load
  // complex data.
  auto stream = juce::MemoryOutputStream(dest_data, true);
  stream.writeFloat(m_hpf_freq->get());
  stream.writeFloat(m_hpf_res->get());
  stream.writeFloat(m_lpf_freq->get());
  stream.writeFloat(m_lpf_res->get());
}

void GlynthProcessor::setStateInformation(const void* data, int size) {
  // Restore parameters from this memory block, whose contents will have been
  // created by the getStateInformation() call.
  auto stream = juce::MemoryInputStream(data, static_cast<size_t>(size), false);
  *m_hpf_freq = stream.readFloat();
  *m_hpf_res = stream.readFloat();
  *m_lpf_freq = stream.readFloat();
  *m_lpf_res = stream.readFloat();
}

void GlynthProcessor::timerCallback() { fflush(Logger::file); }

juce::AudioParameterFloat* GlynthProcessor::getParamById(std::string_view id) {
  auto params = {m_hpf_freq, m_hpf_res, m_lpf_freq, m_lpf_res};
  for (juce::AudioParameterFloat* param : params) {
    if (id == param->paramID.toStdString()) {
      return param;
    }
  }
  // Should be unreachable
  fmt::println(Logger::file, R"(No parameter found with id "{}")", id);
  return nullptr;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
  return new GlynthProcessor();
}

//==============================================================================

SubProcessor::SubProcessor(GlynthProcessor& processor_ref)
    : m_processor_ref(processor_ref) {}

CorruptionSilencer::CorruptionSilencer(GlynthProcessor& processor_ref)
    : SubProcessor(processor_ref) {}

void CorruptionSilencer::processBlock(juce::AudioBuffer<float>& buffer,
                                      juce::MidiBuffer&) {
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
          fmt::println(Logger::file,
                       "Warning: audio buffer contains inf or nan");
          warned = true;
        }
      } else if (std::abs(x) > 2.0f) {
        should_silence = true;
        if (!warned) {
          fmt::println(Logger::file,
                       "Warning: sample significantly out of range");
          warned = true;
        }
      } else if (std::abs(x) > 1.0f) {
        samples[i] = std::clamp(x, -1.0f, 1.0f);
        if (!warned) {
          fmt::println(Logger::file, "Warning: clamped out of range sample");
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

NoiseGenerator::NoiseGenerator(GlynthProcessor& processor_ref)
    : SubProcessor(processor_ref), m_gen(m_rd()), m_dist(-0.5f, 0.5f) {}

void NoiseGenerator::processBlock(juce::AudioBuffer<float>& buffer,
                                  juce::MidiBuffer&) {
  for (int ch = 0; ch < buffer.getNumChannels(); ch++) {
    for (int i = 0; i < buffer.getNumSamples(); i++) {
      buffer.setSample(ch, i, m_dist(m_gen));
    }
  }
}

BiquadFilter::BiquadFilter(GlynthProcessor& processor_ref,
                           juce::AudioParameterFloat* freq_param,
                           juce::AudioParameterFloat* res_param)
    : SubProcessor(processor_ref), m_freq_param(freq_param),
      m_res_param(res_param) {
  // Initialize from param values
  m_freq = *m_freq_param;
  m_res = *m_res_param;
  int num_channels = processor_ref.getTotalNumOutputChannels();
  for (size_t ch = 0; ch < static_cast<size_t>(num_channels); ch++) {
    // Creates a default state
    m_states.emplace_back();
  }
}

void BiquadFilter::prepareToPlay(double sample_rate, int) {
  m_sample_rate = sample_rate;
  configure(m_freq, m_res);
}

void BiquadFilter::processBlock(juce::AudioBuffer<float>& buffer,
                                juce::MidiBuffer&) {
  for (size_t ch = 0; ch < static_cast<size_t>(buffer.getNumChannels()); ch++) {
    for (int i = 0; i < buffer.getNumSamples(); i++) {
      // Ensure filter is using most recent param values
      float freq = m_freq_param->get();
      float res = m_res_param->get();
      if (!juce::exactlyEqual(freq, m_freq) ||
          !juce::exactlyEqual(res, m_res)) {
        configure(freq, res);
      }
      // Apply the filter
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

void LowPassFilter::configure(float freq, float res) {
  m_freq = freq;
  m_res = res;
  double w0 = juce::MathConstants<double>::twoPi * freq / m_sample_rate;
  double cos_w0 = cos(w0);
  double sin_w0 = sin(w0);
  double alpha = sin_w0 / (2 * res);

  double a0 = 1 + alpha;
  b = {(1 - cos_w0) / (2 * a0), (1 - cos_w0) / a0, (1 - cos_w0) / (2 * a0)};
  a = {1, (-2 * cos_w0) / a0, (1 - alpha) / a0};
}

void HighPassFilter::configure(float freq, float res) {
  m_freq = freq;
  m_res = res;
  double w0 = juce::MathConstants<double>::twoPi * freq / m_sample_rate;
  double cos_w0 = cos(w0);
  double sin_w0 = sin(w0);
  double alpha = sin_w0 / (2 * res);

  double a0 = 1 + alpha;
  b = {(1 + cos_w0) / (2 * a0), -(1 + cos_w0) / a0, (1 + cos_w0) / (2 * a0)};
  a = {1, (-2 * cos_w0) / a0, (1 - alpha) / a0};
}

Synth::Synth(GlynthProcessor& processor_ref) : SubProcessor(processor_ref) {
  m_voices.emplace_back();
  m_voices.emplace_back();
  m_voices.emplace_back();
}

void Synth::prepareToPlay(double sample_rate, int) {
  m_sample_rate = sample_rate;
}

void Synth::processBlock(juce::AudioBuffer<float>& buffer,
                         juce::MidiBuffer& midi_messages) {
  juce::MidiBufferIterator it = midi_messages.begin();
  for (int i = 0; i < buffer.getNumSamples(); i++) {
    // Handle all midi messages happening at sample i
    while (it != midi_messages.end() && (*it).samplePosition == i) {
      // Process the midi message
      auto&& msg = (*it).getMessage();
      auto&& description = msg.getDescription().toStdString();
      fmt::println(Logger::file, "MIDI message at buffer sample {}: {}", i,
                   description);

      if (msg.isNoteOn()) {
        auto note = msg.getNoteNumber();
        // Use first inactive voice, or voice with lowest ID if all are active
        size_t min_id_idx = 0;
        bool handled = false;
        for (size_t k = 0; k < m_voices.size(); k++) {
          if (!m_voices[k].active) {
            // Found an inactive voice to use
            m_voices[k].configure(note, m_sample_rate);
            handled = true;
            break;
          } else if (m_voices[k].id < m_voices[min_id_idx].id) {
            min_id_idx = k;
          }
        }
        if (!handled) {
          // Use voice with lowest ID
          m_voices[min_id_idx].configure(note, m_sample_rate);
        }
      } else if (msg.isNoteOff()) {
        auto note = msg.getNoteNumber();
        for (auto& voice : m_voices) {
          if (voice.note == note) {
            voice.reset();
          }
        }
      }

      it++;
    }

    double sample = 0;
    for (auto& voice : m_voices) {
      if (voice.active) {
        sample += voice.sample();
      }
    }

    for (int ch = 0; ch < buffer.getNumChannels(); ch++) {
      buffer.setSample(ch, i, static_cast<float>(sample));
    }
  }
}

SynthVoice::SynthVoice() {
  id = s_next_id;
  s_next_id++;
}

void SynthVoice::configure(int note_number, double sample_rate) {
  active = true;
  note = note_number;
  m_angle = 0;
  auto freq = juce::MidiMessage::getMidiNoteInHertz(note);
  m_inc = freq / sample_rate * juce::MathConstants<double>::twoPi;
  id = s_next_id;
  s_next_id++;
}

double SynthVoice::sample() {
  double value = 0.1 * std::sin(m_angle);
  m_angle += m_inc;
  if (m_angle > juce::MathConstants<double>::twoPi) {
    m_angle -= juce::MathConstants<double>::twoPi;
  }

  return value;
}

void SynthVoice::reset() {
  active = false;
  m_angle = 0;
}
