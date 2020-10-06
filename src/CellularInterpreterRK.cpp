#include "CellularInterpreterRK.h"

static Logger _log("app.cellinterp");

CellularInterpreter *CellularInterpreter::instance = 0;
CellularInterpreterBlinkManager *CellularInterpreterBlinkManager::instance = 0;

CellularInterpreterParser::CellularInterpreterParser() {

}
CellularInterpreterParser::~CellularInterpreterParser() {

}

void CellularInterpreterParser::clear() {
    requestType = RequestType::UNKNOWN_REQUEST;
    command = "";
    args.clear();
}

void CellularInterpreterParser::parse(const char *commandLineIn) {
    String curPart;
    bool inQuote = false;
    bool isAT = false;
    bool isPlus = false;
    
    if (strncmp(commandLineIn, "AT", 2) == 0) {
        isAT = true;
    }
    else if (commandLineIn[0] == '+') {
        isPlus = true;
    }
    
    for(const char *cp = commandLineIn; *cp; cp++) {
        if (isAT) {
            if (*cp == '=' || *cp == '?') {
                // AT command ends at = or ?
                command = curPart;
                curPart = "";

                if (*cp == '?') {
                    requestType = RequestType::READ_REQUEST;
                }
                else {
                    if (cp[1] == '?') {
                        // =?
                        requestType = RequestType::TEST_REQUEST;
                        cp++;
                    }
                    else {
                        // =
                        requestType = RequestType::SET_REQUEST;
                    }
                }

                isAT = false;
            }
        }
        else if (isPlus) {
            if (*cp == ':') {
                command = curPart;
                curPart = "";
                isPlus = false;
                if (cp[1] == ' ') {
                    // Skip space after : as well
                    cp++;
                }
            }
        }
        else if (*cp == '"') {
            inQuote = !inQuote;
        }
        else if (!inQuote && *cp == ',') {
            // Comma separator, not in a quote
            args.push_back(curPart);
            curPart = "";
        }
        else {
            // Include in this string
            curPart += *cp;
        }
        
    }
    if (curPart.length() != 0) {
        args.push_back(curPart);
    }
}

String CellularInterpreterParser::getArgString(size_t index) const {
    if (index < args.size()) {
        return args[index];
    }
    else {
        return "";
    }
}

int CellularInterpreterParser::getArgInt(size_t index) const {
    return atoi(getArgString(index));
}

long CellularInterpreterParser::getArgLongHex(size_t index) const {
    char *end;
    return strtol(getArgString(index).c_str(), &end, 16);
}


CellularInterpreter::CellularInterpreter() : StreamLogHandler(*this, LOG_LEVEL_TRACE) {
    instance = this;
}

CellularInterpreter::~CellularInterpreter() {
    // TODO: Deallocate buffers

    // TODO: Deallocate objects in commandMonitors
}

void CellularInterpreter::setup() {

	// Add this callback into the system log manager
	LogManager::instance()->addHandler(this);

}

void CellularInterpreter::loop() {
    // We skip over logs generated from this thread to prevent recursively
    // generating logs
    loopThread = os_thread_current(NULL);
	
    while(!logQueue.empty()) {
        CellularInterpreterLogEntry *entry = logQueue.front();
        logQueue.pop_front();

        processLog(entry->ts, entry->category, entry->level, entry->message);

        delete entry;
    }

    while(!commandQueue.empty()) {
        CellularInterpreterCommandEntry *entry = commandQueue.front();
        commandQueue.pop_front();

        processCommand(entry->command, entry->toModem);

        delete entry;
    }

    processTimeouts();

    loopThread = NULL;
}

void CellularInterpreter::blinkNotification(uint32_t timesColor, size_t repeats) {

    uint32_t color = (timesColor & 0x00ffffff);
    size_t times = (timesColor >> 24) & 0x0f; 
    
    CellularInterpreterBlinkManager *blinkManager = CellularInterpreterBlinkManager::getInstance();
    if (blinkManager) {
        CellularInterpreterBlinkPatternBlink *pat = new CellularInterpreterBlinkPatternBlink();
        if (pat) {
            pat->withSlowBlink(color, times).withRepeats(repeats);
            blinkManager->addBlinkPattern(pat);
        }
    }
    
}


void CellularInterpreter::addLogMonitor(CellularInterpreterLogMonitor *mon) {
    logMonitors.push_back(mon);
}

