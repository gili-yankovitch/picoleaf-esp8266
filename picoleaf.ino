#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <EEPROM.h>
#include <ArduinoJson.h>

#define DEBUG_ESP_PORT Serial
#define ANIMATION_TRANSLATOR_URL "http://animation-translator.herokuapp.com/get"
#define ENDPOINTS_SERVICE_UPDATE_URL "http://central-endpoints.herokuapp.com/update"
#define ENDPOINTS_SERVICE_GET_URL "http://central-endpoints.herokuapp.com/get"
#define DEVICE_TYPE "LEDS"

#define BITS_IN_BYTE 8
#define FRAME_SIZE   4

#define CLK_PIN      0
#define DATA_PIN     2

#define MAX_LEDS    60

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

// Forward declaration
void updateData();
WiFiClient client;

struct led_s
{
	uint8_t red;
	uint8_t green;
	uint8_t blue;
	uint8_t bright;
};

#define VALID_CODE 0x42

enum bit_e
{
	ZERO = 0,
	ONE,
	MAX_VALUES
};

static int digitalValues[2];

void xmitBit(enum bit_e bit)
{
	if (bit >= MAX_VALUES)
		return;

	// Set value
	digitalWrite(DATA_PIN, digitalValues[bit]);

	// Commit to bit
	digitalWrite(CLK_PIN, HIGH);

	// First clock half-tick
	digitalWrite(CLK_PIN, LOW);
}

void xmitByte(uint8_t byte)
{
	int i = 0;

	// for (i = 0; i < BITS_IN_BYTE; ++i)
	for (i = BITS_IN_BYTE - 1; i >= 0; --i)
		xmitBit((byte & (1 << i)) ? ONE : ZERO);
}

void xmitStart()
{
	unsigned i = 0;

	for (i = 0; i < FRAME_SIZE; ++i)
		xmitByte(0x00);
}

void xmitStop(int count)
{
	int i = 0;

	// for (i = 0; i < FRAME_SIZE; ++i)
	for (i = 0; i < ((count + 14) / 16); ++i)
		xmitByte(0);
}

void xmitColor(uint8_t red, uint8_t green, uint8_t blue, uint8_t brightness)
{
    // Add upper 111
    brightness |= 0b11100000;

    // Transmit data
    xmitByte(brightness);
    xmitByte(blue);
    xmitByte(green);
    xmitByte(red);
}

void resetLEDs()
{
	unsigned i = 0;

	xmitStart();

	for (i = 0; i < MAX_LEDS; ++i)
	{
		xmitColor(0, 0, 0, 0);
	}
}

void resetREDs()
{
	unsigned i = 0;

	xmitStart();

	for (i = 0; i < MAX_LEDS; ++i)
	{
		xmitColor(0xff, 0, 0, 1);
	}
}

void resetGREENs()
{
	unsigned i = 0;

	xmitStart();

	for (i = 0; i < MAX_LEDS; ++i)
	{
		xmitColor(0x0, 0xff, 0, 1);
	}
}

void resetBLUEs()
{
	unsigned i = 0;

	xmitStart();

	for (i = 0; i < MAX_LEDS; ++i)
	{
		xmitColor(0, 0, 0xff, 1);
	}
}

#define MAX_AP_LIST_SIZE 32
#define FORM_SSID_NAME "ssid"
#define FORM_PW_NAME "password"
#define FORM_ENV_ID_NAME "env_id"

enum
{
  E_AP,
  E_CLIENT
} mode = E_AP;

String accesspoints[MAX_AP_LIST_SIZE];
unsigned int num_accesspoints;

/* Set these to your desired credentials. */
const char *ssid = "Picoleaf";
const char *password = "12345678";

/* Create a server to receive commands */
ESP8266WebServer server(80);

/* EEPROM Map:
    00: Initialized
    01: SSID
    ...
    N: \0
    N+1: Password
    ...
    N+M: \0
    N+M+1: A Photo of Mickey Mouse :D
*/
#define CONFIG_EXISTS 0x42
#define CONFIG_EXISTS_SIZE 1
#define CONFIG_SSID_SIZE   (32 + 1)
#define CONFIG_PW_SIZE     (64 + 1)
#define CONFIG_ENV_ID_SIZE    (16 + 1)
#define CONFIG_NAME_SIZE (8 + 1)

