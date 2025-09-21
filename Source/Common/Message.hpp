#pragma once
#include "Detail.hpp"
#include "Fields.hpp"
#include "Abstract.hpp"

namespace Rpc
{
    typedef std::pair<std::string, int> Address;
    
    class JsonMessage : public BaseMessage
    {
    public:
        using ptr = std::shared_ptr<JsonMessage>;

        virtual std::string Serialize() override
        {
            std::string body;
            bool ret = JSON::Serialize(_body, body);
            if (ret == false)
                return std::string();
            return body;
        }
        virtual bool Deserialize(const std::string &data) override
        {
            return JSON::Deserialize(data, _body);
        }

    protected:
        Json::Value _body;
    };

    class JsonRequest : public JsonMessage
    {
    public:
        using ptr = std::shared_ptr<JsonRequest>;
        virtual ~JsonRequest() {}
    };
    class JsonResponse : public JsonMessage
    {
    public:
        using ptr = std::shared_ptr<JsonResponse>;

        virtual bool Check() override
        {
            // 在响应中，三种响应 RPC响应 订阅响应 服务响应  都只有响应状态码rcode
            // 因此检查只需要检查状态码字段是否存在 类型是否正确
            if (_body[KEY_RCODE].isNull() == true)
            {
                LOG(LogLevel::ERROR) << "non-existent rcode field in response";
                return false;
            }
            if (_body[KEY_RCODE].isInt() == false)
            {
                LOG(LogLevel::ERROR) << "rcode field is not an integer in response";
                return false;
            }
            return true;
        }
        virtual RCode GetRcode() const { return (RCode)_body[KEY_RCODE].asInt(); }
        virtual void SetRcode(RCode rcode) { _body[KEY_RCODE] = (int)rcode; }
    };

    class RpcRequest : public JsonRequest
    {
    public:
        using ptr = std::shared_ptr<RpcRequest>;

        virtual bool Check() override
        {
            // 在请求中，RPC请求 订阅请求 服务请求 都只有方法名和参数字段
            // 因此检查只需要检查这两个字段是否存在 类型是否正确
            if (_body[KEY_METHOD].isNull() == true || _body[KEY_METHOD].isString() == false)
            {
                LOG(LogLevel::ERROR) << "non-existent or invalid method field in request";
                return false;
            }
            if (_body[KEY_PARAMS].isNull() == true || _body[KEY_PARAMS].isObject() == false)
            {
                LOG(LogLevel::ERROR) << "non-existent or invalid parameters field in request";
                return false;
            }
            return true;
        }
        std::string GetMethod() const { return _body[KEY_METHOD].asString(); }
        void SetMethod(const std::string &method) { _body[KEY_METHOD] = method; }

        Json::Value GetParams() const { return _body[KEY_PARAMS]; }
        void SetParams(const Json::Value &params) { _body[KEY_PARAMS] = params; }
    };
    class TopicRequest : public JsonRequest
    {
    public:
        using ptr = std::shared_ptr<TopicRequest>;

        virtual bool Check() override
        {
            // 在请求中，订阅请求 只有主题名和消息字段
            // 因此检查只需要检查这两个字段是否存在 类型是否正确
            if (_body[KEY_TOPIC_KEY].isNull() == true || _body[KEY_TOPIC_KEY].isString() == false)
            {
                LOG(LogLevel::ERROR) << "non-existent or invalid topic_key field in request";
                return false;
            }
            if (_body[KEY_OPTYPE].isNull() == true || _body[KEY_OPTYPE].isInt() == false)
            {
                LOG(LogLevel::ERROR) << "non-existent or invalid optype field in request";
                return false;
            }
            if (_body[KEY_OPTYPE].asInt() == (int)TopicOptype::TOPIC_PUBLISH &&
                (_body[KEY_TOPIC_MSG].isNull() == true ||
                 _body[KEY_TOPIC_MSG].isObject() == false))
            {
                LOG(LogLevel::ERROR) << "non-existent or invalid topic_msg field in request";
                return false;
            }
            return true;
        }
        std::string GetTopicKey() const { return _body[KEY_TOPIC_KEY].asString(); }
        void SetTopicKey(const std::string &topic_key) { _body[KEY_TOPIC_KEY] = topic_key; }

        TopicOptype GetOptype() const { return (TopicOptype)_body[KEY_OPTYPE].asInt(); }
        void SetOptype(TopicOptype optype) { _body[KEY_OPTYPE] = (int)optype; }

        Json::Value GetTopicMsg() const { return _body[KEY_TOPIC_MSG]; }
        void SetTopicMsg(const Json::Value &topic_msg) { _body[KEY_TOPIC_MSG] = topic_msg; }
    };

    class ServiceRequest : public JsonRequest
    {
    public:
        using ptr = std::shared_ptr<ServiceRequest>;

        virtual bool Check() override
        {
            // 在请求中，服务请求 只有方法名和参数字段
            // 因此检查只需要检查这两个字段是否存在 类型是否正确
            if (_body[KEY_METHOD].isNull() == true ||
                _body[KEY_METHOD].isString() == false)
            {
                LOG(LogLevel::ERROR) << "non-existent or invalid method field in  service request";
                return false;
            }
            if (_body[KEY_OPTYPE].isNull() == true ||
                _body[KEY_OPTYPE].isIntegral() == false)
            {
                LOG(LogLevel::ERROR) << "non-existent or invalid optype field in service request";
                return false;
            }
            if (_body[KEY_OPTYPE].asInt() != (int)(ServiceOptype::SERVICE_DISCOVERY) &&
                (_body[KEY_HOST].isNull() == true ||
                 _body[KEY_HOST].isObject() == false ||
                 _body[KEY_HOST][KEY_HOST_IP].isNull() == true ||
                 _body[KEY_HOST][KEY_HOST_IP].isString() == false ||
                 _body[KEY_HOST][KEY_HOST_PORT].isNull() == true ||
                 _body[KEY_HOST][KEY_HOST_PORT].isIntegral() == false))
            {
                LOG(LogLevel::ERROR) << "non-existent or invalid host field in service request";
                return false;
            }
            return true;
        }
        std::string GetMethod() const { return _body[KEY_METHOD].asString(); }
        void SetMethod(const std::string &method) { _body[KEY_METHOD] = method; }