void CellularInterpreter::addLogMonitor(const char *category, const char *level, const char *matchString, CellularInterpreterLogCallback callback) {
    CellularInterpreterLogMonitor *mon = new CellularInterpreterLogMonitor();
    
    if (category) {
        mon->category = category;
    }
    if (level) {
        mon->level = level;
    }
    if (matchString) {
        mon->matchString = matchString;
    }
    mon->callback = callback;

    addLogMonitor(mon);
}


void CellularInterpreter::addModemMonitor(CellularInterpreterModemMonitor *mon) {
    commandMonitors.push_back(mon);
}

void CellularInterpreter::addModemMonitor(const char *cmdName, uint32_t reasonFlags, CellularInterpreterModemCallback callback, unsigned long timeout) {
    char *cmdNameMutable = strdup(cmdName);
    if (cmdNameMutable) {
        char *endptr = 0;
        char *token = strtok_r(cmdNameMutable, "|", &endptr);
        while(token) {
            CellularInterpreterModemMonitor *mon = new CellularInterpreterModemMonitor();
            mon->command = token;
            mon->reasonFlags = reasonFlags;
            mon->timeout = timeout;
            mon->callback = callback;

            addModemMonitor(mon);

            token = strtok_r(NULL, "|", &endptr);
        }

        free(cmdNameMutable);
    }
}


void CellularInterpreter::addUrcHandler(const char *urc, CellularInterpreterModemCallback callback) {
    CellularInterpreterModemMonitor *mon = new CellularInterpreterModemMonitor();
    mon->command = urc;
    mon->reasonFlags = CellularInterpreterModemMonitor::REASON_URC;
    mon->timeout = 0;
    mon->callback = callback;

    addModemMonitor(mon);    
}


size_t CellularInterpreter::write(uint8_t c) {
    // Do not add any _log.* statements in this function!

    if ((logSettings & LOG_LITERAL) != 0) {
        // Output everthing exactly as sent as if this module didn't exist at all
        logOutput(c, true);
    }

    if (loopThread && os_thread_is_current(loopThread)) {
        logOutput(c);
        return 1;
    }

    if ((char)c == '\r') {
        // End of line, process this line
        
        // offset will be at most WRITE_BUF_SIZE - 1
        // so this is safe to null terminate the (possibly partial) line
        if (writeOffset > 0) {
            writeBuffer[writeOffset] = 0;

            processLine(writeBuffer);

            writeOffset = 0;
        }
        writeOffset = 0;
    }
    else
    if ((char) c == '\n') {
        // Ignore LFs 
    }
    else
    if (writeOffset < (CellularInterpreter::WRITE_BUF_SIZE - 1)) {
        // Append other characters to the line if there's room
        writeBuffer[writeOffset++] = (char) c;
    }
    else {
        // Line is too long, ignore the rest 
    }
    return 1;
}

void CellularInterpreter::logOutput(uint8_t c, bool literal) {
    if ((logSettings & LOG_LITERAL) == 0) {
        // Literal mode is not configured
        if (literal) {
            return;
        }
    }
    else {
        // Literal mode is configured
        if (!literal) {
            // Not literal in literal mode; we already logged this
            return;
        }
    }

    if ((logSettings & LOG_SERIAL) != 0) {
        Serial.write(c);
    }
    if ((logSettings & LOG_SERIAL1) != 0) {
        Serial1.write(c);
    }
}

void CellularInterpreter::logOutput(const char *s, bool literal) {
    while(*s) {
        logOutput(*s++, literal);
    }
}

