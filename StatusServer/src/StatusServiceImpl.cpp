#include "StatusServiceImpl.h"
#include "ConfigMgr.h"
#include "const.h"
#include "RedisMgr.h"
#include <climits>

std::string generate_unique_string() {
	// ����UUID����
	boost::uuids::uuid uuid = boost::uuids::random_generator()();

	// ��UUIDת��Ϊ�ַ���
	std::string unique_string = to_string(uuid);

	return unique_string;
}

Status StatusServiceImpl::GetChatServer(ServerContext* context, const GetChatServerReq* request, GetChatServerRsp* reply)
{
	const auto& server = getChatServer();
	if (server.name.empty()) {
		std::cout << "No chat server available" << std::endl;
		reply->set_error(ErrorCodes::RPCFailed);
		return Status::OK;
	}
	reply->set_host(server.host);
	reply->set_port(server.port);
	reply->set_error(ErrorCodes::Success);
	reply->set_token(generate_unique_string());
	insertToken(request->uid(), reply->token());
	return Status::OK;
}

StatusServiceImpl::StatusServiceImpl()
{	///��ȡconfig.inid��������Ϣ
	auto& cfg = ConfigMgr::Inst();
	auto server_list = cfg["chatservers"]["Name"];

	std::vector<std::string> words;

	std::stringstream ss(server_list);
	std::string word;
	///����������������ƽ����и �Զ���Ϊ�ָ��� ����chatServer1,chatServer2  
	while (std::getline(ss, word, ',')) {
		words.push_back(word);
	}

	for (auto& word : words) {
		if (cfg[word]["Name"].empty()) {
			continue;
		}

		ChatServer server;
		server.port = cfg[word]["Port"];
		server.host = cfg[word]["Host"];
		server.name = cfg[word]["Name"];
		_servers[server.name] = server;
	}

}

ChatServer StatusServiceImpl::getChatServer() {
	std::lock_guard<std::mutex> guard(_server_mtx);
	if (_servers.empty()) {
		return ChatServer();
	}
	auto minServer = _servers.begin()->second;

	// 读取 minServer 的在线人数，key 不存在说明该节点已宕机
	std::string min_count_str = "";
	bool min_exist = RedisMgr::GetInstance()->Get(IPCOUNTPREFIX + minServer.name, min_count_str);
	minServer.con_count = min_exist ? std::stoi(min_count_str) : INT_MAX;

	// 遍历其余节点，选出连接数最小的服务器
	for (auto& server : _servers) {
		if (server.second.name == minServer.name) {
			continue;
		}

		std::string count_str = "";
		bool exist = RedisMgr::GetInstance()->Get(IPCOUNTPREFIX + server.second.name, count_str);
		server.second.con_count = exist ? std::stoi(count_str) : INT_MAX;

		if (server.second.con_count < minServer.con_count) {
			minServer = server.second;
		}
	}

	// 所有节点均不可用
	if (minServer.con_count == INT_MAX) {
		return ChatServer();
	}

	return minServer;
}
/// <summary>
/// ���ݿͻ����ύ�� uid �� token ȥ Redis��֤�û��ĺϷ���
/// </summary>
/// <param name="context"></param>
/// <param name="request"></param>
/// <param name="reply"></param>
/// <returns></returns>
Status StatusServiceImpl::Login(ServerContext* context, const LoginReq* request, LoginRsp* reply)
{
	auto uid = request->uid();
	auto token = request->token();

	std::string uid_str = std::to_string(uid);
	std::string token_key = USERTOKENPREFIX + uid_str;
	std::string token_value = "";
	bool success = RedisMgr::GetInstance()->Get(token_key, token_value);
	if (!success) {
		// token key 不存在，uid 无效
		reply->set_error(ErrorCodes::UidInvalid);
		return Status::OK;
	}

	if (token_value != token) {
		reply->set_error(ErrorCodes::TokenInvalid);
		return Status::OK;
	}
	reply->set_error(ErrorCodes::Success);
	reply->set_uid(uid);
	reply->set_token(token);
	return Status::OK;
}

void StatusServiceImpl::insertToken(int uid, std::string token)
{
	std::string uid_str = std::to_string(uid);
	std::string token_key = USERTOKENPREFIX + uid_str;
	RedisMgr::GetInstance()->Set(token_key, token);
	// Token 有效期 7 天
	RedisMgr::GetInstance()->Expire(token_key, 7 * 24 * 3600);
}

