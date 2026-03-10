#include "LogicSystem.h"
#include "StatusGrpcClient.h"
#include "MysqlMgr.h"
#include "const.h"
#include "RedisMgr.h"
#include "UserMgr.h"
#include "ChatGrpcClient.h"
#include "DistLock.h"
#include <string>
#include "CServer.h"
using namespace std;

LogicSystem::LogicSystem() : _b_stop(false), _p_server(nullptr)
{
	RegisterCallBacks();
	_worker_thread = std::thread(&LogicSystem::DealMsg, this);
}

LogicSystem::~LogicSystem()
{
	_b_stop = true;
	_consume.notify_one();
	_worker_thread.join();
}

void LogicSystem::PostMsgToQue(shared_ptr<LogicNode> msg)
{
	std::unique_lock<std::mutex> unique_lk(_mutex);
	// 表示加入到消息队列中
	_msg_que.push(msg);
	// 由0变为1则发送通知信号
	if (_msg_que.size() == 1)
	{
		unique_lk.unlock();
		_consume.notify_one();
	}
}

void LogicSystem::SetServer(std::shared_ptr<CServer> pserver)
{
	_p_server = pserver;
}

void LogicSystem::DealMsg()
{
	for (;;)
	{
		std::unique_lock<std::mutex> uniquelock(_mutex);
		// 当消息队列为空并且服务器没有停止的时候，dealMsg会一直的睡眠，直到被唤醒为止(notify-all或者被notify-one)
		// 采用while的方式是为了防止虚假唤醒。
		while (_msg_que.empty() && !_b_stop)
		{
			_consume.wait(uniquelock);
		}

		// 判断是否为关闭状态，把所有逻辑执行完后则退出循环

		if (_b_stop)
		{
			while (!_msg_que.empty())
			{
				// 从消息队列中取出对应的消息
				auto msg_node = _msg_que.front();
				cout << "recv_msg id  is " << msg_node->_recvnode->_msg_id << endl;
				// 通过消息id 获取对应的回调。
				auto call_back_iter = _fun_callbacks.find(msg_node->_recvnode->_msg_id);
				if (call_back_iter == _fun_callbacks.end())
				{
					_msg_que.pop();
					continue;
				}
				call_back_iter->second(msg_node->_session, msg_node->_recvnode->_msg_id,
									   std::string(msg_node->_recvnode->_data, msg_node->_recvnode->_cur_len));
				_msg_que.pop();
			}
			break;
		}

		// 如果没有停服，且说明队列中有数据
		auto msg_node = _msg_que.front();
		cout << "recv_msg id  is " << msg_node->_recvnode->_msg_id << endl;

		/// 之前LogicSystem::RegisterCallBacks()中已经将各个消息id和对应的处理函数绑定到
		///_fun_callbacks中
		///  通过消息id找到对应的处理函数并调用
		auto call_back_iter = _fun_callbacks.find(msg_node->_recvnode->_msg_id);

		/// 如果没有找到对应的处理函数则打印日志并丢弃该消息

		if (call_back_iter == _fun_callbacks.end())
		{
			_msg_que.pop();
			std::cout << "msg id [" << msg_node->_recvnode->_msg_id << "] handler not found" << std::endl;
			continue;
		}

		// 调用对应消息类型的业务处理函数
		call_back_iter->second(msg_node->_session, msg_node->_recvnode->_msg_id,
							   std::string(msg_node->_recvnode->_data, msg_node->_recvnode->_cur_len));
		_msg_que.pop();
	}
}

