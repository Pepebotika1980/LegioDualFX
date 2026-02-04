#pragma once
#include "daisy_legio.h"
#include "daisysp.h"
#include <math.h>

using namespace daisy;
using namespace daisysp;

class ModeFilterDrive {
public:
  void Init(float sample_rate) {
    fs_ = sample_rate;

    // Initialize Filter Stage 1
    svf_l_.Init(fs_);
    svf_r_.Init(fs_);
    svf_l_.SetFreq(1000.0f);
    svf_l_.SetRes(0.5f);
    svf_l_.SetDrive(0.0f);
    svf_r_.SetFreq(1000.0f);
    svf_r_.SetRes(0.5f);
    svf_r_.SetDrive(0.0f);

    // Initialize Filter Stage 2 (for 24dB/oct slope)
    svf_l2_.Init(fs_);
    svf_r2_.Init(fs_);
    svf_l2_.SetFreq(1000.0f);
    svf_l2_.SetRes(0.5f);
    svf_l2_.SetDrive(0.0f);
    svf_r2_.SetFreq(1000.0f);
    svf_r2_.SetRes(0.5f);
    svf_r2_.SetDrive(0.0f);

    // Initialize Input LPF Stage 1 (2-pole anti-aliasing)
    input_lpf_l_.Init(fs_);
    input_lpf_r_.Init(fs_);
    input_lpf_l_.SetFreq(14000.0f); // Cut ultrasonic noise
    input_lpf_l_.SetRes(0.0f);
    input_lpf_r_.SetFreq(14000.0f);
    input_lpf_r_.SetRes(0.0f);

    // Initialize Input LPF Stage 2 (2-pole for 24dB/oct slope)
    input_lpf_l2_.Init(fs_);
    input_lpf_r2_.Init(fs_);
    input_lpf_l2_.SetFreq(14000.0f);
    input_lpf_l2_.SetRes(0.0f);
    input_lpf_r2_.SetFreq(14000.0f);
    input_lpf_r2_.SetRes(0.0f);

    // Initialize Drive
    drive_amount_ = 0.0f;
    freq_ = 1000.0f;
    res_ = 0.0f;
    drive_ = 0.0f;

    // Initialize Noise Gate
    env_follower_l_ = 0.0f;
    env_follower_r_ = 0.0f;

    // Initialize Oversampling History (4 samples for Hermite)
    for (int i = 0; i < 4; i++) {
      hist_l_[i] = 0.0f;
      hist_r_[i] = 0.0f;
    }
  }

