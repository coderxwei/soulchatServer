#pragma once
#include "const.h"
#include <map>
#include <functional>
class HttpConnection;
typedef  std::function<void(std::shared_ptr<HttpConnection>)>  Httphandelr  ;
class LogicSystem :public Singleton<LogicSystem>
{
	friend class Singleton<LogicSystem>;
public:
	//~LogicSystem();
	bool HandleGet(std::string,std::shared_ptr<HttpConnection>);
	bool HandlePost(std::string, std::shared_ptr<HttpConnection>);
	void RegGet(std::string url, Httphandelr handler);
	void RegPost(std::string url, Httphandelr handler);
private:
	LogicSystem();
	std::map<std::string, Httphandelr> _post_handlers;
	std::map<std::string, Httphandelr> _get_handlers;



};