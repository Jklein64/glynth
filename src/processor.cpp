#include "processor.h"
#include "editor.h"
#include "error.h"
#include "logger.h"

#include <fmt/format.h>
#include <npy/npy.h>
#include <npy/tensor.h>

GlynthProcessor::GlynthProcessor()
    : AudioProcessor(s_io_layouts),
      m_hpf_freq(*(new juce::AudioParameterFloat(
          juce::ParameterID("hpf_freq", 1), "Cutoff Freq. (HPF)",
          juce::NormalisableRange(20.0f, 20000.0f, 0.1f, 0.2f), 20.0f,
          juce::AudioParameterFloatAttributes().withLabel("Hz")))),
      m_hpf_res(*(new juce::AudioParameterFloat(
          juce::ParameterID("hpf_res", 1), "Resonance (HPF)",
          juce::NormalisableRange(0.1f, 10.0f), 0.71f,
          juce::AudioParameterFloatAttributes().withLabel("")))),
      m_lpf_freq(*(new juce::AudioParameterFloat(
          juce::ParameterID("lpf_freq", 1), "Cutoff Freq. (LPF)",
          juce::NormalisableRange(20.0f, 20000.0f, 0.1f, 0.2f), 20000.0f,
          juce::AudioParameterFloatAttributes().withLabel("Hz")))),
      m_lpf_res(*(new juce::AudioParameterFloat(
          juce::ParameterID("lpf_res", 1), "Resonance (LPF)",
          juce::NormalisableRange(0.1f, 10.0f), 0.71f,
          juce::AudioParameterFloatAttributes().withLabel("")))),
      m_attack_ms(*(new juce::AudioParameterFloat(
          juce::ParameterID("attack", 1), "Attack (Env)",
          juce::NormalisableRange(0.0f, 10000.0f, 1e-4f, 0.15f), 10.0f,
          juce::AudioParameterFloatAttributes().withLabel("ms")))),
      m_decay_ms(*(new juce::AudioParameterFloat(
          juce::ParameterID("decay", 1), "Decay (Env)",
          juce::NormalisableRange(0.0f, 10000.0f, 1e-4f, 0.15f), 100.0f,
          juce::AudioParameterFloatAttributes().withLabel("ms")))),
      m_synth(*(new Synth(*this, m_attack_ms, m_decay_ms))) {
#ifdef GLYNTH_LOG_TO_FILE
  startTimerHz(1);
#endif
  addParameter(&m_hpf_freq);
  addParameter(&m_hpf_res);
  addParameter(&m_lpf_freq);
  addParameter(&m_lpf_res);
  addParameter(&m_attack_ms);
  addParameter(&m_decay_ms);

  // m_processors.emplace_back(new NoiseGenerator(*this));
  m_processors.emplace_back(&m_synth);
  m_processors.emplace_back(new HighPassFilter(*this, &m_hpf_freq, &m_hpf_res));
  m_processors.emplace_back(new LowPassFilter(*this, &m_lpf_freq, &m_lpf_res));
  m_processors.emplace_back(new CorruptionSilencer(*this));

  m_font_manager.addFace("SplineSansMono-Bold");
  m_font_manager.addFace("SplineSansMono-Medium");
  auto face = m_font_manager.getFace(m_outline_face);
  m_outline = Outline(m_outline_text, face, 20);
  m_synth.updateWavetable(m_outline);
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
  // Normal process block
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
  return new GlynthEditor(*this, m_font_manager);
}

void GlynthProcessor::getStateInformation(juce::MemoryBlock& dest_data) {
  // Stores parameters in the memory block, either as raw data or using the
  // XML or ValueTree classes as intermediaries to make it easy to save and load
  // complex data.
  auto stream = juce::MemoryOutputStream(dest_data, true);
  stream.writeFloat(m_hpf_freq);
  stream.writeFloat(m_hpf_res);
  stream.writeFloat(m_lpf_freq);
  stream.writeFloat(m_lpf_res);
  stream.writeString(m_outline_text);
  stream.writeString(m_outline_face);
}

void GlynthProcessor::setStateInformation(const void* data, int size) {
  // Restore parameters from this memory block, whose contents will have been
  // created by the getStateInformation() call.
  auto stream = juce::MemoryInputStream(data, static_cast<size_t>(size), false);
  m_hpf_freq = stream.readFloat();
  m_hpf_res = stream.readFloat();
  m_lpf_freq = stream.readFloat();
  m_lpf_res = stream.readFloat();
  std::string outline_text = stream.readString().toStdString();
  std::string outline_face = stream.readString().toStdString();
  if (outline_text != "" && outline_face != "") {
    m_outline_text = outline_text;
    m_outline_face = outline_face;
    auto face = m_font_manager.getFace(m_outline_face);
    m_outline = Outline(m_outline_text, face, 20);
    m_synth.updateWavetable(m_outline);
  }
}

