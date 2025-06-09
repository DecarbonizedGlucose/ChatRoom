#ifndef ACTION_HPP
#define ACTION_HPP

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