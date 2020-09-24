#include "CloudDebug.h"


#if Wiring_Cellular

//  
#include "CarrierLookupRK.h"
 
void stateCellularWaitModemOn();
void stateCellularWait();
void stateCellularReport();
void stateCellularCloudWait();
void stateCellularCloudReport();


void reportNonModemInfo();
void reportModemInfo();
void runTowerTest();

// HAL_PLATFORM_GEN is only defined on 2.0.0 and later, so a little backward compatibility here 
#if !defined(HAL_PLATFORM_GEN)
#if (PLATFORM_ID == PLATFORM_BORON) || (PLATFORM_ID == PLATFORM_BSOM)
# define HAL_PLATFORM_GEN 3
#elif defined(PLATFORM_B5SOM) && (PLATFORM_ID == PLATFORM_B5SOM)
# define HAL_PLATFORM_GEN 3
#elif defined(PLATFORM_TRACKER) && (PLATFORM_ID == PLATFORM_TRACKER)
# define HAL_PLATFORM_GEN 3
#else
# define HAL_PLATFORM_GEN 2
#endif
#endif

CellularHelperEnvironmentResponseStatic<32> envResp;
static bool modemInfoReported = false;

void cloudDebugSetup() {

	cellular_on(NULL);

	commandParser.addCommandHandler("tower", "report cell tower information", [](SerialCommandParserBase *) {
		runTowerTest();
		commandParser.printMessagePrompt();
	});

#if (HAL_PLATFORM_GEN >= 3)

	commandParser.addCommandHandler("setCredentials", "set APN for 3rd-party SIM (persistent)", [](SerialCommandParserBase *) {
        CommandParsingState *cps = commandParser.getParsingState();

		String apn, user, pass;
		CommandOptionParsingState *cops;
		
		cops = cps->getByShortOpt('a');
		if (cops) {
			apn = cops->getArgString(0);
		}

		cops = cps->getByShortOpt('u');
		if (cops) {
			user = cops->getArgString(0);
		}

		cops = cps->getByShortOpt('p');
		if (cops) {
			pass = cops->getArgString(0);
		}

		commandParser.printMessageNoPrompt("setCredentials apn=%s user=%s pass=%s (persistent)", apn.c_str(), user.c_str(), pass.c_str());

		Cellular.setCredentials(apn, user, pass);
		
		commandParser.printMessagePrompt();
        })
        .addCommandOption('a', "apn", "specify APN", false, 1)
        .addCommandOption('u', "user", "specify username", false, 1)
        .addCommandOption('p', "pass", "specify password", false, 1);

	commandParser.addCommandHandler("clearCredentials", "clear APN for 3rd-party SIM (persistent)", [](SerialCommandParserBase *) {
		commandParser.printMessageNoPrompt("clearCredentials (persistent)");
		Cellular.clearCredentials();
		
		commandParser.printMessagePrompt();
        });

	commandParser.addCommandHandler("setActiveSim", "set active SIM (persistent)", [](SerialCommandParserBase *) {
        CommandParsingState *cps = commandParser.getParsingState();

		String apn, user, pass;
		
		if (cps->getByShortOpt('i')) {
			commandParser.printMessageNoPrompt("setActiveSim INTERNAL_SIM");
			Cellular.setActiveSim(INTERNAL_SIM);
		}
		else
		if (cps->getByShortOpt('e')) {
			commandParser.printMessageNoPrompt("setActiveSim EXTERNAL_SIM");
			Cellular.setActiveSim(EXTERNAL_SIM);
		}
		else {
			commandParser.printMessageNoPrompt("setActiveSim must specify -i or -e");
		}
		
		commandParser.printMessagePrompt();
        })
        .addCommandOption('e', "external", "use external SIM", false)
        .addCommandOption('i', "internal", "use internal SIM", false);

#else
	// Gen 2 (Electron / E Series)
	commandParser.addCommandHandler("setCredentials", "set APN for 3rd-party SIM (not persistent)", [](SerialCommandParserBase *) {
        CommandParsingState *cps = commandParser.getParsingState();

		String apn, user, pass;
		CommandOptionParsingState *cops;
		
		cops = cps->getByShortOpt('a');
		if (cops) {
			apn = cops->getArgString(0);
		}

		cops = cps->getByShortOpt('u');
		if (cops) {
			user = cops->getArgString(0);
		}

		cops = cps->getByShortOpt('p');
		if (cops) {
			pass = cops->getArgString(0);
		}

		commandParser.printMessageNoPrompt("cellular_credentials_set apn=%s user=%s pass=%s", apn.c_str(), user.c_str(), pass.c_str());

		cellular_credentials_set(apn, user, pass, NULL);
		
		commandParser.printMessagePrompt();
        })
        .addCommandOption('a', "apn", "specify APN", false, 1)
        .addCommandOption('u', "user", "specify username", false, 1)
        .addCommandOption('p', "pass", "specify password", false, 1);

#endif /* HAL_PLATFORM_GEN >= 3 */

    commandParser.addCommandHandler("mnoprof", "Set Mobile Network Operator Profile (persistent)", [](SerialCommandParserBase *) {
        CommandParsingState *cps = commandParser.getParsingState();
        if (cps->getNumExtraArgs() == 1) {
            int mnoprof = cps->getArgInt(0);
			commandParser.printMessageNoPrompt("mnoprof set to %d (persistent)", mnoprof);
			Cellular.command("AT+COPS=2\r\n");
			Cellular.command("AT+UMNOPROF=%d\r\n", mnoprof);
			Cellular.command("AT+CFUN=15\r\n");
        }
        else {
		    commandParser.printMessageNoPrompt("missing mnoprof value");
            commandParser.printHelpForCommand(cps->getCommandHandlerInfo());
        }
		commandParser.printMessagePrompt();
	});

    commandParser.addCommandHandler("command", "Send a raw cellular command", [](SerialCommandParserBase *) {
		if (commandParser.getArgsCount() == 2) {
			commandParser.printMessageNoPrompt("Sending command %s", commandParser.getArgString(1));

			pushTraceLogging(true);
			int res = Cellular.command("%s\r\n", commandParser.getArgString(1));
			popTraceLogging();
			if (res == RESP_OK) {
				commandParser.printMessageNoPrompt("Success! (result=%d)", res);
			}
			else {
				commandParser.printMessageNoPrompt("Error: result=%d", res);
			}
		}
		else {
		    commandParser.printMessageNoPrompt("missing command to send");
            commandParser.printHelpForCommand("command");
		}
		commandParser.printMessagePrompt();
	}).withRawArgs();

	commandParser.addCommandHandler("cellular", "Cellular connect or disconnect", [](SerialCommandParserBase *) {
        CommandParsingState *cps = commandParser.getParsingState();
		if (cps->getByShortOpt('c')) {
            Cellular.connect();
        }
        else
		if (cps->getByShortOpt('d')) {
            Cellular.disconnect();
        }
        else {
		    commandParser.printMessageNoPrompt("missing -c or -d option ready=%d", Cellular.ready());
            commandParser.printHelpForCommand(cps->getCommandHandlerInfo());
        }
		commandParser.printMessagePrompt();
	})
    .addCommandOption('c', "connect", "connect using using Cellular.connect()")
    .addCommandOption('d', "disconnect", "disconnect using Cellular.disconnect()");

}