void GlynthProcessor::timerCallback() {
#ifdef GLYNTH_LOG_TO_FILE
  fflush(Logger::file);
#endif
}

juce::AudioParameterFloat& GlynthProcessor::getParamById(std::string_view id) {
  auto params = {&m_hpf_freq, &m_hpf_res,   &m_lpf_freq,
                 &m_lpf_res,  &m_attack_ms, &m_decay_ms};
  for (juce::AudioParameterFloat* param : params) {
    if (id == param->paramID.toStdString()) {
      return *param;
    }
  }
  // Should be unreachable
  throw GlynthError(fmt::format(R"(No parameter found with id "{}")", id));
}

void GlynthProcessor::setOutlineFace(std::string_view face_name) {
  m_outline_face = face_name;
  auto face = m_font_manager.getFace(face_name);
  m_outline = Outline(m_outline_text, face, 20);
  m_synth.updateWavetable(m_outline);
}

void GlynthProcessor::setOutlineText(std::string_view outline_text) {
  m_outline_text = outline_text;
  auto face = m_font_manager.getFace(m_outline_face);
  m_outline = Outline(outline_text, face, 20);
  m_synth.updateWavetable(m_outline);
}

const Outline& GlynthProcessor::getOutline() { return m_outline; }

FT_Face GlynthProcessor::getOutlineFace() {
  return m_font_manager.getFace(m_outline_face);
}

std::string_view GlynthProcessor::getOutlineText() { return m_outline_text; }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
  return new GlynthProcessor();
}

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

Synth::Synth(GlynthProcessor& processor_ref,
             juce::AudioParameterFloat& attack_ms,
             juce::AudioParameterFloat& decay_ms)
    : SubProcessor(processor_ref) {
  attack_ms.addListener(this);
  decay_ms.addListener(this);
  for (size_t i = 0; i < 32; i++) {
    m_voices.emplace_back(m_wavetable, attack_ms.get(), decay_ms.get());
  }
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
        std::optional<size_t> idx;
        if ((idx = getOldestVoiceWithState(SynthVoice::State::Inactive))) {
          m_voices[*idx].configure(note, m_sample_rate);
        } else if ((idx = getOldestVoiceWithState(SynthVoice::State::Decay))) {
          m_voices[*idx].configure(note, m_sample_rate);
        } else {
          // Can always steal from an active voice, so idx is never null
          idx = getOldestVoiceWithState(SynthVoice::State::Active);
          m_voices[*idx].configure(note, m_sample_rate);
        }
      } else if (msg.isNoteOff()) {
        auto note = msg.getNoteNumber();
        for (auto& voice : m_voices) {
          if (voice.note == note) {
            voice.release();
          }
        }
      }

      it++;
    }

    for (int ch = 0; ch < buffer.getNumChannels(); ch++) {
      float sample = 0;
      for (auto& voice : m_voices) {
        if (!voice.isInactive()) {
          sample += m_gain * voice.sample(static_cast<size_t>(ch));
        }
      }
      buffer.setSample(ch, i, static_cast<float>(sample));
    }
  }
}

void Synth::parameterValueChanged(int index, float new_value) {
  if (auto& attack_param = m_processor_ref.getParamById("attack");
      attack_param.getParameterIndex() == index) {
    auto range = attack_param.getNormalisableRange();
    float value = range.convertFrom0to1(new_value);
    for (auto& voice : m_voices) {
      voice.setAttack(value, m_sample_rate);
    }
  }

  else if (auto& decay_param = m_processor_ref.getParamById("decay");
           decay_param.getParameterIndex() == index) {
    auto range = decay_param.getNormalisableRange();
    float value = range.convertFrom0to1(new_value);
    for (auto& voice : m_voices) {
      voice.setDecay(value, m_sample_rate);
    }
  }
}
void Synth::parameterGestureChanged(int, bool) {}

std::optional<size_t> Synth::getOldestVoiceWithState(SynthVoice::State state) {
  std::optional<size_t> best_i;
  for (size_t i = 0; i < m_voices.size(); i++) {
    if (m_voices[i].state == state) {
      if (!best_i.has_value() || m_voices[i].id < m_voices[*best_i].id) {
        best_i = i;
      }
    }
  }
  return best_i;
}