void LogicSystem::RegisterCallBacks()
{
	_fun_callbacks[MSG_CHAT_LOGIN] = std::bind(&LogicSystem::LoginHandler, this,
											   placeholders::_1, placeholders::_2, placeholders::_3);

	// 添加发送搜索用户请求的处理
	_fun_callbacks[ID_SEARCH_USER_REQ] = std::bind(&LogicSystem::SearchInfo, this,
												   placeholders::_1, placeholders::_2, placeholders::_3);

	// 添加好友请求的处理
	_fun_callbacks[ID_ADD_FRIEND_REQ] = std::bind(&LogicSystem::AddFriendApply, this,
												  placeholders::_1, placeholders::_2, placeholders::_3);

	// 服务器认证的处理
	_fun_callbacks[ID_AUTH_FRIEND_REQ] = std::bind(&LogicSystem::AuthFriendApply, this,
												   placeholders::_1, placeholders::_2, placeholders::_3);

	_fun_callbacks[ID_TEXT_CHAT_MSG_REQ] = std::bind(&LogicSystem::DealChatTextMsg, this,
													 placeholders::_1, placeholders::_2, placeholders::_3);

	_fun_callbacks[ID_MSG_HISTORY_REQ] = std::bind(&LogicSystem::DealMsgHistoryReq, this,
												   placeholders::_1, placeholders::_2, placeholders::_3);

	_fun_callbacks[ID_HEART_BEAT_REQ] = std::bind(&LogicSystem::HeartBeatHandler, this,
												  placeholders::_1, placeholders::_2, placeholders::_3);
}

/// 用户登录的处理函数