void CellularInterpreter::processLine(char *lineBuffer) {
    // Process the (possibly partial) line in lineBuffer. It's a c-string, always null
    // terminated even if the line is truncated.
    // You only get the beginning of the line, the truncated part is discarded and
    // never copied into lineBuffer.

    // Example: Gen 2
    //     50.750 AT send       4 "AT\r\n"
    //     50.753 AT read OK    6 "\r\nOK\r\n"
    //     50.753 AT send      13 "AT+COPS=3,2\r\n"
    //     50.757 AT read OK    6 "\r\nOK\r\n"
    //     50.757 AT send      10 "AT+COPS?\r\n"
    //     50.763 AT read  +   25 "\r\n+COPS: 0,2,\"310410\",2\r\n"
    //     50.764 AT read OK    6 "\r\nOK\r\n"
    //     50.764 AT send       8 "AT+CSQ\r\n"
    //     50.768 AT read  +   14 "\r\n+CSQ: 16,2\r\n"
    //     50.769 AT read OK    6 "\r\nOK\r\n"
    // col=0 token=50.750
    // col=1 token=AT
    // col=2 token=send
    // col=3 token=4
    // col=4 token="AT
    // 
    // Example with binary data
    //     11.037 AT read  +   78 "\r\n+USORF: 0,\"54.86.198.203\",5684,38,\"\x17\xfe\xfd\x00\x01\x00\x00\x00\x00\x00\x9f\x00\x19\x00\x01\x00\x00\x00\x00\x00\x9f\xd0'R@\xf594Rf\xe0g\x89\x85'S\x99\xca\"\r\n"
    // 


    // Example: Gen 3
    // 0000068021 [ncp.at] TRACE: > AT+COPS=3,2
    // 0000068029 [ncp.at] TRACE: < OK
    // 0000068029 [ncp.at] TRACE: > AT+COPS?
    // 0000068039 [ncp.at] TRACE: < +COPS: 0,2,"310410",8
    // 0000068040 [ncp.at] TRACE: < OK
    // 0000068040 [ncp.at] TRACE: > AT+UCGED=5
    // 0000068049 [ncp.at] TRACE: < OK
    // 0000068049 [ncp.at] TRACE: > AT+UCGED?
    // 0000068062 [ncp.at] TRACE: < +RSRP: 476,5110,"-096.50",
    // 0000068062 [ncp.at] TRACE: < +RSRQ: 476,5110,"-20.00",
    // 0000068063 [ncp.at] TRACE: < OK
    // col=0 token=0000068021
    // col=1 token=[ncp.at]
    // col=2 token=TRACE:
    // col=3 token=>
    // col=4 token=AT+COPS=3,2


    /* TODO: Handle this sort of response better
    0000004409 [ncp.at] TRACE: > AT+UGPIOC?
    0000004410 [app.cellinterp] INFO: recv OK
    0000004411 [app] INFO: processCommand command=AT+UGPIOC? toModem=1
    0000004412 [app.cellinterp] INFO: send command AT+UGPIOC?
    0000004422 [ncp.at] TRACE: < +UGPIOC:
    0000004423 [ncp.at] TRACE: < 16,255
    0000004423 [ncp.at] TRACE: < 19,255
    0000004424 [ncp.at] TRACE: < 23,0
    0000004425 [ncp.at] TRACE: < 24,255
    0000004426 [ncp.at] TRACE: < 25,255
    0000004426 [ncp.at] TRACE: < 42,255
    0000004427 [ncp.at] TRACE: < OK
    0000004428 [ncp.at] TRACE: > AT+UGPIOR=23
    0000004430 [app] INFO: processCommand command=+UGPIOC: toModem=0
    0000004430 [app.cellinterp] INFO: recv + +UGPIOC:
    0000004431 [app] INFO: processCommand command=16,255 toModem=0
    0000004432 [app] INFO: processCommand command=19,255 toModem=0
    0000004433 [app] INFO: processCommand command=23,0 toModem=0
    */

    // _log.info("processLine %s", lineBuffer);

    const size_t MAX_COL_TOKENS = 4;
    char *colTokens[MAX_COL_TOKENS];

    bool isGen3 = false;
    bool isLogger = false;
    bool toModem = false;
    char *command = NULL;
    char *saveptr; 

    char *token = strtok_r(lineBuffer, " ", &saveptr);
    for(size_t col = 0; token; col++) {
        if (col < MAX_COL_TOKENS) {
            colTokens[col] = token;
        }
        if (col == 0) {
            if (strcmp(token, "Socket") == 0) {
                // Gen2: Socket 0: handle 0 has 33 bytes pending
                if ((logSettings & LOG_VERBOSE) == 0) {
                    // Ignore this message
                    break;
                }
                // If LOG_FULL_AT_CMD are being logged, we'll output this
                // in the col == 1 case below.
            }
        }
        if (col == 1) {
            if (strcmp(token, "AT") == 0) {
                isGen3 = false;
            }
            else
            if (strcmp(token, "[ncp.at]") == 0) {
                isGen3 = true;
            }
            else 
            if (token[0] == '[') {
                isLogger = true;
            }
            else {
                // Not a statement we care about, ignore the rest of this line.
                char *msg = &token[strlen(token) + 1];

                logOutput(colTokens[0]);
                logOutput(' ');
                logOutput(colTokens[1]);
                logOutput(' ');
                logOutput(msg);
                logOutput('\n');
                break;
            }
        }

        if (isLogger) {
            // Non-modem logging messages:
            // 0000011877 [hal] ERROR: Failed to power off modem
            // 0000011878 [hal] TRACE: Modem already on
            // 0000011879 [hal] TRACE: Setting UART voltage translator state 1

            // 0: Timestamp
            // 1: Category ([hal], [app], etc.)
            // 2: Level (TRACE, INFO, ERROR etc.)
            // 3: rest of the message
            if (col == 2) {
                char *cp;

                // Rest of the line after this token is the message
                char *msg = &token[strlen(token) + 1];

                // Write to Serial and Serial1
                logOutput(colTokens[0]);
                logOutput(' ');
                logOutput(colTokens[1]);
                logOutput(' ');
                logOutput(colTokens[2]);
                logOutput(' ');
                logOutput(msg);
                logOutput('\n');

                // Remove the square brackets from category
                if (colTokens[1][0] == '[') {
                    cp = strrchr(colTokens[1], ']');
                    if (cp) {
                        ++colTokens[1];
                        *cp = 0;
                    }
                }

                // Remove the : from the end of the level
                cp = strrchr(colTokens[2], ':');
                if (cp) {
                    *cp = 0;
                }
            
                char *end;
                long ts = strtol(colTokens[0], &end, 10);

                // _log.info("logger found 0:%s 1:%s 2:%s msg:%s", colTokens[0], colTokens[1], colTokens[2], msg);
                CellularInterpreterLogEntry *entry = new CellularInterpreterLogEntry();
                entry->ts = ts;
                entry->category = colTokens[1];
                entry->level = colTokens[2];
                entry->message = msg;
                logQueue.push_back(entry);
                break;
            }
        }
        else
        if (isGen3) {
            // Gen3
            if (col == 3) {
                // 0000068021 [ncp.at] TRACE: > AT+COPS=3,2
                // 0000068029 [ncp.at] TRACE: < OK
                toModem = token[0] == '>';
                command = &token[2];

                // Generate debug output
                if ((logSettings & LOG_TRACE) != 0) {
                    logOutput(colTokens[0]);
                    logOutput(' ');
                    logOutput(colTokens[1]);
                    logOutput(' ');
                    logOutput(colTokens[2]);
                    logOutput(' ');
                    logOutput(colTokens[3]);
                    logOutput(' ');
                    logOutput(command);
                }
            }
        }
        else {
            // Gen2
            if (col == 2) {
                int tsWhole = atoi(String(colTokens[0]).substring(0, 6));
                int tsDec = atoi(String(colTokens[0]).substring(7));
                int ts = (tsWhole * 1000) + (tsDec % 1000);

                toModem = strcmp(token, "send") == 0;

                char *src = strchr(&token[strlen(token) + 1], '"');
                if (src) {
                    // Command begins after the first double quote
                    command = ++src;

                    // Unescape backslash escapes, remove \r and \n, and end at the unescaped double quote
                    for(char *dst = src; *src; src++) {
                        if (*src == '\\') {
                            if (*++src == '"') {
                                // Literal quote
                                *dst++ = *src;
                            }
                            else {
                                // Probably \r or \n, just ignore
                            }
                        }
                        else
                        if (*src == '"') {
                            // End of string
                            *dst = 0;
                            break;
                        }
                        else {
                            // Copy character
                            *dst++ = *src;
                        }
                    }

                    // Truncate command to a reasonable length
                    if (strlen(command) > 50) {
                        command[50] = 0;
                    }

                    // Generate debug output
                    if ((logSettings & LOG_TRACE) != 0) {
                        String msg = String::format("%010d [ncp.at] TRACE: %s %s\n", ts, (toModem ? ">" : "<"), command);
                        logOutput(msg);
                    }
                }
            }
        }

        if (command) {
            // We have a parsed command, Gen 2 or Gen 3
            // Direction is in toModem (true = to modem, false = from modem)
            // + response and OK/ERROR are issued in separate lines, so there needs to be additional 
            // surrounding state
            CellularInterpreterCommandEntry *entry = new CellularInterpreterCommandEntry();
            entry->command = command;
            entry->toModem = toModem;
            commandQueue.push_back(entry); 
            break;
        }
            
        token = strtok_r(NULL, " ", &saveptr);
    }

}

