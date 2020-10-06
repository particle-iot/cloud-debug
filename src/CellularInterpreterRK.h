#ifndef __CELLULARINTERPRETER_H
#define __CELLULARINTERPRETER_H

#include "Particle.h"

#include <deque>
#include <vector>

#define TIMES_COLOR(n, c) (((n) << 24) | (c))


typedef std::function<void()> CellularInterpreterCallback;

class CellularInterpreterParser {
public:
    enum class RequestType : int {
        UNKNOWN_REQUEST = 0,
        SET_REQUEST,
        READ_REQUEST,
        TEST_REQUEST
    };

    CellularInterpreterParser();
    virtual ~CellularInterpreterParser();

    void clear();

    void parse(const char *command);

    /**
     * @brief Get the number of arguments 
     * 
     * Returns 1 for one argument. The index parameter is 0-based so the largest valid index
     * is getNumArgs() - 1.
     */
    size_t getNumArgs() const { return args.size(); };

    /**
     * @brief Returns the string argument by index
     * 
     * @param index The 0-based index to get. 0 = first argument. Last argument is
     * is getNumArgs() - 1.
     * 
     * @return The argument as a String. If the string argument is quoted, the double quotes
     * are removed. This also works if the item is an integer or hex, it just returns the
     * text representation in the argument response.
     */    
    String getArgString(size_t index) const;

    int getArgInt(size_t index) const;

    long getArgLongHex(size_t index) const;

    /**
     * @brief Returns the command part of the command line, if available
     * 
     * For a send of an AT command, this will be the "AT+XXXX" part of "AT+XXXX=" with the parts being
     * the part after the "="".
     * 
     * For a + response or a URC, this will be the "+XXXX" part of "+XXXX: " with the parts being the
     * part after the ": ".
     */
    String getCommand() const { return command; };

    RequestType getRequestType() const { return requestType; };
    bool isSet() const { return requestType == RequestType::SET_REQUEST; };
    bool isRead() const { return requestType == RequestType::READ_REQUEST; };
    bool isTest() const { return requestType == RequestType::TEST_REQUEST; };

protected:
    RequestType requestType = RequestType::UNKNOWN_REQUEST;
    String command;
    std::vector<String> args;
};


typedef std::function<void(long ts, const char *category, const char *level, const char *msg)> CellularInterpreterLogCallback;

class CellularInterpreterLogMonitor {
public:
    String category;
    String level;
    String matchString;
    
    CellularInterpreterLogCallback callback;
};

class CellularInterpreterModemMonitor; // Forward declaration

typedef std::function<void(uint32_t reason, const char *cmd, CellularInterpreterModemMonitor *mon)> CellularInterpreterModemCallback;

/**
 * @brief Class to monitor the modem commands
 */
class CellularInterpreterModemMonitor {
public:
    static const uint32_t REASON_OK             = 0x00000001;
    static const uint32_t REASON_PLUS           = 0x00000002;
    static const uint32_t REASON_ERROR          = 0x00000004;
    static const uint32_t REASON_SEND           = 0x00000008;
    static const uint32_t REASON_TIMEOUT        = 0x00000010;
    static const uint32_t REASON_URC            = 0x00000020;

    String command;
    uint32_t reasonFlags = 0;
    CellularInterpreterModemCallback callback;
    uint32_t timeout = 10000;
    uint64_t nextTimeout = 0;
    CellularInterpreterParser request;
};

class CellularInterpreterCommand {
public:
    String command;
    unsigned long startTime;
    bool gotPlus;
    bool complete;
};


class CellularInterpreterBlinkPattern {
public:
    CellularInterpreterBlinkPattern();
    virtual ~CellularInterpreterBlinkPattern();

    virtual void start() = 0;
    virtual bool run() = 0;

    virtual void callStart();
    virtual bool callRun();

    CellularInterpreterBlinkPattern &withRepeats(size_t num) { repeats = num; return *this; };

    static void setColor(uint32_t color);

protected:
    size_t repeats = 1;
    size_t curRepeat = 0;
};

class CellularInterpreterBlinkPatternBlink : public CellularInterpreterBlinkPattern {
public:
    CellularInterpreterBlinkPatternBlink();
    virtual ~CellularInterpreterBlinkPatternBlink();

