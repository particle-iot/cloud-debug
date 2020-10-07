#include "Particle.h"

SYSTEM_THREAD(ENABLED);
SYSTEM_MODE(SEMI_AUTOMATIC);

// Log handler is created dynamically below

#include "CloudDebug.h"
#include "CellularInterpreterRK.h"
#include "sourcever.h"

#include <vector>

void buttonHandler();
void stateStart();
void stateStartTest();

#if SYSTEM_VERSION < 0x01020100
#error Device OS 1.2.1 or later is required
#endif

SerialCommandEditor<1000, 256, 16> commandParser;

const std::chrono::milliseconds autoStartScanTime = 15s;

bool showTrace = true;
bool traceManuallySet = false;
bool buttonClicked = false;
bool startTest = false;
StateHandler stateHandler = stateStart;
unsigned long stateTime;
unsigned long lastConnectReport = 0;
unsigned long cloudConnectTime = 0;
std::vector<bool> traceStack;
CellularInterpreter cellularInterpreter;

void subscriptionHandler(const char *eventName, const char *data);

void setup() {
    Particle.subscribe("particle/device/", subscriptionHandler, MY_DEVICES);
	System.on(button_click, buttonHandler);


    // This application also works like Tinker, allowing it to be controlled from
    // the Particle mobile app. This function initializes the Particle.functions.
    tinkerSetup();

	cellularInterpreter.setup();

    networkSetup();

	// Configuration of prompt and welcome message
	commandParser
		.withPrompt("> ")
		.withWelcome("Particle Cloud Debugging Tool\nEnter 'help' command to list available commands");

	commandParser.addCommandHandler("test", "start running tests", [](SerialCommandParserBase *) {
        startTest = true;
		commandParser.printMessage("starting tests...");
	});

#if HAL_PLATFORM_CLOUD_UDP
    commandParser.addCommandHandler("keepAlive", "Set Particle cloud keepAlive ", [](SerialCommandParserBase *) {
        CommandParsingState *cps = commandParser.getParsingState();
        if (cps->getNumExtraArgs() == 1) {
            int keepAlive = cps->getArgInt(0);
            if (keepAlive > 0) {
		        commandParser.printMessageNoPrompt("keepAlive set to %d", keepAlive);
                Particle.keepAlive(keepAlive);
            }
            else {
		        commandParser.printMessageNoPrompt("keepAlive cannot be 0");
            }
        }
        else {
		    commandParser.printMessageNoPrompt("missing keepAlive value in minutes");
            commandParser.printHelpForCommand(cps->getCommandHandlerInfo());
        }
		commandParser.printMessagePrompt();
	});
#endif

	commandParser.addCommandHandler("safemode", "enter safe mode (blinking magenta)", [](SerialCommandParserBase *) {
        System.enterSafeMode();
		commandParser.printMessagePrompt();
	});
	commandParser.addCommandHandler("dfu", "enter DFU (blinking yellow)", [](SerialCommandParserBase *) {
        System.dfu();
		commandParser.printMessagePrompt();
	});

	commandParser.addCommandHandler("cloud", "Particle cloud connect or disconnect", [](SerialCommandParserBase *) {
        CommandParsingState *cps = commandParser.getParsingState();
		if (cps->getByShortOpt('c')) {
		    commandParser.printMessageNoPrompt("connecting...");
            Particle.connect();
        }
        else
		if (cps->getByShortOpt('d')) {
		    commandParser.printMessageNoPrompt("disconnecting...");
            Particle.disconnect();
        }
        else {
		    commandParser.printMessageNoPrompt("missing -c or -d option connected=%d", Particle.connected());
            commandParser.printHelpForCommand(cps->getCommandHandlerInfo());
        }
		commandParser.printMessagePrompt();
	})
    .addCommandOption('c', "connect", "connect using using Particle.connect()")
    .addCommandOption('d', "disconnect", "disconnect using Particle.disconnect()");

	commandParser.addCommandHandler("trace", "enable or disable trace logging", [](SerialCommandParserBase *) {
        CommandParsingState *cps = commandParser.getParsingState();
		
		if (cps->getByShortOpt('e')) {
		    commandParser.printMessageNoPrompt("enabling trace mode");
            showTrace = true;
        }        
        else
		if (cps->getByShortOpt('d')) {
		    commandParser.printMessageNoPrompt("disabling trace mode");
            showTrace = false;
        }        
        else {
            // Toggle with no options
            showTrace = !showTrace;
		    commandParser.printMessageNoPrompt("trace mode %s", showTrace ? "enabled" : "disabled");
        }
        traceManuallySet = true;

        setTraceLogging(showTrace);
		commandParser.printMessagePrompt();
	})
    .addCommandOption('e', "enable", "enable trace logging")
    .addCommandOption('d', "disable", "disable trace logging");
        
	commandParser.addCommandHandler("literal", "enable or disable literal (non-filtered) logging", [](SerialCommandParserBase *) {
        CommandParsingState *cps = commandParser.getParsingState();
		
        bool literal = (CellularInterpreter::getInstance()->getLogSettings() & CellularInterpreter::LOG_LITERAL) != 0;

		if (cps->getByShortOpt('e')) {
		    commandParser.printMessageNoPrompt("enabling literal mode");
            literal = true;
        }        
        else
		if (cps->getByShortOpt('d')) {
		    commandParser.printMessageNoPrompt("disabling literal mode");
            literal = false;
        }        
        else {
            // Toggle with no options
            literal = !literal;
		    commandParser.printMessageNoPrompt("literal mode %s", literal ? "enabled" : "disabled");
        }

        if (literal) {
            CellularInterpreter::getInstance()->updateLogSettings(~0, CellularInterpreter::LOG_LITERAL);
        }
        else {
            CellularInterpreter::getInstance()->updateLogSettings(~CellularInterpreter::LOG_LITERAL, 0);
        }

		commandParser.printMessagePrompt();
	})
    .addCommandOption('e', "enable", "enable trace logging")
    .addCommandOption('d', "disable", "disable trace logging");


#if 0
    // This does not currently work, not sure why
	commandParser.addCommandHandler("ping", "ping a host or IP address", [](SerialCommandParserBase *) {
        CommandParsingState *cps = commandParser.getParsingState();
		if (cps->getParseSuccess()) {
            int tries = 3;
			IPAddress address;
			CommandOptionParsingState *cops;

			cops = cps->getByShortOpt('t');
			if (cops) {
                tries = cops->getArgInt(0);
                if (tries < 1) {
                    tries = 1;
                }
			}

            if (cps->getNumExtraArgs() == 1) {
                const char *hostOrIP = cps->getArgString(0);
                if (parseIP(hostOrIP, address, true)) {       
#if Wiring_WiFi
                    int res = inet_ping(&address.raw(), WiFi, tries,  NULL);
#else
                    int res = inet_ping(&address.raw(), Cellular, tries,  NULL);
#endif
                    if (res > 0) {
                        commandParser.printMessageNoPrompt("Ping %s succeeded!", address.toString().c_str());
                    }
                    else {
                        commandParser.printMessageNoPrompt("Unable to ping %s", address.toString().c_str());
                    }
                }
            }
            else {
                commandParser.printMessageNoPrompt("hostname or IP address required");
                commandParser.printHelpForCommand(cps->getCommandHandlerInfo());
            }
        }
        else {
		    commandParser.printMessageNoPrompt("invalid options");
            commandParser.printHelpForCommand(cps->getCommandHandlerInfo());
        }

		commandParser.printMessagePrompt();
	})
    .addCommandOption('t', "tries", "number of tries");
#endif

    // Connect to Serial and start running
	commandParser
		.withSerial(&Serial)
		.setup();

    stateTime = millis();
}