void CellularInterpreter::processCommand(const char *command, bool toModem) {
    // Note start of new command, + responses, OK/ERROR responses, and URCs here
    // _log.info("processCommand command=%s toModem=%d", command, toModem);
    if (toModem) {
        // Sending to modem
        if (ignoreNextSend) {
            // On Gen2, sending binary data with AT+USOST or AT+USOWR, see "@" below
            ignoreNextSend = false;
            return;
        }
        // _log.info("send command %s", command);        
        callCommandMonitors(CellularInterpreterModemMonitor::REASON_SEND, command); 
        lastCommand = command;
    }
    else {
        // Receiving data from modem
        if (command[0] == '+') {
            // + response to a command, or a URC
            // _log.info("recv + or URC %s", command);        

            callCommandMonitors(CellularInterpreterModemMonitor::REASON_PLUS, command); 
        }
        else 
        if (strcmp(command, "@") == 0) {
            // On Gen2, sending binary data with AT+USOST or AT+USOWR

            // Example with write binary data after @ response (AT+USOST, AT+USOWR)
            //      5.565 AT send      36 "AT+USOST=0,\"54.86.198.203\",5684,50\r\n"
            //      5.573 AT read  >    3 "\r\n@"
            //      5.573 AT send      50 "\x17\xfe\xfd\x00\x01\x00\x00\x00\x00\x00\x99\x00%\x00\x01\x00\x00\x00\x00\x00\x99.\xcc\x9dz\xec\xd54\xeb\x87\xbd{\xb2\xc0$}\x19\xf4\x11\xfc\x85\t\xb4\xe8\xae\xe5\xa6\x0e|\x15"
            //      5.709 AT read  +   16 "\r\n+USOST: 0,50\r\n"
            ignoreNextSend = true;
        }
        else 
        if (strcmp(command, "OK") == 0) {
            // _log.info("recv OK lastCommand=%s", lastCommand.c_str());        
            callCommandMonitors(CellularInterpreterModemMonitor::REASON_OK, lastCommand); 
            lastCommand = "";
        }
        else 
        if (strncmp(command, "ERROR", 5) == 0) {
            // _log.info("recv ERROR lastCommand=%s", lastCommand.c_str());       
            callCommandMonitors(CellularInterpreterModemMonitor::REASON_ERROR, lastCommand); 
            lastCommand = "";
        }
        else {
            // There are a bunch of other responses here like:
            // CONNECT, RING, NO CARRIER, NO DIALTONE, BUSY, NO ANSWER, CONNECT, ABORTED
            // however we don't need to handle those, so just ignore them
        }
        
    }
}

