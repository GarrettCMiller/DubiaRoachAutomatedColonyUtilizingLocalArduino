// LEDMatrix64x32.h

#ifndef _LEDMATRIX64X32_h
#define _LEDMATRIX64X32_h

#if defined(ARDUINO) && ARDUINO >= 100
	#include "arduino.h"
#else
	#include "WProgram.h"
#endif

#include <RGBmatrixPanel.h>

// Most of the signal pins are configurable, but the CLK pin has some
// special constraints.  On 8-bit AVR boards it must be on PORTB...
// Pin 8 works on the Arduino Uno & compatibles (e.g. Adafruit Metro),
// Pin 11 works on the Arduino Mega.  On 32-bit SAMD boards it must be
// on the same PORT as the RGB data pins (D2-D7)...
// Pin 8 works on the Adafruit Metro M0 or Arduino Zero,
// Pin A4 works on the Adafruit Metro M4 (if using the Adafruit RGB
// Matrix Shield, cut trace between CLK pads and run a wire to A4).

#define CLK  50   // USE THIS ON ADAFRUIT METRO M0, etc.
//#define CLK A4 // USE THIS ON METRO M4 (not M0)
//#define CLK 11 // USE THIS ON ARDUINO MEGA
#define OE   34
#define LAT 35
#define A   A8
#define B   A9
#define C   A10
#define D   A11

class LEDMatrix64x32 : public RGBmatrixPanel
{
public:
	LEDMatrix64x32()
		: RGBmatrixPanel(A, B, C, D, CLK, LAT, OE, false, 64)
	{

	}

	void Init()
	{
		begin();
	}
};

//RGBmatrixPanel matrix(A, B, C, D, CLK, LAT, OE, false, 64);
//extern RGBmatrixPanel matrix;

#endif

