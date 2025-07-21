#pragma once

/*
 * enum class Action 描述了各种用户行为和系统操作
 * 客户端和服务器对命令的反应不同
 * 服务器接收命令后，处理云端数据后，将该命令发送到波及到的用户
 * 客户端接收命令后，修改对应的数据
 * 这期间还会有额外数据包的传输
*/

// 由于发送时arg[]外已经有sender字段，以下arg中均不含自己的邮箱

enum class Action {
    /*      账号系统      */
    Sign_In,                      // 登录 --password
    Sign_Out,                     // 登出
    Register,                     // 注册 --veri-code --user_ID --password
    Get_Veri_Code,                // 获取验证码
    Find_Password,                // 找回密码* --veri-code
    Change_Password,              // 修改密码 --old-password --new-password
    Change_Username,              // 修改用户名
    Authentication,               // 身份验证 --veri-code
    Refuse,                       // 拒绝
    Accept,                       // 接受

    /*      私信行为      */
    Add_Friend,                   // 添加好友 --email
    Remove_Friend,                // 删除好友 --email
    Search_Person,                // 搜索用户 --email

    /*      群聊管理      */
    Create_Group,                 // 创建群组 --name
    Join_Group,                   // 加入群组 --number
    Leave_Group,                  // 退出群组 --number
    Disband_Group,                // 解散群组 --number
    Invite_To_Group,              // 邀请加入群组 --email --number
    Remove_From_Group,            // 从群组中移除 --email --number
    Search_Group,                 // 搜索群组* --number
    Add_Admin,                    // 添加管理员 --email --number
    Remove_Admin,                 // 移除管理员 --email --number

    Get_Relation_Net,             // 获取关系网

    /*      消息行为      */
    Download_File,                // 下载文件 --file-hash
    
};