void CellularInterpreter::callCommandMonitors(uint32_t reasonFlags, const char *command) {
    // Check command monitors
    for (std::vector<CellularInterpreterModemMonitor *>::iterator it = commandMonitors.begin() ; it != commandMonitors.end(); ++it) {
        CellularInterpreterModemMonitor *mon = *it;
        if (isMatchingCommand(command, mon->command)) {
            // Matching command or URC
            if ((reasonFlags & CellularInterpreterModemMonitor::REASON_PLUS) != 0) {
                // A + can be a URC if there was no send
                if (mon->nextTimeout == 0) {
                    // Yes, it's a URC, change the reason from PLUS to URC
                    // _log.info("converting PLUS to URC %s", command);
                    reasonFlags &= ~CellularInterpreterModemMonitor::REASON_PLUS;
                    reasonFlags |= CellularInterpreterModemMonitor::REASON_URC;
                }
            }

            if ((reasonFlags & mon->reasonFlags) != 0) {
                // Handler is interested in this reason
                if (mon->callback) {
                    mon->callback(reasonFlags, command, mon);
                }
            }

            if ((reasonFlags & CellularInterpreterModemMonitor::REASON_SEND) != 0) {
                // Sending a command, start timeout
                mon->request.clear();
                mon->request.parse(command);
                if (mon->timeout) {
                    mon->nextTimeout = System.millis() + mon->timeout;
                }
                else {
                    mon->nextTimeout = 0;
                }
            }
            else
            if ((reasonFlags & (CellularInterpreterModemMonitor::REASON_OK | CellularInterpreterModemMonitor::REASON_ERROR)) != 0) {
                // On OK or ERROR, clear timeout
                // _log.info("clearing timeout on %s", mon->command.c_str());
                mon->nextTimeout = 0;
            }                    
        }
    }
}