void stateStartNetworkTest() {
	modemInfoReported = false;
	stateTime = millis();
	stateHandler = stateCellularWaitModemOn;	
}

void stateButtonTest() {
	Log.info("running tower scan");

	runTowerTest();

	stateTime = millis();
	stateHandler = stateIdle;	
}

void stateCellularWaitModemOn() {
	if (millis() - stateTime < 8000) {
		// Wait 8 seconds for modem to stabilize
		return;
	}

	reportNonModemInfo();

	// 
	reportModemInfo();

	if (!traceManuallySet) {
		showTrace = true;
		setTraceLogging(true);
	}

	Cellular.connect();

	stateTime = millis();
	stateHandler = stateCellularWait;	
}


void stateCellularWait() {
	// This only reports if we haven't yet reported and only tries every 10 seconds.
	// This is done because if the modem needs to be reset, the initial check above will fail
	// to retrieve any data.
	reportModemInfo();

	if (Cellular.ready()) {
		stateHandler = stateCellularReport;
		return;
	}

	if (millis() - lastConnectReport >= 10000) {
		lastConnectReport = millis();
		Log.info("Still trying to connect to cellular %s", elapsedString((millis() - stateTime) / 1000).c_str());
		runPowerReport();
	}
}

void stateCellularReport() {
	Log.info("Connected to cellular in %s", elapsedString((millis() - stateTime) / 1000).c_str());

	Log.info("Connecting to the Particle cloud...");
	Particle.connect();

	stateTime = lastConnectReport = millis();
	stateHandler = stateCloudWait;
}


