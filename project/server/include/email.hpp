#pragma once

#include <curl/curl.h>
#include <string>
#include <vector>

class QQMailSender {
public:
    QQMailSender();
    ~QQMailSender();

    void init(const std::string& qq_email, const std::string& authorization_code);
    void set_content(const std::string& subject, 
                     const std::vector<std::string>& to,
                     const std::string& body);
    bool send();
    std::string get_error() const;

private:
    void cleanup();
    static size_t read_callback(char* ptr, size_t size, size_t nmemb, void* userdata);
    bool is_valid_qq_email(const std::string& email) const;
    std::string generate_message_id() const;
    std::string base64_encode_subject(const std::string& subject);
    std::string base64_encode(const std::string& input);

    CURL* curl_;
    curl_slist* recipients_;
    std::string email_;
    std::string auth_code_;
    std::string headers_;
    std::string body_;
    std::string email_content_;
    size_t read_offset_ = 0;
    CURLcode last_error_ = CURLE_OK;
};
