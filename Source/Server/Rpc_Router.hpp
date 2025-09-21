#pragma once
#include "../Common/Message.hpp"
#include "../Common/Net.hpp"
#include "../Common/Log.hpp"

namespace Rpc
{
    namespace Server
    {
        enum class VType
        {
            BOOL = 0,
            INTEGRAL,
            NUMERIC,
            STRING,
            ARRAY,
            OBJECT,
        };

        class ServerDescribe
        {
        public:
            using ptr = std::shared_ptr<ServerDescribe>;
            using ServiceCallback = std::function<void(const Json::Value &, Json::Value &)>;
            using ParamDescribe = std::pair<std::string, VType>;
            ServerDescribe(std::string &&method, ServiceCallback &&callback,
                           VType &&return_type, std::vector<ParamDescribe> &&params_desc)
                : _method(std::move(method)), _callback(std::move(callback)),
                  _return_type(std::move(return_type)), _params_desc(std::move(params_desc)) {}

            const std::string GetMethod() { return _method; }

            // 对收到的请求进行校验
            bool ParamCheck(const Json::Value &params)
            {
                for (auto &desc : _params_desc)
                {
                    if (params.isMember(desc.first) == false)
                    {
                        LOG(LogLevel::ERROR) << "字段缺失" << desc.first.c_str();
                        return false;
                    }
                    if (Check(desc.second, params[desc.first]) == false)
                    {
                        LOG(LogLevel::ERROR) << "字段类型错误" << desc.first.c_str();
                        return false;
                    }
                }
                return true;
            }

            bool Call(const Json::Value &params, Json::Value &result)
            {
                _callback(params, result);
                if (ReturnCheck(result))
                {
                    return true;
                }
                else
                {
                    LOG(LogLevel::ERROR) << "返回值类型错误";
                    return false;
                }
            }

        private:
            bool ReturnCheck(const Json::Value &result)
            {
                return Check(_return_type, result);
            }
            bool Check(VType vtype, const Json::Value &params)
            {
                switch (vtype)
                {
                case VType::BOOL:
                    return params.isBool();
                case VType::INTEGRAL:
                    return params.isIntegral();
                case VType::NUMERIC:
                    return params.isNumeric();
                case VType::STRING:
                    return params.isString();
                case VType::ARRAY:
                    return params.isArray();
                case VType::OBJECT:
                    return params.isObject();
                default:
                    return false;
                }
            }

        private:
            std::string _method;
            ServiceCallback _callback;
            std::vector<ParamDescribe> _params_desc;
            VType _return_type;
        };
        class SDescribeFactory
        {
        public:
            void SetMethod(const std::string &method) { _method = method; }

            void SetCallback(const ServerDescribe::ServiceCallback &callback) { _callback = callback; }

            void SetVType(VType vtype) { _return_type = vtype; }

            void SetParamsDesc(const std::string &name, VType vtype)
            {
                _params_desc.push_back(ServerDescribe::ParamDescribe(name, vtype));
                //_params_desc.emplace_back(name,vtype);
            }

            ServerDescribe::ptr Build()
            {
                return std::make_shared<ServerDescribe>(std::move(_method), std::move(_callback)
                , std::move(_return_type), std::move(_params_desc));
            }

        private:
            std::string _method;
            ServerDescribe::ServiceCallback _callback;
            std::vector<ServerDescribe::ParamDescribe> _params_desc;
            VType _return_type;
        };

        class ServerManager
        {
        public:
            using ptr = std::shared_ptr<ServerManager>;
            void insert(const ServerDescribe::ptr &desc)
            {
                std::unique_lock<std::mutex> lock(_mutex);
                _services.insert(std::make_pair(desc->GetMethod(), desc));
            }

            ServerDescribe::ptr Select(const std::string &method)
            {
                std::unique_lock<std::mutex> lock(_mutex);
                auto it = _services.find(method);
                if (it == _services.end())
                {
                    return nullptr;
                }
                return it->second;
            }

            void Remove(const std::string &method)
            {
                std::unique_lock<std::mutex> lock(_mutex);
                _services.erase(method);
            }

        private:
            std::mutex _mutex;
            std::unordered_map<std::string, ServerDescribe::ptr> _services;
        };

        class RpcRouter
        {
        public:
            using ptr = std::shared_ptr<RpcRouter>;
            RpcRouter() : _server_manager(std::make_shared<ServerManager>()) {}
            // 这是注册到Dispatcher模块针对RpcRequest消息的处理函数
            void OnRpcRequest(const BaseConnection::ptr &conn, const RpcRequest::ptr &msg)
            {
                // 1.收到RpcRequest消息，查询客户端请求的方法 -- 判断是否能提供服务
                auto service = _server_manager->Select(msg->GetMethod());
                if (service.get() == nullptr)
                {
                    LOG(LogLevel::DEBUG) << "服务不存在";
                    return Response(conn, msg, Json::Value(), RCode::RCODE_NOT_FOUND_SERVICE);
                }
                // 2.进行参数校验，确定能否提供
                if (service->ParamCheck(msg->GetParams()) == false)
                {
                    LOG(LogLevel::DEBUG) << "参数校验失败";
                    return Response(conn, msg, Json::Value(), RCode::RCODE_INVALID_PARAMS);
                }
                // 3.如果能提供服务，则调用服务的回调函数
                Json::Value result;
                bool ret = service->Call(msg->GetParams(), result);
                if (ret == false)
                {
                    LOG(LogLevel::DEBUG) << "服务调用失败";
                    return Response(conn, msg, Json::Value(), RCode::RCODE_INTERNAL_ERROR);
                }
                // 4.如果服务的回调函数返回值，则将返回值封装成RpcResponse消息，发送给客户端
                Response(conn, msg, result, RCode::RCODE_OK);
            }
            void RegisterMethod(const ServerDescribe::ptr &service)
            {
                return _server_manager->insert(service);
            }

        private:
            void Response(const BaseConnection::ptr &conn, const RpcRequest::ptr &req,
                          const Json::Value &result, RCode rcode)
            {
                auto msg = MessageFactory::CreateMessage<RpcResponse>();
                msg->SetId(req->GetId());
                msg->SetRcode(rcode);
                msg->SetResult(result);
                msg->SetType(MType::RSP_RPC);
                conn->Send(msg);
            }

        private:
            ServerManager::ptr _server_manager;
        };
    }

}