void runReport10s() {
	// Runs every 10 seconds in stateIdle
	static int runs = 0;

	// First two times, leave trace logging on
	// After that, only report the file results to avoid noise
	pushTraceLogging(++runs <= 2);

	if (Particle.connected()) {
		CellularGlobalIdentity cgi = {0};
        cgi.size = sizeof(CellularGlobalIdentity);
        cgi.version = CGI_VERSION_LATEST;

        cellular_result_t res = cellular_global_identity(&cgi, NULL);
        if (res == SYSTEM_ERROR_NONE) {
            Log.info("Cellular Info: cid=%lu lac=%u mcc=%u mnc=%u", cgi.cell_id, cgi.location_area_code, cgi.mobile_country_code, cgi.mobile_network_code);

			String country = lookupCountry(cgi.mobile_country_code);
			String carrier = lookupMccMnc(cgi.mobile_country_code, cgi.mobile_network_code);

			Log.info("Carrier: %s Country: %s", carrier.c_str(), country.c_str());
        }
        else {
            Log.info("cellular_global_identity failed %d", res);
        }

		CellularHelperNetworkInfo networkInfo;
		if (CellularHelper.getNetworkInfo(networkInfo)) {
			Log.info("Technology: %s, Band: %s", networkInfo.accessTechnology.c_str(), networkInfo.band.c_str());
		}

	}

	if (Cellular.ready()) {
		String ratString;

		CellularSignal sig = Cellular.RSSI();
		int rat = sig.getAccessTechnology();
		switch(rat) {
		case NET_ACCESS_TECHNOLOGY_GSM:
			ratString = "2G";
			break;

		case NET_ACCESS_TECHNOLOGY_EDGE:
			ratString = "EDGE";
			break;

		case NET_ACCESS_TECHNOLOGY_UMTS: 
//		case NET_ACCESS_TECHNOLOGY_UTRAN: These are the same constant values
//		case NET_ACCESS_TECHNOLOGY_WCDMA:
			ratString = "3G";
			break;

		case NET_ACCESS_TECHNOLOGY_LTE:
			ratString = "LTE";
			break;

		case NET_ACCESS_TECHNOLOGY_LTE_CAT_M1:
			ratString = "LTE Cat M1";
			break;

		case NET_ACCESS_TECHNOLOGY_LTE_CAT_NB1:
			ratString = "LTE Cat NB1";
			break;

		default:
			ratString = String::format("unknown rat %d", rat);
			break;
		}

		Log.info("Strength: %.1f, Quality: %.1f, RAT: %s", sig.getStrength(), sig.getQuality(), ratString.c_str());

	}

	popTraceLogging();
}

void reportNonModemInfo() {
	Log.info("deviceID: %s", System.deviceID().c_str());

//#if (defined(HAL_PLATFORM_PMIC_BQ24195) && HAL_PLATFORM_PMIC_BQ24195) 
    {
        PMIC pmic(true);

        Log.info("PMIC inputVoltageLimit: %d mV", 
            (int) pmic.getInputVoltageLimit());
			
		Log.info("PMIC inputCurrentLimit: %d mA",
			(int) pmic.getInputCurrentLimit());

        Log.info("PMIC minimumSystemVoltage: %d mV", 
            (int) pmic.getMinimumSystemVoltage());

        Log.info("PMIC chargeCurrentValue: %d mA", 
            (int) pmic.getChargeCurrentValue());

        // Pre-Charge/Termination Current Register 3 does not appear to be exposed
       
        Log.info("PMIC chargeVoltageValue: %d mV", 
            (int) pmic.getChargeVoltageValue());
    }


}