void LogicSystem::LoginHandler(shared_ptr<CSession> session, const short &msg_id, const string &msg_data)
{
	Json::Reader reader;
	Json::Value root;
	reader.parse(msg_data, root);
	auto uid = root["uid"].asInt();
	auto token = root["token"].asString();
	std::cout << "user login uid is  " << uid << " user token  is "
			  << token << endl;

	Json::Value rtvalue;
	Defer defer([this, &rtvalue, session]()
				{
		std::string return_str = rtvalue.toStyledString();
		/// 发送消息
		session->Send(return_str, MSG_CHAT_LOGIN_RSP); });

	// 从redis获取用户token是否正确
	std::string uid_str = std::to_string(uid);
	std::string token_key = USERTOKENPREFIX + uid_str;
	std::string token_value = "";
	bool success = RedisMgr::GetInstance()->Get(token_key, token_value);
	if (!success)
	{
		rtvalue["error"] = ErrorCodes::UidInvalid;
		return;
	}

	if (token_value != token)
	{
		rtvalue["error"] = ErrorCodes::TokenInvalid;
		return;
	}

	rtvalue["error"] = ErrorCodes::Success;

	//--------------------------------------------------------------------------------
	std::string base_key = USER_BASE_INFO + uid_str;
	auto user_info = std::make_shared<UserInfo>();
	bool b_base = GetBaseInfo(base_key, uid, user_info);
	if (!b_base)
	{
		rtvalue["error"] = ErrorCodes::UidInvalid;
		return;
	}
	rtvalue["uid"] = uid;
	rtvalue["pwd"] = user_info->pwd;
	rtvalue["name"] = user_info->name;
	rtvalue["email"] = user_info->email;
	rtvalue["nick"] = user_info->nick;
	rtvalue["desc"] = user_info->desc;
	rtvalue["sex"] = user_info->sex;
	rtvalue["icon"] = user_info->icon;

	// 从数据库获取申请列表- 因为不但要返回申请人的信息，可能有多个申请人所以要返回一个申请列表

	// 1-定义一个 std::vector<std::shared_ptr<ApplyInfo>> apply_list; 用于存放当前用户收到的所有好友申请信息。
	/*
	 *	每个 ApplyInfo 对象代表一个好友申请，包含了申请人的相关信息（如用户ID、昵称、头像、性别、备注、申请说明、申请状态等）。
		这些信息是从数据库（如 MySQL）查询出来的，表示“谁向当前用户发起了好友申请”
	 *
	 *
	 */
	std::vector<std::shared_ptr<ApplyInfo>> apply_list;
	// 2-从数据库（通常是 MySQL）查询该用户的好友申请列表，并将结果填充到 apply_list 中。
	auto b_apply = GetFriendApplyInfo(uid, apply_list);
	if (b_apply)
	{

		// 遍历 apply_list 中的每个申请信息，将其转换为 JSON 对象，
		// 并添加到 rtvalue["apply_list"] 数组中。
		for (auto &apply : apply_list)
		{
			Json::Value obj;
			obj["name"] = apply->_name;
			obj["uid"] = apply->_uid;
			obj["icon"] = apply->_icon;
			obj["nick"] = apply->_nick;
			obj["sex"] = apply->_sex;
			obj["desc"] = apply->_desc;
			obj["status"] = apply->_status;
			rtvalue["apply_list"].append(obj); // 添加一个申请人列表的字段
		}
	}

	// 获取好友列表
	std::vector<std::shared_ptr<UserInfo>> friend_list;
	bool b_friend_list = GetFriendList(uid, friend_list);
	for (auto &friend_ele : friend_list)
	{
		Json::Value obj;
		obj["name"] = friend_ele->name;
		obj["uid"] = friend_ele->uid;
		obj["icon"] = friend_ele->icon;
		obj["nick"] = friend_ele->nick;
		obj["sex"] = friend_ele->sex;
		obj["desc"] = friend_ele->desc;
		obj["back"] = friend_ele->back;
		rtvalue["friend_list"].append(obj);
	}

	auto &cfg = ConfigMgr::Inst();
	auto server_name = cfg["SelfServer"]["Name"];

	// session绑定用户uid（本地操作，不需要分布式锁）
	session->SetUserId(uid);

	// 使用带锁的方式更新登录计数，防止并发写冲突
	RedisMgr::GetInstance()->IncreaseCount(server_name);

	// 分布式锁：保证"检查旧登录 -> 踢人 -> 写入新状态"是原子操作
	// 防止同一用户在多端并发登录时出现状态不一致
	auto lock_key = LOCK_PREFIX + uid_str;
	auto identifier = RedisMgr::GetInstance()->acquireLock(lock_key, LOCK_TIME_OUT, ACQUIRE_TIME_OUT);

	// 这个是最后的执行释放锁的操作。
	Defer defer2([identifier, lock_key]()
				 { RedisMgr::GetInstance()->releaseLock(lock_key, identifier); });

	// 查询该用户是否已在某台服务器上登录
	std::string uid_ip_value = "";
	auto uid_ip_key = USERIPPREFIX + uid_str;
	bool b_ip = RedisMgr::GetInstance()->Get(uid_ip_key, uid_ip_value);
	if (b_ip)
	{
		// 用户已登录，需要踢掉旧连接
		if (uid_ip_value == server_name)
		{
			// 旧连接在本服务器，直接踢掉
			auto old_session = UserMgr::GetInstance()->GetSession(uid);
			if (old_session)
			{
				old_session->NotifyOffline(uid);
				_p_server->ClearSession(old_session->GetSessionId());
			}
		}
		else
		{
			// 旧连接在其他服务器，通过gRPC通知对端踢掉
			KickUserReq kick_req;
			kick_req.set_uid(uid);
			ChatGrpcClient::GetInstance()->NotifyKickUser(uid_ip_value, kick_req);
		}
	}

	// 写入新的登录状态 要设置过期时间防止，服务器宕机后要清理掉已经在线的用户。
	std::string ipkey = USERIPPREFIX + uid_str;

	int expaire_time=90;

	RedisMgr::GetInstance()->Set(ipkey, server_name,expaire_time);

	// 记录session id，方便后续踢人定位
	std::string uid_session_key = USER_SESSION_PREFIX + uid_str;

	RedisMgr::GetInstance()->Set(uid_session_key, session->GetSessionId(),expaire_time);

	// uid和session绑定管理
	UserMgr::GetInstance()->SetUserSession(uid, session);

	// 拉取并推送未读消息（登录后补齐离线期间收到的消息）
	long long last_seq = MysqlMgr::GetInstance()->GetMsgCursor(uid);
	std::vector<TextMsg> unread_msgs;
	MysqlMgr::GetInstance()->GetUnreadMessages(uid, last_seq, unread_msgs);
	if (!unread_msgs.empty()) {
		Json::Value offline_val;
		offline_val["error"] = ErrorCodes::Success;
		long long max_seq = last_seq;
		for (auto &msg : unread_msgs) {
			Json::Value obj;
			obj["msgid"]    = msg.msg_id;
			obj["conv_id"]  = msg.conv_id;
			obj["fromuid"]  = msg.from_uid;
			obj["touid"]    = msg.to_uid;
			obj["content"]  = msg.content;
			obj["msg_type"] = msg.msg_type;
			obj["seq"]      = (Json::Int64)msg.seq;
			offline_val["messages"].append(obj);
			if (msg.seq > max_seq) max_seq = msg.seq;
		}
		session->Send(offline_val.toStyledString(), ID_OFFLINE_MSG_RSP);
		MysqlMgr::GetInstance()->UpdateMsgCursor(uid, max_seq);
	}

	return;
}

