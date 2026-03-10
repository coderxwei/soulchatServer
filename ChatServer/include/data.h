#pragma once
#include <string>

struct UserInfo {
    UserInfo() : name(""), pwd(""), uid(0), email(""), nick(""), desc(""), sex(0), icon(""), back("") {
    }

    std::string name;
    std::string pwd;
    int uid;
    std::string email;
    std::string nick;
    std::string desc;
    int sex;
    std::string icon;
    std::string back;
};

struct ApplyInfo {
    ApplyInfo(int uid, std::string name, std::string desc,
              std::string icon, std::string nick, int sex, int status)
        : _uid(uid), _name(name), _desc(desc),
          _icon(icon), _nick(nick), _sex(sex), _status(status) {
    }

    int _uid;
    std::string _name;
    std::string _desc;
    std::string _icon;
    std::string _nick;
    int _sex;
    int _status;
};


// 这个是消息结构体 用于存储用消息 ，以方便存储到 数据库中
struct TextMsg {
    TextMsg() : from_uid(0), to_uid(0), msg_type(1), seq(0), status(0) {
    }

    std::string msg_id; // 客户端生成的 UUID，全局唯一
    std::string conv_id; // 会话 ID = min(uid)_max(uid)
    int from_uid; //消息的来源  uid 是用户注册的时候就确定的。
    int to_uid; //消息的接受方
    std::string content;
    int msg_type; // 1=文本 2=图片 3=文件
    long long seq; // Redis INCR 分配，会话内单调递增
    int status; // 0=未读 1=已送达 2=已读
    std::string created_at;
};



