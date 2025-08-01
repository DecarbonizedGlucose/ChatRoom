#include "../include/email.hpp"
#include "../../global/include/logging.hpp"
#include <stdexcept>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <cctype>
#include <algorithm>
#include <cstring>

QQMailSender::QQMailSender() : curl_(nullptr), recipients_(nullptr) {}

QQMailSender::~QQMailSender() {
    cleanup();
}

void QQMailSender::init(const std::string& qq_email, const std::string& authorization_code) {
    cleanup();

    if (!is_valid_qq_email(qq_email)) {
        log_error("无效的QQ邮箱地址: {}", qq_email);
        return;
    }

    email_ = qq_email;
    auth_code_ = authorization_code;
    curl_ = curl_easy_init();

    if (!curl_) {
        log_error("CURL初始化失败");
        return;
    }

    std::string url = "smtps://smtp.qq.com:465";
    curl_easy_setopt(curl_, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl_, CURLOPT_USERNAME, email_.c_str());
    curl_easy_setopt(curl_, CURLOPT_PASSWORD, auth_code_.c_str());
    curl_easy_setopt(curl_, CURLOPT_USE_SSL, CURLUSESSL_ALL);
    curl_easy_setopt(curl_, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl_, CURLOPT_SSL_VERIFYHOST, 2L);
    curl_easy_setopt(curl_, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_2);
    curl_easy_setopt(curl_, CURLOPT_CONNECTTIMEOUT, 10L);
    curl_easy_setopt(curl_, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl_, CURLOPT_LOGIN_OPTIONS, "AUTH=LOGIN");
    curl_easy_setopt(curl_, CURLOPT_NOPROGRESS, 1L);  // 禁用进度输出
    // curl_easy_setopt(curl_, CURLOPT_VERBOSE, 1L);  // 注释掉详细输出以避免控制台spam
}

void QQMailSender::set_content(const std::string& subject,
                               const std::vector<std::string>& to,
                               const std::string& body) {
    if (!curl_) {
        log_error("CURL未初始化, 无法设置邮件内容");
        return;
    }

    if (recipients_) {
        curl_slist_free_all(recipients_);
        recipients_ = nullptr;
    }

    for (const auto& recipient : to) {
        recipients_ = curl_slist_append(recipients_, recipient.c_str());
    }

    std::time_t now = std::time(nullptr);
    std::tm* gmt = std::gmtime(&now);
    char date_str[128];
    std::strftime(date_str, sizeof(date_str), "%a, %d %b %Y %H:%M:%S +0000", gmt);

    std::ostringstream headers_stream;
    headers_stream << "Date: " << date_str << "\r\n"
                   << "From: " << email_ << "\r\n"
                   << "To: ";

    for (size_t i = 0; i < to.size(); ++i) {
        headers_stream << to[i];
        if (i < to.size() - 1) headers_stream << ", ";
    }

    headers_stream << "\r\n"
                   << "Subject: " << base64_encode_subject(subject) << "\r\n"
                   << "Message-ID: <" << generate_message_id() << ".qq.com>\r\n"
                   << "X-Priority: 3\r\n"
                   << "X-Mailer: QQMailSender (C++)\r\n"
                   << "MIME-Version: 1.0\r\n"
                   << "Content-Type: text/plain; charset=utf-8\r\n"
                   << "Content-Transfer-Encoding: base64\r\n\r\n";

    headers_ = headers_stream.str();
    body_ = base64_encode(body) + "\r\n";
}

bool QQMailSender::send() {
    if (!curl_ || !recipients_) {
        log_error("发送前未正确初始化 或 邮件接收者列表为空");
        return false;
    }

    curl_easy_setopt(curl_, CURLOPT_MAIL_FROM, email_.c_str());
    curl_easy_setopt(curl_, CURLOPT_MAIL_RCPT, recipients_);

    email_content_ = headers_ + body_;
    read_offset_ = 0;

    curl_easy_setopt(curl_, CURLOPT_READFUNCTION, read_callback);
    curl_easy_setopt(curl_, CURLOPT_READDATA, this);
    curl_easy_setopt(curl_, CURLOPT_UPLOAD, 1L);

    CURLcode res = curl_easy_perform(curl_);
    last_error_ = res;

    return res == CURLE_OK;
}

std::string QQMailSender::get_error() const {
    return curl_easy_strerror(last_error_);
}

void QQMailSender::cleanup() {
    if (recipients_) {
        curl_slist_free_all(recipients_);
        recipients_ = nullptr;
    }
    if (curl_) {
        curl_easy_cleanup(curl_);
        curl_ = nullptr;
    }
}

size_t QQMailSender::read_callback(char* ptr, size_t size, size_t nmemb, void* userdata) {
    QQMailSender* self = static_cast<QQMailSender*>(userdata);
    const size_t buffer_size = size * nmemb;
    const size_t remaining = self->email_content_.size() - self->read_offset_;

    if (remaining == 0) return 0;

    size_t copy_size = (buffer_size < remaining) ? buffer_size : remaining;
    memcpy(ptr, self->email_content_.data() + self->read_offset_, copy_size);
    self->read_offset_ += copy_size;

    return copy_size;
}

bool QQMailSender::is_valid_qq_email(const std::string& email) const {
    size_t at_pos = email.find('@');
    if (at_pos == std::string::npos) return false;

    std::string domain = email.substr(at_pos + 1);
    return domain == "qq.com" || domain == "vip.qq.com";
}

std::string QQMailSender::generate_message_id() const {
    std::stringstream ss;
    ss << std::hex << std::setfill('0')
       << std::setw(8) << (std::time(nullptr) & 0xFFFFFFFF)
       << std::setw(8) << (rand() & 0xFFFFFFFF);
    return ss.str();
}

std::string QQMailSender::base64_encode_subject(const std::string& subject) {
    bool need_encode = false;
    for (char c : subject) {
        if (static_cast<unsigned char>(c) > 127) {
            need_encode = true;
            break;
        }
    }
    if (!need_encode) return subject;
    return "=?UTF-8?B?" + base64_encode(subject) + "?=";
}

std::string QQMailSender::base64_encode(const std::string& input) {
    static const char* base64_chars =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    std::string result;
    int i = 0, j = 0;
    unsigned char char_array_3[3];
    unsigned char char_array_4[4];
    size_t len = input.length();
    const char* bytes_to_encode = input.data();

    while (len--) {
        char_array_3[i++] = *(bytes_to_encode++);
        if (i == 3) {
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) +
                              ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) +
                              ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] = char_array_3[2] & 0x3f;

            for (i = 0; i < 4; i++)
                result += base64_chars[char_array_4[i]];
            i = 0;
        }
    }

    if (i) {
        for (j = i; j < 3; j++)
            char_array_3[j] = '\0';

        char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
        char_array_4[1] = ((char_array_3[0] & 0x03) << 4) +
                          ((char_array_3[1] & 0xf0) >> 4);
        char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) +
                          ((char_array_3[2] & 0xc0) >> 6);
        char_array_4[3] = char_array_3[2] & 0x3f;

        for (j = 0; j < i + 1; j++)
            result += base64_chars[char_array_4[j]];

        while (i++ < 3)
            result += '=';
    }

    return result;
}