void LogicSystem::SearchInfo(std::shared_ptr<CSession> session, const short &msg_id, const string &msg_data)
{
	Json::Reader reader;
	Json::Value root;
	reader.parse(msg_data, root);
	auto uid_str = root["uid"].asString();
	std::cout << "user SearchInfo uid is  " << uid_str << endl;

	Json::Value rtvalue;

	Defer defer([this, &rtvalue, session]()
				{
		//序列为化为字符串发送
		std::string return_str = rtvalue.toStyledString();
		session->Send(return_str, ID_SEARCH_USER_RSP); });

	bool b_digit = isPureDigit(uid_str);
	if (b_digit)
	{
		/// 按uid查找
		GetUserByUid(uid_str, rtvalue);
	}
	else
	{
		/// 按名字查找
		GetUserByName(uid_str, rtvalue);
	}
	return;
}

/// 添加好友申请
void LogicSystem::AddFriendApply(std::shared_ptr<CSession> session, const short &msg_id, const string &msg_data)
{
	Json::Reader reader;
	Json::Value root;
	reader.parse(msg_data, root);
	auto uid = root["uid"].asInt();
	auto applyname = root["applyname"].asString();
	auto bakname = root["bakname"].asString();
	auto touid = root["touid"].asInt();
	std::cout << "user login uid is  " << uid << " applyname  is "
			  << applyname << " bakname is " << bakname << " touid is " << touid << endl;

	Json::Value rtvalue;
	rtvalue["error"] = ErrorCodes::Success;
	Defer defer([this, &rtvalue, session]()
				{
		std::string return_str = rtvalue.toStyledString();
		session->Send(return_str, ID_ADD_FRIEND_RSP); });

	// 先更新数据库
	MysqlMgr::GetInstance()->AddFriendApply(uid, touid);

	// 查询redis 查找touid对应的server ip
	auto to_str = std::to_string(touid);
	auto to_ip_key = USERIPPREFIX + to_str;
	std::string to_ip_value = "";
	// 没有找到说明对方不在线，直接返回
	bool b_ip = RedisMgr::GetInstance()->Get(to_ip_key, to_ip_value);
	if (!b_ip)
	{
		return;
	}

	auto &cfg = ConfigMgr::Inst();
	auto self_name = cfg["SelfServer"]["Name"];

	std::string base_key = USER_BASE_INFO + std::to_string(uid);
	auto apply_info = std::make_shared<UserInfo>();
	bool b_info = GetBaseInfo(base_key, uid, apply_info);

	/// 对方的服务器和自己在同一台服务器上---就直接通知对方
	if (to_ip_value == self_name)
	{
		auto session = UserMgr::GetInstance()->GetSession(touid);
		if (session)
		{
			// 在内存中则直接发送通知对方
			Json::Value notify;
			notify["error"] = ErrorCodes::Success;
			notify["applyuid"] = uid;
			notify["name"] = applyname;
			notify["desc"] = "";
			if (b_info)
			{
				notify["icon"] = apply_info->icon;
				notify["sex"] = apply_info->sex;
				notify["nick"] = apply_info->nick;
			}
			std::string return_str = notify.toStyledString();
			session->Send(return_str, ID_NOTIFY_ADD_FRIEND_REQ);
		}

		return;
	}

	///--------------------对方与自己不在同一台服务器- 发送GRpc 请求通知对端的服务器-由对端的服务器通知对方
	AddFriendReq add_req;
	add_req.set_applyuid(uid);
	add_req.set_touid(touid);
	add_req.set_name(applyname);
	add_req.set_desc("");
	if (b_info)
	{
		add_req.set_icon(apply_info->icon);
		add_req.set_sex(apply_info->sex);
		add_req.set_nick(apply_info->nick);
	}

	/// 发送添加好友GRPC请求，  由ChatServiceImpl::NotifyAddFriend处理
	ChatGrpcClient::GetInstance()->NotifyAddFriend(to_ip_value, add_req);
}

