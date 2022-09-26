#include "CloudDebug.h"

#if Wiring_WiFi && !HAL_PLATFORM_WIFI_SCAN_ONLY

void stateWiFiWait();
void stateWiFiReport();
void stateWiFiCloudWait();
void stateWiFiCloudReport();

void wiFiScanCallback(WiFiAccessPoint* wap, void* data);
String securityString(int value);
String cipherString(int value);

void networkSetup() {
	// Running in semi-automatic mode, turn on WiFi before beginning
	WiFi.on();

    // antenna 
	commandParser.addCommandHandler("antenna", "select antenna (persistent)", [](SerialCommandParserBase *) {
        CommandParsingState *cps = commandParser.getParsingState();
        WLanSelectAntenna_TypeDef ant = ANT_NONE;

        if (cps->getByShortOpt('i')) {
		    commandParser.printMessageNoPrompt("selecting internal antenna");
            ant = ANT_INTERNAL;
        }
        else
        if (cps->getByShortOpt('e')) {
		    commandParser.printMessageNoPrompt("selecting external antenna");
            ant = ANT_EXTERNAL;
        }
        else
        if (cps->getByShortOpt('a')) {
		    commandParser.printMessageNoPrompt("selecting automatic antenna");
            ant = ANT_AUTO;
        }
        else {
		    commandParser.printMessageNoPrompt("missing or unknown command options");
            commandParser.printHelpForCommand(cps->getCommandHandlerInfo());
        }
        if (ant != ANT_NONE) {
            WiFi.selectAntenna(ant);
        }
		
		commandParser.printMessagePrompt();
	})
	.addCommandOption('a', "auto", "select antenna automatically")
	.addCommandOption('i', "internal", "select internal antenna (factory default")
	.addCommandOption('e', "external", "select external antenna");

	commandParser.addCommandHandler("wifi", "Wi-Fi connect or disconnect", [](SerialCommandParserBase *) {
        CommandParsingState *cps = commandParser.getParsingState();
		if (cps->getByShortOpt('c')) {
            WiFi.connect();
        }
        else
		if (cps->getByShortOpt('d')) {
            WiFi.disconnect();
        }
        else {
		    commandParser.printMessageNoPrompt("missing -c or -d option ready=%d", WiFi.ready());
            commandParser.printHelpForCommand(cps->getCommandHandlerInfo());
        }
		commandParser.printMessagePrompt();
	})
    .addCommandOption('c', "connect", "connect using using WiFi.connect()")
    .addCommandOption('d', "disconnect", "disconnect using WiFi.disconnect()");

	// 
	commandParser.addCommandHandler("clearCredentials", "Clear Wi-Fi credentials (persistent)", [](SerialCommandParserBase *) {
		WiFi.clearCredentials();
        commandParser.printMessage("WiFi.clearCredentials called");
	});

	// 
	commandParser.addCommandHandler("setCredentials", "Set Wi-Fi credentials (persistent)", [](SerialCommandParserBase *) {
	    CommandParsingState *cps = commandParser.getParsingState();
		if (cps->getParseSuccess()) {
			CommandOptionParsingState *cops;
			String ssid, password;
			unsigned long authType = WPA2;

			cops = cps->getByShortOpt('s');
			if (cops) {
				ssid = cops->getArgString(0);
			}
			else
			cops = cps->getByShortOpt('p');
			if (cops) {
				password = cops->getArgString(0);
			}
			else
			cops = cps->getByShortOpt('t');
			if (cops) {
				String authTypeStr = cops->getArgString(0);
				if (authTypeStr == "UNSEC") {
					authType = UNSEC;
				}
				else
				if (authTypeStr == "WEP") {
					authType = WEP;
				}
				else
				if (authTypeStr == "WPA") {
					authType = WPA;
				}
				else
				if (authTypeStr == "WPA2") {
					authType = WPA2;
				}
			}

			if (password.length() > 0) {
				// authType default is WPA2, this is what the Wiring API does if authType is not specified
				WiFi.setCredentials(ssid, password, authType);
			}
			else {
				// No password, only use SSID (also ignore auth)
				WiFi.setCredentials(ssid);
			}
			commandParser.printMessage("setCredentials ssid=%s password=%s authType=%lu", 
				ssid.c_str(), password.c_str(), authType);
		}
		else {
		    commandParser.printMessageNoPrompt("invalid options");
            commandParser.printHelpForCommand(cps->getCommandHandlerInfo());
		}
	})
    .addCommandOption('s', "ssid", "SSID (required)", true, 1)
    .addCommandOption('p', "password", "Password (required except for open networks)", false, 1)
    .addCommandOption('t', "t", "Authentication type WEP, WPA, WPA2 (required if AP not available)", false, 1);


#if defined(PLATFORM_PHOTON_PRODUCTION) && (PLATFORM_ID == PLATFORM_PHOTON_PRODUCTION || PLATFORM_ID == PLATFORM_P1)
	// 
	commandParser.addCommandHandler("useDynamicIP", "Turn off static IP address mode (persistent)", [](SerialCommandParserBase *) {
		WiFi.useDynamicIP();
        commandParser.printMessage("WiFi.useDynamicIP called");
	});

	// 
	commandParser.addCommandHandler("setStaticIP", "Set static IP address mode (persistent)", [](SerialCommandParserBase *) {
	    CommandParsingState *cps = commandParser.getParsingState();
		if (cps->getParseSuccess()) {
			IPAddress address, subnet, gateway, dns;
			CommandOptionParsingState *cops;

			cops = cps->getByShortOpt('a');
			if (cops) {
				parseIP(cops->getArgString(0), address, false);
			}
			cops = cps->getByShortOpt('s');
			if (cops) {
				parseIP(cops->getArgString(0), subnet, false);
			}
			cops = cps->getByShortOpt('g');
			if (cops) {
				parseIP(cops->getArgString(0), gateway, false);
			}
			cops = cps->getByShortOpt('d');
			if (cops) {
				parseIP(cops->getArgString(0), dns, false);
			}

			if (address && subnet && gateway && dns) {
				WiFi.setStaticIP(address, subnet, gateway, dns);
				WiFi.useStaticIP();
				commandParser.printMessage("Static IP address set address=%s subnet=%s gateway=%s dns=%s",
					address.toString().c_str(), subnet.toString().c_str(), gateway.toString().c_str(), dns.toString().c_str());
			}
			else {
				commandParser.printMessageNoPrompt("invalid IP address(es)");
				commandParser.printHelpForCommand(cps->getCommandHandlerInfo());
			}
		}
		else {
		    commandParser.printMessageNoPrompt("invalid options");
            commandParser.printHelpForCommand(cps->getCommandHandlerInfo());
		}
	})
    .addCommandOption('a', "address", "IP address (required)", true, 1)
    .addCommandOption('s', "subnet", "subnet mask", true, 1)
    .addCommandOption('g', "gateway", "Gateway IP address", true, 1)
    .addCommandOption('d', "dns", "DNS server IP address", true, 1);

#endif

}

