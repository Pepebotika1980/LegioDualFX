#pragma once
#include "daisy_legio.h"
#include "daisysp.h"

using namespace daisy;
using namespace daisysp;

// Helper: Delay-based Allpass Filter
class ReverbAllpass {
public:
  void Init(float *buffer, int size) {
    buf_ = buffer;
    max_size_ = size;
    write_pos_ = 0;
    for (int i = 0; i < max_size_; i++) {
      buf_[i] = 0.0f;
    }
  }

  float Process(float in, float coeff) {
    float buf_out = buf_[write_pos_];
    float out = -in + buf_out;
    buf_[write_pos_] = in + (buf_out * coeff);

    write_pos_++;
    if (write_pos_ >= max_size_)
      write_pos_ = 0;

    return out;
  }

private:
  float *buf_;
  int max_size_;
  int write_pos_;
};

class PlateReverb {
public:
  void Init(float sample_rate) {
    fs_ = sample_rate;

    // Initialize Input Diffusion Allpasses
    ap1_.Init(ap1_buf_, 142);
    ap2_.Init(ap2_buf_, 107);
    ap3_.Init(ap3_buf_, 379);
    ap4_.Init(ap4_buf_, 277);

    // Initialize Tank Left
    del_l_.Init();
    del_l_.SetDelay(4453.0f); // Base delay
    ap5_.Init(ap5_buf_, 1800);
    ap6_.Init(ap6_buf_, 2656);
    tank_l_end_.Init();
    tank_l_end_.SetDelay(3720.0f);

    // Initialize Tank Right
    del_r_.Init();
    del_r_.SetDelay(4217.0f); // Base delay
    ap7_.Init(ap7_buf_, 2656);
    ap8_.Init(ap8_buf_, 1800);
    tank_r_end_.Init();
    tank_r_end_.SetDelay(3163.0f);

    // LFO for modulation
    lfo_.Init(fs_);
    lfo_.SetWaveform(Oscillator::WAVE_SIN);
    lfo_.SetFreq(1.0f); // Slow modulation
    lfo_.SetAmp(10.0f); // +/- 10 samples

    decay_ = 0.5f;
    damping_ = 0.005f; // Lowpass coefficient
    lp_l_ = 0.0f;
    lp_r_ = 0.0f;
    tank_l_out_ = 0.0f;
    tank_r_out_ = 0.0f;
  }

  void Process(float in_l, float in_r, float *out_l, float *out_r) {
    // Mono-sum input for diffusion (simplification)
    float in = (in_l + in_r) * 0.5f;

    // Input Diffusion
    float t = ap1_.Process(in, 0.75f);
    t = ap2_.Process(t, 0.75f);
    t = ap3_.Process(t, 0.625f);
    t = ap4_.Process(t, 0.625f);

    // Cross-coupling feedback with Safety Clip
    // Prevent explosion if decay is high or input is hot
    float tank_in_l = t + (tank_r_out_ * decay_);
    float tank_in_r = t + (tank_l_out_ * decay_);

    tank_in_l = fclamp(tank_in_l, -4.0f, 4.0f);
    tank_in_r = fclamp(tank_in_r, -4.0f, 4.0f);

    // Modulation
    float mod = lfo_.Process();

    // Tank Left
    // Modulated Delay
    del_l_.Write(tank_in_l);
    float d_l = del_l_.Read(4453.0f + mod);

    // Damping (Lowpass)
    lp_l_ = (d_l * (1.0f - damping_)) + (lp_l_ * damping_);

    // Allpasses
    float a_l = ap5_.Process(lp_l_, 0.5f);
    a_l = ap6_.Process(a_l, 0.5f);

    // End Delay
    tank_l_end_.Write(a_l);
    tank_l_out_ = tank_l_end_.Read(3720.0f);

    // Tank Right
    // Modulated Delay
    del_r_.Write(tank_in_r);
    float d_r = del_r_.Read(4217.0f - mod); // Anti-phase mod

    // Damping
    lp_r_ = (d_r * (1.0f - damping_)) + (lp_r_ * damping_);

    // Allpasses
    float a_r = ap7_.Process(lp_r_, 0.5f);
    a_r = ap8_.Process(a_r, 0.5f);

    // End Delay
    tank_r_end_.Write(a_r);
    tank_r_out_ = tank_r_end_.Read(3163.0f);

    // Output Taps (Simplified for safety and clarity)
    // Summing multiple taps creates the dense plate sound
    // But for now, let's stick to the main tank outputs + early reflections
    *out_l = tank_l_out_ + d_l - d_r;
    *out_r = tank_r_out_ + d_r - d_l;
  }

  void SetDecay(float decay) {
    decay_ = fclamp(decay, 0.0f, 0.99f); // Don't explode
  }

  void SetDamping(float damping) {
    // 0.0 = no damping (bright), 1.0 = full damping (dark)
    damping_ = fclamp(damping, 0.0f, 1.0f);
  }

private:
  float fs_;
  float decay_;
  float damping_;
  float lp_l_, lp_r_;
  float tank_l_out_, tank_r_out_;

  Oscillator lfo_;

  // Diffusion Allpasses
  ReverbAllpass ap1_, ap2_, ap3_, ap4_;
  float ap1_buf_[142];
  float ap2_buf_[107];
  float ap3_buf_[379];
  float ap4_buf_[277];

  // Tank Left
  DelayLine<float, 6000> del_l_;
  ReverbAllpass ap5_, ap6_;
  float ap5_buf_[1800];
  float ap6_buf_[2656];
  DelayLine<float, 4000> tank_l_end_;

  // Tank Right
  DelayLine<float, 6000> del_r_;
  ReverbAllpass ap7_, ap8_;
  float ap7_buf_[2656];
  float ap8_buf_[1800];
  DelayLine<float, 4000> tank_r_end_;
};
