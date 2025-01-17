/*
	Name:       DubiaRoachAutomatedColonyUtilizingLocalArduino.ino (D.R.A.C.U.L.A.)
	Created:	11/9/2020 12:19:12 AM
	Author:     Garrett Miller
	Notes:		This code heavily uses Editor Outlining as used in Visual Studio, with
				frequent use of #pragma region (Region Label/Name)\n ... \n ... \n #pragma endregion
				to collapse regions of code and comments into logical groupings.
				For those unfamiliar, Ctrl+M, Ctrl+L toggles all outlining open/closed
				while Ctrl+M, Ctrl+O collapses all outlining to closed. Handy for seeing related
				groups of code and temporarily "hiding" other code.
*/

///////////////////////////////////////////////////////////////////////////////////////////////////
// Includes
#include "BlunoShield.h"
#include "OLEDMenu.h"

//A simple swap macro, I may replace with a typesafe templated function instead...
#define Swap(_T, a, b)		{ _T t; t = a; a = b; b = t; }

#pragma region Local Derived Classes (The PC fans and the fogger)

/// <summary>
/// A class that represents a standard PC-case fan, possibly hooked to a PWM port,
/// derived from a SwitchDevice that can be turned on/off (and if PWM-enabled, PWM-ed)
/// </summary>
class PCFan : public SwitchDevice
{
private:
	PCFan();						//private default ctor, not acceptable outside this class
	PCFan(PCFan&& ctrArg);			//private move ctor, not acceptable outside this class
	PCFan(const PCFan& ctrArg);		//private copy ctor, not acceptable outside this class
public:
	/// <summary>
	/// The only proper way to create a fan device, according to this project's usage
	/// </summary>
	/// <param name="pin">The pin number this fan's control wire is linked to</param>
	/// <param name="fanName">The name of this particular fan (for menu use and input processing)</param>
	/// <param name="timerLength">The time that the fan will run before shutting off. Can always be changed later.</param>
	/// <remarks>
	/// Only allowable constructor to ensure proper initialization
	/// </remarks>
	PCFan(uint8_t pin, const char* fanName, unsigned long timerLength = 5 TMINUTES)
		: SwitchDevice(pin, fanName, timerLength) //Ensure we call the base class ctor
	{
		//any additional special code for a fan can be inserted here
	}

	//via SwitchDevice
	virtual uint8_t Initialize()
	{
		//Any pre-base-class-initialization code here

		//Base-class initialization, sets pinMode to OUTPUT and then either digitalWrite's or analogWrite's
		//the default state (off) to the pin
		uint8_t retCode = SwitchDevice::Initialize();

		//Any post-base-class-initialization here

		return retCode;
	}

	/// <summary>
	/// Process any input received via PlainProtocol
	/// </summary>
	/// <param name="input">A reference to the PlainProtocol object representing the input (and possible output)</param>
	/// <remarks>
	/// Derived from (and required by) IInputCommandListener
	/// </remarks>
	virtual void ProcessInput(PlainProtocol& input)
	{
		//Serial.println("HEREpp");
		if (input == F("Fan"))
		{
			String firstArg = input.readString();

			if (firstArg == F("?")) //if the command is <Fan>?; output the fan's name -- each fan SHOULD listen to this command meaning all names SHOULD be printed
			{
				input.write(GetName());
			}
			else //otherwise, let's see if the first argument names this fan
			{
				if (firstArg == GetName()) //if the first argument is exactly the name of this fan
				{
					Serial.println("Adjusting fan...");

					String targetState = input.readString();
					
					//set the string to all lowercase to easily respond to all forms of "On"/"Off"/"on"/"off"/"oN"/"ofF"/"oFf"/"oFF"
					//and because, for real, case should RARELY EVER matter *hard eye-roll*
					targetState.toLowerCase();

					if (targetState == F("on"))				// <Fan>[this fan name];on;
						TurnOn();
					else if (targetState == F("off"))		// <Fan>[this fan name];off;
						TurnOn(false);
					else //otherwise we don't accept whatever this string is that isn't "On" or "Off"
					{
						String output = F("*** INVALID ARGUMENT: Fan state = \"");
						output = output + targetState;	//print invalid command
						output = output + F("\"");
						input.write(output);
					}
				}
			}
		}
	}
};

