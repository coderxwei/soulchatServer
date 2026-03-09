#include <iostream>
#include <json/json.h>
#include <json/value.h>
#include <json/reader.h>
#include "CServer.h"
#include "ConfigMgr.h"
#include "const.h"
#include "RedisMgr.h"

//测试redis 函数
void TestRedis() {
	//连接redis 需要启动才可以进行连接
//redis默认监听端口为6387 可以再配置文件中修改
	redisContext* c = redisConnect("127.0.0.1", 6380);
	if (c->err)
	{
		printf("Connect to redisServer faile:%s\n", c->errstr);
		redisFree(c);        return;
	}
	printf("Connect to redisServer Success\n");

	const char* redis_password = "123";
	redisReply* r = (redisReply*)redisCommand(c, "AUTH %s", redis_password);
	if (r->type == REDIS_REPLY_ERROR) {
		printf("Redis认证失败！\n");
	}
	else {
		printf("Redis认证成功！\n");
	}

	//为redis设置key
	const char* command1 = "set stest1 value1";

	//执行redis命令行
	r = (redisReply*)redisCommand(c, command1);

	//如果返回NULL则说明执行失败
	if (NULL == r)
	{
		printf("Execut command1 failure\n");
		redisFree(c);        return;
	}

	//如果执行失败则释放连接
	if (!(r->type == REDIS_REPLY_STATUS && (strcmp(r->str, "OK") == 0 || strcmp(r->str, "ok") == 0)))
	{
		printf("Failed to execute command[%s]\n", command1);
		freeReplyObject(r);
		redisFree(c);        return;
	}

	//执行成功 释放redisCommand执行后返回的redisReply所占用的内存
	freeReplyObject(r);
	printf("Succeed to execute command[%s]\n", command1);

	const char* command2 = "strlen stest1";
	r = (redisReply*)redisCommand(c, command2);

	//如果返回类型不是整形 则释放连接
	if (r->type != REDIS_REPLY_INTEGER)
	{
		printf("Failed to execute command[%s]\n", command2);
		freeReplyObject(r);
		redisFree(c);        return;
	}

	//获取字符串长度
	int length = r->integer;
	freeReplyObject(r);
	printf("The length of 'stest1' is %d.\n", length);
	printf("Succeed to execute command[%s]\n", command2);

	//获取redis键值对信息
	const char* command3 = "get stest1";
	r = (redisReply*)redisCommand(c, command3);
	if (r->type != REDIS_REPLY_STRING)
	{
		printf("Failed to execute command[%s]\n", command3);
		freeReplyObject(r);
		redisFree(c);        return;
	}
	printf("The value of 'stest1' is %s\n", r->str);
	freeReplyObject(r);
	printf("Succeed to execute command[%s]\n", command3);

	const char* command4 = "get stest2";
	r = (redisReply*)redisCommand(c, command4);
	if (r->type != REDIS_REPLY_NIL)
	{
		printf("Failed to execute command[%s]\n", command4);
		freeReplyObject(r);
		redisFree(c);        return;
	}
	freeReplyObject(r);
	printf("Succeed to execute command[%s]\n", command4);

	//释放连接资源
	redisFree(c);

}
// redis 测试案例

