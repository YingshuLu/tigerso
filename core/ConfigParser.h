#ifndef TS_CORE_CONFIGPARSER_H_
#define TS_CORE_CONFIGPARSER_H_

/*Common */
#include <iostream>
#include <string>
#include <map>

namespace tigerso {

class ConfigParser
{

#define SECTB  '['
#define SECTE  ']'
#define COMTB  '#'
#define KEYB   '='

/*
const int SECTION = 0;
const int COMMENT = 1;
const int KEYVAL = 2;
const int ERRLINE = -1;
*/
#define SECTION 0
#define COMMENT 1
#define KEYVAL  2
#define ERRLINE -1

public:

	static ConfigParser* getInstance();

	void setConfigPath(const std::string&);

	std::string getValueByKey(const std::string& section, const std::string& key);

	void getAllKey();

    int reload();

    ~ConfigParser() { delete pInstance; }

private:

	ConfigParser() :isFileExisted(false), isRefreshed(false){}

	ConfigParser(const std::string&);

	std::string filename;

	bool isFileExisted;

	bool isRefreshed;

	//map <section map<key, value>>
	std::map<std::string, std::map<std::string, std::string> > configMap;

	int loadConfig2Map();

	bool isCommented(const std::string& line);

	bool isVaildSetting(const std::string& line);

	bool isVaildSection(const std::string& line);

	int decideLineType(const std::string& line, std::string& content);

	static ConfigParser* pInstance;

};

}//namespace mcutil

#endif //MELTCAT_UTIL_MCCONFIGPARSER_H_
