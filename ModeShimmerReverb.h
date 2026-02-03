#pragma once
// #include "PlateReverb.h" // Reverting to ReverbSc for stability
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
    predelay_l_.SetDelay(0.04f * fs_); // Fixed 40ms
    predelay_r_.SetDelay(0.04f * fs_);

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

    // Attenuate input to prevent internal clipping (0.8x)
    wet_in_l *= 0.8f;
    wet_in_r *= 0.8f;

    // 2. Pre-Delay with Hermite Interpolation
    predelay_l_.Write(wet_in_l);
    predelay_r_.Write(wet_in_r);
    float pre_l = predelay_l_.ReadHermite(0.04f * fs_);
    float pre_r = predelay_r_.ReadHermite(0.04f * fs_);

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
    fonepole(current_pitch_l_, target_pitch_l_, 0.001f);
    fonepole(current_pitch_r_, target_pitch_r_, 0.001f);
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
    shimmer_env_l_ = 0.99f * shimmer_env_l_ + 0.01f * fabsf(filtered_shifted_l);
    shimmer_env_r_ = 0.99f * shimmer_env_r_ + 0.01f * fabsf(filtered_shifted_r);

    const float kShimmerThreshold = 0.4f;
    float shimmer_gain_l = 1.0f;
    float shimmer_gain_r = 1.0f;

    if (shimmer_env_l_ > kShimmerThreshold) {
      float over = shimmer_env_l_ - kShimmerThreshold;
      shimmer_gain_l = kShimmerThreshold / (kShimmerThreshold + over * 0.66f);
    }
    if (shimmer_env_r_ > kShimmerThreshold) {
      float over = shimmer_env_r_ - kShimmerThreshold;
      shimmer_gain_r = kShimmerThreshold / (kShimmerThreshold + over * 0.66f);
    }

    filtered_shifted_l *= shimmer_gain_l;
    filtered_shifted_r *= shimmer_gain_r;

    // Soft Limiter for Feedback Loop
    filtered_shifted_l = tanhf(filtered_shifted_l * 1.1f) * 0.9f;
    filtered_shifted_r = tanhf(filtered_shifted_r * 1.1f) * 0.9f;

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
    shimmer_amount_ += inc * 0.05f;
    shimmer_amount_ = fclamp(shimmer_amount_, 0.0f, 0.4f);

    // Switches
    int sw_pitch = hw.sw[DaisyLegio::SW_LEFT].Read();
    int sw_tone = hw.sw[DaisyLegio::SW_RIGHT].Read();

    // Map Pitch Interval (Left Switch)
    float base_pitch = 12.0f;
    if (sw_pitch == 2)
      base_pitch = 12.0f; // +1 Octave
    else if (sw_pitch == 1)
      base_pitch = 7.0f; // +5th
    else
      base_pitch = -12.0f; // -1 Octave

    // Set target pitch with slight detune for width (+/- 5 cents)
    target_pitch_l_ = base_pitch - 0.05f;
    target_pitch_r_ = base_pitch + 0.05f;

    // Map Tone (Right Switch)
    if (sw_tone == 2) { // Bright
      tone_filter_.SetFreq(15000.0f);
      tone_filter_r_.SetFreq(15000.0f);
      verb_.SetLpFreq(12000.0f);
    } else if (sw_tone == 1) { // Normal
      tone_filter_.SetFreq(5000.0f);
      tone_filter_r_.SetFreq(5000.0f);
      verb_.SetLpFreq(4000.0f);
    } else { // Dark
      tone_filter_.SetFreq(1000.0f);
      tone_filter_r_.SetFreq(1000.0f);
      verb_.SetLpFreq(1000.0f);
    }

    // Variable HPF (150Hz - 500Hz) controlled by bottom knob
    float target_hpf = 150.0f + (k_hpf * 350.0f);
    fonepole(hpf_freq_, target_hpf, 0.01f);
    input_hpf_l_.SetFreq(hpf_freq_);
    input_hpf_r_.SetFreq(hpf_freq_);
    input_hpf_l2_.SetFreq(hpf_freq_);
    input_hpf_r2_.SetFreq(hpf_freq_);

    // Map decay to feedback 0.7 -> 0.98
    verb_.SetFeedback(0.7f + (k_decay * 0.28f));

    // Mix is now fixed at 0.5 (50/50) since bottom knob controls HPF
    mix_ = 0.5f;
  }

private:
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

  float SoftClip(float in) {
    if (in < -1.0f)
      return -0.6666667f;
    else if (in > 1.0f)
      return 0.6666667f;
    else
      return in - (in * in * in) * 0.3333333f;
  }
};
