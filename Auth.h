#pragma once
#include <string>

// SHA-256（十六进制小写）
std::string Auth_Sha256Hex(const std::string & data);
// 生成 n 字节随机数的十六进制串（2n 字符）
std::string Auth_RandomHex(int nBytes);
// 账号密码哈希：salt + password
std::string Auth_PasswordHash(const std::string & salt, const std::string & password);