    CellularInterpreterBlinkPatternBlink &withSlowBlink(uint32_t color, size_t blinks);

    virtual void start();
    virtual bool run();

    bool stateStart();
    bool stateBeforeOff();
    bool stateOn();
    bool stateOff();
    bool stateAfterOff();

protected:
    // Input parameters
    unsigned long beforeOffMs = 1000;
    unsigned long onPeriodMs = 250;
    unsigned long offPeriodMs = 750;
    size_t blinks = 1;
    unsigned long afterOffMs = 250;
    uint32_t onColor = RGB_COLOR_GREEN;
    uint32_t offColor = 0;

    // Current state
    std::function<bool(CellularInterpreterBlinkPatternBlink &)> stateHandler = &CellularInterpreterBlinkPatternBlink::stateStart;
    size_t curBlink = 0;
    unsigned long stateTime = 0;
};


class CellularInterpreterBlinkManager {
public:
    CellularInterpreterBlinkManager();
    virtual ~CellularInterpreterBlinkManager();

    void setup();

    void loop();

    void addBlinkPattern(CellularInterpreterBlinkPattern *pat);

    static CellularInterpreterBlinkManager *getInstance() { return instance; };

protected:
    CellularInterpreterBlinkPattern *curPattern = 0;
    std::deque<CellularInterpreterBlinkPattern *> patternQueue;
    static CellularInterpreterBlinkManager *instance;
};

class CellularInterpreterLogEntry {
public:
    long ts;
    String category;
    String level;
    String message;
};

class CellularInterpreterCommandEntry {
public:
    String command;
    bool toModem;
};


class CellularInterpreter : public StreamLogHandler, public Print  {
public:
    CellularInterpreter();
    virtual ~CellularInterpreter();

    void setup();

    void loop();

    void blinkNotification(uint32_t timesColor, size_t repeats = 1);

    void addLogMonitor(CellularInterpreterLogMonitor *mon);

    void addLogMonitor(const char *category, const char *level, const char *matchString, CellularInterpreterLogCallback callback);

    void addModemMonitor(CellularInterpreterModemMonitor *mon);

    void addModemMonitor(const char *cmdName, uint32_t reasonFlags, CellularInterpreterModemCallback callback, unsigned long timeout = 5000);

    /**
     * @brief Add a URC callback
     * 
     * @param urc The URC. Just the text part ("CIEV") without the + or :.
     */
    void addUrcHandler(const char *urc, CellularInterpreterModemCallback callback);

    void processLine(char *lineBuffer);

    void processCommand(const char *command, bool toModem);

    void callCommandMonitors(uint32_t reasonFlags, const char *command);

    void processTimeouts();

    void processLog(long ts, const char *category, const char *level, const char *msg);

    /**
	 * @brief Virtual override for the StreamLogHandler to write data to the log
     * 
     * Declared in class Print, overridden here.
	 */
    virtual size_t write(uint8_t);

    
    static const size_t WRITE_BUF_SIZE = 100;
    static const size_t MAX_LINES = 30;

    static const uint32_t BLINK_NCP_FAILURE = TIMES_COLOR(5, RGB_COLOR_ORANGE);

    static bool isMatchingCommand(const char *test, const char *cmd);

    static String mapValueToString(const char *mapping, int value);

    static CellularInterpreter *getInstance() { return instance; };


protected:
    char     writeBuffer[WRITE_BUF_SIZE];
    size_t      writeOffset = 0;

    os_thread_t loopThread = 0;
    bool        ignoreNextSend = false;
    String lastCommand;

    std::deque<CellularInterpreterCommandEntry *> commandQueue;
    std::deque<CellularInterpreterLogEntry *> logQueue;
    

    std::vector<CellularInterpreterModemMonitor *> commandMonitors;
    std::vector<CellularInterpreterLogMonitor *> logMonitors;
    static CellularInterpreter *instance;
};


class CellularInterpreterCheckNcpFailure {
public:
    CellularInterpreterCheckNcpFailure();
    virtual ~CellularInterpreterCheckNcpFailure();

    void setup(CellularInterpreterCallback callback);

    static CellularInterpreterCheckNcpFailure *check(CellularInterpreterCallback callback = 0);

protected:
    CellularInterpreterLogMonitor logMonitor;
    int noResponseCount = 0;
};



#endif /* __CELLULARINTERPRETER_H */
