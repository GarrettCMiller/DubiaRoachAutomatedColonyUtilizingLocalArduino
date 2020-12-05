/*
    Name:       DubiaRoachAutomatedColonyUtilizingLocalArduino.ino (D.R.A.C.U.L.A.)
    Created:	11/9/2020 12:19:12 AM
    Author:     Garrett Miller
*/

#include "BlunoOLED.h"
#include "Arduino.h"
#include "BlunoShield.h"
#include "OLEDMenu.h"

#pragma region Helper Macros (That Should REALLY Be Defined Elsewhere)

#define Swap(_T, a, b)		{ _T t; t = a; a = b; b = t; }
//#define Swap(a, b)			{ type(a) t; t = a; a = b; b = t;}

#pragma endregion

#pragma region Helper Classes

//////////////////////////////
//fogger
class Fogger : public SwitchDevice, public IInputCommandListener
{
public:
	Fogger(uint8_t pin, const char* foggerName = "Fogger")
		: SwitchDevice(pin, foggerName)
	{
		//any special code for the fogger can be inserted here
	}

	virtual void ProcessInput(PlainProtocol& input)
	{
		if (input == F("Fogger"))
		{
			String arg = input.readString();

			if (arg == F("On"))
				SetOn();
			else if (arg == F("Off"))
				SetOn(false);
		}
	}
};

//////////////////////////////
//fans
class PCFan : public SwitchDevice
{
public:
	PCFan(uint8_t pin, const char* fanName)
		: SwitchDevice(pin, fanName)
	{
		//any special code for a fan can be inserted here
	}

	virtual void ProcessInput(PlainProtocol& input)
	{
		//Serial.println("HEREpp");
		if (input == F("Fan"))
		{
			//String firstArg = input.readString();

			//if (firstArg == F("?"))
			//{
			//	input.write(name);
			//}
			//else
			//{
				//if (firstArg == name) //if the first argument is exactly the name of this fan
			{
				Serial.println(F("Adjusting fan..."));

				String targetState = input.readString();

				if (targetState == F("On"))
					SetOn();
				else if (targetState == F("Off"))
					SetOn(false);
				else
				{
					String output = F("*** INVALID ARGUMENT: Fan state = \"");
					output = output + targetState;
					output = output + F("\"");
					input.write(output);
				}
			}
			//}
		}
	}
};

//////////////////////////////
//camera
// ??

//////////////////////////////
//light
// ??

#pragma endregion

#pragma region Global Variables

//Create a Bluno shield object (with default parameters for now)
BlunoShield blunoShield;

///////////////////////////////////////////////////////////////////////////////////////////////////
// The ranged values that represent acceptable temperature and humidity ranges
// Ideal breeding, thriving temperature range: 90-95 deg F
// Minimum (non-crippling) survivable temperature: 40-ish deg F?
// Minimum "comfortable (?)" temperature: 60 deg F
//											name			inital		min			max
RangedValue8 tempMin = RangedValue8(		"Temp Min",		90,			60,			95);	//deg F
RangedValue8 tempMax = RangedValue8(		"Temp Max",		95,			70,			110);	//deg F
///////////////////////////////////////////////////////////////////////////////////////////////////
// Ideal breeding, thriving humidity range: 40-60%
// Minimum (non-crippling) survivable humidity: 20-30% ??
RangedValue8 humMin = RangedValue8(			"Hum Min",		40,			30,			60);	//%
RangedValue8 humMax = RangedValue8(			"Hum Max",		60,			40,			70);	//%

//The intake fan
PCFan fanIntake(24, "Intake Fan");
//The exhaust fan
PCFan fanExhaust(25, "Exhaust Fan");

//The fogger
Fogger fogger(22);

#pragma endregion

#pragma region Menu System

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
// The menu allowing the min/max temp/humidity ranges to be adjusted
//
MenuOption rangesPageOptions[] =
{
	MenuOption("Temp Min", tempMin),
	MenuOption("Temp Max", tempMax),
	MenuOption("Hum Min", humMin),
	MenuOption("Hum Max", humMax),
};

