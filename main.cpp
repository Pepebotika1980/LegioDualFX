#include "ModeFilterDrive.h"
#include "ModeShepardTone.h"
#include "ModeShimmerReverb.h"
#include "ModeSpaceEcho.h"
#include "daisy_legio.h"
#include "daisysp.h"

using namespace daisy;
using namespace daisysp;

#include <new>

DaisyLegio hw;
ModeFilterDrive mode_filter;

// Allocate memory for ModeSpaceEcho in SDRAM
DSY_SDRAM_BSS char mode_echo_mem[sizeof(ModeSpaceEcho)];
ModeSpaceEcho *mode_echo;

// Allocate memory for ModeShimmerReverb in SDRAM
DSY_SDRAM_BSS char mode_shimmer_mem[sizeof(ModeShimmerReverb)];
ModeShimmerReverb *mode_shimmer;

// Allocate memory for ModeShepardTone in SDRAM
DSY_SDRAM_BSS char mode_shepard_mem[sizeof(ModeShepardTone)];
ModeShepardTone *mode_shepard;

enum FxMode { MODE_FILTER, MODE_ECHO, MODE_SHIMMER, MODE_SHEPARD };
FxMode current_mode = MODE_FILTER;

// Global Crossfade Variables
float crossfade_vol = 1.0f;
bool switching_mode = false;
int next_mode = -1;

// Global Limiters
Limiter lim_l, lim_r;

// Stereo Widening
float stereo_width = 0.5f; // 0.0 = mono, 0.5 = normal, 1.0 = wide

// Audio Processing Constants
static constexpr float kCrossfadeSpeed = 0.006f;
static constexpr float kCrossfadeThreshold = 0.001f;
static constexpr float kStereoWidthScale = 1.0f;

// Mode-specific input gains
static constexpr float kFilterInputGain = 1.0f;
static constexpr float kEchoInputGain = 1.2f;
static constexpr float kShimmerInputGain = 1.0f;
static constexpr float kShepardInputGain = 0.0f; // Generator, ignore input

// Mode-specific limiter pregains
static constexpr float kFilterLimiterGain = 1.3f;
static constexpr float kEchoLimiterGain = 1.5f;
static constexpr float kShimmerLimiterGain = 1.2f;
static constexpr float kShepardLimiterGain = 1.4f;

void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out,
                   size_t size) {
  hw.ProcessAnalogControls();

  // Handle Crossfade Logic with Exponential Curves
  if (switching_mode) {
    crossfade_vol -= kCrossfadeSpeed;
    crossfade_vol = crossfade_vol * crossfade_vol; // Exponential fade out
    if (crossfade_vol <= kCrossfadeThreshold) {
      crossfade_vol = 0.0f;
      current_mode = (FxMode)next_mode; // Switch mode when silent
      switching_mode = false;           // Start fading in
    }
  } else {
    if (crossfade_vol < 1.0f) {
      crossfade_vol += kCrossfadeSpeed;
      crossfade_vol = sqrtf(crossfade_vol); // Exponential fade in
      if (crossfade_vol > 1.0f)
        crossfade_vol = 1.0f;
    }
  }

  // Update Controls based on Mode (once per buffer)
  if (current_mode == MODE_FILTER) {
    mode_filter.UpdateControls(hw);
  } else if (current_mode == MODE_ECHO) {
    mode_echo->UpdateControls(hw);
  } else if (current_mode == MODE_SHIMMER) {
    mode_shimmer->UpdateControls(hw);
  } else {
    mode_shepard->UpdateControls(hw);
  }

  // Get mode-specific parameters (moved outside loop for efficiency)
  float input_gain = kFilterInputGain;
  float limiter_pregain = kFilterLimiterGain;

  if (current_mode == MODE_FILTER) {
    input_gain = kFilterInputGain;
    limiter_pregain = kFilterLimiterGain;
  } else if (current_mode == MODE_ECHO) {
    input_gain = kEchoInputGain;
    limiter_pregain = kEchoLimiterGain;
  } else if (current_mode == MODE_SHIMMER) {
    input_gain = kShimmerInputGain;
    limiter_pregain = kShimmerLimiterGain;
  } else {
    input_gain = kShepardInputGain;
    limiter_pregain = kShepardLimiterGain;
  }

  // Process Audio Block
  for (size_t i = 0; i < size; i++) {
    float in_l = in[0][i] * input_gain;
    float in_r = in[1][i] * input_gain;

    float out_l, out_r;

    // Process based on current mode
    if (current_mode == MODE_FILTER) {
      mode_filter.Process(in_l, in_r, &out_l, &out_r);
    } else if (current_mode == MODE_ECHO) {
      mode_echo->Process(in_l, in_r, &out_l, &out_r);
    } else if (current_mode == MODE_SHIMMER) {
      mode_shimmer->Process(in_l, in_r, &out_l, &out_r);
    } else {
      mode_shepard->Process(in_l, in_r, &out_l, &out_r);
    }

    // Stereo Widening (Mid/Side Processing)
    float mid = (out_l + out_r) * 0.5f;
    float side = (out_l - out_r) * 0.5f;

    // Apply width control (0.0 = mono, 0.5 = normal, 1.0 = wide)
    side *= (kStereoWidthScale + stereo_width);

    out_l = mid + side;
    out_r = mid - side;

    // Apply Crossfade Volume
    out[0][i] = out_l * crossfade_vol;
    out[1][i] = out_r * crossfade_vol;
  }

  // Adaptive Output Limiters (applied once per buffer)
  lim_l.ProcessBlock(out[0], size, limiter_pregain);
  lim_r.ProcessBlock(out[1], size, limiter_pregain);
}

