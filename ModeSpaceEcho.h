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
    lfo_flutter_.SetFreq(kFlutterFreq);
    lfo_flutter_.SetAmp(kFlutterAmount);

    // Init Drift LFO (Analog drift - slow pitch/tone modulation)
    lfo_drift_.Init(fs_);
    lfo_drift_.SetWaveform(Oscillator::WAVE_TRI);
    lfo_drift_.SetFreq(kDriftFreq);
    lfo_drift_.SetAmp(1.0f); // Will be scaled

    // Init Feedback Compressor (Envelope Follower)
    fb_env_l_ = 0.0f;
    fb_env_r_ = 0.0f;

    delay_time_ = 0.1f * fs_;
    reverb_amount_ = 0.0f;

    // Init noise state for organic flutter
    noise_state_ = 12345;
  }

  void Process(float in_l, float in_r, float *out_l, float *out_r) {
    // 1. Delay Logic with Analog Drift
    // Read from Delay Line (Interpolated)
    // Stereo Width: Offset Right channel read head by ~15ms
    float width_offset = kStereoWidthOffset * fs_;

    // Add Flutter (Tape Wobble) with organic noise modulation
    float flutter = lfo_flutter_.Process();

    // Add subtle noise to flutter for more organic tape feel
    float noise = GenerateNoise() * kFlutterNoiseAmount;
    flutter += noise;

    // Add Drift (Slow analog drift for pitch/tone variation)
    float drift = lfo_drift_.Process();
    float drift_amount =
        drift * kDriftAmount; // +/- 3 samples for subtle pitch drift

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
    fb_env_l_ = kCompAttack * fb_env_l_ + kCompRelease * fabsf(fb_l);
    fb_env_r_ = kCompAttack * fb_env_r_ + kCompRelease * fabsf(fb_r);

    // Soft compression (ratio ~3:1 above threshold)
    float comp_gain_l = 1.0f;
    float comp_gain_r = 1.0f;

    if (fb_env_l_ > kCompThreshold) {
      float over = fb_env_l_ - kCompThreshold;
      comp_gain_l = kCompThreshold / (kCompThreshold + over * kCompRatio);
    }
    if (fb_env_r_ > kCompThreshold) {
      float over = fb_env_r_ - kCompThreshold;
      comp_gain_r = kCompThreshold / (kCompThreshold + over * kCompRatio);
    }

    fb_l *= comp_gain_l;
    fb_r *= comp_gain_r;

    // Enhanced Tape Saturation (Asymmetric + High-freq roll-off)
    // Boost into saturation for more character
    fb_l = AsymmetricTapeSat(fb_l * kTapeSatGain);
    fb_r = AsymmetricTapeSat(fb_r * kTapeSatGain);

    // Soft Limiter before write (prevent runaway feedback)
    fb_l = tanhf(fb_l * kFeedbackLimitGain) * kFeedbackLimitScale;
    fb_r = tanhf(fb_r * kFeedbackLimitGain) * kFeedbackLimitScale;

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
    *out_l = in_l + (read_l * kDelayWetMix) + (verb_out_l * reverb_amount_);
    *out_r = in_r + (read_r * kDelayWetMix) + (verb_out_r * reverb_amount_);
  }

  void UpdateControls(DaisyLegio &hw) {
    // Knobs
    float k_time = hw.controls[DaisyLegio::CONTROL_KNOB_TOP].Value();
    float k_feedback = hw.controls[DaisyLegio::CONTROL_KNOB_BOTTOM].Value();

    // Encoder Turn (Reverb Amount)
    float inc = hw.encoder.Increment();
    reverb_amount_ += inc * kReverbEncoderSensitivity;
    reverb_amount_ = fclamp(reverb_amount_, 0.0f, 1.0f);

    // Switches
    int sw_head = hw.sw[DaisyLegio::SW_LEFT].Read();
    int sw_tone = hw.sw[DaisyLegio::SW_RIGHT].Read();

    // Map Head Mode (Top=Short, Mid=Med, Bot=Long)
    float delay_time_target = 0.1f;
    // FIX: Inverted Switch Logic (2=Top, 1=Mid, 0=Bot)
    if (sw_head == 2)
      delay_time_target = kDelayShortMin + (k_time * kDelayShortRange);
    else if (sw_head == 1)
      delay_time_target = kDelayMedMin + (k_time * kDelayMedRange);
    else
      delay_time_target = kDelayLongMin + (k_time * kDelayLongRange);

    // Smooth delay time changes to simulate tape speed change (pitch warp)
    fonepole(delay_time_, delay_time_target * fs_, kDelayTimeSmooth);

    // Map Tone (Top=Bright, Mid=Normal, Bot=Dark)
    // FIX: Inverted Switch Logic (2=Top, 1=Mid, 0=Bot)
    if (sw_tone == 2) { // Bright
      tone_lp_.SetFreq(kToneBrightLP);
      tone_hp_.SetFreq(kToneBrightHP);
    } else if (sw_tone == 1) { // Normal
      tone_lp_.SetFreq(kToneNormalLP);
      tone_hp_.SetFreq(kToneNormalHP);
    } else { // Dark
      tone_lp_.SetFreq(kToneDarkLP);
      tone_hp_.SetFreq(kToneDarkHP);
    }

    feedback_amount_ =
        k_feedback * kFeedbackMax; // Allow self-oscillation (>1.0)
  }

private:
  // Audio Processing Constants
  static constexpr float kStereoWidthOffset = 0.015f;
  static constexpr float kFlutterFreq = 2.5f;
  static constexpr float kFlutterAmount = 10.0f;
  static constexpr float kFlutterNoiseAmount = 2.0f;
  static constexpr float kDriftFreq = 0.2f;
  static constexpr float kDriftAmount = 3.0f;
  static constexpr float kCompThreshold = 0.3f;
  static constexpr float kCompAttack = 0.99f;
  static constexpr float kCompRelease = 0.01f;
  static constexpr float kCompRatio = 0.66f;
  static constexpr float kTapeSatGain = 1.8f;
  static constexpr float kFeedbackLimitGain = 1.2f;
  static constexpr float kFeedbackLimitScale = 0.85f;
  static constexpr float kDelayWetMix = 0.8f;

  // Control Constants
  static constexpr float kReverbEncoderSensitivity = 0.05f;
  static constexpr float kDelayShortMin = 0.1f;
  static constexpr float kDelayShortRange = 0.2f;
  static constexpr float kDelayMedMin = 0.3f;
  static constexpr float kDelayMedRange = 0.4f;
  static constexpr float kDelayLongMin = 0.5f;
  static constexpr float kDelayLongRange = 1.0f;
  static constexpr float kDelayTimeSmooth = 0.05f;
  static constexpr float kFeedbackMax = 1.1f;

  // Tone Constants
  static constexpr float kToneBrightLP = 12000.0f;
  static constexpr float kToneBrightHP = 200.0f;
  static constexpr float kToneNormalLP = 4500.0f;
  static constexpr float kToneNormalHP = 100.0f;
  static constexpr float kToneDarkLP = 1200.0f;
  static constexpr float kToneDarkHP = 400.0f;

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
  uint32_t noise_state_;      // For noise generation

  // Simple noise generator for organic flutter
  float GenerateNoise() {
    noise_state_ = noise_state_ * 1103515245 + 12345;
    return ((float)(noise_state_ >> 16) / 32768.0f) - 1.0f;
  }

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
};
