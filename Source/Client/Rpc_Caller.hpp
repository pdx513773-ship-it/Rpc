#pragma once

#include "Requestor.hpp"

namespace Rpc
{
    namespace Client
    {
        class RpcCaller
        {
        public:
            RpcCaller(const Requestor::ptr &requesor) : _requesor(requesor) {}
            using ptr = std::shared_ptr<RpcCaller>;
            using JsonAsyncResponse = std::future<Json::Value>;
            using JsonResponseCallback = std::function<void(const Json::Value &)>;
            // 三种不同调用方式 同步 异步 回调
            bool Call(const BaseConnection::ptr &conn, const std::string &method,
                      const Json::Value &params, Json::Value &result)
            {
                // 1.组织请求
                auto req_msg = MessageFactory::CreateMessage<RpcRequest>();
                req_msg->SetMethod(method);
                req_msg->SetId(UUID::Uuid());
                req_msg->SetParams(params);
                BaseMessage::ptr rsp_msg;
                // 2.发送请求
                bool ret = _requesor->Send(conn, std::dynamic_pointer_cast<BaseMessage>(req_msg), rsp_msg);
                if (ret == false)
                {
                    LOG(LogLevel::ERROR) << "请求发送失败";
                    return false;
                }
                LOG(LogLevel::DEBUG) << "RPC同步请求成功";

                // 3.处理响应
                auto rpc_rsp = std::dynamic_pointer_cast<RpcResponse>(rsp_msg);
                if (!rpc_rsp)
                {
                    LOG(LogLevel::ERROR) << "向下类型转换失败";
                    return false;
                }
                if (rpc_rsp->GetRcode() != RCode::RCODE_OK)
                {
                    LOG(LogLevel::ERROR) << "RPC同步请求失败" << ErrReason(rpc_rsp->GetRcode());
                    return false;
                }
                result = rpc_rsp->GetResult();
                LOG(LogLevel::DEBUG) << "结果设置完毕";
                return true;
            }
            bool Call(const BaseConnection::ptr &conn, const std::string &method,
                      const Json::Value &params, JsonAsyncResponse &result)
            {
                auto req_msg = MessageFactory::CreateMessage<RpcRequest>();
                req_msg->SetMethod(method);
                req_msg->SetId(UUID::Uuid());
                req_msg->SetParams(params);

                // 使用 shared_ptr 管理 promise
                auto json_promise = std::make_shared<std::promise<Json::Value>>();

                // 注意：这里捕获的是共享指针，确保回调函数持有它
                auto cb = [this, json_promise](const BaseMessage::ptr &msg)
                {
                    this->CallBack(msg, *json_promise); // 解引用得到 promise 的引用
                };

                result = json_promise->get_future();
                bool ret = _requesor->Send(conn, std::dynamic_pointer_cast<BaseMessage>(req_msg), cb);
                if (ret == false)
                {
                    LOG(LogLevel::ERROR) << "请求发送失败";
                    return false;
                }
                LOG(LogLevel::DEBUG) << "RPC异步请求成功";
                return true;
            }
            bool Call(const BaseConnection::ptr &conn, const std::string &method,
                      const Json::Value &params, const JsonResponseCallback &cb)
            {
                auto req_msg = MessageFactory::CreateMessage<RpcRequest>();
                req_msg->SetMethod(method);
                req_msg->SetId(UUID::Uuid());
                req_msg->SetParams(params);

                auto req_cb = std::bind(&RpcCaller::CallBack1, this,
                                        std::placeholders::_1,
                                        std::ref(cb)); // 使用 std::ref 包装引用
                bool ret = _requesor->Send(conn, std::dynamic_pointer_cast<BaseMessage>(req_msg), req_cb);
                if (ret == false)
                {
                    LOG(LogLevel::ERROR) << "请求发送失败";
                    return false;
                }
                LOG(LogLevel::DEBUG) << "RPC回调请求成功";
                return true;
            }

        private:
            void CallBack1(const BaseMessage::ptr &msg, const JsonResponseCallback &cb)
            {
                auto rpc_rsp = std::dynamic_pointer_cast<RpcResponse>(msg);
                if (!rpc_rsp)
                {
                    LOG(LogLevel::ERROR) << "向下类型转换失败";
                    return;
                }
                if (rpc_rsp->GetRcode() != RCode::RCODE_OK)
                {
                    LOG(LogLevel::ERROR) << "RPC回调请求失败" << ErrReason(rpc_rsp->GetRcode());
                    return;
                }
                cb(rpc_rsp->GetResult());
            }
            void CallBack(const BaseMessage::ptr &msg, std::promise<Json::Value> &result)
            {
                auto rpc_rsp = std::dynamic_pointer_cast<RpcResponse>(msg);
                if (!rpc_rsp)
                {
                    LOG(LogLevel::ERROR) << "向下类型转换失败";
                    return;
                }
                if (rpc_rsp->GetRcode() != RCode::RCODE_OK)
                {
                    LOG(LogLevel::ERROR) << "RPC异步请求失败" << ErrReason(rpc_rsp->GetRcode());
                    return;
                }
                result.set_value(rpc_rsp->GetResult());
            }

        private:
            Requestor::ptr _requesor;
        };

    }
}