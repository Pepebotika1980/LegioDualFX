#pragma once
#include "daisy_legio.h"
#include "daisysp-lgpl.h"
#include "daisysp.h"
#include <stddef.h>

using namespace daisy;
using namespace daisysp;

class ModeShimmerReverb {
public:
  void Init(float sample_rate) {
    fs_ = sample_rate;

    // Init Reverb (ReverbSc - Sean Costello FDN)
    // Tuned for "Lush" sound
    verb_.Init(fs_);
    verb_.SetFeedback(0.85f);
    verb_.SetLpFreq(3500.0f); // Lower cutoff for warmer, less metallic sound

    // Init Pitch Shifter
    pshift_l_.Init(fs_);
    pshift_r_.Init(fs_);
    pshift_l_.SetTransposition(12.0f);
    pshift_r_.SetTransposition(12.0f);

    // Init Tone Filter
    tone_filter_.Init(fs_);
    tone_filter_.SetFreq(10000.0f);
    tone_filter_.SetRes(0.0f);

    tone_filter_r_.Init(fs_);
    tone_filter_r_.SetFreq(10000.0f);
    tone_filter_r_.SetRes(0.0f);

    // Init DC Blocker
    dc_blocker_.Init(fs_);
    dc_blocker_.SetFreq(20.0f);
    dc_blocker_.SetRes(0.0f);

    dc_blocker_r_.Init(fs_);
    dc_blocker_r_.SetFreq(20.0f);
    dc_blocker_r_.SetRes(0.0f);

    // Init Anti-Rumble Filter
    anti_rumble_.Init(fs_);
    anti_rumble_.SetFreq(150.0f);
    anti_rumble_.SetRes(0.0f);

    anti_rumble_r_.Init(fs_);
    anti_rumble_r_.SetFreq(150.0f);
    anti_rumble_r_.SetRes(0.0f);

    // Init Variable Input HPF (2-pole for smoother slope)
    input_hpf_l_.Init(fs_);
    input_hpf_l_.SetFreq(250.0f); // Default middle position
    input_hpf_l_.SetRes(0.0f);

    input_hpf_r_.Init(fs_);
    input_hpf_r_.SetFreq(250.0f);
    input_hpf_r_.SetRes(0.0f);

    // Init Input HPF Stage 2 (2-pole)
    input_hpf_l2_.Init(fs_);
    input_hpf_l2_.SetFreq(250.0f);
    input_hpf_l2_.SetRes(0.0f);

    input_hpf_r2_.Init(fs_);
    input_hpf_r2_.SetFreq(250.0f);
    input_hpf_r2_.SetRes(0.0f);

    // Init Pre-Delay
    predelay_l_.Init();
    predelay_r_.Init();
    predelay_l_.SetDelay(kPredelayTime * fs_);
    predelay_r_.SetDelay(kPredelayTime * fs_);

    shimmer_amount_ = 0.0f;
    mix_ = 0.5f;
    shimmer_fb_l_ = 0.0f;
    shimmer_fb_r_ = 0.0f;
    shimmer_env_l_ = 0.0f;
    shimmer_env_r_ = 0.0f;
    hpf_freq_ = 250.0f;
    target_pitch_l_ = 12.0f;
    target_pitch_r_ = 12.0f;
    current_pitch_l_ = 12.0f;
    current_pitch_r_ = 12.0f;
  }