void stateStartNetworkTest() {

#if defined(PLATFORM_PHOTON_PRODUCTION) && ((PLATFORM_ID == PLATFORM_PHOTON_PRODUCTION) || (PLATFORM_ID == PLATFORM_P1))
	{
		WLanSelectAntenna_TypeDef ant = wlan_get_antenna(NULL);

		String antStr;
		switch(ant) {
		case ANT_INTERNAL:
		  	antStr = "internal";
			break;
			
  		case ANT_EXTERNAL:
		  	antStr = "external";
			break;
			
  		case ANT_AUTO:
		  	antStr = "auto";
			break;

		default:
			antStr = String::format("unknown antenna %d", (int) ant);
			break;
		}

		Log.info("Antenna: %s", antStr.c_str());
	}
	{
		IPAddressSource antSource = wlan_get_ipaddress_source(NULL);
		
		String antSourceString;
		switch(antSource) {
			case STATIC_IP:
				antSourceString = "static";
				break;

			case DYNAMIC_IP:
				antSourceString = "dynamic";
				break;

			default:
				antSourceString = String::format("unknown %d", (int)antSource);
				break;
		}
		Log.info("IP Address Configuration: %s", antSourceString.c_str());

		if (antSource == STATIC_IP) {
			IPConfig conf;
			conf.size = sizeof(conf);
			wlan_get_ipaddress(&conf, NULL);
			Log.info("Static ipAddr: %s", IPAddress(conf.nw.aucIP).toString().c_str());
			Log.info("  subnetMask: %s", IPAddress(conf.nw.aucSubnetMask).toString().c_str());
			Log.info("  gateway: %s", IPAddress(conf.nw.aucDefaultGateway).toString().c_str());
			Log.info("  dns: %s", IPAddress(conf.nw.aucDNSServer).toString().c_str());
		}
	}
#endif

	// If WiFi has been configured, print out the configuration (does not include passwords)
	if (WiFi.hasCredentials()) {
		Log.info("Configured credentials:");
		WiFiAccessPoint ap[5];
		int found = WiFi.getCredentials(ap, 5);
		for(int ii = 0; ii < found; ii++) {
			Log.info("  ssid=%s security=%s cipher=%s", 
				ap[ii].ssid, 
				securityString(ap[ii].security).c_str(), 
				cipherString(ap[ii].cipher).c_str());
		}
	}

	Log.info("Available access points:");

	WiFi.scan(wiFiScanCallback);


	Log.info("Connecting to Wi-Fi");

	if (!traceManuallySet) {
		showTrace = true;
		setTraceLogging(true);
	}

	WiFi.connect();

	lastConnectReport = millis();
	stateTime = millis();
	stateHandler = stateWiFiWait;

}

void networkLoop() {
	
}

void stateButtonTest() {
	
}