void loop() {
    networkLoop();

	cellularInterpreter.loop();

	commandParser.loop();

    if (stateHandler) {
        stateHandler(); 
    }
    if (buttonClicked) {
        buttonClicked = false;
        stateHandler = stateButtonTest;
    }
}


void buttonHandler() {
	buttonClicked = true;
}

void stateStart() {
    if (millis() - stateTime >= autoStartScanTime.count() && (commandParser.getBuffer()[0] == 0)) {
        // Automatically start running tests unless commands are being typed
        startTest = true;
    }

    if (startTest) {
        stateHandler = stateStartTest;
    }
}

/*
static uint16_t convertCurrentLimit(byte b) {
    uint16_t result = 128;

    if (b & 0x1) result += 128;
    if (b & 0x2) result += 256;
    if (b & 0x4) result += 512;
    if (b & 0x8) result += 1024;

    return result;
}
*/

void stateStartTest() {
    startTest = false;

    Log.info("Starting Tests!");

    {
        String platform;

        switch(PLATFORM_ID) {
        case PLATFORM_GCC:
            platform = "gcc";
            break;

        case PLATFORM_PHOTON_PRODUCTION:
            platform = "Photon";
            break;

        case PLATFORM_P1:
            platform = "P1";
            break;

        case PLATFORM_ELECTRON_PRODUCTION:
            platform = "Electron/E Series";
            break;

        case PLATFORM_ARGON:
            platform = "Argon";
            break;

        case PLATFORM_BORON:
            platform = "Boron";
            break;

        case PLATFORM_ASOM:
            platform = "A SoM";
            break;

        case PLATFORM_BSOM:
            platform = "B SoM";
            break;

#ifdef PLATFORM_B5SOM
        case PLATFORM_B5SOM:
            platform = "B5 SoM";
            break;
#endif

#ifdef PLATFORM_TRACKER
        case PLATFORM_TRACKER:
            platform = "Tracker";
            break;
#endif
        default:
            platform = String::format("unknown platform %d", PLATFORM_ID);
            break;
        }

        Log.info("Platform: %s", platform.c_str());
    }

    {
        uint8_t a, b, c, d;

        a = (uint8_t) (SYSTEM_VERSION >> 24);
        b = (uint8_t) (SYSTEM_VERSION >> 16);
        c = (uint8_t) (SYSTEM_VERSION >> 8);
        d = (uint8_t) (SYSTEM_VERSION);

        const char *relType = 0;
        switch(d & 0xc0) {
        case 0x80: 
            relType = "rc";
            d &= 0x3f;
            break;

        case 0x40:
            relType = "b";
            d &= 0x3f;
            break;

        case 0x00:
            if ((a == 0) || ((a == 1) && (b < 2))) {
                // Prior to 1.2.0, all were RC if a was != 0
                relType = "rc";
            }
            else {
                relType = "a";
            }
            break;

        default:
            break;   
        }

        if (relType) {
            Log.info("Binary compiled for: %d.%d.%d-%s.%d", a, b, c, relType, d);
        }
        else {
            Log.info("Binary compiled for: %d.%d.%d", a, b, c);
        }

        Log.info("Cloud Debug Release %d.%d.%d", a, b, SOURCEVER);

        Log.info("System version: %s", System.version().c_str());

        Log.info("Device ID: %s", System.deviceID().c_str());
    }

    runPowerReport();

    checkEthernet();

    stateHandler = stateStartNetworkTest;
}

