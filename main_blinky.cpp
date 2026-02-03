#include "daisy_legio.h"

using namespace daisy;

DaisyLegio hw;

int main(void) {
  hw.Init();

  bool state = false;

  while (1) {
    // Blink both LEDs
    // SetLed(idx, r, g, b)
    if (state) {
      hw.SetLed(DaisyLegio::LED_LEFT, 1.0f, 0.0f, 0.0f);  // Red
      hw.SetLed(DaisyLegio::LED_RIGHT, 0.0f, 1.0f, 0.0f); // Green
    } else {
      hw.SetLed(DaisyLegio::LED_LEFT, 0.0f, 0.0f, 1.0f);  // Blue
      hw.SetLed(DaisyLegio::LED_RIGHT, 1.0f, 1.0f, 1.0f); // White
    }

    hw.UpdateLeds();

    System::Delay(500);
    state = !state;
  }
}
