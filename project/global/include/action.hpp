#pragma once

/*
 * enum class Action 描述了各种用户行为和系统操作
 * 客户端和服务器对命令的反应不同
 * 服务器接收命令后，处理云端数据后，将该命令发送到波及到的用户
 * 客户端接收命令后，修改对应的数据
 * 这期间还会有额外数据包的传输
*/

// 由于发送时arg[]外已经有sender字段，以下arg中均不含自己的邮箱/ID

enum class Action {
    /*      账号系统      */
    Sign_In,               // 登录 --password
    Sign_Out,              // 登出
    Register,              // 注册 --user_ID --password
    Get_Veri_Code,         // 获取验证码
    Find_Password,         // 找回密码* --veri_code
    Change_Password,       // 修改密码 --old-password --new-password
    Change_Username,       // 修改用户名
    Authentication,        // 身份验证 --veri-code

    /*      服务器通知      */
    Refuse,                // 拒绝
    Accept,                // 接受
    Notify,                // 通知 --description
    Notify_Exist,          // 通知已存在 --user_ID
    Notify_Not_Exist,      // 通知不存在 --user_ID
    Friend_Online,         // 好友上线 --user_ID
    Friend_Offline,        // 好友下线 --user_ID

    /*      私信行为      */
    Add_Friend,            // 添加好友 --user_ID
    Remove_Friend,         // 删除好友 --user_ID
    Search_Person,         // 搜索用户 --user_ID

    /*      群聊管理      */
    Create_Group,          // 创建群组 --name
    Join_Group,            // 加入群组 --group_ID --time
    Leave_Group,           // 退出群组 --group_ID --time
    Disband_Group,         // 解散群组 --group_ID --time
    Invite_To_Group,       // 邀请加入群组 --user_ID --group_ID --time
    Remove_From_Group,     // 从群组中移除 --user_ID --group_ID --time
    Search_Group,          // 搜索群组* --group_ID --time
    Add_Admin,             // 添加管理员 --user_ID --group_ID --time
    Remove_Admin,          // 移除管理员 --user_ID --group_ID --time

    Get_Relation_Net,      // 获取关系网
    Update_Relation_Net,   // 更新关系网 这个似乎轮不到客户端发送

    /*      消息行为      */
    Download_File,         // 下载文件 --file_ID

    /*      连接管理      */
    Remember_Connection,   // 记住连接
    Online_Init,           // 在线初始化
};
