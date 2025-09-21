#pragma once

#include <iostream>
#include <string>
#include <jsoncpp/json/json.h>
#include <sstream>
#include <iomanip>
#include <atomic>
#include <random>

#include "Log.hpp"

namespace Rpc
{
    using namespace LogModule;
    class JSON
    {
    public:
        static bool Serialize(const Json::Value &val, std::string &body)
        {
            std::stringstream ss;

            Json::StreamWriterBuilder builder;

            // 这里是一个new出来的对象所以选择智能指针去管理起来
            std::unique_ptr<Json::StreamWriter> writer(builder.newStreamWriter());
            int ret = writer->write(val, &ss);
            if (ret != 0)
            {
                LOG(LogLevel::ERROR) << "Failed to serialize JSON object: " << ret;
                return false;
            }

            body = ss.str();
            return true;
        }

        static bool Deserialize(const std::string &body, Json::Value &val)
        {
            std::string ss;
            Json::CharReaderBuilder builder;
            std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
            bool ret = reader->parse(body.c_str(), body.c_str() + body.size(), &val, &ss);
            if (ret == false)
            {
                LOG(LogLevel::ERROR) << "Failed to parse JSON object: " << ss.c_str();
                return false;
            }
            return true;
        }
    };

    class UUID
    {
    public:
        static std::string Uuid()
        {
            std::stringstream ss;
            // 1. 构造一个机器随机数对象
            std::random_device rd;
            // 2. 以机器随机数为种子构造伪随机数对象
            std::mt19937 generator(rd());
            // 3. 构造限定数据范围的对象
            std::uniform_int_distribution<int> distribution(0, 255);
            // 4. 生成8个随机数，按照特定格式组织成为16进制数字字符的字符串
            for (int i = 0; i < 8; i++)
            {
                if (i == 4 || i == 6)
                    ss << "-";
                ss << std::setw(2) << std::setfill('0') << std::hex << distribution(generator);
            }
            ss << "-";
            // 5. 定义一个8字节序号，逐字节组织成为16进制数字字符的字符串
            static std::atomic<size_t> seq(1); //  00 00 00 00 00 00 00 01
            size_t cur = seq.fetch_add(1);
            for (int i = 7; i >= 0; i--)
            {
                if (i == 5)
                    ss << "-";
                ss << std::setw(2) << std::setfill('0') << std::hex << ((cur >> (i * 8)) & 0xFF);
            }
            return ss.str();
        }
    };

}