  void Process(float in_l, float in_r, float *out_l, float *out_r) {
    float verb_out_l, verb_out_r;

    // 1. Variable Input HPF (2-pole for smooth slope)
    input_hpf_l_.Process(in_l);
    input_hpf_r_.Process(in_r);
    float stage1_l = input_hpf_l_.High();
    float stage1_r = input_hpf_r_.High();

    // Stage 2 for 24dB/oct slope
    input_hpf_l2_.Process(stage1_l);
    input_hpf_r2_.Process(stage1_r);
    float wet_in_l = input_hpf_l2_.High();
    float wet_in_r = input_hpf_r2_.High();

    // Attenuate input to prevent internal clipping
    wet_in_l *= kInputAttenuation;
    wet_in_r *= kInputAttenuation;

    // 2. Pre-Delay with Hermite Interpolation
    predelay_l_.Write(wet_in_l);
    predelay_r_.Write(wet_in_r);
    float pre_l = predelay_l_.ReadHermite(kPredelayTime * fs_);
    float pre_r = predelay_r_.ReadHermite(kPredelayTime * fs_);

    // 3. Reverb Engine
    // Add Shimmer Feedback to Input
    float shimmer_in_l = pre_l + (shimmer_fb_l_ * shimmer_amount_);
    float shimmer_in_r = pre_r + (shimmer_fb_r_ * shimmer_amount_);

    verb_.Process(shimmer_in_l, shimmer_in_r, &verb_out_l, &verb_out_r);

    // 4. Pitch Shift Loop with Compression
    anti_rumble_.Process(verb_out_l);
    float clean_l = anti_rumble_.High();

    anti_rumble_r_.Process(verb_out_r);
    float clean_r = anti_rumble_r_.High();

    // Smooth pitch transitions to reduce artifacts
    fonepole(current_pitch_l_, target_pitch_l_, kPitchSmoothCoeff);
    fonepole(current_pitch_r_, target_pitch_r_, kPitchSmoothCoeff);
    pshift_l_.SetTransposition(current_pitch_l_);
    pshift_r_.SetTransposition(current_pitch_r_);

    float shifted_l = pshift_l_.Process(clean_l);
    float shifted_r = pshift_r_.Process(clean_r);

    tone_filter_.Process(shifted_l);
    float filtered_shifted_l = tone_filter_.Low();

    tone_filter_r_.Process(shifted_r);
    float filtered_shifted_r = tone_filter_r_.Low();

    dc_blocker_.Process(filtered_shifted_l);
    filtered_shifted_l = dc_blocker_.High();

    dc_blocker_r_.Process(filtered_shifted_r);
    filtered_shifted_r = dc_blocker_r_.High();

    // Shimmer Loop Compressor (Envelope Follower + Soft Knee)
    shimmer_env_l_ = kShimmerCompAttack * shimmer_env_l_ +
                     kShimmerCompRelease * fabsf(filtered_shifted_l);
    shimmer_env_r_ = kShimmerCompAttack * shimmer_env_r_ +
                     kShimmerCompRelease * fabsf(filtered_shifted_r);

    float shimmer_gain_l = 1.0f;
    float shimmer_gain_r = 1.0f;

    if (shimmer_env_l_ > kShimmerThreshold) {
      float over = shimmer_env_l_ - kShimmerThreshold;
      shimmer_gain_l =
          kShimmerThreshold / (kShimmerThreshold + over * kShimmerCompRatio);
    }
    if (shimmer_env_r_ > kShimmerThreshold) {
      float over = shimmer_env_r_ - kShimmerThreshold;
      shimmer_gain_r =
          kShimmerThreshold / (kShimmerThreshold + over * kShimmerCompRatio);
    }

    filtered_shifted_l *= shimmer_gain_l;
    filtered_shifted_r *= shimmer_gain_r;

    // Soft Limiter for Feedback Loop
    filtered_shifted_l =
        tanhf(filtered_shifted_l * kShimmerLimitGain) * kShimmerLimitScale;
    filtered_shifted_r =
        tanhf(filtered_shifted_r * kShimmerLimitGain) * kShimmerLimitScale;

    // Update feedback vars for next frame
    shimmer_fb_l_ = filtered_shifted_l;
    shimmer_fb_r_ = filtered_shifted_r;

    // Safety Limiter for Reverb Output (before mix)
    verb_out_l = tanhf(verb_out_l);
    verb_out_r = tanhf(verb_out_r);

    // 5. Mix Output
    *out_l = (in_l * (1.0f - mix_)) + (verb_out_l * mix_);
    *out_r = (in_r * (1.0f - mix_)) + (verb_out_r * mix_);
  }

