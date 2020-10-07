#include "CloudDebug.h"

void checkEthernet() {
#if Wiring_Ethernet
    Log.info("This device could have Ethernet (is 3rd generation)");

    if (System.featureEnabled(FEATURE_ETHERNET_DETECTION)) {
        Log.info("FEATURE_ETHERNET_DETECTION enabled");

        uint8_t mac[6];
        if (Ethernet.macAddress(mac) != 0) {
            Log.info("Ethernet adapter present");

            Ethernet.connect();
            waitFor(Ethernet.ready, 10000);

            if (Ethernet.ready()) {
                Log.info("Ethernet ready (has link and IP address %s)", Ethernet.localIP().toString().c_str());
            }
            else {
                Log.info("Was unable to establish link or no DHCP");
            }
        }
        else {
            // the macAddress function returns 0 if there is no Ethernet adapter present
            Log.info("Ethernet detection enabled, but no Ethernet present");
        }

    }
    else {
        Log.info("FEATURE_ETHERNET_DETECTION not enabled, so Ethernet will not be used (even if present)");
    }
#else
    Log.info("No Ethernet (not Gen 3)");
#endif
}
