#pragma once
#include <boost/beast/http.hpp>
#include <boost/beast.hpp>
#include <boost/asio.hpp>
#include <memory>
#include <iostream>
#include <functional>
#include <map>
#include <unordered_map>
#include "Singleton.h"
#include <json/json.h>
#include <json/value.h>
#include <json/reader.h>
//读取文件
#include <boost/filesystem.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>
#include "ConfigMgr.h"
#include <atomic>
#include "Singleton.h"
#include <queue>
#include <condition_variable>
#include  "hiredis/hiredis.h"
#include <cassert>
#include "RedisConPool.h"

//#include <mysqlx/xdevapi.h>
//#include <mysql/jdbc/mysql_connection.h>
#include <jdbc/mysql_driver.h>
#include <jdbc/mysql_connection.h>
#include <jdbc/cppconn/prepared_statement.h>
#include <jdbc/cppconn/resultset.h>
#include <jdbc/cppconn/statement.h>
#include <jdbc/cppconn/exception.h>


namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>

enum ErrorCodes {
	Success = 0,
	Error_Json = 1001,  //Json解析错误
	RPCFailed = 1002,  //RPC 请求错误
	VarifyExpired = 1003, // 验证吗过期
	VarifyCodeErr = 1004, //  验证码错误
	UserExist = 1005,       //  用户已经不存在
	PasswdErr = 1006,    // 密码错误
	EmailNotMatch = 1007,  // 邮箱不匹配
	PasswdUpFailed = 1008,  //更新密码失败
	PasswdInvalid = 1009,   //密码更新失败
	TokenInvalid = 1010,   //
	UidInvalid = 1011,  //

};
class Defer {
public:
	//
	Defer(std::function<void()> func) : func_(func) {}
	// 
	~Defer() {
		func_();
	}

private:
	std::function<void()> func_;
};




#define CODEPREFIX  "code_"