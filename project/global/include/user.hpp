#ifndef USER_HPP
#define USER_HPP

#include <string>
#include <memory>

class Account {
public:
    Account() = default;

    std::string user_ID;          // 唯一标识ID, 6位字符串(0-9, a-z, A-Z)
    std::string username;         // 用户名, 6-20位字符串(0-9, a-z, A-Z, _ - .)

private:
    std::string password;         // 密码, 8-16位字符串(0-9, a-z, A-Z, !@#$%^&*()-_=+[]{}|;:'",.<>?/~`)(暂定)
};

enum class User_Status {
    Online,                       // 在线
    Offline                       // 离线
};

enum class User_Role {
    Normal,                       // 普通用户(私信好友)
    Member,                       // 群成员
    Admin,                        // 群管理员
    Master                        // 群主
};

class User {
public:
    User() = default;

    const Account* user_info;
    User_Status status;
    User_Role role;               // 用户角色 in session
};

using US = User_Status;
using UR = User_Role;
using UserPtr = std::shared_ptr<User>;

#endif