  void UpdateControls(DaisyLegio &hw) {
    // Knobs
    float k_decay = hw.controls[DaisyLegio::CONTROL_KNOB_TOP].Value();
    float k_hpf = hw.controls[DaisyLegio::CONTROL_KNOB_BOTTOM].Value();

    // Encoder Turn (Shimmer Amount)
    float inc = hw.encoder.Increment();
    shimmer_amount_ += inc * kShimmerEncoderSensitivity;
    shimmer_amount_ = fclamp(shimmer_amount_, 0.0f, kShimmerAmountMax);

    // Switches
    int sw_pitch = hw.sw[DaisyLegio::SW_LEFT].Read();
    int sw_tone = hw.sw[DaisyLegio::SW_RIGHT].Read();

    // Map Pitch Interval (Left Switch)
    float base_pitch = 12.0f;
    if (sw_pitch == 2)
      base_pitch = kPitchOctaveUp;
    else if (sw_pitch == 1)
      base_pitch = kPitchFifthUp;
    else
      base_pitch = kPitchOctaveDown;

    // Set target pitch with slight detune for width (+/- 5 cents)
    target_pitch_l_ = base_pitch - kPitchDetune;
    target_pitch_r_ = base_pitch + kPitchDetune;

    // Map Tone (Right Switch)
    if (sw_tone == 2) { // Bright
      tone_filter_.SetFreq(kToneBrightFreq);
      tone_filter_r_.SetFreq(kToneBrightFreq);
      verb_.SetLpFreq(kVerbBrightFreq);
    } else if (sw_tone == 1) { // Normal
      tone_filter_.SetFreq(kToneNormalFreq);
      tone_filter_r_.SetFreq(kToneNormalFreq);
      verb_.SetLpFreq(kVerbNormalFreq);
    } else { // Dark
      tone_filter_.SetFreq(kToneDarkFreq);
      tone_filter_r_.SetFreq(kToneDarkFreq);
      verb_.SetLpFreq(kVerbDarkFreq);
    }

    // Variable HPF (150Hz - 500Hz) controlled by bottom knob
    float target_hpf = kHPFMin + (k_hpf * kHPFRange);
    fonepole(hpf_freq_, target_hpf, kHPFSmoothCoeff);
    input_hpf_l_.SetFreq(hpf_freq_);
    input_hpf_r_.SetFreq(hpf_freq_);
    input_hpf_l2_.SetFreq(hpf_freq_);
    input_hpf_r2_.SetFreq(hpf_freq_);

    // Map decay to feedback 0.7 -> 0.98
    verb_.SetFeedback(kDecayMin + (k_decay * kDecayRange));

    // Mix is now fixed at 0.5 (50/50) since bottom knob controls HPF
    mix_ = kMixFixed;
  }

private:
  // Audio Processing Constants
  static constexpr float kPredelayTime = 0.04f;
  static constexpr float kInputAttenuation = 0.8f;
  static constexpr float kPitchSmoothCoeff = 0.001f;
  static constexpr float kShimmerCompAttack = 0.99f;
  static constexpr float kShimmerCompRelease = 0.01f;
  static constexpr float kShimmerThreshold = 0.4f;
  static constexpr float kShimmerCompRatio = 0.66f;
  static constexpr float kShimmerLimitGain = 1.1f;
  static constexpr float kShimmerLimitScale = 0.9f;

  // Control Constants
  static constexpr float kShimmerEncoderSensitivity = 0.05f;
  static constexpr float kShimmerAmountMax = 0.4f;
  static constexpr float kPitchOctaveUp = 12.0f;
  static constexpr float kPitchFifthUp = 7.0f;
  static constexpr float kPitchOctaveDown = -12.0f;
  static constexpr float kPitchDetune = 0.05f;
  static constexpr float kHPFMin = 150.0f;
  static constexpr float kHPFRange = 350.0f;
  static constexpr float kHPFSmoothCoeff = 0.01f;
  static constexpr float kDecayMin = 0.7f;
  static constexpr float kDecayRange = 0.28f;
  static constexpr float kMixFixed = 0.5f;

  // Tone Constants
  static constexpr float kToneBrightFreq = 15000.0f;
  static constexpr float kToneNormalFreq = 5000.0f;
  static constexpr float kToneDarkFreq = 1000.0f;
  static constexpr float kVerbBrightFreq = 12000.0f;
  static constexpr float kVerbNormalFreq = 4000.0f;
  static constexpr float kVerbDarkFreq = 1000.0f;

  ReverbSc verb_;
  PitchShifter pshift_l_, pshift_r_;
  Svf tone_filter_, tone_filter_r_;
  Svf dc_blocker_, dc_blocker_r_;
  Svf anti_rumble_, anti_rumble_r_;
  Svf input_hpf_l_, input_hpf_r_;                  // Input HPF stage 1
  Svf input_hpf_l2_, input_hpf_r2_;                // Input HPF stage 2 (2-pole)
  DelayLine<float, 4800> predelay_l_, predelay_r_; // ~100ms max
  float fs_;

  float shimmer_amount_;
  float mix_;
  float shimmer_fb_l_, shimmer_fb_r_;
  float shimmer_env_l_, shimmer_env_r_;     // Shimmer loop compressor envelope
  float hpf_freq_;                          // Variable HPF frequency
  float target_pitch_l_, target_pitch_r_;   // Target pitch for smoothing
  float current_pitch_l_, current_pitch_r_; // Current pitch (smoothed)
};
