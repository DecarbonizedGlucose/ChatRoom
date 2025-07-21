#include "../include/mysql.hpp"

/*
#include <mysql_driver.h>
#include <mysql_connection.h>
#include <cppconn/statement.h>
#include <cppconn/prepared_statement.h>
#include <cppconn/resultset.h>
#include <memory>
#include <iostream>

int main() {
    sql::mysql::MySQL_Driver* driver = sql::mysql::get_mysql_driver_instance();
    std::unique_ptr<sql::Connection> conn(
        driver->connect("tcp://127.0.0.1:3306", "用户名", "密码")
    );
    conn->setSchema("数据库名");

    // 普通查询
    std::unique_ptr<sql::Statement> stmt(conn->createStatement());
    std::unique_ptr<sql::ResultSet> res(stmt->executeQuery("SELECT * FROM 表名"));
    while (res->next()) {
        std::cout << res->getString("字段名") << std::endl;
    }

    // 预处理
    std::unique_ptr<sql::PreparedStatement> pstmt(
        conn->prepareStatement("INSERT INTO 表名(col1, col2) VALUES (?, ?)")
    );
    pstmt->setString(1, "abc");
    pstmt->setInt(2, 123);
    pstmt->executeUpdate();
}
*/