#ifndef __CELLULARHELP_H
#define __CELLULARHELP_H

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

    static CellularHelp *check();

protected:
};


#endif /* __CELLULARHELP_H */