void stateCloudWait() {
	if (Particle.connected()) {
		stateHandler = stateCloudReport;
		return;
	}

	if (millis() - lastConnectReport >= 10000) {
		lastConnectReport = millis();
		Log.info("Still trying to connect to the cloud %s", elapsedString((millis() - stateTime) / 1000).c_str());
        runPowerReport();
	}
}

void stateCloudReport() {
	Log.info("Successfully connected to the Particle cloud in %s", elapsedString((millis() - stateTime) / 1000).c_str());
    stateHandler = stateCloudConnected;
    cloudConnectTime = millis();
}

void stateCloudConnected() {
    if (!Particle.connected()) {
    	Log.info("Lost cloud connection after %s", elapsedString((millis() - cloudConnectTime) / 1000).c_str());
        commandParser.printMessagePrompt();
        lastConnectReport = millis();
        stateHandler = stateCloudWait;   
        return;
    }

    static unsigned long lastReport10s = 0;
    if (millis() - lastReport10s >= 10000) {
        lastReport10s = millis();

    	Log.info("Cloud connected for %s", elapsedString((millis() - cloudConnectTime) / 1000).c_str());
        runReport10s();
        runPowerReport();
        commandParser.printMessagePrompt();
    }

    static bool reportedTime = false;
    if (Time.isValid() && !reportedTime) {
        reportedTime = true;
        Log.info("Time: %s", Time.format().c_str());
        commandParser.printMessagePrompt();
    }

    static bool requestedName = false;
    if (!requestedName && millis() - cloudConnectTime >= 10000) {
        // 10 seconds after connecting to the cloud request the device name and IP address
        requestedName = true;
        Log.info("Requesting device name and IP address...");
        Particle.publish("particle/device/name", "", PRIVATE);
        Particle.publish("particle/device/ip", "", PRIVATE);
    }

}

void stateIdle() {

}

void setTraceLogging(bool trace) {
	// Log.info("%s trace logging", (trace ? "enabling" : "disabling"));

    if (trace) {
        CellularInterpreter::getInstance()->queueLogSettings(~0, CellularInterpreter::LOG_TRACE);
    }
    else {
        CellularInterpreter::getInstance()->queueLogSettings(~CellularInterpreter::LOG_TRACE, 0);
    }
}

void pushTraceLogging(bool trace) {
    bool curTrace = (CellularInterpreter::getInstance()->getLogSettings() & CellularInterpreter::LOG_TRACE) != 0;

    traceStack.push_back(curTrace);
    setTraceLogging(trace);
}