void CellularInterpreter::processTimeouts() {
    for (std::vector<CellularInterpreterModemMonitor *>::iterator it = commandMonitors.begin() ; it != commandMonitors.end(); ++it) {
        CellularInterpreterModemMonitor *mon = *it;

        if (mon->nextTimeout != 0 && mon->nextTimeout < System.millis()) {
            // Timeout occurred
            _log.info("timeout %s", mon->command.c_str());
            callCommandMonitors(CellularInterpreterModemMonitor::REASON_TIMEOUT, mon->command); 

            mon->nextTimeout = 0;
        }
    }

}

void CellularInterpreter::processLog(long ts, const char *category, const char *level, const char *msg) {
    
    // _log.info("processLog testing ts=%ld category=%s level=%s msg=%s", ts, category, level, msg);

    for (std::vector<CellularInterpreterLogMonitor *>::iterator it = logMonitors.begin() ; it != logMonitors.end(); ++it) {
        CellularInterpreterLogMonitor *mon = *it;

        bool match = true;
        if (match && mon->category.length() > 0 && !mon->category.equals(category)) {
            match = false;
        }
        if (match && mon->level.length() > 0 && !mon->level.equals(level)) {
            match = false;
        }
        if (match && mon->matchString.length() > 0 && strstr(msg, mon->matchString.c_str()) == 0) {
            match = false;
        }
        if (match) {
            _log.info("processLog match ts=%ld category=%s level=%s msg=%s", ts, category, level, msg);
            mon->callback(ts, category, level, msg);
        }
    }
}

// [static]
bool CellularInterpreter::isMatchingCommand(const char *test, const char *cmd) {
    size_t start = 0;
    if (test[0] == '+') {
        // Plus response or URC
        start = 1;
    }
    else 
    if (strncmp(test, "AT+", 3) == 0) {
        // Sending command
        start = 3;
    }

    size_t cmdLen = strlen(cmd);

    if (strncmp(&test[start], cmd, cmdLen) == 0) {
        switch(test[start + cmdLen]) {
        case 0:
        case '+':
        case '?':
        case ':':
            // Is a match!
            return true;

        default:
            break;
        }
    }
    return false;
}

// [static] 
String CellularInterpreter::mapValueToString(const char *mapping, int value) {
    
    for(const char *cp = mapping; *cp;) {
        String numStr;
        while(*cp && *cp != ':') {
            numStr += *cp++;
        }
        if (!*cp++) {
            break;
        }
        while(*cp && *cp == ' ') {
            cp++;
        }
        if (!*cp) {
            break;
        }

        int num = atoi(numStr);

        // _log.info("testing numStr=%s num=%d", numStr.c_str(), value);

        if (num == value) {
            String desc;

            // This is what we're looking for
            while(*cp && *cp != '\n') {
                desc += *cp++;
            }
            return String::format("%s (%d)", desc.c_str(), value);
        }
        // Skip to next
        while(*cp && *cp++ != '\n') {
        }
    }
    // Unknown code
    return String::format("unknown %d", value);
}



CellularInterpreterBlinkPattern::CellularInterpreterBlinkPattern() {
}

CellularInterpreterBlinkPattern::~CellularInterpreterBlinkPattern() {
}

void CellularInterpreterBlinkPattern::callStart() {
    curRepeat = 0;
    start();
}

bool CellularInterpreterBlinkPattern::callRun() {
    bool done = false;

    if (run()) {
        // Done with sequence. Should we repeat?
        if (++curRepeat < repeats) {
            // Yes
            start();
        }
        else {
            // No, really done now
            done = true;
        }
    }
    return done;
}

// [static]
void CellularInterpreterBlinkPattern::setColor(uint32_t color) {
    uint8_t r, g, b;

    r = (uint8_t)(color >> 16);
    g = (uint8_t)(color >> 8);
    b = (uint8_t)color;

    RGB.color(r, g, b);
}


CellularInterpreterBlinkPatternBlink::CellularInterpreterBlinkPatternBlink() {

}

CellularInterpreterBlinkPatternBlink::~CellularInterpreterBlinkPatternBlink() {
}

CellularInterpreterBlinkPatternBlink &CellularInterpreterBlinkPatternBlink::withSlowBlink(uint32_t color, size_t blinks) {
    this->onColor = color;
    this->blinks = blinks;
    return *this;
}


void CellularInterpreterBlinkPatternBlink::start() {
    stateHandler = &CellularInterpreterBlinkPatternBlink::stateStart;
}