void reportModemInfo() {
	if (modemInfoReported) {
		return;

	}
	static unsigned long lastCheck = 0;
	if (millis() - lastCheck < 10000) {
		return;
	}
	lastCheck = millis();

	String mfg = CellularHelper.getManufacturer().c_str();
	if (mfg.length() == 0) {
		Log.info("modem is not yet responding");
		return;
	}

	modemInfoReported = true;

	pushTraceLogging(false);
	
	Log.info("manufacturer: %s", mfg.c_str());

	Log.info("model: %s", CellularHelper.getModel().c_str());

	Log.info("firmware version: %s", CellularHelper.getFirmwareVersion().c_str());

	Log.info("ordering code: %s", CellularHelper.getOrderingCode().c_str());

	Log.info("IMEI: %s", CellularHelper.getIMEI().c_str());

	Log.info("IMSI: %s", CellularHelper.getIMSI().c_str());

	Log.info("ICCID: %s", CellularHelper.getICCID().c_str());
	
	popTraceLogging();

}


void printCellData(CellularHelperEnvironmentCellData *data) {
	const char *whichG = data->isUMTS ? "3G" : "2G";

	// Log.info("mcc=%d mnc=%d", data->mcc, data->mnc);

	String operatorName = lookupMccMnc(data->mcc, data->mnc);

	Log.info("%s %s %s %d bars", whichG, operatorName.c_str(), data->getBandString().c_str(), data->getBars());
}


void runTowerTest() {
	envResp.clear();
	envResp.enableDebug = true;

	const char *model = CellularHelper.getModel().c_str();
	if (strncmp(model, "SARA-R4", 7) == 0) {
		Log.info("Tower scan not available on LTE (SARA-R4)");
		return;
	}
	if (CellularHelper.getManufacturer().equals("Quectel")) {
		Log.info("Tower scan not available on Quectel");
		return;
	}

	Log.info("Looking up operators (this may take up to 3 minutes)...");

	// Command may take up to 3 minutes to execute!
	envResp.resp = Cellular.command(CellularHelperClass::responseCallback, (void *)&envResp, 360000, "AT+COPS=5\r\n");
	if (envResp.resp == RESP_OK) {
		envResp.logResponse();
	}

	Log.info("Results:");

	printCellData(&envResp.service);
	if (envResp.neighbors) {
		for(size_t ii = 0; ii < envResp.numNeighbors; ii++) {
			if (envResp.neighbors[ii].isValid(true /* ignoreCI */)) {
				printCellData(&envResp.neighbors[ii]);
			}
		}
	}
}

#endif /* WiringCellular */