void popTraceLogging() {
    setTraceLogging(traceStack.back());
    traceStack.pop_back();
}


#if PLATFORM_ID == PLATFORM_ARGON
static bool hasVUSB() {
    uint32_t *pReg = (uint32_t *)0x40000438; // USBREGSTATUS

    return (*pReg & 1) != 0;
}
#endif /* PLATFORM_ID == PLATFORM_ARGON */

void runPowerReport() {

#if HAL_PLATFORM_POWER_MANAGEMENT
    {
        String powerSourceStr;

        int powerSource = System.powerSource();
        switch(powerSource) {
        case POWER_SOURCE_UNKNOWN:
            powerSourceStr = "unknown";
            break;

        case POWER_SOURCE_VIN:
            powerSourceStr = "VIN";
            break;
            
        case POWER_SOURCE_USB_HOST:
            powerSourceStr = "USB Host";
            break;
            
        case POWER_SOURCE_USB_ADAPTER:
            powerSourceStr = "USB Adapter";
            break;
            
        case POWER_SOURCE_USB_OTG:
            powerSourceStr = "USB OTG";
            break;
            
        case POWER_SOURCE_BATTERY:
            powerSourceStr = "Battery";
            break;
            
        default:
            powerSourceStr = String::format("powerSource %d", powerSource);
            break;
        }
        Log.info("Power source: %s", powerSourceStr.c_str());
    }
    {
        String batteryStateStr;

        int batteryState = System.batteryState();
        switch(batteryState) {
        case BATTERY_STATE_UNKNOWN:
            batteryStateStr = "unknown";
            break;

        case BATTERY_STATE_NOT_CHARGING:
            batteryStateStr = "not charging";
            break;
            
        case BATTERY_STATE_CHARGING:
            batteryStateStr = "charging";
            break;
            
        case BATTERY_STATE_CHARGED:
            batteryStateStr = "charged";
            break;
            
        case BATTERY_STATE_DISCHARGING:
            batteryStateStr = "discharging";
            break;
            
        case BATTERY_STATE_FAULT:
            batteryStateStr = "fault";
            break;
            
        case BATTERY_STATE_DISCONNECTED:
            batteryStateStr = "disconnected";
            break;

        default:            
            batteryStateStr = String::format("batteryState %d", batteryState);
            break;
        }
        Log.info("Battery state: %s, SoC: %.2f", batteryStateStr.c_str(), System.batteryCharge());
    }
#endif /* HAL_PLATFORM_POWER_MANAGEMENT */

#if PLATFORM_ID == PLATFORM_ARGON
    float voltage = analogRead(BATT) * 0.0011224;

    Log.info("USB power: %s, Battery Volage: %.2f", (hasVUSB() ? "true" : "false"), voltage); 
#endif /* PLATFORM_ID == PLATFORM_ARGON */

}


String elapsedString(int sec) {

    int hh = sec / 3600;
    int mm = (sec / 60) % 60;
    int ss = sec % 60;

    if (sec < 3600) {
        // Less than an hour
        return String::format("%02d:%02d", mm, ss);
    }
    else {
        // More than an hour
        return String::format("%d:%02d:%02d", hh, mm, ss);
    }
}


bool parseIP(const char *str, IPAddress &addr, bool resolveIfNecessary) {
    bool valid = false;
    int value[4];

    if (sscanf(str, "%d.%d.%d.%d", &value[0], &value[1], &value[2], &value[3]) == 4) {
        valid = true;
        for(size_t ii = 0; ii < 4; ii++) {
            if (value[ii] < 0 || value[ii] > 255) {
                valid = false;
                break;
            }
        }
        if (valid) {
            addr = IPAddress((uint8_t)value[0], (uint8_t)value[1], (uint8_t)value[2], (uint8_t)value[3]);
        }
    }
    else
    if (resolveIfNecessary) {
        HAL_IPAddress halAddress;
        
        // Not an IP address, try looking up DNS
        if (inet_gethostbyname(str, strlen(str), &halAddress, 0, NULL) == 0) {
            valid = true;
            addr = halAddress;
        }
    }

    return valid;
}


void subscriptionHandler(const char *eventName, const char *data) {
    const char *lastPart = strrchr(eventName, '/');
    if (!lastPart) {
        return;
    }
    lastPart++;

    if (strcmp(lastPart, "ip") == 0) {
        Log.info("Public IP address: %s", data);
    }
    else
    if (strcmp(lastPart, "name") == 0) {
        Log.info("Device name: %s", data);        
    }
    commandParser.printMessagePrompt();
}