        Json::Value GetParams() const { return _body[KEY_PARAMS]; }
        void SetParams(const Json::Value &params) { _body[KEY_PARAMS] = params; }

        ServiceOptype GetOptype() const { return (ServiceOptype)_body[KEY_OPTYPE].asInt(); }
        void SetOptype(ServiceOptype optype) { _body[KEY_OPTYPE] = (int)optype; }

        Address GetHost() const
        {
            Address addr;
            addr.first = _body[KEY_HOST][KEY_HOST_IP].asString();
            addr.second = _body[KEY_HOST][KEY_HOST_PORT].asInt();
            return addr;
        }
        void SetHost(const Address &addr)
        {
            Json::Value val;
            val[KEY_HOST_IP] = addr.first;
            val[KEY_HOST_PORT] = addr.second;
            _body[KEY_HOST] = val;
        }
    };

    class RpcResponse : public JsonResponse
    {
    public:
        using ptr = std::shared_ptr<RpcResponse>;
        virtual bool Check() override
        {
            if (_body[KEY_RESULT].isNull() == true ||
                _body[KEY_RESULT].isObject() == false)
            {
                LOG(LogLevel::ERROR) << "non-existent or invalid result field in Rpc response";
                return false;
            }
            if (_body[KEY_RCODE].isNull() == true ||
                _body[KEY_RCODE].isIntegral() == false)
            {
                LOG(LogLevel::ERROR) << "non-existent or invalid rcode field in Rpc response";
                return false;
            }
            return true;
        }

        Json::Value GetResult() const { return _body[KEY_RESULT]; }
        void SetResult(const Json::Value &result) { _body[KEY_RESULT] = result; }
    };

    class TopicResponse : public JsonResponse
    {
    public:
        using ptr = std::shared_ptr<TopicResponse>;
        
    };

    class ServiceResponse : public JsonResponse
    {
    public:
        using ptr = std::shared_ptr<ServiceResponse>;
        virtual bool Check() override
        {
            if (_body[KEY_RCODE].isNull() == true ||
                _body[KEY_RCODE].isIntegral() == false)
            {
                LOG(LogLevel::ERROR) << "non-existent or invalid rcode field in service response";
                return false;
            }
            if (_body[KEY_OPTYPE].isNull() == true ||
                _body[KEY_OPTYPE].isIntegral() == false)
            {
                LOG(LogLevel::ERROR) << "non-existent or invalid optype field in service response";
                return false;
            }
            // 如果服务类型是发现类型，那需要去检查方法和HOST字段
            if (_body[KEY_OPTYPE].asInt() == (int)(ServiceOptype::SERVICE_DISCOVERY) &&
                (_body[KEY_METHOD].isNull() == true ||
                 _body[KEY_METHOD].isString() == false ||
                 _body[KEY_HOST].isNull() == true ||
                 _body[KEY_HOST].isArray() == false))
            {
                LOG(LogLevel::ERROR) << "non-existent or invalid method or host field in service response";
                return false;
            }
            return true;
        }
        ServiceOptype GetOptype()
        {
            return (ServiceOptype)_body[KEY_OPTYPE].asInt();
        }

        void SetOptype(ServiceOptype optype)
        {
            _body[KEY_OPTYPE] = (int)optype;
        }

        std::string GetMethod()
        {
            return _body[KEY_METHOD].asString();
        }

        void SetMethod(const std::string &method)
        {
            _body[KEY_METHOD] = method;
        }
        void SetHost(std::vector<Address> addrs)
        {
            for (auto &addr : addrs)
            {
                Json::Value val;
                val[KEY_HOST_IP] = addr.first;
                val[KEY_HOST_PORT] = addr.second;
                _body[KEY_HOST].append(val);
            }
        }
        std::vector<Address> GetHosts()
        {
            std::vector<Address> addrs;
            int sz = _body[KEY_HOST].size();
            for (int i = 0; i < sz; i++)
            {
                Address addr;
                addr.first = _body[KEY_HOST][i][KEY_HOST_IP].asString();
                addr.second = _body[KEY_HOST][i][KEY_HOST_PORT].asInt();
                addrs.push_back(addr);
            }
            return addrs;
        }
    };

    class MessageFactory
    {
    public:
        static BaseMessage::ptr CreateMessage(MType mtype)
        {
            switch (mtype)
            {
            case MType::REQ_RPC:
                return std::make_shared<RpcRequest>();
            case MType::RSP_RPC:
                return std::make_shared<RpcResponse>();
            case MType::REQ_TOPIC:
                return std::make_shared<TopicRequest>();
            case MType::RSP_TOPIC:
                return std::make_shared<TopicResponse>();
            case MType::REQ_SERVICE:
                return std::make_shared<ServiceRequest>();
            case MType::RSP_SERVICE:
                return std::make_shared<ServiceResponse>();
            }
            return BaseMessage::ptr();
        }

        template <typename T, typename... Args>
        static std::shared_ptr<T> CreateMessage(Args &&...args)
        {
            return std::make_shared<T>(std::forward<Args>(args)...);
        }
    };

}