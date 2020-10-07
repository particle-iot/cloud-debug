#ifndef __CLOUDDEBUG_H
#define __CLOUDDEBUG_H

#include "CellularHelper.h"
#include "SerialCommandParserRK.h"

// CloudDebug.h
typedef void (*StateHandler)();

extern bool showTrace;
extern bool traceManuallySet;
extern bool buttonClicked;
extern bool startTest;
extern unsigned long stateTime;
extern unsigned long lastConnectReport;
extern StateHandler stateHandler;
extern SerialCommandEditor<1000, 256, 16> commandParser;

// Tinker.cpp
void tinkerSetup();

// CloudDebug.cpp
void stateCloudReport();
void stateCloudWait();
void stateCloudConnected();
void stateIdle();
void setTraceLogging(bool trace);
void pushTraceLogging(bool trace);
void popTraceLogging();
String elapsedString(int sec);
void runPowerReport();
bool parseIP(const char *str, IPAddress &addr, bool resolveIfNecessary);

// CloudDebugCellular.cpp or CloudDebugWiFi.cpp
void networkSetup();
void networkLoop();
void stateStartNetworkTest();
void stateButtonTest();
void runReport10s();

// CloudDebugEthernet.cpp
void checkEthernet();

#endif /* __CLOUDDEBUG_H */
