#pragma once
#include "daisy_legio.h"
#include "daisysp-lgpl.h"
#include "daisysp.h"
#include <stddef.h>

using namespace daisy;
using namespace daisysp;

// Allocate Delay Line in SDRAM (64MB available on Seed)
#define MAX_DELAY_SAMPLES (48000 * 2) // 2 Seconds max delay

class ModeSpaceEcho {
public:
  void Init(float sample_rate) {
    fs_ = sample_rate;

    // Init Delay
    del_l_.Init();
    del_r_.Init();
    del_l_.SetDelay((size_t)MAX_DELAY_SAMPLES);
    del_r_.SetDelay((size_t)MAX_DELAY_SAMPLES);

    // Init Reverb (Simple ReverbSc for Spring emulation)
    verb_.Init(fs_);
    verb_.SetFeedback(0.85f);
    verb_.SetLpFreq(4000.0f); // Spring-ish dark tail

    // Init Tone Filters
    tone_lp_.Init(fs_);
    tone_hp_.Init(fs_);

    // Init Flutter LFO (Tape wobble)
    lfo_flutter_.Init(fs_);
    lfo_flutter_.SetWaveform(Oscillator::WAVE_SIN);
    lfo_flutter_.SetFreq(2.5f); // 2.5Hz flutter speed
    lfo_flutter_.SetAmp(10.0f); // ~10 samples for audible tape wobble

    // Init Drift LFO (Analog drift - slow pitch/tone modulation)
    lfo_drift_.Init(fs_);
    lfo_drift_.SetWaveform(Oscillator::WAVE_TRI);
    lfo_drift_.SetFreq(0.2f); // Very slow 0.2Hz drift
    lfo_drift_.SetAmp(1.0f);  // Will be scaled

    // Init Feedback Compressor (Envelope Follower)
    fb_env_l_ = 0.0f;
    fb_env_r_ = 0.0f;

    delay_time_ = 0.1f * fs_;
    reverb_amount_ = 0.0f;
  }

  void Process(float in_l, float in_r, float *out_l, float *out_r) {
    // 1. Delay Logic with Analog Drift
    // Read from Delay Line (Interpolated)
    // Stereo Width: Offset Right channel read head by ~15ms
    float width_offset = 0.015f * fs_;

    // Add Flutter (Tape Wobble)
    float flutter = lfo_flutter_.Process();

    // Add Drift (Slow analog drift for pitch/tone variation)
    float drift = lfo_drift_.Process();
    float drift_amount = drift * 3.0f; // +/- 3 samples for subtle pitch drift

    float read_time_l = delay_time_ + flutter + drift_amount;
    float read_time_r = delay_time_ + width_offset + flutter + drift_amount;

    // Use Hermite Interpolation for cleaner pitch shifting
    float read_l = del_l_.ReadHermite(read_time_l);
    float read_r = del_r_.ReadHermite(read_time_r);

    // 2. Feedback Processing
    float fb_l = read_l;
    float fb_r = read_r;

    // Tone Shaping on Feedback (with drift modulation)
    tone_lp_.Process(fb_l);
    fb_l = tone_lp_.Low();
    tone_hp_.Process(fb_l);
    fb_l = tone_hp_.High();

    // Same for right channel
    tone_lp_.Process(fb_r);
    fb_r = tone_lp_.Low();
    tone_hp_.Process(fb_r);
    fb_r = tone_hp_.High();

    // Feedback Compressor (Envelope Follower + Soft Knee)
    // Track envelope
    fb_env_l_ = 0.99f * fb_env_l_ + 0.01f * fabsf(fb_l);
    fb_env_r_ = 0.99f * fb_env_r_ + 0.01f * fabsf(fb_r);

    // Soft compression (ratio ~3:1 above threshold)
    const float kCompThreshold = 0.3f;
    float comp_gain_l = 1.0f;
    float comp_gain_r = 1.0f;

    if (fb_env_l_ > kCompThreshold) {
      float over = fb_env_l_ - kCompThreshold;
      comp_gain_l = kCompThreshold / (kCompThreshold + over * 0.66f);
    }
    if (fb_env_r_ > kCompThreshold) {
      float over = fb_env_r_ - kCompThreshold;
      comp_gain_r = kCompThreshold / (kCompThreshold + over * 0.66f);
    }

    fb_l *= comp_gain_l;
    fb_r *= comp_gain_r;

    // Enhanced Tape Saturation (Asymmetric + High-freq roll-off)
    // Boost into saturation for more character
    fb_l = AsymmetricTapeSat(fb_l * 1.8f);
    fb_r = AsymmetricTapeSat(fb_r * 1.8f);

    // High-frequency roll-off increases with feedback (tape head wear)
    // This is already handled by tone_lp_, but we can add more character

    // Soft Limiter before write (prevent runaway feedback)
    fb_l = tanhf(fb_l * 1.2f) * 0.85f;
    fb_r = tanhf(fb_r * 1.2f) * 0.85f;

    // Write back to delay (Input + Feedback)
    float write_val_l = in_l + (fb_l * feedback_amount_);
    float write_val_r = in_r + (fb_r * feedback_amount_);

    del_l_.Write(write_val_l);
    del_r_.Write(write_val_r);

    // 3. Reverb Logic
    float verb_in_l = read_l; // Reverb comes after delay heads
    float verb_in_r = read_r;
    float verb_out_l, verb_out_r;

    verb_.Process(verb_in_l, verb_in_r, &verb_out_l, &verb_out_r);

    // 4. Mix
    // Dry + Wet Delay + Wet Reverb
    *out_l = in_l + (read_l * 0.8f) + (verb_out_l * reverb_amount_);
    *out_r = in_r + (read_r * 0.8f) + (verb_out_r * reverb_amount_);
  }