void Synth::updateWavetable(const Outline& outline) {
  size_t n = Wavetable::s_num_samples;
  auto samples = outline.sample(n);
  auto bbox = outline.bbox();
  float x_mean = 0;
  float y_mean = 0;
  std::array<float, Wavetable::s_num_samples> ch0;
  std::array<float, Wavetable::s_num_samples> ch1;
  for (size_t i = 0; i < n; i++) {
    ch0[i] = (samples[i].x - bbox.min.x) / bbox.width() * 2;
    ch1[i] = (samples[i].y - bbox.min.y) / bbox.height() * 2;
    x_mean += ch0[i];
    y_mean += ch1[i];
  }
  x_mean /= static_cast<float>(n);
  y_mean /= static_cast<float>(n);
  // Subtract the mean so there's no DC component
  for (size_t i = 0; i < n; i++) {
    ch0[i] -= x_mean;
    ch1[i] -= y_mean;
  }

  m_wavetable.ch0_old = m_wavetable.ch0;
  std::copy(ch0.begin(), ch0.end(), m_wavetable.ch0.begin());
  m_wavetable.ch1_old = m_wavetable.ch1;
  std::copy(ch1.begin(), ch1.end(), m_wavetable.ch1.begin());
  for (auto& voice : m_voices) {
    voice.crossfade();
  }
}

SynthVoice::SynthVoice(Wavetable& wavetable_ref, float attack_ms,
                       float decay_ms)
    : state(m_state), m_wavetable_ref(wavetable_ref), m_attack_ms(attack_ms),
      m_decay_ms(decay_ms) {
  id = s_next_id;
  s_next_id++;
}

void SynthVoice::configure(int note_number, double sample_rate) {
  note = note_number;
  m_sample_rate = sample_rate;
  setAttack(m_attack_ms, sample_rate);
  setDecay(m_decay_ms, sample_rate);
  m_gain = {1e-8f, 1e-8f};
  m_crossfade = {0, 0};
  m_state = State::Active;
  // Angle goes from 0 -> 1
  m_angle = {0, 0};
  auto freq = juce::MidiMessage::getMidiNoteInHertz(note);
  m_inc = freq / sample_rate;
  id = s_next_id;
  s_next_id++;
}

float SynthVoice::sample(size_t channel) {
  size_t n = Wavetable::s_num_samples;
  double i_float = m_angle[channel] * static_cast<double>(n);
  size_t i = static_cast<size_t>(i_float) % n;
  float value = m_wavetable_ref.sample(channel, i);
  float t = m_crossfade[channel];
  if (t > 0) {
    float old_value = m_wavetable_ref.sample(channel, i, true);
    value = old_value * sqrt(t) + sqrt(1 - t) * value;
    // Works well enough, though it is a hardcoded value
    m_crossfade[channel] -= 1 / static_cast<float>(m_sample_rate);
  }
  m_angle[channel] += m_inc;
  if (m_angle[channel] > 1) {
    m_angle[channel] -= 1;
  }
  if (m_state == State::Active && m_gain[channel] < 1) {
    value *= m_gain[channel];
    // Equivalent to g_k = 1 - c^k
    m_gain[channel] = 1 - m_attack_coeff * (1 - m_gain[channel]);
  } else if (m_state == State::Decay) {
    value *= m_gain[channel];
    m_gain[channel] *= m_decay_coeff;
    if (m_gain[channel] < 1e-8) {
      m_state = State::Inactive;
    }
  }
  return value;
}

void SynthVoice::release() { m_state = State::Decay; }

void SynthVoice::crossfade() { m_crossfade = {1, 1}; }

void SynthVoice::setAttack(float attack_ms, double sample_rate) {
  m_attack_ms = attack_ms;
  float f = static_cast<float>(sample_rate);
  // Calculated so that gain is -3dB after m_attack_ms milliseconds
  m_attack_coeff = std::pow(10.0f, -3 / (f * m_attack_ms / 1000));
}

void SynthVoice::setDecay(float decay_ms, double sample_rate) {
  m_decay_ms = decay_ms;
  float f = static_cast<float>(sample_rate);
  // Calculated so that gain is -80dB after m_decay_ms milliseconds
  m_decay_coeff = std::pow(10.0f, -8 / (f * m_decay_ms / 1000));
}