OLEDPage rangesPage("Range Settings", MenuOptionCount(rangesPageOptions), rangesPageOptions);

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
// The main menu and its options
//
MenuOption mainPageOptions[] =
{
	MenuOption("Set T/H Ranges", &rangesPage),
	MenuOption("Shield Settings", &mainSettingsPage)
};

OLEDPage mainPage("Main Menu", MenuOptionCount(mainPageOptions), mainPageOptions);

//////////////////////////////////////////////////////////////////////////////
// A singleton object that represents the entire menu system
//				initial page,		OLED screen reference
OLEDMenu menu(	mainPage,			blunoShield.GetOLED());

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

#pragma endregion

#pragma region Callbacks

/// <summary>
/// The callback function for the bluno shield OLED screen to draw what you want
/// </summary>
/// <param name="oled">A reference to the bluno shield OLED screen</param>
void drawCallback(OLED& oled)
{
	//Just in case the menu system's pointer to the OLED is NULL, set it
	if (!menu.pOled)
		menu.pOled = &oled;

	menu.Draw();
}

/// <summary>
/// The callback function for the bluno shield PlainProtocol (technically not the shield but an actual bluno-board)
/// </summary>
/// <param name="input">The PlainProtocol object (reference)</param>
void processInputCallback(PlainProtocol& input)
{
	//if (input.available())
	{
		//Serial.println("HERE");
		//do your own BLE processing if needed
		fanIntake.ProcessInput(input);
		fanExhaust.ProcessInput(input);
	}
}

#pragma endregion

#pragma region Main Sub Routines

/// <summary>
/// Set the heater (and other related things) on or off, based on temperature
/// </summary>
void ProcessTemperature()
{
	//Get reference to bluno shield LED
	LED_RGB& led = blunoShield.GetLED();

	//if it's colder than the minimum temperature
	if (blunoShield.temperature < tempMin)
	{
		//Turn the heater relay on if it isn't already
		if (!blunoShield.GetRelay().IsOn())
			blunoShield.GetRelay().SetOn();

		//Set the LED to the "solid color" mode, if it isn't already
		if (led.eLEDState != LED_RGB::eLS_OnSolid)
			led.eLEDState = LED_RGB::eLS_OnSolid;

		//Set the color to red if it isn't already
		if (!led.colorEquals(255, 0, 0))
			led.setColor(255, 0, 0);

		//Set the brightness of the LED relative to the temperature, brighter when it's colder
		//and further away from the minimum. As it approaches the minimum (from "underneath"), it gradually dims.
		led.SetBrightness(255 - map(blunoShield.temperature, 60, 90, 0, 255));
	}
	else //otherwise, if it's hotter than the maximum temperature
		if (blunoShield.temperature > tempMax)
	{
		//Set the heater relay off, if it's on
		if (blunoShield.GetRelay().IsOn())
			blunoShield.GetRelay().SetOn(false);

		//Set the LED back to the "blink" mode, if it isn't already
		if (led.eLEDState != LED_RGB::eLS_OnBlink)
			led.eLEDState = LED_RGB::eLS_OnBlink;

		//Set the LED color back to green, if it isn't already
		if (!led.colorEquals(0, 255, 0))
			led.setColor(0, 255, 0);

		//Set the LED brightness back to the default (minimum visible) brightness, if it isn't already
		if (led.GetBrightness() != 1)
			blunoShield.GetLED().SetBrightness(1);
	}
}

/// <summary>
/// Set the fogger (and other related things) on or off, based on humidity
/// </summary>
void ProcessHumidity()
{
	//if it's dryer than the minimum humidity
	if (blunoShield.humidity < humMin)
	{
		//Turn the fogger on, if it isn't already
		if (!fogger.IsOn())
			fogger.SetOn();

		//Turn the fans off, keeping as much heat and moisture as possible
		/*if (fanExhaust.IsOn())
			fanExhaust.SetOn(false);
		if (fanIntake.IsOn())
			fanIntake.SetOn(false);*/
	}
	else //otherwise, if it's wetter and more humid than the max humidity
		if (blunoShield.humidity > humMax)
	{
		//Turn the fogger off, if it isn't already
		if (fogger.IsOn())
			fogger.SetOn(false);

		//Turn the fans on, if they aren't already, to move as much heat and moisture as possible
		/*if (!fanExhaust.IsOn())
			fanExhaust.SetOn();
		if (!fanIntake.IsOn())
			fanIntake.SetOn();*/
	}
}