  void Process(float in_l, float in_r, float *out_l, float *out_r) {
    // 0. Noise Gate (Downward Expander)
    // Simple envelope follower
    env_follower_l_ = 0.99f * env_follower_l_ + 0.01f * fabsf(in_l);
    env_follower_r_ = 0.99f * env_follower_r_ + 0.01f * fabsf(in_r);

    float gate_gain_l = 1.0f;
    float gate_gain_r = 1.0f;
    const float kThreshold = 0.002f; // ~ -54dB

    if (env_follower_l_ < kThreshold) {
      gate_gain_l = env_follower_l_ / kThreshold; // Soft knee expansion
      gate_gain_l *= gate_gain_l;                 // Square it for steeper curve
    }
    if (env_follower_r_ < kThreshold) {
      gate_gain_r = env_follower_r_ / kThreshold;
      gate_gain_r *= gate_gain_r;
    }

    // 0.5 Input LPF (2-pole anti-aliasing before drive)
    input_lpf_l_.Process(in_l);
    input_lpf_r_.Process(in_r);
    float stage1_l = input_lpf_l_.Low() * gate_gain_l;
    float stage1_r = input_lpf_r_.Low() * gate_gain_r;

    // Stage 2 for 24dB/oct slope
    input_lpf_l2_.Process(stage1_l);
    input_lpf_r2_.Process(stage1_r);
    float clean_l = input_lpf_l2_.Low();
    float clean_r = input_lpf_r2_.Low();

    // 1. Apply Drive (Pre-Filter) with 2x Hermite Oversampling
    // Gain staging: Boost input based on drive amount
    float drive_gain = 1.0f + (drive_amount_ * 16.0f);
    float dry_l = clean_l * drive_gain;
    float dry_r = clean_r * drive_gain;

    // Hermite Interpolation for upsampling
    // Generate intermediate sample using 4-point Hermite
    float dry_l_mid =
        HermiteInterpolate(hist_l_[0], hist_l_[1], hist_l_[2], dry_l, 0.5f);
    float dry_r_mid =
        HermiteInterpolate(hist_r_[0], hist_r_[1], hist_r_[2], dry_r, 0.5f);

    // Process both samples through drive
    float dist_l_mid = ApplyDrive(dry_l_mid);
    float dist_r_mid = ApplyDrive(dry_r_mid);
    float dist_l_curr = ApplyDrive(dry_l);
    float dist_r_curr = ApplyDrive(dry_r);

    // Decimation with weighted averaging (anti-aliasing)
    float driven_l = (dist_l_mid * 0.4f + dist_l_curr * 0.6f);
    float driven_r = (dist_r_mid * 0.4f + dist_r_curr * 0.6f);

    // Update history buffer
    hist_l_[0] = hist_l_[1];
    hist_l_[1] = hist_l_[2];
    hist_l_[2] = dry_l;

    hist_r_[0] = hist_r_[1];
    hist_r_[1] = hist_r_[2];
    hist_r_[2] = dry_r;

    // 2. Apply Filter (24dB/oct - 4 Pole)
    // Stereo Spread: Offset Right channel cutoff slightly for width
    float spread = 1.0f + (res_ * 0.05f); // Up to 5% spread

    // Stage 1
    svf_l_.SetFreq(freq_);
    svf_l_.SetRes(res_);
    svf_l_.SetDrive(drive_);

    svf_r_.SetFreq(freq_ * spread);
    svf_r_.SetRes(res_);
    svf_r_.SetDrive(drive_);

    // Stage 2
    svf_l2_.SetFreq(freq_);
    svf_l2_.SetRes(res_);
    svf_l2_.SetDrive(drive_);

    svf_r2_.SetFreq(freq_ * spread);
    svf_r2_.SetRes(res_);
    svf_r2_.SetDrive(drive_);

    // Process Stage 1
    svf_l_.Process(driven_l);
    svf_r_.Process(driven_r);

    // Process Stage 2 (Input is output of Stage 1)
    // We need to select the correct output from Stage 1 to feed Stage 2
    float l1_out, r1_out;
    if (filter_mode_ == FILTER_HP) {
      l1_out = svf_l_.High();
      r1_out = svf_r_.High();
    } else if (filter_mode_ == FILTER_BP) {
      l1_out = svf_l_.Band();
      r1_out = svf_r_.Band();
    } else { // LP
      l1_out = svf_l_.Low();
      r1_out = svf_r_.Low();
    }

    svf_l2_.Process(l1_out);
    svf_r2_.Process(r1_out);

    float l_filtered = 0.0f;
    float r_filtered = 0.0f;

    if (filter_mode_ == FILTER_HP) { // HP
      l_filtered = svf_l2_.High();
      r_filtered = svf_r2_.High();
    } else if (filter_mode_ == FILTER_BP) { // BP
      l_filtered = svf_l2_.Band();
      r_filtered = svf_r2_.Band();
    } else { // LP
      l_filtered = svf_l2_.Low();
      r_filtered = svf_r2_.Low();
    }

    // 3. Output Gain Compensation & Limiting
    // As drive increases, we attenuate output to maintain constant perceived
    // loudness.
    float comp_gain = 1.0f / sqrtf(drive_gain);

    // Apply compensation
    l_filtered *= comp_gain;
    r_filtered *= comp_gain;

    // Final Safety Limiter (Soft Clip)
    float final_l = tanhf(l_filtered);
    float final_r = tanhf(r_filtered);

    // NAN Check / Safety Recovery (Soluciona el "petado" reiniciando el filtro)
    if (isnan(final_l) || isinf(final_l) || isnan(final_r) || isinf(final_r)) {
      Init(fs_);
      final_l = 0.0f;
      final_r = 0.0f;
    }

    *out_l = final_l;
    *out_r = final_r;
  }