/// <summary>
/// A class that represents a relay attached to the board that controls a fogger/humidifier
/// </summary>
/// <inheritdoc/>
class Fogger : public SwitchDevice
{
private:
	Fogger();						//private default ctor, not acceptable outside this class
	Fogger(Fogger&& ctrArg);		//private move ctor, not acceptable outside this class
	Fogger(const Fogger& ctrArg);	//private copy ctor, not acceptable outside this class
public:
	/// <summary>
	/// Create a fogger device
	/// </summary>
	/// <param name="pin">The pin number the fogger relay control wire is linked to</param>
	/// <param name="foggerName">The name of the fogger (for menu use and input processing)</param>
	/// <param name="maxRunTime">The maximum time the fogger will be allowed to run before being shut off for a while</param>
	/// <remarks>
	/// Only allowable constructor to ensure proper initialization
	/// </remarks>
	Fogger(uint8_t pin, const char* foggerName = "Fogger", unsigned long maxRunTime = 60 TMINUTES)
		: SwitchDevice(pin, foggerName, maxRunTime)	//don't forget the base class ctor
	{
		//any special code for the fogger can be inserted here
	}

	virtual uint8_t Initialize()
	{
		return SwitchDevice::Initialize();
	}

	/// <summary>
	/// Process any input received via PlainProtocol
	/// </summary>
	/// <param name="input">A reference to the PlainProtocol object representing the input (and possible output)</param>
	/// <remarks>
	/// Derived from (and required by) IInputCommandListener
	/// </remarks>
	virtual void ProcessInput(PlainProtocol& input)
	{
		if (input == F("Fogger"))
		{
			String arg = input.readString();

			if (arg == F("On"))
				TurnOn();
			else if (arg == F("Off"))
				TurnOn(false);
		}
	}
};

#pragma endregion

#pragma region Global Variables

///////////////////////////////////////////////////////////////////////////////////////////////////
// The ranged values that represent acceptable temperature and humidity ranges
// Ideal breeding, thriving temperature range: 90-95 deg F
///////////////////////////////////////////////////////////////////////////////////////////////////
// Minimum (non-crippling) survivable temperature: 40-ish deg F?
// Minimum "comfortable (?)" temperature: 60 deg F
//											name			inital		min			max
RangedValue8 tempMin = RangedValue8("Temp Min", 90, 60, 95);	//deg F
RangedValue8 tempMax = RangedValue8("Temp Max", 95, 70, 110);	//deg F

///////////////////////////////////////////////////////////////////////////////////////////////////
// Ideal breeding, thriving humidity range: 40-60%
// Minimum (non-crippling) survivable humidity: 20-30% ??
RangedValue8 humMin = RangedValue8("Hum Min", 40, 30, 60);	//%
RangedValue8 humMax = RangedValue8("Hum Max", 60, 40, 70);	//%

//The intake fan
PCFan fanIntake(24, "Intake Fan");
//The exhaust fan
PCFan fanExhaust(25, "Exhaust Fan");

//The fogger
Fogger fogger(22);

#pragma endregion

#pragma region Menu System

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
// The menu options on the "Ranges" page that allows adjustment of min/max temperature and humidity
MenuOption rangesPageOptions[] =
{
	MenuOption("Temp Min",	tempMin),	//The name to display on-screen and then a reference to the variable to change, in this case, the RangedValue8 defined above called tempMin
	MenuOption("Temp Max",	tempMax),
	MenuOption("Hum Min",	humMin),
	MenuOption("Hum Max",	humMax),
};

//The menu page for the min/max temp and humidity options
OLEDPage rangesPage("Range Settings", MenuOptionCount(rangesPageOptions), rangesPageOptions);

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
// The main menu page options
MenuOption mainPageOptions[] =
{
	MenuOption("Set Ranges",		&rangesPage),		//The name to display on-screen as the selectable choice, and then a pointer to the OLEDPage this links to when selected
	MenuOption("Shield Settings",	&mainSettingsPage)
};

// The main menu page that is activated when the user
// presses the joystick and "exits" the default screen mode.
OLEDPage mainPage("Main Menu", MenuOptionCount(mainPageOptions), mainPageOptions);

//////////////////////////////////////////////////////////////////////////////
// The whole "singular" menu system for this sketch. THERE SHOULD ONLY EVER BE ONE PER SKETCH
OLEDMenu menu(mainPage, blunoShield.GetOLED());

