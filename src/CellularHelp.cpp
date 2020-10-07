#include "CellularHelp.h"
#include "CellularInterpreterRK.h"
#include "CarrierLookupRK.h"


static Logger _log("app.help");

static const char _cregnMapping[] = 
    "0: default\n"
    "1: stat enabled\n"
    "2: location enabled\n";


static const char _cregStatMapping[] = // 6, 7, 9, 10 are CREG only
    "0: not registered, not searching\n"
    "1: registered, home network\n"
    "2: not registered, searching\n"
    "3: registration denied\n"
    "4: unknown, may be out of coverage area\n"
    "5: registered, roaming\n"
    "6: registered, SMS only, home network\n" // CREG only
    "7: registered, SMS only, roaming\n" // CREG only
    "8: emergency service only\n" // CEREG only
    "9: registered CSFB not preferred, home network\n" // CREG only
    "10: registered CSFB not preferred, roaming\n"; // CREG only

static const char _cregActMapping[] = 
    "0: GSM\n"      // CREG only
    "1: GSM COMPACT\n"      // CREG only
    "2: UTRAN\n"      // CREG only
    "3: GSM + EDGE\n"      // CREG only
    "4: GSM + HSDPA\n"      // CREG only
    "5: GSM + USUPA\n"      // CREG only
    "6: GSM + HSDPA & USUPA\n"      // CREG only
    "7: LTE\n"      // CEREG only
    "8: LTE Cat-M1\n" // CEREG only
    "9: LTE Cat-NB1\n"; // CEREG only

static const char _copsModeMapping[] = 
    "0: automatic\n"
    "1: manual\n"
    "2: deregister\n"
    "3: set format\n"
    "4: manual/automatic\n";

static const char _copsFormatMapping[] = 
    "0: long alphanumeric\n"
    "1: short alphanumeric\n"
    "2: numeric\n";

static const char _copsStatMapping[] = 
    "0: unknown\n"
    "1: available\n"
    "2: current\n"
    "3: forbidden\n";

static const char _umnoProfMapping[] = 
    "0: default\n"
    "1: SIM ICCID select\n"
    "2: AT&T\n"
    "3: Verizon\n"
    "19: Vodafone\n"
    "21: Telus\n"
    "31: Deutsche Telekom\n"
    "100: European\n"
    "101: European (no ePCO)\n"
    "198: AT&T (no band 5)\n";

static const char _cfunMapping[] = 
    "0: minimum functionality\n"
    "1: full functionality\n"
    "4: airplane\n"
    "15: silent reset\n"
    "16: silent reset plus SIM\n"
    "19: minimum functionality, deactivate SIM\n"
    "127: deep sleep\n";

static const char _cfunResetMapping[] = 
    "0: do not reset before setting fun\n"
    "1: silent reset plus SIM before setting fun\n";

static const char _cievSignalMapping[] = 
    "0: < -105 dBm\n"
    "1: < -193 dBm\n"
    "2: < -81 dBm\n"
    "3: < -69 dBm\n"
    "4: < -57 dBm\n"
    "5: >= -57 dBm\n";

static const char _cievGprsMapping[] = 
    "0: no GPRS available\n"
    "1: GPRS available but not registered\n"
    "2: registered to GPRS\n";


CellularHelp::CellularHelp() {
}


CellularHelp::~CellularHelp() {
}

