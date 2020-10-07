#ifndef __CELLULARHELP_H
#define __CELLULARHELP_H

#include "Particle.h"

/**
 * @brief Generate helpful messages to help decode the cellular commands, particularly for connection failures
 * 
 * Just call CellularInterpreterHelpCellularConnection::check() from setup() to enable after calling
 * CellularInterpreter::setup().
 * 
 * This is only intended for human-readable applications.
 */
class CellularHelp {
public:
    CellularHelp();
    virtual ~CellularHelp();

    void setup();

    void loop();
    
protected:
    uint64_t networkRegStartTime = 0;
    uint64_t networkRegTime = 0;
    uint64_t grpsRegTime = 0;
    uint64_t deactivatedSimNextReport = 0;
    uint64_t noServiceNextReport = 0;
};


#endif /* __CELLULARHELP_H */