/// <summary>
/// Set the Bluno shield's default message text, based on the status of both the fogger and heater
/// </summary>
void SetBlunoMessageText()
{
	//If the heater is on
	if (blunoShield.GetRelay().IsOn())
	{
		//if the fogger is also on
		if (fogger.IsOn())
			blunoShield.SetMessageText("Heat: ON\tFog: ON");
		else //otherwise, if the fogger is off
			blunoShield.SetMessageText("Heat: ON\tFog: OFF");
	}
	else //otherwise, if the heater is off
	{
		//if the fogger is on
		if (fogger.IsOn())
			blunoShield.SetMessageText("Heat: OFF\tFog: ON");
		else //otherwise, if the fogger is also off
			blunoShield.SetMessageText("Heat: OFF\tFog: OFF");
	}
}

/// <summary>
/// Update the menu system
/// </summary>
void UpdateMenuSystem()
{
	//if the Bluno Accessory Shield screen draw mode is set to custom, indicating WE tell it what to draw,
	//update the menu system (which essentially handles input and drawing the appropriate page).
	if (blunoShield.GetDrawMode() == BlunoShield::eDM_Custom)
		menu.Update();

	//otherwise, if we are in the default "root" screen of the menu system (defaulted to the temperature and humidity display),
	//AND then the joystick is pushed, set the Bluno Shield draw mode to custom, so WE can draw what we want, (which will be the menu system)
	else if (blunoShield.GetJoystickValue() == eJoy_Push && blunoShield.GetDrawMode() == BlunoShield::eDM_TempAndHumidity)
		blunoShield.SetDrawMode(BlunoShield::eDM_Custom);
}

/// <summary>
/// Send data over the serial port (every second, by default) to any device that may be listening
/// </summary>
void SendData()
{
	//Create a local, static (persistent) timer, set to 1000 ms (or 1 second)
	static ArduinoTimer serialTimer(1 TSECONDS, true);
	if (serialTimer.IsReady())	//The timer auto-resets by default if IsReady() is true
	{
		String send = "T:";
		send = send + blunoShield.temperature;
		Serial.println(send);
	}
}

/// <summary>
/// Update the fans and the Bluno shield itself
/// </summary>
void UpdateObjects()
{
	//Update the intake and exhaust fans
	fanIntake.Update();
	fanExhaust.Update();

	//Update the fogger
	fogger.Update();

	//Update the bluno shield itself
	blunoShield.Update();
}

#pragma endregion

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void setup()
{
	//Begin serial
	Serial.begin(115200);

	//Wait for Serial to initialize
	while (!Serial);

	//Initialize Bluno shield
	blunoShield.Init();

	//Set Bluno shield callbacks for drawing and input
	blunoShield.currentDrawCallback = drawCallback;
	blunoShield.currentInputCallback = processInputCallback;

	//Set the Bluno shield to do internal (default) PlainProtocol processing, but also our external, callback function too (processInputCallback(PlainProtocol& input))
	blunoShield.SetInputProcessingMode(BlunoShield::eIPM_Both);

	//Init menu system with reference to the Bluno shield
	menu.Init(blunoShield);

	//Initialize Bluno shield's built-in menu pages
	blunoShield.InitMenuPages(menu);

	//Initialize local menu pages
	rangesPage.Init(menu);
	mainPage.Init(menu);

	//Initialize fogger
	//fogger.Init();

	//Initialize both fans
	//fanIntake.Init();
	//fanExhaust.Init();
}

void loop()
{
	//Enforce that the minimum temp is the smaller of the two between it and the maximum
	if (tempMin > tempMax)
		Swap(RangedValue8, tempMin, tempMax);

	//Enforce that the minimum humidity is the smaller of the two between it and the maximum
	if (humMin > humMax)
		Swap(RangedValue8, humMin, humMax);

	UpdateMenuSystem();
	
	ProcessTemperature();

	ProcessHumidity();

	SetBlunoMessageText();

	SendData();

	UpdateObjects();
}