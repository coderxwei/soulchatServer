#pragma once
#include "const.h"
class HttpConnection :public std::enable_shared_from_this<HttpConnection>
{
public:
	//设置友元
	friend class LogicSystem;
	//构造函数
	HttpConnection(boost::asio::io_context &ioc);
	void Start(); //监听读写事件

	tcp::socket& GetSocket()
	{
		return _socket;
	}

	


private:
	//超时监测
	void CheckDeadLine();
	//相应请求
	void WriteResponse();
	void HandelReq();
	void PreParseGetParam();
	tcp::socket _socket;
	//缓冲区的大小
	beast::flat_buffer _bufffer{8192};
	http::request<http::dynamic_body> _request;
	http::response<http::dynamic_body> _response;
	//定义一个计时器
	net::steady_timer deadLine_{ _socket.get_executor(),std::chrono::seconds(60) };

	std::string _get_url;
	std::unordered_map<std::string, std::string> _get_params;

};