#pragma endregion

#pragma region Callbacks

///////////////////////////////////////////////////////////////////////////////////////////////////
/// <summary>
/// The callback function for the Bluno shield OLED screen to draw what you want (the menu system).
/// This is necessary because drawing to the OLED can only be done at a very specific point in the
/// code, namely the oled.firstPage()/oled.nextPage() loop found in BlunoShield::UpdateOLED(), that
/// is called only ONCE PER LOOP()
/// </summary>
/// <param name="oled">A reference to the Bluno shield OLED screen</param>
void drawCallback(OLED& oled)
{
	//Just in case the menu system's pointer to the OLED is NULL, set it
	if (!menu.pOled)
		menu.pOled = &oled;

	menu.Draw();
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/// <summary>
/// The callback function for the bluno shield PlainProtocol (technically not the shield but an actual DFRobot Bluno-board)
/// </summary>
/// <param name="input">The PlainProtocol object (reference)</param>
void processInputCallback(PlainProtocol& input)
{
	//if (input.available())
	{
		//Serial.println("HERE");
		//do your own BLE processing if needed
		//fanIntake.ProcessInput(input);
		//fanExhaust.ProcessInput(input);
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
			blunoShield.GetRelay().TurnOn();

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
				blunoShield.GetRelay().TurnOn(false);

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
			fogger.TurnOn();

		//Turn the fans off, keeping as much moisture as possible
		if (fanExhaust.IsOn())
			fanExhaust.TurnOn(false);
		if (fanIntake.IsOn())
			fanIntake.TurnOn(false);
	}
	else //otherwise, if it's wetter and more humid than the max humidity
		if (blunoShield.humidity > humMax)
		{
			//Turn the fogger off, if it isn't already
			if (fogger.IsOn())
				fogger.TurnOn(false);

			//Turn the fans on, if they aren't already, to move as much moisture as possible
			if (!fanExhaust.IsOn())
				fanExhaust.TurnOn();
			if (!fanIntake.IsOn())
				fanIntake.TurnOn();
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
	//if the Bluno Accessory Shield screen draw mode is set to custom, indicating WE (end-user, here, in the main Arduino sketch) tell it what to draw,
	//update the menu system (which essentially handles input and drawing the appropriate page).
	if (blunoShield.GetDrawMode() == BlunoShield::eDM_Custom)
		menu.Update();

	//otherwise, if we are in the default, initial (or "root") screen of the menu system (defaulted to display temperature and humidity),
	//AND then the joystick is pushed, set the Bluno Shield draw mode to custom, so WE can draw what we want, (which will be the actual menu system and pages)
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
/// Update the fans, the fogger, and the Bluno shield itself
/// </summary>
void UpdateObjects()
{
	//Update the intake and exhaust fans (updates any internal timers if necessary)
	fanIntake.Update();
	fanExhaust.Update();

	//Update the fogger for the same reason
	fogger.Update();

	//Update the bluno shield itself
	blunoShield.Update();
}

#pragma endregion

void setup()
{
	//Begin serial
	Serial.begin(115200);

	//Wait for Serial to initialize
	while (!Serial);

	//Initialize Bluno shield
	blunoShield.Init();

	//Set Bluno shield callbacks for drawing and input
	blunoShield.SetDrawCallback(drawCallback);
	blunoShield.SetInputCallback(processInputCallback);

	//Set the Bluno shield to do internal (default) PlainProtocol processing, but also our external, callback function too (processInputCallback(PlainProtocol& input))
	blunoShield.SetInputProcessingMode(BlunoShield::eIPM_Both);

	//Init menu system with reference to the Bluno shield
	menu.Init(blunoShield);

	//Initialize Bluno shield's built-in menu pages
	blunoShield.InitMenuPages(menu);

	//Initialize local menu pages
	rangesPage.Init(menu);
	mainPage.Init(menu);

	//heater initialization is built into the shield via the built-in relay,
	//but we still want to set a safety timer as a backup (at worst) for the
	//heating pad's built-in 2 hour timer, and (at best), the main timer that
	//the heater is scheduled to
	blunoShield.GetRelay().SetTimerPeriod(2 THOURS);

	//Initialize fogger
	fogger.Initialize();

	//Initialize both fans
	fanIntake.Initialize();
	fanExhaust.Initialize();
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