bool CellularInterpreterBlinkPatternBlink::run() {
    return stateHandler(*this);
}

bool CellularInterpreterBlinkPatternBlink::stateStart() {
    curBlink = 0;
    // RGB.control will be enabled and the LED is off when start() is called
    stateTime = millis();
    stateHandler = &CellularInterpreterBlinkPatternBlink::stateBeforeOff;

    return false;
}
bool CellularInterpreterBlinkPatternBlink::stateBeforeOff() {
    if (millis() - stateTime >= beforeOffMs) {
        setColor(onColor);
        stateTime = millis();
        stateHandler = &CellularInterpreterBlinkPatternBlink::stateOn;
    }
    return false;
}
bool CellularInterpreterBlinkPatternBlink::stateOn() {
    if (millis() - stateTime >= onPeriodMs) {
        setColor(offColor);
        stateTime = millis();
        stateHandler = &CellularInterpreterBlinkPatternBlink::stateOff;
    }

    return false;
}
bool CellularInterpreterBlinkPatternBlink::stateOff() {
    if (millis() - stateTime >= offPeriodMs) {
        stateTime = millis();
        if (++curBlink < blinks) {
            setColor(onColor);
            stateHandler = &CellularInterpreterBlinkPatternBlink::stateOn;
        }
        else {
            stateHandler = &CellularInterpreterBlinkPatternBlink::stateAfterOff;
        }
    }

    return false;
}
bool CellularInterpreterBlinkPatternBlink::stateAfterOff() {
    if (millis() - stateTime >= afterOffMs) {
        // Done!
        return true;
    }

    return false;
}


CellularInterpreterBlinkManager::CellularInterpreterBlinkManager() {
    instance = this;
}

CellularInterpreterBlinkManager::~CellularInterpreterBlinkManager() {

}

void CellularInterpreterBlinkManager::setup() {
}

void CellularInterpreterBlinkManager::loop() {
    if (curPattern == 0 && !patternQueue.empty()) {
        curPattern = patternQueue.front();
        patternQueue.pop_front();

        // About to start a pattern. Set direct RGB control and turn the LED off (color = black)
        RGB.control(true);
        CellularInterpreterBlinkPattern::setColor(0x000000);

        curPattern->callStart();
    }
    if (curPattern) {
        if (curPattern->callRun()) {
            // Done with this pattern
            curPattern = 0;
            if (patternQueue.empty()) {
                // No more patterns to display, restore default RGB behavior
                RGB.control(false);
            }
        }
    }
}

void CellularInterpreterBlinkManager::addBlinkPattern(CellularInterpreterBlinkPattern *pat) {
    patternQueue.push_back(pat);
}

/*
RGB_COLOR_BLUE : blue (0x000000ff)
RGB_COLOR_GREEN : green (0x0000ff00)
RGB_COLOR_CYAN : cyan (0x0000ffff)
RGB_COLOR_RED : red (0x00ff0000)
*/


//
// CellularInterpreterCheckNcpFailure
//

CellularInterpreterCheckNcpFailure::CellularInterpreterCheckNcpFailure() {
}

CellularInterpreterCheckNcpFailure::~CellularInterpreterCheckNcpFailure() {
}

void CellularInterpreterCheckNcpFailure::setup(CellularInterpreterCallback callback) {
    logMonitor.category = "hal";
    logMonitor.level = "ERROR";
    logMonitor.matchString = "No response from NCP";
    logMonitor.callback = [this, callback](long ts, const char *category, const char *level, const char *msg) {
        _log.info("CellularInterpreterCheckNcpFailure callback called");
        if (noResponseCount >= 0) {
            if (++noResponseCount >= 2) {
                _log.info("checkNcpFailure detected");
                if (callback) {
                    callback();
                }
                 CellularInterpreter::getInstance()->blinkNotification(CellularInterpreter::BLINK_NCP_FAILURE, 3);
                noResponseCount = -1;
            }
        }
    };
    
    CellularInterpreter::getInstance()->addLogMonitor(&logMonitor);
}

// [static]
CellularInterpreterCheckNcpFailure *CellularInterpreterCheckNcpFailure::check(CellularInterpreterCallback callback) {
    CellularInterpreterCheckNcpFailure *obj = new CellularInterpreterCheckNcpFailure();
    if (obj) {
        obj->setup(callback);
    }
    return obj;
}