void CellularHelp::setup() {

    CellularInterpreter::getInstance()->addModemMonitor(
            "CREG|CGREG|CEREG", 
            CellularInterpreterModemMonitor::REASON_SEND | CellularInterpreterModemMonitor::REASON_PLUS | CellularInterpreterModemMonitor::REASON_URC,
            [](uint32_t reason, const char *cmd, CellularInterpreterModemMonitor *mon) {
        // 
        CellularInterpreterParser parser;
        parser.parse(cmd);
        
        String regType;
        if (mon->command.equals("CREG")) {
            regType = "Network registration (CREG)";
        }
        else
        if (mon->command.equals("CGREG")) {
            regType = "GPRS network (CGREG)";
        }
        if (mon->command.equals("CEREG")) {
            regType = "EPS network (CEREG)";
        }

        if ((reason & CellularInterpreterModemMonitor::REASON_SEND) != 0) {
            _log.info("%s Set to %s", 
                regType.c_str(), 
                CellularInterpreter::mapValueToString(_cregnMapping, parser.getArgInt(0)).c_str());
            return;
        }

        size_t statArg;
        
        if ((reason & CellularInterpreterModemMonitor::REASON_PLUS) != 0) {
            // Response
            // int n = getArgInt(0);
            //_log.info("REASON_PLUS: %s", cmd);
            statArg = 1;
        }
        else {
            // URC
            // _log.info("REASON_URC: %s", cmd);
            statArg = 0;
        }

        int stat = parser.getArgInt(statArg);

        String statStr = CellularInterpreter::mapValueToString(_cregStatMapping, stat);

        _log.info("%s Status: %s", regType.c_str(), statStr.c_str());

        if (stat == 1 || stat == 5) {
            if (parser.getNumArgs() >= (statArg + 4)) {
                _log.info("Tracking area code: %s", parser.getArgString(statArg +1).c_str());
                _log.info("Cell identifier: %s", parser.getArgString(statArg + 2).c_str());
                int act = parser.getArgInt(statArg + 3);
                String actStr = CellularInterpreter::mapValueToString(_cregActMapping, act); 
                _log.info("Access technology: %s", actStr.c_str());
            }
        }
    });

    CellularInterpreter::getInstance()->addModemMonitor(
            "COPS", 
            CellularInterpreterModemMonitor::REASON_SEND | CellularInterpreterModemMonitor::REASON_PLUS,
            [](uint32_t reason, const char *cmd, CellularInterpreterModemMonitor *mon) {
        // Ignore when trace is disabled
        if (!CellularInterpreter::getInstance()->logSettingsTraceEnabled()) {
            return;
        }
        
        // 
        CellularInterpreterParser parser;
        parser.parse(cmd);

        if ((reason & CellularInterpreterModemMonitor::REASON_SEND) != 0) {
            // Request
            // _log.info("COPS SEND: %s", cmd);
            if (parser.isSet()) {
                _log.info("Operator Selection (COPS) Set: mode=%s format=%s",
                    CellularInterpreter::mapValueToString(_copsModeMapping, parser.getArgInt(0)).c_str(),
                    CellularInterpreter::mapValueToString(_copsFormatMapping, parser.getArgInt(1)).c_str());
            }
        }
        else {
            // Response
            if (mon->request.isRead()) {
                if (parser.getNumArgs() >= 1) {
                    _log.info("Operator Selection (COPS) Read: mode=%s",
                        CellularInterpreter::mapValueToString(_copsModeMapping, parser.getArgInt(0)).c_str());
                }
                if (parser.getNumArgs() >= 2) {
                    _log.info("  format=%s",
                        CellularInterpreter::mapValueToString(_copsFormatMapping, parser.getArgInt(1)).c_str());
                }
                if (parser.getNumArgs() >= 3) {
                    String oper = parser.getArgString(2);


                    if (parser.getArgInt(1) == 2) {
                        // COPS mode is numeric
                        int mcc = atoi(oper.substring(0, 3));
                        int mnc = atoi(oper.substring(3));

                        String carrier = lookupMccMnc((uint16_t) mcc, (uint16_t) mnc);
                        String country = lookupCountry((uint16_t) mcc);
                        _log.info("  oper=%s carrier=%s country=%s", oper.c_str(), carrier.c_str(), country.c_str());
                    }
                    else {
                        _log.info("  oper=%s", oper.c_str());
                    }
                }
            }
            else {

            }
        }
        
    });

    CellularInterpreter::getInstance()->addModemMonitor(
            "UMNOPROF", 
            CellularInterpreterModemMonitor::REASON_PLUS,
            [](uint32_t reason, const char *cmd, CellularInterpreterModemMonitor *mon) {
        // 
        CellularInterpreterParser parser;
        parser.parse(cmd);

        int prof = parser.getArgInt(0);

        String profStr = CellularInterpreter::mapValueToString(_umnoProfMapping, prof);

        _log.info("Mobile Network Operator Profile (UMNOPROF): %s", profStr.c_str());

        
    });


    CellularInterpreter::getInstance()->addModemMonitor(
            "CGDCONT", 
            CellularInterpreterModemMonitor::REASON_SEND,
            [](uint32_t reason, const char *cmd, CellularInterpreterModemMonitor *mon) {
        // 
        CellularInterpreterParser parser;
        parser.parse(cmd);

        int cid = parser.getArgInt(0);

        _log.info("PDP context definition (CGDCONT): cid=%d pdpType=%s APN=%s", 
            cid, parser.getArgString(1).c_str(), parser.getArgString(2).c_str());

    });

    CellularInterpreter::getInstance()->addModemMonitor(
            "CFUN", 
            CellularInterpreterModemMonitor::REASON_SEND,
            [](uint32_t reason, const char *cmd, CellularInterpreterModemMonitor *mon) {
        // 
        CellularInterpreterParser parser;
        parser.parse(cmd);

        _log.info("Set module functionality (CFUN): %s", 
            CellularInterpreter::mapValueToString(_cfunMapping, parser.getArgInt(0)).c_str());
        if (parser.getNumArgs() > 1) {
            _log.info("  Reset Mode: %s", 
                CellularInterpreter::mapValueToString(_cfunResetMapping, parser.getArgInt(1)).c_str());
        }
    });

    CellularInterpreter::getInstance()->addModemMonitor(
            "USOCTL", 
            CellularInterpreterModemMonitor::REASON_ERROR,
            [](uint32_t reason, const char *cmd, CellularInterpreterModemMonitor *mon) {
        // 
        _log.info("operation not allowed is expected here.");
    });

    CellularInterpreter::getInstance()->addModemMonitor(
            "CIEV", 
            CellularInterpreterModemMonitor::REASON_URC,
            [](uint32_t reason, const char *cmd, CellularInterpreterModemMonitor *mon) {
        // 
        CellularInterpreterParser parser;
        parser.parse(cmd);

        int descr = parser.getArgInt(0);
        int value = parser.getArgInt(1);

        switch(descr) {
        case 1:
            _log.info("CIEV battery charge level 0-5: %d", value);
            break;

        case 2:
            _log.info("CIEV signal level %s", 
                CellularInterpreter::mapValueToString(_cievSignalMapping, value).c_str());
            break;
        case 3:
            _log.info("CIEV service %s registered to network", ((value == 0) ? "not" : ""));
            break;            

        case 7:
            _log.info("CIEV service %s roaming", ((value == 0) ? "not" : ""));
            break;            

        case 9:
            _log.info("CIEV %s", 
                CellularInterpreter::mapValueToString(_cievGprsMapping, value).c_str());
            break;            

        default:
            _log.info("CIEV descr=%d value=%d", descr, value);
            break;
        }

        _log.info("Set module functionality (CFUN): %s", 
            CellularInterpreter::mapValueToString(_cfunMapping, parser.getArgInt(0)).c_str());
        if (parser.getNumArgs() > 1) {
            _log.info("  Reset Mode: %s", 
                CellularInterpreter::mapValueToString(_cfunResetMapping, parser.getArgInt(1)).c_str());
        }
    });

    CellularInterpreter::getInstance()->addModemMonitor(
            "CSQ", 
            CellularInterpreterModemMonitor::REASON_PLUS,
            [](uint32_t reason, const char *cmd, CellularInterpreterModemMonitor *mon) {
        // Ignore when trace is disabled
        if (!CellularInterpreter::getInstance()->logSettingsTraceEnabled()) {
            return;
        }

        // 
        CellularInterpreterParser parser;
        parser.parse(cmd);

        int rssi = parser.getArgInt(0);
        int qual = parser.getArgInt(1);

        String rssiStr;
        switch(rssi) {
        case 0:
            rssiStr = "<-113 dBm";
            break;

        case 31:
            rssiStr = ">=-51 dBm";
            break;

        case 99:
            rssiStr = "unknown";
            break;

        default:
            if (rssi < 31) {
                rssiStr = String::format("%d dBm", -113 + 2 * rssi);
            }
            else {
                rssiStr = String::format("unknown (%d)", rssi);
            }
        }

        String qualStr;
        if (qual <= 7) {
            qualStr = String::format("%d (0-7, lower is better)", qual);
        }
        else {
            qualStr = String::format("unknown (%d)", qual);
        }

        _log.info("Signal Quality (CSQ): rssi=%s qual=%s", rssiStr.c_str(), qualStr.c_str());
    });

}

// [static]
CellularHelp *CellularHelp::check() {
    CellularHelp *obj = new CellularHelp();
    if (obj) {
        obj->setup();
    }
    return obj;
}