#define CONFIG_EXISTS_ADDR 0
#define CONFIG_SSID_ADDR   (CONFIG_EXISTS_ADDR + CONFIG_EXISTS_SIZE)
#define CONFIG_PW_ADDR     (CONFIG_SSID_ADDR   + CONFIG_SSID_SIZE)
#define CONFIG_ENV_ID_ADDR (CONFIG_PW_ADDR     + CONFIG_PW_SIZE)
#define CONFIG_NAME_ADDR   (CONFIG_ENV_ID_ADDR + CONFIG_ENV_ID_SIZE)

static String envId;
static String devName;

bool isConfigured()
{
	return EEPROM.read(CONFIG_EXISTS_ADDR) == CONFIG_EXISTS;
}

void resetConfig()
{
	EEPROM.write(CONFIG_EXISTS_ADDR, 0);
	EEPROM.commit();
}

void EEPROMWriteString(int addr, String str)
{
	for (unsigned int i = 0; i < str.length(); ++i)
	{
		EEPROM.write(addr + i, str.c_str()[i]);
	}

	EEPROM.write(addr + str.length(), 0);
}

void EEPROMReadString(int addr, String * str)
{
	byte val;

	while ((val = EEPROM.read(addr++)))
	{
		str->concat((char)val);
	}
}

static void EEPROMWriteConfig()
{
  EEPROM.write(CONFIG_EXISTS_ADDR, CONFIG_EXISTS);
}

static void EEPROMWriteSSID(String ssid)
{
	EEPROMWriteString(CONFIG_SSID_ADDR, ssid.substring(0, MIN(ssid.length(), CONFIG_SSID_SIZE)));
}

static void EEPROMWritePW(String pw)
{
	EEPROMWriteString(CONFIG_PW_ADDR, pw.substring(0, MIN(pw.length(), CONFIG_PW_SIZE)));
}

static void EEPROMWriteID(String env_id)
{
	EEPROMWriteString(CONFIG_ENV_ID_ADDR, env_id.substring(0, MIN(env_id.length(), CONFIG_ENV_ID_SIZE)));
}

static void EEPROMWriteName(String name)
{
	EEPROMWriteString(CONFIG_NAME_ADDR, name.substring(0, MIN(name.length(), CONFIG_NAME_SIZE)));
}