void stateWiFiWait() {
	if (WiFi.ready()) {
		stateHandler = stateWiFiReport;
		return;
	}

	if (millis() - lastConnectReport >= 10000) {
		lastConnectReport = millis();
		Log.info("Still trying to connect to Wi-Fi %s", elapsedString((millis() - stateTime) / 1000).c_str());
	}
}

void stateWiFiReport() {
	Log.info("Connected to Wi-Fi in %s", elapsedString((millis() - stateTime) / 1000).c_str());
	{
		uint8_t mac[6];

		WiFi.macAddress(mac);
		Log.info("MAC address: %02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
	}
	Log.info("localIP: %s", WiFi.localIP().toString().c_str());
	Log.info("subnetMask: %s", WiFi.subnetMask().toString().c_str());

	IPAddress gatewayIP = WiFi.gatewayIP();
	Log.info("gatewayIP: %s", gatewayIP.toString().c_str());

	IPAddress dnsServerIP = WiFi.dnsServerIP();
	Log.info("dnsServerIP: %s (often 0.0.0.0)", dnsServerIP.toString().c_str());
	Log.info("dhcpServerIP: %s (often 0.0.0.0)", WiFi.dhcpServerIP().toString().c_str());

	{
		byte bssid[6];
		WiFi.BSSID(bssid);
		Log.info("BSSID: %02x:%02x:%02x:%02x:%02x:%02x", bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5]);
	}

	if (gatewayIP) {
		int count = WiFi.ping(gatewayIP, 1);
		Log.info("ping gateway=%d", count);
	}
	if (dnsServerIP) {
		int count = WiFi.ping(dnsServerIP, 1);
		Log.info("ping dnsServerIP=%d", count);
	}

	{
		IPAddress addr = IPAddress(8,8,8,8);
		int count = WiFi.ping(addr, 1);
		Log.info("ping addr %s=%d", addr.toString().c_str(), count);
	}

#if defined(PLATFORM_PHOTON_PRODUCTION) && ((PLATFORM_ID == PLATFORM_PHOTON_PRODUCTION) || (PLATFORM_ID == PLATFORM_P1))
	{
		IPAddress addr;

		for(size_t tries = 1; tries <= 3; tries++) {
			addr = WiFi.resolve("device.spark.io");
			if (addr) {
				break;
			}
			Log.info("failed to get device.spark.io from DNS, try %d", tries);
			delay(2000);
		}
		Log.info("device.spark.io=%s", addr.toString().c_str());

		if (addr) {
			// This will always fail
			// int count = WiFi.ping(addr, 1);
			// Log.info("ping addr %s=%d", addr.toString().c_str(), count);

			TCPClient client;
			if (client.connect(addr, 5683)) {
				Log.info("connected to device server CoAP (testing connection only)");
				client.stop();
			}
			else {
				Log.info("could not connect to device server by CoAP");
			}
		}
	}
#endif

	Log.info("Connecting to the Particle cloud...");
	Particle.connect();

	stateTime = lastConnectReport = millis();
	stateHandler = stateCloudWait;

}

void runReport10s() {
	// Runs every 10 seconds in stateIdle

	WiFiSignal sig = WiFi.RSSI();
	
	byte bssid[6];
	WiFi.BSSID(bssid);

	Log.info("rssi=%.1f bssid=%02X:%02X:%02X:%02X:%02X:%02X", 
		sig.getStrengthValue(),
		bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5]);

}

String securityString(int value) {
	String result;

	switch(value) {
	case WLAN_SEC_UNSEC:
		result = "unsecured";
		break;

	case WLAN_SEC_WEP:
		result = "wep";
		break;

	case WLAN_SEC_WPA:
		result = "wpa";
		break;

	case WLAN_SEC_WPA2:
		result = "wpa2";
		break;

	case WLAN_SEC_WPA_ENTERPRISE:
		result = "wpa enterprise";
		break;

    case WLAN_SEC_WPA2_ENTERPRISE:
		result = "wpa2 enterprise";
		break;

	case WLAN_SEC_NOT_SET:
		result = "not set";
		break;

	default:
		result = String::format("unknown security %d", value);
		break;
	}
	return result;
}

String cipherString(int value) {
	String result;

	switch(value) {
	case WLAN_CIPHER_NOT_SET:
		result = "not set";
		break;

	case WLAN_CIPHER_AES:
		result = "AES";
		break;

	case WLAN_CIPHER_TKIP:
		result = "TKIP";
		break;

	case WLAN_CIPHER_AES_TKIP:
		result = "AES or TKIP";
		break;

	default:
		result = String::format("unknown cipher %d", value);
		break;
	}

	return result;
}

void wiFiScanCallback(WiFiAccessPoint* wap, void* data) {


	Log.info("  ssid=%s security=%s channel=%d rssi=%d",
			wap->ssid, securityString(wap->security).c_str(), wap->channel, wap->rssi);

}


#endif /* Wiring_WiFi */