void LogicSystem::AuthFriendApply(std::shared_ptr<CSession> session, const short &msg_id, const string &msg_data)
{

	Json::Reader reader;
	Json::Value root;
	reader.parse(msg_data, root);

	auto uid = root["fromuid"].asInt();		  // 申请人的uid
	auto touid = root["touid"].asInt();		  // 被申请人的id
	auto back_name = root["back"].asString(); // 备注名
	std::cout << "from " << uid << " auth friend to " << touid << std::endl;

	Json::Value rtvalue;
	rtvalue["error"] = ErrorCodes::Success;
	auto user_info = std::make_shared<UserInfo>();

	std::string base_key = USER_BASE_INFO + std::to_string(touid); // 获取被申请人的信息
	bool b_info = GetBaseInfo(base_key, touid, user_info);		   // 获得结果存储在user_info中
	if (b_info)
	{
		rtvalue["name"] = user_info->name;
		rtvalue["nick"] = user_info->nick;
		rtvalue["icon"] = user_info->icon;
		rtvalue["sex"] = user_info->sex;
		rtvalue["uid"] = touid;
	}
	else
	{
		rtvalue["error"] = ErrorCodes::UidInvalid;
	}

	Defer defer([this, &rtvalue, session]()
				{
		std::string return_str = rtvalue.toStyledString();
		session->Send(return_str, ID_AUTH_FRIEND_RSP); });

	// 先更新数据库
	MysqlMgr::GetInstance()->AuthFriendApply(uid, touid);

	// 更新数据库添加好友
	MysqlMgr::GetInstance()->AddFriend(uid, touid, back_name);

	// 查询redis 查找touid对应的server ip
	auto to_str = std::to_string(touid);
	auto to_ip_key = USERIPPREFIX + to_str;
	std::string to_ip_value = "";
	bool b_ip = RedisMgr::GetInstance()->Get(to_ip_key, to_ip_value); // 获得对方服务器的名字
	if (!b_ip)
	{
		return;
	}

	auto &cfg = ConfigMgr::Inst();
	auto self_name = cfg["SelfServer"]["Name"];
	// 表示 申请人和被申请人在同一台服务器上
	if (to_ip_value == self_name)
	{
		auto session = UserMgr::GetInstance()->GetSession(touid);
		if (session)
		{
			// 在内存中则直接发送通知对方
			Json::Value notify;
			notify["error"] = ErrorCodes::Success;
			notify["fromuid"] = uid;
			notify["touid"] = touid;
			std::string base_key = USER_BASE_INFO + std::to_string(uid);
			auto user_info = std::make_shared<UserInfo>();
			bool b_info = GetBaseInfo(base_key, uid, user_info);
			if (b_info)
			{
				notify["name"] = user_info->name;
				notify["nick"] = user_info->nick;
				notify["icon"] = user_info->icon;
				notify["sex"] = user_info->sex;
			}
			else
			{
				notify["error"] = ErrorCodes::UidInvalid;
			}

			std::string return_str = notify.toStyledString();
			/// 通知对端客户端---添加好友的情况
			session->Send(return_str, ID_NOTIFY_AUTH_FRIEND_REQ);
		}

		return;
	}

	AuthFriendReq auth_req;
	auth_req.set_fromuid(uid);
	auth_req.set_touid(touid);

	// 发送通知
	ChatGrpcClient::GetInstance()->NotifyAuthFriend(to_ip_value, auth_req);
}

