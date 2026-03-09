#include "StatusGrpcClient.h"

/// <summary>
/// 构造 StatusGrpcClient 对象，并初始化连接池。
/// </summary>
StatusGrpcClient::StatusGrpcClient()
{
	auto& gCfgMgr = ConfigMgr::Inst();
	std::string host = gCfgMgr["StatusServer"]["Host"];
	std::string port = gCfgMgr["StatusServer"]["Port"];
	///构造一个连接池 ，大小为5
	pool_.reset(new StatusConPool(5, host, port));
}

/// <summary>
/// 获得一个新的连接
/// </summary>
/// <param name="uid"></param>
/// <returns></returns>


GetChatServerRsp StatusGrpcClient::GetChatServer(int uid)
{
	///创建三个 远程过程调用时候需要传入的对象
	ClientContext context;
	GetChatServerRsp reply;
	GetChatServerReq request;
	request.set_uid(uid);
	///从连接池中获取一个连接
	auto stub = pool_->getConnection();
	/// 进行rpc调用开启GRPC的调用
	Status status = stub->GetChatServer(&context, request, &reply);
	Defer defer([&stub, this]() {
		pool_->returnConnection(std::move(stub));
		});
	if (status.ok()) {
		return reply;
	}
	else {
		reply.set_error(ErrorCodes::RPCFailed);
		return reply;
	}
}
LoginRsp StatusGrpcClient::Login(int uid, std::string token)
{
	ClientContext context;
	LoginRsp reply;
	LoginReq request;
	request.set_uid(uid);
	request.set_token(token);

	auto stub = pool_->getConnection();
	Status status = stub->Login(&context, request, &reply);
	Defer defer([&stub, this]() {
		pool_->returnConnection(std::move(stub));
		});
	if (status.ok()) {
		return reply;
	}
	else {
		reply.set_error(ErrorCodes::RPCFailed);
		return reply;
	}
}