void TestRedisMgr() {
	//assert() 是 C/C++ 标准库提供的一个宏，用来在 调试阶段检查程序假设，如果条件不满足就终止程序并报
	//assert(RedisMgr::GetInstance()->Connect("81.68.86.146", 6380));
	//assert(RedisMgr::GetInstance()->Auth("123456"));
	assert(RedisMgr::GetInstance()->Set("blogwebsite", "llfc.club"));
	std::string value = "";
	assert(RedisMgr::GetInstance()->Get("blogwebsite", value));
	assert(RedisMgr::GetInstance()->Get("nonekey", value) == false);
	assert(RedisMgr::GetInstance()->HSet("bloginfo", "blogwebsite", "llfc.club"));
	assert(RedisMgr::GetInstance()->HGet("bloginfo", "blogwebsite") != "");
	assert(RedisMgr::GetInstance()->ExistsKey("bloginfo"));
	assert(RedisMgr::GetInstance()->Del("bloginfo"));
	assert(RedisMgr::GetInstance()->Del("bloginfo"));
	assert(RedisMgr::GetInstance()->ExistsKey("bloginfo") == false);
	assert(RedisMgr::GetInstance()->LPush("lpushkey1", "lpushvalue1"));
	assert(RedisMgr::GetInstance()->LPush("lpushkey1", "lpushvalue2"));
	assert(RedisMgr::GetInstance()->LPush("lpushkey1", "lpushvalue3"));
	//assert(RedisMgr::GetInstance()->RPop("lpushkey1", value));
	//assert(RedisMgr::GetInstance()->RPop("lpushkey1", value));
//	assert(RedisMgr::GetInstance()->LPop("lpushkey1", value));
	//assert(RedisMgr::GetInstance()->LPop("lpushkey2", value) == false);
	//RedisMgr::GetInstance()->Close();
}





int main()
{
	//TestRedis();
	//TestRedisMgr();
	// 1. 读取配置
	auto& gCfgMgr =ConfigMgr::Inst();
	std::string gate_port_str = gCfgMgr["GateServer"]["Port"];
	unsigned short gate_port = atoi(gate_port_str.c_str());
	try
	{
		// 2. 初始化网络
		unsigned short port = static_cast<unsigned short>(8080); //绑定端口号
		net::io_context ioc{ 1 }; //创建一个事件循环，相当于 muduo库的eventloop
		/*
		boost::asio::signal_set 是 Boost.Asio 提供的异步信号（OS signal）封装器，用来在 io_context 上接收系统信号并以异步回调处理
		构造时传入的 ioc 表明信号处理回调将在该 io_context 所在线程中执行
		SIGINT/SIGTERM 表示关注的信号（Ctrl‑C、中止/终止），也可以用 add() 动态增加其它信号。
							 是用来处理进程终止信号
		*/
		boost::asio::signal_set signals(ioc, SIGINT, SIGTERM);
		//1.在创建 signal_set 后，必须调用 signals.async_wait(handler) 来注册异步处理器
		signals.async_wait([&ioc](const boost::system::error_code& error, int singnal_number)
			{
				//这会在收到 SIGINT 或 SIGTERM 时停止 ioc，从而触发优雅退出。
				if (error)
				{
					return;
				}
				ioc.stop();
			});
		// 3. 启动服务器

		/*
		服务器通过 boost::asio::ip::tcp::acceptor 异步监听端口，
		收到新连接后创建 HttpConnection 对象，并调用其 Start() 方法。
		
		
		*/
		
		/*
		  -通过make_shared 创建一个Csserver 对象(构造函数初始化 io_context 和监听端口)  并调用Start（）方法
				start()
					-1 -获得自身的智能指针， 就是确保异步操作未完成时，CServer 对象不会被提前析构。
					-2 -获得一个事件循环池
					-3 -创建一个HttpConnection对象 ( 初始化boost::asio::io_context& ioc) :_socket(ioc)  )
					-4 -调用accept 接受新的连接
							-1 -调用HttpConnection的start（）函数
									1-获取自身的对像
									2-调用async_read() 读取sockt
											-调用HandReq 处理请求
													-调用了LogicSystem（构造函数中 不同事件的处理的回调已经被加入到了 _post_handlers 
																		或者是_get_handlers 中只需要根据请求获取对应的处理即可）::HandleGet
																	处理具体的请求（post,get)
		
		
		*/

		std::make_shared<CServer>(ioc, port)->Start();
		std::cout << "GATE server listen on port" << port << std::endl;
		ioc.run();// // 4. 进入事件循环

	}
	
	catch (const std::exception&exp)
	{
		std::cout << "ERRoR:" << exp.what() << std::endl;
		return EXIT_FAILURE;
	}
    
}