void LogicSystem::DealChatTextMsg(std::shared_ptr<CSession> session, const short &msg_id, const string &msg_data)
{
	Json::Reader reader;
	Json::Value root;
	reader.parse(msg_data, root);

	auto uid   = root["fromuid"].asInt();
	auto touid = root["touid"].asInt();
	const Json::Value arrays = root["text_array"];

	Json::Value rtvalue;
	rtvalue["error"]   = ErrorCodes::Success;
	rtvalue["fromuid"] = uid;
	rtvalue["touid"]   = touid;

	Defer defer([this, &rtvalue, session]() {
		session->Send(rtvalue.toStyledString(), ID_TEXT_CHAT_MSG_RSP);
	});

	auto &cfg      = ConfigMgr::Inst();
	auto self_name = cfg["SelfServer"]["Name"];

	// conv_id：保证同一会话两端相同
	std::string conv_id = std::to_string(std::min(uid, touid))
	                    + "_"
	                    + std::to_string(std::max(uid, touid));
	std::string seq_key = CONV_SEQ_PREFIX + conv_id;
    // 执行服务器落库操作。


	// 逐条落库（写库失败则整体返回错误，不推送）
	Json::Value saved_array;
	for (const auto &txt_obj : arrays) {
		TextMsg msg;
		msg.msg_id   = txt_obj["msgid"].asString();
		msg.conv_id  = conv_id;
		msg.from_uid = uid;
		msg.to_uid   = touid;
		msg.content  = txt_obj["content"].asString();
		msg.msg_type = 1;
		msg.seq      = RedisMgr::GetInstance()->Incr(seq_key);  // 原子自增
		// 保存消息
		if (!MysqlMgr::GetInstance()->SaveMessage(msg)) {
			rtvalue["error"] = ErrorCodes::RPCFailed;
			return;
		}

		Json::Value obj;
		obj["msgid"]   = msg.msg_id;
		obj["content"] = msg.content;
		obj["seq"]     = (Json::Int64)msg.seq;
		saved_array.append(obj);
	}
	rtvalue["text_array"] = saved_array;

	// 查询 touid 所在服务器
	auto to_ip_key = USERIPPREFIX + std::to_string(touid);
	std::string to_ip_value = "";
	bool b_ip = RedisMgr::GetInstance()->Get(to_ip_key, to_ip_value);
	if (!b_ip) {
		// 对方离线：消息已入库，等登录时拉取
		return;
	}

	if (to_ip_value == self_name) {
		// 同服务器：直接推送
		auto to_session = UserMgr::GetInstance()->GetSession(touid);
		if (to_session) {
			to_session->Send(rtvalue.toStyledString(), ID_NOTIFY_TEXT_CHAT_MSG_REQ);
		}
		return;
	}

	// 跨服务器：gRPC 转发
	TextChatMsgReq text_msg_req;
	text_msg_req.set_fromuid(uid);
	text_msg_req.set_touid(touid);
	for (const auto &obj : saved_array) {
		auto *text_msg = text_msg_req.add_textmsgs();
		text_msg->set_msgid(obj["msgid"].asString());
		text_msg->set_msgcontent(obj["content"].asString());
	}
	ChatGrpcClient::GetInstance()->NotifyTextChatMsg(to_ip_value, text_msg_req, rtvalue);
}

void LogicSystem::DealMsgHistoryReq(std::shared_ptr<CSession> session, const short &msg_id, const string &msg_data)
{
	Json::Reader reader;
	Json::Value root;
	reader.parse(msg_data, root);

	// 客户端请求：{ "conv_id": "1001_1002", "before_seq": 100, "limit": 50 }
	std::string conv_id  = root["conv_id"].asString();
	long long before_seq = root["before_seq"].asInt64();
	int limit            = root["limit"].asInt();
	if (limit <= 0 || limit > 50) limit = 50;

	Json::Value rtvalue;
	rtvalue["error"]   = ErrorCodes::Success;
	rtvalue["conv_id"] = conv_id;

	Defer defer([&rtvalue, session]() {
		session->Send(rtvalue.toStyledString(), ID_MSG_HISTORY_RSP);
	});

	std::vector<TextMsg> msgs;
	MysqlMgr::GetInstance()->GetHistoryMessages(conv_id, before_seq, limit, msgs);

	for (auto &msg : msgs) {
		Json::Value obj;
		obj["msgid"]      = msg.msg_id;
		obj["fromuid"]    = msg.from_uid;
		obj["touid"]      = msg.to_uid;
		obj["content"]    = msg.content;
		obj["msg_type"]   = msg.msg_type;
		obj["seq"]        = (Json::Int64)msg.seq;
		obj["created_at"] = msg.created_at;
		rtvalue["messages"].append(obj);
	}
}

