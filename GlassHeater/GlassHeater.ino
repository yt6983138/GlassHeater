#include <Arduino.h>
#include <NTC_Thermistor.h> // Need to download manually cuz the library manager one isn't patched
#include "Utility.h"

#define NullFunc []() {}
#define UIUseSerial 0
#define DEBUG 0

#if (!UIUseSerial)

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
//#include <Fonts/FreeSans12pt7b.h>

#endif

enum class Status
{
	OFF,
	ON,
	AUTO_CLOSED
};

constexpr uint8 FORMAT_BUFFER_SIZE = 22;

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
uint8 AutoCloseAfterMinutes = 0;

#if (!UIUseSerial)
Adafruit_SSD1306 Screen = Adafruit_SSD1306(128, 64, &Wire, -1);
#endif


char* Lines[] =
{
	"Cur.:%c%s C",
	"Stat.:%c%s",
	"Target:%c%s C",
	"Auto:%c%dmin"
};

char*& CurrentTemperatureString = Lines[0];
char*& CurrentStatusString = Lines[1];
char*& TargetTemperatureString = Lines[2];
char*& TargetAutoCloseString = Lines[3];


uint16 CurrentOperatingLineIndex;

static inline void FormatString(char*& out, char*& input, ...)
{
	//static char buffer[32];

	va_list args;
	va_start(args, input);

#if DEBUG
	Serial.println(input);
#endif
	memset(out, 0, FORMAT_BUFFER_SIZE);
	auto count = vsnprintf(out, FORMAT_BUFFER_SIZE, input, args);

#if DEBUG
	Serial.println(count);
#endif
}
static inline String FormatFloat(double value, uint8 decimalPlaces)
{
	return String(value, decimalPlaces);
}

static inline char* StatusToString(Status stat)
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

#if DEBUG || UIUseSerial
	Serial.begin(9600);
	Serial.println("s");
#endif

#if (!UIUseSerial)
	if (!Screen.begin(SSD1306_SWITCHCAPVCC, 0x3C))
	{
#if DEBUG
		Serial.println("x");
#endif
		while ((volatile int)1);
	}
#endif
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

	delay(300);
}

static inline void UpdateHeater()
{
#if DEBUG
	Serial.println("uh");
#endif
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
#if DEBUG
	Serial.println("udp");
#endif
	auto GetSelectChar = [](uint16 index) -> char
		{
			return (index == CurrentOperatingLineIndex) ? '>' : ' ';
		};

	auto currentTemperature = Thermistor.readCelsius();

	static char* formatted[4] = 
	{ 
		new char[FORMAT_BUFFER_SIZE], 
		new char[FORMAT_BUFFER_SIZE], 
		new char[FORMAT_BUFFER_SIZE],
		new char[FORMAT_BUFFER_SIZE]
	};
	FormatString(formatted[0], Lines[0], GetSelectChar(0), FormatFloat(currentTemperature, 1).c_str());
	FormatString(formatted[1], Lines[1], GetSelectChar(1), StatusToString(HeaterStatus));
	FormatString(formatted[2], Lines[2], GetSelectChar(2), FormatFloat(TargetTemperatureCelsius, 1).c_str());
	FormatString(formatted[3], Lines[3], GetSelectChar(3), AutoCloseAfterMinutes);

#if (!UIUseSerial)
	Screen.clearDisplay();
	Screen.setTextWrap(false);
	Screen.setTextSize(1);
	Screen.setTextColor(WHITE);
	Screen.setCursor(0, 0);
	Screen.cp437(true);

	for (int i = 0, cursorY = 0; i < 4; cursorY += 16, i++)
	{
		Screen.println(formatted[i]);

#if DEBUG
		Serial.println(formatted[i]);
#endif
	}
	Screen.display();
#else
	//static uint16 lastOperatingLineIndex = 0;

	//if (lastOperatingLineIndex == CurrentOperatingLineIndex) 
	//	return;
	for (int i = 0; i < 4; i++)
	{
		Serial.println(formatted[i]);
	}
	Serial.println();
	//lastOperatingLineIndex = CurrentOperatingLineIndex;
#endif
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
