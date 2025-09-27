#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <NTC_Thermistor.h> // Need to download manually cuz the library manager one isn't patched
#include "Utility.h"

#define NullFunc []() {}

enum class Status
{
	OFF,
	ON,
	AUTO_CLOSED
};

constexpr uint8 HEATER_MOSFET_PIN = 3;

constexpr uint8 BUTTON_NEXT_PIN = 4;
constexpr uint8 BUTTON_A_PIN = 5;
constexpr uint8 BUTTON_B_PIN = 6;

constexpr uint8 THERMISTER_PIN = A0;
constexpr double R0 = 1000 * 100;
constexpr double RN = 1000 * 100;
constexpr double TN = 25;
constexpr double B_VALUE = 3645;

NTC_Thermistor Thermistor = NTC_Thermistor(THERMISTER_PIN, R0, RN, TN, B_VALUE);

double TargetTemperatureCelsius = 50.0;
Status HeaterStatus = Status::OFF;
uint16 AutoCloseAfterMinutes = 0;

Adafruit_SSD1306 Screen = Adafruit_SSD1306(128, 64, &Wire, -1);

const String CurrentTemperatureString = "Cur.:%c%s C";
const String CurrentStatusString = "Stat.:%c%s";
const String TargetTemperatureString = "Target:%c%s C";
const String TargetAutoCloseString = "Auto:%c%dmin";

const String Lines[] =
{
	CurrentTemperatureString,
	CurrentStatusString,
	TargetTemperatureString,
	TargetAutoCloseString
};

uint16 CurrentOperatingLineIndex;

static inline void FormatString(String& out, const String& input, ...)
{
	static char buffer[64];

	va_list args;
	va_start(args, input);

	memset(buffer, 0, sizeof(buffer));
	vsnprintf(buffer, sizeof(buffer), input.c_str(), args);
	out = String(buffer);
}
static inline String FormatFloat(double value, uint8 decimalPlaces)
{
	return String(value, decimalPlaces);
}

static inline String StatusToString(Status stat)
{
	switch (stat)
	{
	case Status::OFF:
		return "OFF";
	case Status::ON:
		return "ON";
	case Status::AUTO_CLOSED:
		return "AUTO_OFF";
	default:
		abort();
	}
}

void setup()
{
	pinMode(HEATER_MOSFET_PIN, OUTPUT);

	pinMode(BUTTON_NEXT_PIN, INPUT_PULLUP);
	pinMode(BUTTON_A_PIN, INPUT_PULLUP);
	pinMode(BUTTON_B_PIN, INPUT_PULLUP);

	pinMode(THERMISTER_PIN, INPUT);

	Serial.begin(9600);

	if (!Screen.begin(SSD1306_SWITCHCAPVCC, 0x7E))
	{
		Serial.println("Bad screen");
		while ((volatile int)1);
	}
}
void loop()
{
	UpdateHeater();
	UpdateDisplay();
	bool nextPressed = !digitalRead(BUTTON_NEXT_PIN);
	bool aPressed = !digitalRead(BUTTON_A_PIN);
	bool bPressed = !digitalRead(BUTTON_B_PIN);

	if (nextPressed)
		CurrentOperatingLineIndex = (++CurrentOperatingLineIndex) % 4;
	if (aPressed || bPressed)
		HandleButton(aPressed);

	delay(100);
}

static inline void UpdateHeater()
{
	auto temp = Thermistor.readCelsius();
	if (HeaterStatus != Status::ON)
	{
		digitalWrite(HEATER_MOSFET_PIN, LOW);
		return;
	}

	if (temp < TargetTemperatureCelsius)
	{
		digitalWrite(HEATER_MOSFET_PIN, HIGH);
	}
	else
	{
		digitalWrite(HEATER_MOSFET_PIN, LOW);
	}
}
static inline void UpdateDisplay()
{
	auto GetSelectChar = [](uint16 index) -> char
		{
			return (index == CurrentOperatingLineIndex) ? '>' : ' ';
		};

	auto currentTemperature = Thermistor.readCelsius();

	String formatted[4];
	FormatString(formatted[0], Lines[0], GetSelectChar(0), FormatFloat(currentTemperature, 1).c_str());
	FormatString(formatted[1], Lines[1], GetSelectChar(1), StatusToString(HeaterStatus).c_str());
	FormatString(formatted[2], Lines[2], GetSelectChar(2), FormatFloat(TargetTemperatureCelsius, 1).c_str());
	FormatString(formatted[3], Lines[3], GetSelectChar(3), AutoCloseAfterMinutes);

	for (int i = 0, cursorY = 1; i < 4; cursorY += 16, i++)
	{
		Screen.setCursor(1, cursorY);
		Screen.setTextSize(1);
		Screen.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
		Screen.println(formatted[i]);
	}
	Screen.display();
}
static inline void HandleButton(bool isA)
{
	switch (CurrentOperatingLineIndex)
	{
	case 0: // Current Temperature
		break;
	case 1: // Status
		if (isA)
		{
			HeaterStatus = Status::ON;
		}
		else
		{
			HeaterStatus = Status::OFF;
		}
		break;
	case 2: // Target Temperature
		if (isA)
		{
			TargetTemperatureCelsius++;
		}
		else
		{
			TargetTemperatureCelsius--;
		}
		break;
	case 3: // Auto Close After Minutes
		if (isA)
		{
			AutoCloseAfterMinutes++;
		}
		else
		{
			if (AutoCloseAfterMinutes > -1)
			{
				AutoCloseAfterMinutes -= 1;
			}
		}
		break;
	default:
		break;
	}
}
