#include <stdarg.h>
#include <time.h>
#include <sys/io.h>
#include <sys/types.h>
#include <unistd.h>
#include <iostream>
#include <string>
#include "core/Logging.h"
#include "core/tigerso.h"

namespace tigerso::core {

Logging* Logging::pInstance = NULL;

Logging* Logging::getInstance()
{
	if (pInstance == NULL)
	{
		pInstance = new Logging();
	}
	return pInstance;
}

int Logging::setLogFile(const std::string& file)
{
	std::string filepath = file;
	size_t pos = file.rfind(SPLIT);
	bool isPath = false;

	if (pos != std::string::npos){
		filepath = file.substr(0, pos);
		isPath = true;
	}

	if (access(filepath.c_str(), 0) < 0 || access(filepath.c_str(), 2) < 0)
	{
		if (isPath)
		{
			std::cout << "[Fatal]: File dir #" << filepath << "# not existed or has no write right!" << std::endl;
			return -1;
		}
		else if (access(filepath.c_str(), 0) > 0)
		{
			std::cout << "[Fatal]: File  #" << filepath << "#  has no write right!" << std::endl;
			return -1;
		}
	}

	logFile = file;
	return 0;
}

int Logging::setLogPath(const std::string& path)
{
	logPath = path;
	return 0;
}

int Logging::setLevel(const std::string& le)
{
    level = INFO;
    if(le == "enable")
    {
        level = ALLLOG;
    }
	return 0;
}

std::string Logging::getLogFile() const
{
	const size_t BUFSIZE = 256;
	time_t tm = time(0);
	char tmpBuf[BUFSIZE];
	strftime(tmpBuf, BUFSIZE, ".%Y%m%d", localtime(&tm));

	std::string fmtPrefix = logPath + "/" + LOGNAME;
	std::string secPrefix = fmtPrefix + tmpBuf;
    return secPrefix + ".log";
}

int Logging::dbgLog(const char* fmt, ...)
{
    logFile = getLogFile();
	if (!(level & DEBUG) || logFile == "")
	{
		return -1;
	}

	std::string sign = "[DEBUG]";
	std::string headStr = sign + '>' + getTime() + "<[%d] ";
	
	char headbuf[128];
	sprintf(headbuf,headStr.c_str(), getpid());

	FILE* FF = fopen(logFile.c_str(), "ab");
	if (FF == NULL)
	{
		std::cerr << "Open " << logFile << " failed" << std::endl;
		return -1;
	}

	const std::string fmtTemplate = std::string(headbuf) + fmt + '\n';

	va_list args;
	va_start(args, fmt);
	vfprintf(FF, fmtTemplate.c_str(), args);
	va_end(args);

	fclose(FF);
	return 0;
}

int Logging::infoLog(const char* fmt, ...)
{
    logFile = getLogFile();
	if (!(level & INFO) || logFile == "")
	{
		return -1;
	}

	std::string sign = "[INFO ]";
	std::string headStr = sign + '>' + getTime() + "<[%d] ";

	char headbuf[128];
	sprintf(headbuf,headStr.c_str(), getpid());

	FILE* FF = fopen(logFile.c_str(), "ab");
	if (FF == NULL)
	{
		std::cerr << "Open " << logFile << " failed" << std::endl;
		return -1;
	}

	const std::string fmtTemplate = std::string(headbuf) + fmt + '\n';

	va_list args;
	va_start(args, fmt);
	vfprintf(FF, fmtTemplate.c_str(), args);
	va_end(args);

	//fprintf(FF, " Line: %d <%s>@%s\n", __LINE__, __func__, __FILE__);
	fclose(FF);
	return 0;
}

std::string Logging::getTime()
{
	const size_t BUFSIZE = 256;
	time_t tm = time(0);
	char tmpBuf[BUFSIZE];
	strftime(tmpBuf, BUFSIZE, "%H:%M:%S", localtime(&tm));

	return tmpBuf;
}

} // namespace tscore