void LogicSystem::HeartBeatHandler(std::shared_ptr<CSession> session, const short &msg_id, const string &msg_data)
{
	Json::Reader reader;
	Json::Value root;
	reader.parse(msg_data, root);
	auto uid = root["fromuid"].asInt();
	std::cout << "receive heart beat msg, uid is " << uid << std::endl;
	Json::Value rtvalue;
	rtvalue["error"] = ErrorCodes::Success;
	session->Send(rtvalue.toStyledString(), ID_HEARTBEAT_RSP);
}

bool LogicSystem::isPureDigit(const std::string &str)
{
	for (char c : str)
	{
		if (!std::isdigit(c))
		{
			return false;
		}
	}
	return true;
}

void LogicSystem::GetUserByUid(std::string uid_str, Json::Value &rtvalue)
{
	rtvalue["error"] = ErrorCodes::Success;

	std::string base_key = USER_BASE_INFO + uid_str;

	// 优先查redis中查询用户信息
	std::string info_str = "";
	bool b_base = RedisMgr::GetInstance()->Get(base_key, info_str);
	if (b_base)
	{
		Json::Reader reader;
		Json::Value root;
		reader.parse(info_str, root);
		auto uid = root["uid"].asInt();
		auto name = root["name"].asString();
		auto pwd = root["pwd"].asString();
		auto email = root["email"].asString();
		auto nick = root["nick"].asString();
		auto desc = root["desc"].asString();
		auto sex = root["sex"].asInt();
		auto icon = root["icon"].asString();
		std::cout << "user  uid is  " << uid << " name  is "
				  << name << " pwd is " << pwd << " email is " << email << " icon is " << icon << endl;

		rtvalue["uid"] = uid;
		rtvalue["pwd"] = pwd;
		rtvalue["name"] = name;
		rtvalue["email"] = email;
		rtvalue["nick"] = nick;
		rtvalue["desc"] = desc;
		rtvalue["sex"] = sex;
		rtvalue["icon"] = icon;
		return;
	}

	auto uid = std::stoi(uid_str);
	// redis中没有则查询mysql
	// 查询数据库
	std::shared_ptr<UserInfo> user_info = nullptr;
	user_info = MysqlMgr::GetInstance()->GetUser(uid);
	if (user_info == nullptr)
	{
		rtvalue["error"] = ErrorCodes::UidInvalid;
		return;
	}

	// 将数据库内容写入redis缓存
	Json::Value redis_root;
	redis_root["uid"] = user_info->uid;
	redis_root["pwd"] = user_info->pwd;
	redis_root["name"] = user_info->name;
	redis_root["email"] = user_info->email;
	redis_root["nick"] = user_info->nick;
	redis_root["desc"] = user_info->desc;
	redis_root["sex"] = user_info->sex;
	redis_root["icon"] = user_info->icon;
	// 写入redis
	RedisMgr::GetInstance()->Set(base_key, redis_root.toStyledString());

	// 返回数据
	rtvalue["uid"] = user_info->uid;
	rtvalue["pwd"] = user_info->pwd;
	rtvalue["name"] = user_info->name;
	rtvalue["email"] = user_info->email;
	rtvalue["nick"] = user_info->nick;
	rtvalue["desc"] = user_info->desc;
	rtvalue["sex"] = user_info->sex;
	rtvalue["icon"] = user_info->icon;
}
// 按名字查找用户
void LogicSystem::GetUserByName(std::string name, Json::Value &rtvalue)
{
	rtvalue["error"] = ErrorCodes::Success;

	std::string base_key = NAME_INFO + name;

	/// 优先查redis中查询用户信息
	std::string info_str = "";
	bool b_base = RedisMgr::GetInstance()->Get(base_key, info_str);
	if (b_base)
	{
		Json::Reader reader;
		Json::Value root;
		reader.parse(info_str, root);
		auto uid = root["uid"].asInt();
		auto name = root["name"].asString();
		auto pwd = root["pwd"].asString();
		auto email = root["email"].asString();
		auto nick = root["nick"].asString();
		auto desc = root["desc"].asString();
		auto sex = root["sex"].asInt();
		std::cout << "user  uid is  " << uid << " name  is "
				  << name << " pwd is " << pwd << " email is " << email << endl;

		rtvalue["uid"] = uid;
		rtvalue["pwd"] = pwd;
		rtvalue["name"] = name;
		rtvalue["email"] = email;
		rtvalue["nick"] = nick;
		rtvalue["desc"] = desc;
		rtvalue["sex"] = sex;
		return;
	}

	///  走到这里表示redis中没有，则查询mysql---是通过名字查找用户 （上面的GetUerbyid是通过--- id 查找的）
	// 查询数据库
	std::shared_ptr<UserInfo> user_info = nullptr;
	user_info = MysqlMgr::GetInstance()->GetUser(name);
	if (user_info == nullptr)
	{
		rtvalue["error"] = ErrorCodes::UidInvalid;
		return;
	}

	// 将数据库内容写入redis缓存
	Json::Value redis_root;
	redis_root["uid"] = user_info->uid;
	redis_root["pwd"] = user_info->pwd;
	redis_root["name"] = user_info->name;
	redis_root["email"] = user_info->email;
	redis_root["nick"] = user_info->nick;
	redis_root["desc"] = user_info->desc;
	redis_root["sex"] = user_info->sex;
	/// 设置进redis当中
	RedisMgr::GetInstance()->Set(base_key, redis_root.toStyledString());

	// 返回数据
	rtvalue["uid"] = user_info->uid;
	rtvalue["pwd"] = user_info->pwd;
	rtvalue["name"] = user_info->name;
	rtvalue["email"] = user_info->email;
	rtvalue["nick"] = user_info->nick;
	rtvalue["desc"] = user_info->desc;
	rtvalue["sex"] = user_info->sex;
}

