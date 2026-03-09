#include "CServer.h"
#include "HttpConnection.h"
#include "AsioIOServicePool.h"
CServer::CServer(boost::asio::io_context& ioc, unsigned short& port):_ioc(ioc),
_acceptor(ioc, tcp::endpoint(tcp::v4(), port))
{
	// 构造函数初始化 io_context 和监听端口

}
void CServer::Start()
{

	//捕获自身（auto self = shared_from_this(); 并在 lambda 里 [self, ...]）的目的，
	// 就是确保异步操作未完成时，CServer 对象不会被提前析构。
	auto self = shared_from_this();
	auto& io_contex = AsioIOServicePool::GetInstance()->GetIOService();
	/*
	1.	std::make_shared<HttpConnection>(io_contex)
		会调用 HttpConnection 的构造函数，传入 io_contex 参数。
		自动在堆上分配 HttpConnection 对象，并分配引用计数控制块。
	*/


	std::shared_ptr<HttpConnection>new_con = std::make_shared<HttpConnection>(io_contex);
	_acceptor.async_accept(new_con->GetSocket(), [self,new_con](beast::error_code ec) {
		try
		{
			// 如果有错误，递归调用 Start() 继续监听
			if (ec)
			{
				self->Start();  //就是Cerver 的start()函数
				return;
			}
			// 创建新的连接对象并启动处理
			new_con->Start();
			//继续进行监听
			self->Start();

		}
		catch (const std::exception& exp)
		{
			// 异常处理（此处未输出日志）
			std::cout << "exception is " << exp.what() << std::endl;
			self->Start();
		}
		});

}