String GenerateRandomName()
{
	char letters[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
	String name;
	unsigned i;

	for (i = 0; i < CONFIG_NAME_SIZE - 1; ++i)
	{
		name += letters[random(sizeof(letters))];
	}

	return name;
}

void WriteConfig(String ssid, String pw, String env_id, String name)
{
	/* Write SSID */
	EEPROMWriteSSID(ssid);

	/* Write Password */
	EEPROMWritePW(pw);

	/* Write URL */
	EEPROMWriteID(env_id);

	EEPROMWriteName(name);

	/* Configured */
	EEPROMWriteConfig();

	EEPROM.commit();
}

void ReadConfig(String * ssid, String * pw)
{
	EEPROMReadString(CONFIG_SSID_ADDR, ssid);
	EEPROMReadString(CONFIG_PW_ADDR, pw);
	EEPROMReadString(CONFIG_ENV_ID_ADDR, &envId);
	EEPROMReadString(CONFIG_NAME_ADDR, &devName);
}

String head = "<!DOCTYPE html><html><head> <title>PicoLeaf</title> <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"/>";
String css = "<style>body{font-family: Lato, sans-serif; font-size: 14px; text-align: center;}h1{font-size: 3em;}body>div{width: 50%; margin: 0 auto; position: absolute; top: 50%; left: 50%; transform: translateY(-50%) translateX(-50%);}.label{display: inline-block; width: 100px; margin-bottom: 10px; margin-top: 10px;}.label.no-margin{margin: 0;}small{display: inline-block; margin-bottom: 10px;}select, input{width: 200px; height: 25px; border: 1px solid #008CBA; background-color: white; padding: 3px; box-sizing: border-box;}button{width: 200px; background-color: #008CBA; border: none; border-radius: 5px; color: white; padding: 15px 32px; text-align: center; text-decoration: none; display: inline-block; font-size: 16px; margin-top: 5px;}@media only screen and (min-device-width: 320px) and (max-device-width: 480px){h2{font-size: 16px;}body>div{width: 90%;}}</style>";

String body_upper = "</head> <body> <div> <form method=\"post\"> <h1>Picoleaf</h1> <h2>Please select your network SSID from the list and enter its password below</h2> <br/> <div class=\"label\">Network</div><select name=\"ssid\">";
String body_lower = "</select> <br/> <div class=\"label\">Password</div><input type=\"password\" name=\"password\"/> <br/> <div class=\"label\">Env ID</div><input name=\"env_id\"/> <br/> <div class=\"label\"></div><button type=\"submit\">Connect</button> </form> </div></body></html>";

String body_upper_landing = "</head> <body> <div> <h1>Connecting to: ";
String body_lower_landing = "</h1> <h2>Please check your device, and make sure the led is on</h2> </div></body></html>";

/* Forward declaration */
void actAsClient();

/* Go to http://192.168.4.1 in a web browser
   connected to this access point to see it.
*/
void handleRoot()
{
	if ((!server.hasArg(FORM_SSID_NAME)) || (!server.hasArg(FORM_PW_NAME)) || (!server.hasArg(FORM_ENV_ID_NAME)))
	{
		String page = head + css + body_upper;
		for (unsigned i = 0; i < num_accesspoints; ++i)
		{
			page += "<option>" + accesspoints[i] + "</option>";
		}

		page += body_lower;

		server.send(200, "text/html", page );
	}
	else
	{
		String ssid = server.arg(FORM_SSID_NAME);
		String pw = server.arg(FORM_PW_NAME);
		String env_id = server.arg(FORM_ENV_ID_NAME);

		Serial.print("Connecting to: ");
		Serial.println(ssid);
		Serial.print("Password: ");
		Serial.println(pw);
		Serial.print("Env ID: ");
		Serial.println(env_id);

		String page = head + css + body_upper_landing + ssid + body_lower_landing;
		server.send(200, "text/html", page);

		WriteConfig(ssid, pw, env_id, GenerateRandomName());

		/* Try connecting... */
		actAsClient();
	}
}

void actAsAP()
{
	WiFi.mode(WIFI_STA);
	mode = E_AP;
	Serial.println("Scanning for networks...");
	int n = WiFi.scanNetworks();
	num_accesspoints = (n > MAX_AP_LIST_SIZE) ? MAX_AP_LIST_SIZE : n;
	for (unsigned int i = 0; i < num_accesspoints; ++i)
	{
		accesspoints[i] = WiFi.SSID(i);
		Serial.println(accesspoints[i]);
	}

	Serial.println("Scan done.");

	Serial.print("Configuring access point...");
	WiFi.mode(WIFI_STA);

	/* You can remove the password parameter if you want the AP to be open. */
	WiFi.softAP(ssid, password);

	IPAddress myIP = WiFi.softAPIP();
	Serial.print("AP IP address: ");
	Serial.println(myIP);
	server.on("/", handleRoot);
	server.begin();
	Serial.println("HTTP server started");
}

#define WIFI_CONNECT_TIMEOUT 10
#define FORM_SSID_NAME "ssid"

#define HEADER_SIZE 2
#define PROTO_VALID_OFF 0
#define PROTO_VER_OFF   1

#define PROTO_REL_RED_OFF    0
#define PROTO_REL_GREEN_OFF  1
#define PROTO_REL_BLUE_OFF   2
#define PROTO_REL_BRIGHT_OFF 3
#define PROTO_LEDS_LEN       4

#define OPCODE_FRAME_START   0
#define OPCODE_FRAME_END     1
#define OPCODE_SLEEP         2
#define OPCODE_LED           3

#define BUFFER_SIZE          (1024 * 8)
uint8_t ledsData[BUFFER_SIZE];
size_t ledsLen = 0;

// Default with no data
void reportEndpoint(String data = "");

void reportEndpoint(String data)
{
	HTTPClient http;
	StaticJsonDocument<2048> doc;
	String report;

	doc["id"] = envId;
	doc["type"] = DEVICE_TYPE;
	doc["name"] = devName;
	
	if (data == "")
	{
		// Update address
		doc["address"] = WiFi.localIP().toString();
	}
	else
	{
		// Update data
		doc["data"] = data;
	}

	serializeJson(doc, report);

	http.begin(client, ENDPOINTS_SERVICE_UPDATE_URL);
	http.addHeader("Content-Type", "application/json");	

	http.POST(report);
	http.end();
}

void reloadData()
{
	int res;
	HTTPClient http;
	http.begin(client, ANIMATION_TRANSLATOR_URL "?id=" + envId + "&name=" + devName);

	if ((res = http.GET()) == 200)
	{
		uint8_t isValid;
		uint8_t * data;

		String payload = http.getString();

		if (payload.length() < HEADER_SIZE)
		{
			Serial.println("Error in header size");

			return;
		}

		// Parse the incoming data
		data = (uint8_t *)payload.c_str();

		isValid = data[PROTO_VALID_OFF];

		// Should update?
		if (isValid == VALID_CODE)
		{
			ledsLen = MIN(payload.length() - 2, BUFFER_SIZE);

			memcpy(ledsData, data + 2, ledsLen);
		}
	}
	else
	{
		ledsLen = 0;
	}

	http.end();
}

void handleUpdate()
{
	reloadData();

	server.sendHeader("Access-Control-Allow-Origin", "*");
	server.send(200, "application/json", "{\"succcess\": True}");
}

void handleEverythingElse()
{
	if (server.method() == HTTP_OPTIONS)
	{
		server.sendHeader("Access-Control-Allow-Origin", "*");
		server.sendHeader("Access-Control-Allow-Methods", "PUT, POST, GET, DELETE, OPTIONS");
		server.sendHeader("Access-Control-Allow-Headers", "Origin, X-Requested-With, Content-Type, Accept");
		server.send(204);
	}
	else
	{
		server.send(404, "text/plain", "");
	}
}

void actAsClient()
{
	String ssid;
	String pw;
	int tries = 0;

	/* Read SSID and Password from EEPROM */
	ReadConfig(&ssid, &pw);

	Serial.print("Read SSID from EEPROM: ");
	Serial.println(ssid);
	Serial.print("Read Password from EEPROM: ");
	Serial.println(pw);
	Serial.print("Read Env Id from EEPROM: ");
	Serial.println(envId);

	resetBLUEs();

	mode = E_CLIENT;

	WiFi.mode(WIFI_STA);

	WiFi.begin(ssid.c_str(), pw.c_str());

	while ((WiFi.status() != WL_CONNECTED) && (tries < WIFI_CONNECT_TIMEOUT))
	{
		tries++;
    	delay(1000);
		Serial.print(".");
	}

	/* Failed to connect. Go back to setup */
	if (WiFi.status() != WL_CONNECTED)
	{
		actAsAP();
	}
	else
	{
		Serial.println("Connected.");
		resetGREENs();

		configTime(3 * 3600, 0, "pool.ntp.org", "time.nist.gov");

		// Reload data
		reloadData();

		// Report on wakeup
		reportEndpoint();

		/* Configure server to handle commands */
		server.on("/update", HTTP_POST, handleUpdate);
		server.onNotFound(handleEverythingElse);
		
		server.begin();
	}
}

void setup()
{
	// Define digital values for logical bits
	digitalValues[ZERO] = LOW;
	digitalValues[ONE] = HIGH;

	pinMode(CLK_PIN, OUTPUT);

	pinMode(DATA_PIN, OUTPUT);

	// Reset clock and data to LOW
	digitalWrite(CLK_PIN, LOW);
	digitalWrite(DATA_PIN, LOW);

	resetLEDs();

	// Reset random
	randomSeed(analogRead(0));

	Serial.begin(115200);
	delay(1000);
	Serial.println();

	/* Start File System */
	Serial.println("Initializing EEPROM Util");
	EEPROM.begin(512);

	if (!isConfigured())
	{
		actAsAP();
	}
	else
	{
		actAsClient();
	}
}

void animate()
{
	uint8_t * data = ledsData;
	unsigned i = 0;

	// Parse data
	i = 0;

	while (i < ledsLen)
	{
		size_t leds_num = 0;

		// Parse opcode
		switch(data[i++])
		{
			case OPCODE_FRAME_START:
			{
				// Serial.printf("===== FRAME START =====\r\n");
				xmitStart();
				leds_num = 0;

				break;
			}

			case OPCODE_FRAME_END:
			{
				// Serial.printf("===== FRAME END =====\r\n");

				xmitStop(leds_num);
				break;
			}

			case OPCODE_SLEEP:
			{
				uint8_t milisecs = data[i++];

				// Serial.printf("===== SLEEP %u =====\r\n", milisecs);

				delay(milisecs);

				break;
			}

			case OPCODE_LED:
			{
				uint8_t red    = data[i + PROTO_REL_RED_OFF];
				uint8_t green  = data[i + PROTO_REL_GREEN_OFF];
				uint8_t blue   = data[i + PROTO_REL_BLUE_OFF];
				uint8_t bright = data[i + PROTO_REL_BRIGHT_OFF];

				++leds_num;

				i += 4;

				// Serial.printf("Red: %x Green: %x Blue: %x Brightness: %x\r\n", red, green, blue, bright);

				xmitColor(red, green, blue, bright);

				break;
			}
		}
	}
}

void loop()
{
	/* Anyway handle server */
	server.handleClient();

	if (mode == E_AP)
	{
		resetREDs();
	}
	else
	{
		animate();
	}
}