#if 0
Opening serial monitor for com port: "/dev/tty.usbmodem143201"
Serial monitor opened successfully:
> 0000015322 [app] INFO: Starting Tests!
0000015323 [app] INFO: Platform: Tracker
0000015323 [app] INFO: Binary compiled for: 1.5.4-rc.1
0000015324 [app] INFO: System version: 1.5.4-rc.1
0000015326 [app] INFO: Device ID: e00fce68145526b04bc146f6
0000015327 [app] INFO: Power source: USB Host
0000015328 [app] INFO: Battery state: charged, SoC: 98.83
0000023698 [app] INFO: deviceID: e00fce68145526b04bc146f6
0000023997 [app] INFO: manufacturer: Quectel
0000024047 [app] INFO: model: BG96
0000024097 [app] INFO: firmware version: BG96MAR04A04M1G
0000024148 [app] INFO: ordering code: QuectelBG96Revision: BG96MAR04A04M1G
0000024197 [app] INFO: IMEI: 865284048234441
0000024247 [app] INFO: IMSI: Quectel
0000024297 [app] INFO: ICCID: 89014103271407775244
0000024298 [app] INFO: enabling trace logging
0000024299 [app] INFO: Still trying to connect to cellular 00:00
0000024301 [app] INFO: Power source: USB Host
0000024304 [app] INFO: Battery state: charged, SoC: 98.83
0000024303 [net.ppp.client] TRACE: PPP netif -> 8
0000024306 [net.ppp.client] TRACE: PPP thread event LOWER_DOWN
0000024307 [net.ppp.client] TRACE: PPP thread event ADM_DOWN
0000024309 [net.ppp.client] TRACE: PPP thread event ADM_UP
0000024311 [net.ppp.client] TRACE: State NONE -> READY
0000025309 [ncp.at] TRACE: > AT+CIMI
0000025347 [ncp.at] TRACE: < 310410140777524
0000025348 [ncp.at] TRACE: < OK
0000025349 [ncp.at] TRACE: > AT+CGDCONT=1,"IP","10569.mcs"
0000025397 [ncp.at] TRACE: < OK
0000025398 [ncp.at] TRACE: > AT+CREG=2
0000025447 [ncp.at] TRACE: < OK
0000025448 [ncp.at] TRACE: > AT+CGREG=2
0000025497 [ncp.at] TRACE: < OK
0000025498 [ncp.at] TRACE: > AT+CEREG=2
0000025547 [ncp.at] TRACE: < OK
0000025548 [hal] TRACE: NCP connection state changed: 1
0000025549 [net.ppp.client] TRACE: PPP thread event LOWER_DOWN
0000025550 [ncp.at] TRACE: > AT+COPS=0
0000025598 [ncp.at] TRACE: < OK
0000025598 [ncp.at] TRACE: > AT+CREG?
0000025599 [ncp.at] TRACE: < +QIND: SMS DONE
0000025600 [ncp.at] TRACE: < +CREG: 2
0000025601 [ncp.at] TRACE: < +CEREG: 2
0000025647 [ncp.at] TRACE: < +CREG: 2,2
0000025648 [ncp.at] TRACE: < OK
0000025649 [ncp.at] TRACE: > AT+CGREG?
0000025697 [ncp.at] TRACE: < +CGREG: 2,0
0000025698 [ncp.at] TRACE: < OK
0000025699 [ncp.at] TRACE: > AT+CEREG?
0000025747 [ncp.at] TRACE: < +CEREG: 2,2
0000025748 [ncp.at] TRACE: < OK
0000027648 [ncp.at] TRACE: < +CREG: 1,"D0B","C45C010",8
0000027749 [ncp.at] TRACE: < +CEREG: 1,"D0B","C45C010",8
0000027750 [hal] TRACE: NCP connection state changed: 2
0000027847 [net.ppp.client] TRACE: PPP thread event LOWER_UP
0000027848 [net.ppp.client] TRACE: State READY -> CONNECT
0000027849 [net.ppp.client] TRACE: TX: 13
0000027850 [net.ppp.client] TRACE: State CONNECT -> CONNECTING
0000027851 [net.ppp.client] TRACE: PPP phase -> 2
0000027897 [net.ppp.client] TRACE: RX: 12
0000027898 [net.ppp.client] TRACE: RX: 21
0000028852 [net.ppp.client] TRACE: PPP phase -> 3
0000028853 [net.ppp.client] TRACE: PPP phase -> 6
0000028854 [net.ppp.client] TRACE: TX: 46
0000028898 [net.ppp.client] TRACE: RX: 55
0000028899 [net.ppp.client] TRACE: TX: 53
0000028900 [net.ppp.client] TRACE: RX: 45
0000028901 [net.ppp.client] TRACE: TX: 15
0000028902 [net.ppp.client] TRACE: PPP phase -> 7
0000028952 [net.ppp.client] TRACE: RX: 22
0000028953 [net.ppp.client] TRACE: RX: 41
0000028954 [net.ppp.client] TRACE: TX: 28
0000028955 [net.ppp.client] TRACE: RX: 26
0000029005 [net.ppp.client] TRACE: RX: 10
0000029006 [net.ppp.client] TRACE: PPP phase -> 9
0000029007 [net.ppp.client] TRACE: TX: 29
0000029057 [net.ppp.client] TRACE: RX: 10
0000029058 [net.ppp.client] TRACE: TX: 11
0000029059 [net.ppp.client] TRACE: RX: 28
0000029060 [net.ppp.client] TRACE: TX: 29
0000029110 [net.ppp.client] TRACE: RX: 28
0000029111 [net.ppp.client] TRACE: PPP netif -> 4
0000029112 [net.ppp.client] TRACE: PPP status -> 0
0000029113 [net.ppp.client] TRACE: PPP netif -> b0
0000029114 [net.ppp.client] TRACE: PPP thread event UP
0000029115 [net.ppp.client] TRACE: State CONNECTING -> CONNECTED
0000029117 [net.ppp.client] TRACE: PPP phase -> 10
0000029118 [app] INFO: Connected to cellular in 00:04
0000029119 [app] INFO: Connecting to the Particle cloud...
0000029220 [system] ERROR: Failed to load session data from persistent storage
0000029224 [net.ppp.client] TRACE: TX: 96
0000029280 [net.ppp.client] TRACE: TX: 95
0000030280 [net.ppp.client] TRACE: TX: 96
0000030417 [net.ppp.client] TRACE: RX: 122
0000030418 [net.ppp.client] TRACE: RX: 122
0000030419 [net.ppp.client] TRACE: RX: 41
0000030420 [net.ppp.client] TRACE: TX: 96
0000030520 [net.ppp.client] TRACE: RX: 122
0000030521 [net.ppp.client] TRACE: RX: 122
0000030522 [net.ppp.client] TRACE: RX: 41
0000030523 [net.ppp.client] TRACE: TX: 61
0000030674 [net.ppp.client] TRACE: RX: 122
0000030675 [net.ppp.client] TRACE: RX: 122
0000030726 [net.ppp.client] TRACE: RX: 116
0000031216 [ncp.at] TRACE: > AT+CCID
0000031227 [ncp.at] TRACE: < +CCID: 89014103271407775244
0000031229 [ncp.at] TRACE: < OK
0000031231 [ncp.at] TRACE: > AT+CGSN
0000031277 [ncp.at] TRACE: < 865284048234441
0000031278 [ncp.at] TRACE: < OK
0000031288 [net.ppp.client] TRACE: TX: 145
0000033901 [net.ppp.client] TRACE: TX: 16
0000033927 [net.ppp.client] TRACE: RX: 26
0000034268 [net.ppp.client] TRACE: TX: 145
0000038901 [net.ppp.client] TRACE: TX: 16
0000038927 [net.ppp.client] TRACE: RX: 26
0000039120 [app] INFO: Still trying to connect to the cloud 00:10
0000039123 [app] INFO: Power source: USB Host
0000039125 [app] INFO: Battery state: charged, SoC: 98.83
0000040268 [net.ppp.client] TRACE: TX: 145
0000040627 [net.ppp.client] TRACE: RX: 94
0000040629 [net.ppp.client] TRACE: TX: 178
0000040828 [net.ppp.client] TRACE: RX: 122
0000040879 [net.ppp.client] TRACE: RX: 81
0000040880 [net.ppp.client] TRACE: RX: 122
0000040881 [net.ppp.client] TRACE: RX: 34
0000040882 [net.ppp.client] TRACE: RX: 66
0000040883 [net.ppp.client] TRACE: RX: 58
0000041265 [net.ppp.client] TRACE: TX: 411
0000041834 [net.ppp.client] TRACE: RX: 47
0000041835 [net.ppp.client] TRACE: RX: 88
0000041841 [net.ppp.client] TRACE: TX: 92
0000041986 [net.ppp.client] TRACE: RX: 66
0000041991 [net.ppp.client] TRACE: TX: 103
0000041994 [net.ppp.client] TRACE: TX: 102
0000041997 [net.ppp.client] TRACE: TX: 105
0000042000 [net.ppp.client] TRACE: TX: 105
0000042002 [net.ppp.client] TRACE: TX: 75
0000042006 [net.ppp.client] TRACE: TX: 78
0000042008 [net.ppp.client] TRACE: TX: 68
0000042237 [net.ppp.client] TRACE: RX: 66
0000042238 [net.ppp.client] TRACE: RX: 66
0000042239 [net.ppp.client] TRACE: RX: 66
0000042240 [net.ppp.client] TRACE: RX: 66
0000042291 [net.ppp.client] TRACE: RX: 66
0000042292 [net.ppp.client] TRACE: RX: 66
0000042342 [net.ppp.client] TRACE: RX: 72
0000043010 [app] INFO: Successfully connected to the Particle cloud in 00:13
0000043012 [ncp.at] TRACE: > AT+COPS=3,2
0000043042 [ncp.at] TRACE: < OK
0000043043 [ncp.at] TRACE: > AT+COPS?
0000043092 [ncp.at] TRACE: < +COPS: 0,2,"310410",8
0000043093 [ncp.at] TRACE: < OK
0000043094 [ncp.at] TRACE: > AT+CEREG?
0000043142 [ncp.at] TRACE: < +CEREG: 2,1,"D0B","C45C010",8
0000043143 [ncp.at] TRACE: < OK
0000043144 [ncp.at] TRACE: > AT+CGREG?
0000043192 [ncp.at] TRACE: < +CGREG: 2,0
0000043193 [ncp.at] TRACE: < OK
0000043194 [ncp.at] TRACE: > AT+CREG?
0000043242 [ncp.at] TRACE: < +CREG: 2,1,"D0B","C45C010",8
0000043243 [ncp.at] TRACE: < OK
0000043244 [app] INFO: Cellular Info: cid=205897744 lac=3339 mcc=310 mnc=410
0000043247 [app] INFO: Carrier: AT&T Wireless Inc. Country: United States
0000043249 [ncp.at] TRACE: > AT+COPS=3,2
0000043292 [ncp.at] TRACE: < OK
0000043293 [ncp.at] TRACE: > AT+COPS?
0000043342 [ncp.at] TRACE: < +COPS: 0,2,"310410",8
0000043343 [ncp.at] TRACE: < OK
0000043344 [ncp.at] TRACE: > AT+QCSQ
0000043392 [ncp.at] TRACE: < +QCSQ: "CAT-M1",-62,-101,71,-20
0000043393 [ncp.at] TRACE: < OK
0000043394 [app] INFO: Strength: 41.2, Quality: 0.0, RAT: LTE Cat M1
0000043395 [app] INFO: Power source: USB Host
0000043397 [app] INFO: Battery state: charged, SoC: 98.83
0000043592 [net.ppp.client] TRACE: RX: 71
0000043593 [net.ppp.client] TRACE: RX: 71
0000043901 [net.ppp.client] TRACE: TX: 16
0000043944 [net.ppp.client] TRACE: RX: 26
0000044114 [ncp.at] TRACE: > AT+CCID
0000044144 [ncp.at] TRACE: < +CCID: 89014103271407775244
0000044146 [ncp.at] TRACE: < OK
0000044148 [ncp.at] TRACE: > AT+CGSN
0000044194 [ncp.at] TRACE: < 865284048234441
0000044195 [ncp.at] TRACE: < OK
0000044203 [net.ppp.client] TRACE: TX: 585
0000044686 [ncp.at] TRACE: > AT+CCID
0000044694 [ncp.at] TRACE: < +CCID: 89014103271407775244
0000044695 [ncp.at] TRACE: < OK
0000044696 [ncp.at] TRACE: > AT+CGSN
0000044744 [ncp.at] TRACE: < 865284048234441
0000044745 [ncp.at] TRACE: < OK
0000044847 [net.ppp.client] TRACE: TX: 139
0000045844 [net.ppp.client] TRACE: RX: 109
0000045850 [net.ppp.client] TRACE: TX: 66
0000048901 [net.ppp.client] TRACE: TX: 16
0000048945 [net.ppp.client] TRACE: RX: 26
0000049045 [net.ppp.client] TRACE: RX: 72
0000049053 [ncp.at] TRACE: > AT+COPS=3,2
0000049096 [ncp.at] TRACE: < OK
0000049098 [ncp.at] TRACE: > AT+COPS?
0000049146 [ncp.at] TRACE: < +COPS: 0,2,"310410",8
0000049147 [ncp.at] TRACE: < OK
0000049148 [ncp.at] TRACE: > AT+QCSQ
0000049196 [ncp.at] TRACE: < +QCSQ: "CAT-M1",-62,-101,62,-20
0000049197 [ncp.at] TRACE: < OK
0000049199 [ncp.at] TRACE: > AT+COPS=3,2
0000049246 [ncp.at] TRACE: < OK
0000049248 [ncp.at] TRACE: > AT+COPS?
0000049296 [ncp.at] TRACE: < +COPS: 0,2,"310410",8
0000049298 [ncp.at] TRACE: < OK
0000049299 [ncp.at] TRACE: > AT+CEREG?
0000049346 [ncp.at] TRACE: < +CEREG: 2,1,"D0B","C45C010",8
0000049347 [ncp.at] TRACE: < OK
0000049348 [ncp.at] TRACE: > AT+CGREG?
0000049396 [ncp.at] TRACE: < +CGREG: 2,0
0000049397 [ncp.at] TRACE: < OK
0000049398 [ncp.at] TRACE: > AT+CREG?
0000049446 [ncp.at] TRACE: < +CREG: 2,1,"D0B","C45C010",8
0000049447 [ncp.at] TRACE: < OK
0000049450 [net.ppp.client] TRACE: TX: 226
0000053011 [ncp.at] TRACE: > AT+COPS=3,2
0000053046 [ncp.at] TRACE: < OK
0000053047 [ncp.at] TRACE: > AT+COPS?
0000053096 [ncp.at] TRACE: < +COPS: 0,2,"310410",8
0000053097 [ncp.at] TRACE: < OK
0000053098 [ncp.at] TRACE: > AT+CEREG?
0000053146 [ncp.at] TRACE: < +CEREG: 2,1,"D0B","C45C010",8
0000053147 [ncp.at] TRACE: < OK
0000053148 [ncp.at] TRACE: > AT+CGREG?
0000053196 [ncp.at] TRACE: < +CGREG: 2,0
0000053197 [ncp.at] TRACE: < OK
0000053198 [ncp.at] TRACE: > AT+CREG?
0000053246 [ncp.at] TRACE: < +CREG: 2,1,"D0B","C45C010",8
0000053247 [ncp.at] TRACE: < OK
0000053248 [app] INFO: Cellular Info: cid=205897744 lac=3339 mcc=310 mnc=410
0000053251 [app] INFO: Carrier: AT&T Wireless Inc. Country: United States
0000053253 [ncp.at] TRACE: > AT+COPS=3,2
0000053296 [ncp.at] TRACE: < OK
0000053297 [ncp.at] TRACE: > AT+COPS?
0000053346 [ncp.at] TRACE: < +COPS: 0,2,"310410",8
0000053347 [ncp.at] TRACE: < OK
0000053348 [ncp.at] TRACE: > AT+QCSQ
0000053396 [ncp.at] TRACE: < +QCSQ: "CAT-M1",-62,-101,65,-20
0000053397 [ncp.at] TRACE: < OK
0000053398 [app] INFO: Strength: 41.2, Quality: 0.0, RAT: LTE Cat M1
0000053399 [app] INFO: Power source: USB Host
0000053400 [app] INFO: Battery state: charged, SoC: 98.83
0000053901 [net.ppp.client] TRACE: TX: 16
0000053946 [net.ppp.client] TRACE: RX: 27
0000058901 [net.ppp.client] TRACE: TX: 16
0000058946 [net.ppp.client] TRACE: RX: 26
#endif