  void UpdateControls(DaisyLegio &hw) {
    // Knobs
    float k_time = hw.controls[DaisyLegio::CONTROL_KNOB_TOP].Value();
    float k_feedback = hw.controls[DaisyLegio::CONTROL_KNOB_BOTTOM].Value();

    // Encoder Turn (Reverb Amount)
    float inc = hw.encoder.Increment();
    reverb_amount_ += inc * 0.05f;
    reverb_amount_ = fclamp(reverb_amount_, 0.0f, 1.0f);

    // Switches
    int sw_head = hw.sw[DaisyLegio::SW_LEFT].Read();
    int sw_tone = hw.sw[DaisyLegio::SW_RIGHT].Read();

    // Map Head Mode (Top=Short, Mid=Med, Bot=Long)
    float delay_time_target = 0.1f;
    // FIX: Inverted Switch Logic (2=Top, 1=Mid, 0=Bot)
    if (sw_head == 2)
      delay_time_target = 0.1f + (k_time * 0.2f); // Short: 100-300ms
    else if (sw_head == 1)
      delay_time_target = 0.3f + (k_time * 0.4f); // Med: 300-700ms
    else
      delay_time_target = 0.5f + (k_time * 1.0f); // Long: 500-1500ms

    // Smooth delay time changes to simulate tape speed change (pitch warp)
    // Increased smoothing speed for better responsiveness (0.001 -> 0.05)
    fonepole(delay_time_, delay_time_target * fs_, 0.05f);

    // Map Tone (Top=Bright, Mid=Normal, Bot=Dark)
    // FIX: Inverted Switch Logic (2=Top, 1=Mid, 0=Bot)
    // Adjusted frequencies to be less "shrill"
    if (sw_tone == 2) {           // Bright
      tone_lp_.SetFreq(12000.0f); // Was 18000
      tone_hp_.SetFreq(200.0f);
    } else if (sw_tone == 1) {   // Normal
      tone_lp_.SetFreq(4500.0f); // Was 8000
      tone_hp_.SetFreq(100.0f);
    } else {                     // Dark
      tone_lp_.SetFreq(1200.0f); // Was 3000
      tone_hp_.SetFreq(400.0f);
    }

    feedback_amount_ = k_feedback * 1.1f; // Allow self-oscillation (>1.0)
  }

private:
  ReverbSc verb_;
  DelayLine<float, MAX_DELAY_SAMPLES> del_l_;
  DelayLine<float, MAX_DELAY_SAMPLES> del_r_;
  Svf tone_lp_, tone_hp_;
  Oscillator lfo_flutter_;
  Oscillator lfo_drift_; // Analog drift LFO
  float fs_;
  float feedback_amount_;
  float reverb_amount_;
  float delay_time_;
  float fb_env_l_, fb_env_r_; // Feedback compressor envelope

  // Asymmetric tape saturation (different curves for +/-)
  float AsymmetricTapeSat(float x) {
    if (x > 0.0f) {
      // Positive: softer saturation
      return tanhf(x * 0.9f);
    } else {
      // Negative: harder saturation (asymmetric like tape)
      return tanhf(x * 1.2f) * 0.95f;
    }
  }

  float SoftClip(float in) {
    if (in < -1.0f)
      return -0.6666667f;
    else if (in > 1.0f)
      return 0.6666667f;
    else
      return in - (in * in * in) * 0.3333333f;
  }
};
