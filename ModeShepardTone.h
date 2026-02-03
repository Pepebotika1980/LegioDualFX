#pragma once
#include "daisy_legio.h"
#include "daisysp.h"
#include <math.h>

using namespace daisy;
using namespace daisysp;

#define NUM_VOICES 8
#define SHEPARD_TWOPI 6.28318530717958647692f

class ModeShepardTone {
public:
  void Init(float sample_rate) {
    fs_ = sample_rate;

    // Init 8 voices for cleaner sound (less mud)
    for (int i = 0; i < NUM_VOICES; i++) {
      voice_phase_[i] = (float)i / (float)NUM_VOICES;
      // Exponential frequency distribution handled in Process
    }

    // Integrated Reverb for "Beautiful" sound
    verb_.Init(fs_);
    verb_.SetFeedback(0.85f);
    verb_.SetLpFreq(10000.0f);

    // Stereo spread LFO
    lfo_spread_.Init(fs_);
    lfo_spread_.SetWaveform(Oscillator::WAVE_SIN);
    lfo_spread_.SetFreq(0.1f);
    lfo_spread_.SetAmp(0.5f);

    // Final Limiter
    limiter_.Init();

    // Default parameters
    speed_ = 0.2f;
    range_ = 0.5f;
    direction_ = 1.0f;
    reverb_amount_ = 0.3f;
    tone_cutoff_ = 12000.0f;

    // Tone Filter
    tone_filter_l_.Init(fs_);
    tone_filter_r_.Init(fs_);
    tone_filter_l_.SetRes(0.0f);
    tone_filter_r_.SetRes(0.0f);
  }

  void Process(float in_l, float in_r, float *out_l, float *out_r) {
    // 1. Calculate Envelope Position
    float speed_val = speed_ * direction_;
    float delta = speed_val / fs_; // increment per sample

    float sum_l = 0.0f;
    float sum_r = 0.0f;

    float spread_mod = lfo_spread_.Process();

    for (int i = 0; i < NUM_VOICES; i++) {
      voice_phase_[i] += delta;
      if (voice_phase_[i] >= 1.0f)
        voice_phase_[i] -= 1.0f;
      if (voice_phase_[i] < 0.0f)
        voice_phase_[i] += 1.0f;

      // Calculate Amplitude Envelope (Hann Window)
      // 0.5 * (1 - cos(2*pi*x))
      float envelope = 0.5f * (1.0f - cosf(voice_phase_[i] * SHEPARD_TWOPI));

      // Calculate Frequency
      // 20Hz * 2^(10 * position) -> 10 octaves range
      // Center freq adjustable by "range" parameter
      float center_octave = 5.0f + (range_ * 4.0f); // 5 to 9 octaves center
      float octave = (voice_phase_[i] * 10.0f) - 5.0f + center_octave;
      // Map 0..1 to -5..+5 octaves relative to center?
      // Let's simplify: 20Hz base. 2^10 = 1024 * 20 = 20kHz.
      // Full range 20Hz to 20kHz is ~10 octaves.

      float freq = 20.0f * powf(2.0f, voice_phase_[i] * 10.0f);

      // Oscillator Generation (Pure Sine)
      // Integrate phase: phase += freq/fs
      osc_phasor_[i] += freq / fs_;
      if (osc_phasor_[i] >= 1.0f)
        osc_phasor_[i] -= 1.0f;

      float sine_out = sinf(osc_phasor_[i] * SHEPARD_TWOPI);

      // Stereo Pan based on pitch or random?
      // Let's pan increasingly wide based on LFO and pitch
      float pan = spread_mod * 0.5f; // -0.5 to 0.5
      // Add subtle offset per voice
      if (i % 2 == 0)
        pan += 0.2f;
      else
        pan -= 0.2f;

      float gain_l = envelope * (0.5f + pan);
      float gain_r = envelope * (0.5f - pan);

      sum_l += sine_out * gain_l;
      sum_r += sine_out * gain_r;
    }

    // 2. Normalize Sum (8 voices, max overlap is limited, but safe div by 6)
    sum_l *= 0.15f;
    sum_r *= 0.15f;

    // 3. Tone Shaping (Low Pass for warmth)
    tone_filter_l_.SetFreq(tone_cutoff_);
    tone_filter_r_.SetFreq(tone_cutoff_ * 1.1f); // Subtle stereo diff
    tone_filter_l_.Process(sum_l);
    tone_filter_r_.Process(sum_r);
    sum_l = tone_filter_l_.Low();
    sum_r = tone_filter_r_.Low();

    // 4. Reverb (The "Beauty" layer)
    float verb_l, verb_r;
    verb_.Process(sum_l, sum_r, &verb_l, &verb_r);

    // Mix Reverb
    sum_l = sum_l * (1.0f - reverb_amount_) + verb_l * reverb_amount_;
    sum_r = sum_r * (1.0f - reverb_amount_) + verb_r * reverb_amount_;

    // 5. Final Limiting (Safety)
    // Soft tanh limit
    sum_l = tanhf(sum_l * 1.5f) * 0.9f;
    sum_r = tanhf(sum_r * 1.5f) * 0.9f;

    *out_l = sum_l;
    *out_r = sum_r;
  }

  void UpdateControls(DaisyLegio &hw) {
    // Knob 1: Speed
    float k_speed = hw.controls[DaisyLegio::CONTROL_KNOB_TOP].Value();
    // Exponential speed: 0.01Hz to 5.0Hz (octaves per sec)
    speed_ = 0.01f * powf(100.0f, k_speed);

    // Knob 2: Tone / Brightness
    float k_tone = hw.controls[DaisyLegio::CONTROL_KNOB_BOTTOM].Value();
    tone_cutoff_ = 200.0f + (k_tone * k_tone * 12000.0f); // 200Hz to 12kHz

    // Encoder Turn: Reverb Amount (The aesthetic control)
    float inc = hw.encoder.Increment();
    reverb_amount_ += inc * 0.05f;
    if (reverb_amount_ > 1.0f)
      reverb_amount_ = 1.0f;
    if (reverb_amount_ < 0.0f)
      reverb_amount_ = 0.0f;

    // Sw Left: Direction
    int sw_dir = hw.sw[DaisyLegio::SW_LEFT].Read();
    if (sw_dir == 2)
      direction_ = 1.0f; // Up
    else if (sw_dir == 1)
      direction_ = 0.0f; // Pause
    else
      direction_ = -1.0f; // Down

    // Sw Right: Frequency Range Center (Low, Mid, High)
    int sw_range = hw.sw[DaisyLegio::SW_RIGHT].Read();
    // Just shifts the center octave
    if (sw_range == 2)
      range_ = 0.8f; // High
    else if (sw_range == 1)
      range_ = 0.5f; // Mid
    else
      range_ = 0.2f; // Low
  }

private:
  float fs_;
  float voice_phase_[NUM_VOICES]; // 0.0 to 1.0 (shepard cycle position)
  float osc_phasor_[NUM_VOICES];  // 0.0 to 1.0 (sine wave phase)

  // Parameters
  float speed_;
  float range_;
  float direction_;
  float reverb_amount_;
  float tone_cutoff_;

  // Modules
  ReverbSc verb_;
  Oscillator lfo_spread_;
  Limiter limiter_;
  Svf tone_filter_l_, tone_filter_r_;
};
