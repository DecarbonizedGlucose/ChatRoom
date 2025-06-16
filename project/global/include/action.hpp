#ifndef ACTION_HPP
#define ACTION_HPP

/*
 * enum class Action 描述了各种用户行为和系统操作
 * 客户端和服务器对命令的反应不同
 * 服务器接收命令后，处理云端数据后，将该命令发送到波及到的用户
 * 客户端接收命令后，修改对应的数据
 * 这期间还会有额外数据包的传输
*/
enum class Action {
    /*      账号系统      */
    Sign_In,                      // 登录
    Sign_Out,                     // 登出
    Register,                     // 注册
    Find_Password,                // 找回密码*
    Change_Password,              // 修改密码
    Change_Username,              // 修改用户名
    Change_Email,                 // 修改邮箱*
    Authentication,               // 身份验证

    /*      私信行为      */
    Add_Friend,                   // 添加好友
    Remove_Friend,                // 删除好友
    Search_Person,                // 搜索用户

    /*      群聊管理      */
    Create_Group,                 // 创建群组
    Join_Group,                   // 加入群组
    Leave_Group,                  // 退出群组
    Disband_Group,                // 解散群组
    Invite_To_Group,              // 邀请加入群组
    Remove_From_Group,            // 从群组中移除
    Search_Group,                 // 搜索群组*
    Add_Admin,                    // 添加管理员
    Remove_Admin,                 // 移除管理员
};

#endif