int main(void) {
  hw.Init();
  hw.StartAdc();

  float sample_rate = hw.AudioSampleRate();

  // Init Limiters
  lim_l.Init();
  lim_r.Init();

  // Init Modes
  mode_filter.Init(sample_rate);

  // Construct ModeSpaceEcho in SDRAM memory
  mode_echo = new (mode_echo_mem) ModeSpaceEcho();
  mode_echo->Init(sample_rate);

  // Construct ModeShimmerReverb in SDRAM memory
  mode_shimmer = new (mode_shimmer_mem) ModeShimmerReverb();
  mode_shimmer->Init(sample_rate);

  // Construct ModeShepardTone in SDRAM memory
  mode_shepard = new (mode_shepard_mem) ModeShepardTone();
  mode_shepard->Init(sample_rate);

  hw.StartAudio(AudioCallback);

  while (1) {
    hw.ProcessDigitalControls();

    // Handle Mode Switching (Encoder Press)
    if (hw.encoder.RisingEdge()) {
      if (!switching_mode) {   // Only switch if not already switching
        switching_mode = true; // Start fade out

        // Simple, robust cycling logic
        int next_val = (int)current_mode + 1;
        if (next_val > 3) {
          next_val = 0; // Wrap to start (MODE_FILTER)
        }
        next_mode = next_val;
      }
    }

    // Update LEDs
    int mode_to_display = switching_mode ? next_mode : current_mode;

    if (mode_to_display == MODE_FILTER) {
      hw.SetLed(DaisyLegio::LED_LEFT, 1.0f, 0.0f, 0.0f); // RED
      hw.SetLed(DaisyLegio::LED_RIGHT, 1.0f, 0.0f, 0.0f);
    } else if (mode_to_display == MODE_ECHO) {
      hw.SetLed(DaisyLegio::LED_LEFT, 0.0f, 1.0f, 0.0f); // GREEN
      hw.SetLed(DaisyLegio::LED_RIGHT, 0.0f, 1.0f, 0.0f);
    } else if (mode_to_display == MODE_SHIMMER) {
      hw.SetLed(DaisyLegio::LED_LEFT, 1.0f, 1.0f, 1.0f); // WHITE
      hw.SetLed(DaisyLegio::LED_RIGHT, 1.0f, 1.0f, 1.0f);
    } else {
      hw.SetLed(DaisyLegio::LED_LEFT, 0.0f, 1.0f, 1.0f); // CYAN
      hw.SetLed(DaisyLegio::LED_RIGHT, 0.0f, 1.0f, 1.0f);
    }
    hw.UpdateLeds();

    System::Delay(1);
  }
}
