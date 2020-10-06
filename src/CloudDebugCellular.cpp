#include "CloudDebug.h"


#if Wiring_Cellular

//  
#include "CarrierLookupRK.h"
#include "CellularInterpreterRK.h"
#include "CellularHelp.h"
 
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
CellularInterpreter cellularInterpreter;

void networkSetup() {

	cellular_on(NULL);

	cellularInterpreter.setup();

	// Add CellularHelp functions to CellularInterpreter
	CellularHelp::check();

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

void networkLoop() {
	cellularInterpreter.loop();
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