bool LogicSystem::GetBaseInfo(std::string base_key, int uid, std::shared_ptr<UserInfo> &userinfo)
{
	// 优先查redis中查询用户信息
	std::string info_str = "";
	bool b_base = RedisMgr::GetInstance()->Get(base_key, info_str);
	if (b_base)
	{
		Json::Reader reader;
		Json::Value root;
		reader.parse(info_str, root);
		userinfo->uid = root["uid"].asInt();
		userinfo->name = root["name"].asString();
		userinfo->pwd = root["pwd"].asString();
		userinfo->email = root["email"].asString();
		userinfo->nick = root["nick"].asString();
		userinfo->desc = root["desc"].asString();
		userinfo->sex = root["sex"].asInt();
		userinfo->icon = root["icon"].asString();
		std::cout << "user login uid is  " << userinfo->uid << " name  is "
				  << userinfo->name << " pwd is " << userinfo->pwd << " email is " << userinfo->email << endl;
	}
	else
	{
		// redis中没有则查询mysql
		// 查询数据库
		std::shared_ptr<UserInfo> user_info = nullptr;
		user_info = MysqlMgr::GetInstance()->GetUser(uid);
		if (user_info == nullptr)
		{
			return false;
		}

		userinfo = user_info;

		// 将数据库内容写入redis缓存
		Json::Value redis_root;
		redis_root["uid"] = uid;
		redis_root["pwd"] = userinfo->pwd;
		redis_root["name"] = userinfo->name;
		redis_root["email"] = userinfo->email;
		redis_root["nick"] = userinfo->nick;
		redis_root["desc"] = userinfo->desc;
		redis_root["sex"] = userinfo->sex;
		redis_root["icon"] = userinfo->icon;
		RedisMgr::GetInstance()->Set(base_key, redis_root.toStyledString());
	}

	return true;
}
// 获得申请的信息
bool LogicSystem::GetFriendApplyInfo(int to_uid, std::vector<std::shared_ptr<ApplyInfo>> &list)
{
	// 从mysql获取好友申请列表
	return MysqlMgr::GetInstance()->GetApplyList(to_uid, list, 0, 10);
}

bool LogicSystem::GetFriendList(int self_id, std::vector<std::shared_ptr<UserInfo>> &user_list)
{
	// 从mysql获取好友列表
	return MysqlMgr::GetInstance()->GetFriendList(self_id, user_list);
}
