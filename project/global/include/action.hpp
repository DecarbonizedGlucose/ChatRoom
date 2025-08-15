#pragma once

/*
 * enum class Action 描述了各种用户行为和系统操作
 * 客户端和服务器对命令的反应不同
 * 服务器接收命令后, 处理云端数据后, 将该命令发送到波及到的用户
 * 客户端接收命令后, 修改对应的数据
 * 这期间还会有额外数据包的传输
*/

// 由于发送时arg[]外已经有sender字段, 以下arg中均不含自己的邮箱/ID

enum class Action {
    /*      账号系统      */
    Sign_In,               // 登录 --password
    Sign_Out,              // 登出
    Register,              // 注册 --user_ID --password
    Unregister,            // 销毁账号 --user_ID
    Get_Veri_Code,         // 获取验证码
    Find_Password,         // 找回密码* --veri_code
    Change_Password,       // 修改密码 --old-password --new-password
    Change_Username,       // 修改用户名
    Authentication,        // 身份验证 --veri_code

    /*      服务器通知      */
    Refuse_Regi,           // 拒绝注册 --description
    Accept_Regi,           // 接受注册
    Refuse_Login,          // 拒绝登录 --description
    Accept_Login,          // 接受登录 --user_ID/email
    Refuse_FReq,           // 拒绝好友请求 --time --user_ID
    Accept_FReq,           // 接受好友请求 --time --user_ID
    Refuse_GReq,           // 拒绝群组请求 --time (--group_ID --user_ID)/(--cmd_id)
    Accept_GReq,           // 接受群组请求 --time (--group_ID --user_ID --group_name --member_count --owner_ID)/(--cmd_id)
    Refuse_Post_Code,      // 拒绝获取验证码 --description
    Accept_Post_Code,      // 接受获取验证码
    Success_Auth,          // 成功验证
    Failed_Auth,           // 验证失败 --description
    Notify,                // 通知 --time --description
    Notify_Exist,          // 通知已存在 --user_ID/group_ID --group_name
    Notify_Not_Exist,      // 通知不存在 --user_ID/group_ID [--group_name]
    Friend_Online,         // 好友上线 --user_ID
    Friend_Offline,        // 好友下线 --user_ID
    Success,               // 群聊事务处理成功
    Managed,               // 请求已经被其他管理员处理

    /*      私信行为      */
    Add_Friend_Req,        // 添加好友请求 --time --user_ID
    Remove_Friend,         // 删除好友 --time --user_ID
    Search_Person,         // 搜索用户 --user_ID
    Block_Friend,          // 屏蔽好友 --user_ID
    Unblock_Friend,        // 取消屏蔽好友 --user_ID

    /*      群聊管理      */
    Create_Group,          // 创建群组 --time --name
    Give_Group_ID,         // 给群组ID --group_ID 如果创建失败, group_ID为""
    Join_Group_Req,        // 加入群组请求 --time --group_ID [--cmd_id]
    Leave_Group,           // 退出群组 --time --group_ID
    Disband_Group,         // 解散群组 --time --group_ID
    Invite_To_Group_Req,   // 邀请加入群组 --time --group_ID --group_name --user_ID
    Remove_From_Group,     // 从群组中移除 --time --group_ID --user_ID
    Search_Group,          // 搜索群组* --time --group_ID
    Add_Admin,             // 添加管理员 --time --group_ID --user_ID
    Remove_Admin,          // 移除管理员 --time --group_ID --user_ID

    /*      消息行为      */
    Upload_File,           // 上传文件 --file_hash --file_size
    Download_File,         // 下载文件 --file_ID
    Accept_File,           // 接受文件上传 --file_hash --file_ID
    Deny_File,             // 拒绝文件上传 --file_hash --sendable [--file_ID]
    Accept_File_Req,       // 接受文件下载请求 --file_ID --file_hash --file_size
    Deny_File_Req,         // 拒绝文件下载请求 --file_ID

    /*      连接管理      */
    Set_Temp_Connection,   // 设置临时连接
    Remember_Connection,   // 记住连接 --idx
    Online_Init,           // 在线初始化
    HEARTBEAT,             // 心跳检测
};