  void UpdateControls(DaisyLegio &hw) {
    // Knobs
    float k_cutoff = hw.controls[DaisyLegio::CONTROL_KNOB_TOP].Value();
    float k_res = hw.controls[DaisyLegio::CONTROL_KNOB_BOTTOM].Value();

    // Encoder Turn (Drive Amount)
    float inc = hw.encoder.Increment();
    drive_amount_ += inc * 0.05f; // 5% change per click
    drive_amount_ = fclamp(drive_amount_, 0.0f, 1.0f);

    // Switches
    int sw_drive = hw.sw[DaisyLegio::SW_LEFT].Read();
    int sw_filter = hw.sw[DaisyLegio::SW_RIGHT].Read();

    // Map Filter Mode (Inverted: 2=Top, 1=Mid, 0=Bot)
    if (sw_filter == 2)
      filter_mode_ = FILTER_HP;
    else if (sw_filter == 1)
      filter_mode_ = FILTER_BP;
    else
      filter_mode_ = FILTER_LP;

    // Map Drive Mode (Inverted: 2=Top, 1=Mid, 0=Bot)
    if (sw_drive == 2)
      drive_mode_ = DRIVE_WARM;
    else if (sw_drive == 1)
      drive_mode_ = DRIVE_HARD;
    else
      drive_mode_ = DRIVE_DESTROY;

    // Update DSP Parameters
    // Extended Range: 5Hz to 18kHz for deep sub-bass control
    float target_freq = fmap(k_cutoff, 5.0f, 18000.0f, Mapping::LOG);

    // Smooth parameters
    fonepole(freq_, target_freq, 0.05f);
    fonepole(res_, k_res, 0.05f);
    fonepole(drive_, drive_amount_, 0.05f);
  }

private:
  Svf svf_l_, svf_r_;
  Svf svf_l2_, svf_r2_;             // Second stage for 24dB/oct
  Svf input_lpf_l_, input_lpf_r_;   // Input LPF stage 1
  Svf input_lpf_l2_, input_lpf_r2_; // Input LPF stage 2 (2-pole)
  float fs_;
  float drive_amount_;
  float freq_, res_, drive_;
  float env_follower_l_, env_follower_r_; // For Noise Gate
  float hist_l_[4], hist_r_[4];           // Hermite interpolation history

  enum FilterMode { FILTER_HP, FILTER_BP, FILTER_LP } filter_mode_;
  enum DriveMode { DRIVE_WARM, DRIVE_HARD, DRIVE_DESTROY } drive_mode_;

  float ApplyDrive(float x) {
    switch (drive_mode_) {
    case DRIVE_WARM:
      return AsymmetricSoftClip(x);
    case DRIVE_HARD:
      return x / sqrtf(1.0f + (x * x));
    case DRIVE_DESTROY:
      return Wavefolder(x);
    default:
      return x;
    }
  }

  // Hermite interpolation for smooth upsampling
  float HermiteInterpolate(float xm1, float x0, float x1, float x2, float t) {
    float c0 = x0;
    float c1 = 0.5f * (x1 - xm1);
    float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
    float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
    return ((c3 * t + c2) * t + c1) * t + c0;
  }

  float AsymmetricSoftClip(float x) {
    // Smoother asymmetric clipping with gradual knee
    if (x > 1.5f)
      return 0.85f;
    if (x < -1.5f)
      return -0.45f;

    // Smooth transition using tanh-like curve
    float pos = x * 0.7f;
    float neg = x * 0.5f;
    return x > 0.0f ? pos + (x - pos) * expf(-x * x)
                    : neg + (x - neg) * expf(-x * x * 0.5f);
  }

  float Wavefolder(float x) {
    // Safety Clamp: Impide que entren valores locos que hagan explotar el
    // algoritmo
    if (x > 5.0f)
      x = 5.0f;
    if (x < -5.0f)
      x = -5.0f;

    // Multi-stage wavefolder for complex harmonics
    // Stage 1
    if (x > 1.0f)
      x = 2.0f - x;
    else if (x < -1.0f)
      x = -2.0f - x;

    // Stage 2 (with gain boost)
    x *= 1.5f;
    if (x > 1.0f)
      x = 2.0f - x;
    else if (x < -1.0f)
      x = -2.0f - x;

    // Stage 3 (subtle fold for complexity)
    x *= 1.2f;
    if (x > 1.0f)
      x = 2.0f - x;
    else if (x < -1.0f)
      x = -2.0f - x;

    return x * 0.7f; // Scale down to prevent clipping
  }
};
