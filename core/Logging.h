/*+ Logging.h
 - Create for Logging, this class is designed with single instance mode.
 - To use looging, Logging level and log file should be given first.
 - Kam Lu 11.22.2016 @trendmicro
*/
#ifndef TS_CORE_LOGGING_H_
#define TS_CORE_LOGGING_H_

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <iostream>
#include <string>
#include <memory>

#define SPLIT '/'

#ifdef WINDOWS
#define SPLIT '\\'
#endif

namespace tigerso {

#define DBG_LOG(fmt,...)  Logging::getInstance()->dbgLog(fmt" <%s>@<%s:%d>", ##__VA_ARGS__, __func__, basename(__FILE__), __LINE__)
#define INFO_LOG(fmt,...) Logging::getInstance()->infoLog(fmt" <%s>@<%s:%d>", ##__VA_ARGS__, __func__, basename(__FILE__), __LINE__)
#define ERR_RETURN(ret) DBG_LOG("return value: %d, errno: %d,  %s", ret, errno, strerror(errno))

enum LOGLEVEL{
	NOLOG = 0,
	INFO = 1,
	DEBUG = 2,
	ALLLOG = 3
};

class Logging {
public:
	static Logging* getInstance();
	int setLogPath(const std::string& path);
	int setLogFile(const std::string& file);
	std::string getLogFile() const;

	int dbgLog(const char* fmt, ...);
	int infoLog(const char* fmt, ...);
	int setLevel(const std::string& le);
    ~Logging() {}

private:
	static std::unique_ptr<Logging> pInstance;
	Logging():endFmt("<__FUNC__>@__FILE__"), level(NOLOG){}
	int genLogging(int choice, const char* fmt, ...);
	std::string getTime();

	const std::string endFmt;
	std::string logPath;
	std::string logFile;
	int level;
};

} //namespace tscore

#endif // TS_CORE_LOGGING_H_
