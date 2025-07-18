#include "../include/handler.hpp"
#include "../include/email.hpp"

void CommandHandler::handle_send_veri_code(std::string subj) {
    // 安全起见，这俩玩意之后安排从mysql里取
    const std::string qq_email = "decglu@qq.com";
    const std::string auth_code = "gowkltdykhdmgehh";
    try {
        QQMailSender sender;
        sender.init(qq_email, auth_code);
        sender.set_content(
            "聊天室账户验证", // 主题
            {subj}, // 收件人
            "尊敬的用户：\n\n"
            "欢迎注册我们的聊天室(チャットルーム)服务！\n"
            "您的验证码是：114514\n\n"
            "请勿将此验证码分享给他人。\n"
            "此验证码10分钟后失效。\n\n"
            "感谢您的支持！\n"
            "聊天室团队"
        );
        if (sender.send()) {
            std::cout << "\n=== 邮件发送成功！===" << std::endl;
        } else {
            std::cerr << "\n!!! 邮件发送失败: " << sender.get_error() << " !!!" << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "初始化错误: " << e.what() << std::endl;
        return;
    }
}