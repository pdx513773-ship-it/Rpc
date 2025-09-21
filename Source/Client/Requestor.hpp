#pragma once

#include "../Common/Net.hpp"
#include "../Common/Message.hpp"
#include <future>
namespace Rpc
{
    namespace Client
    {
        //因为我们使用的是muduo库，muduo库对于消息的IO大部分都是异步的，所以我们需要对请求进行管理，对请求发送合适的响应
        class Requestor
        {
        public:
            using ptr = std::shared_ptr<Requestor>;
            using RequestCallback = std::function<void(const BaseMessage::ptr &)>;
            using AsyncResponse = std::future<BaseMessage::ptr>;
            struct RequestDescribe
            {
                using ptr = std::shared_ptr<RequestDescribe>;
                BaseMessage::ptr request;
                RType rtype;
                std::promise<BaseMessage::ptr> response;
                RequestCallback callback;
            };
            void OnResponse(const BaseConnection::ptr &conn, const BaseMessage::ptr &msg)
            {
                std::string rid = msg->GetId();
                RequestDescribe::ptr rd = GetDescribe(rid);
                if (rd.get() == nullptr)
                {
                    LOG(LogLevel::ERROR) << "收到了响应，但是没有请求";
                    return;
                }
                if (rd->rtype == RType::REQ_ASYNC)
                {
                    rd->response.set_value(msg);
                }
                else if (rd->rtype == RType::REQ_CALLBACK)
                {
                    rd->callback(msg);
                }
                else
                {
                    LOG(LogLevel::DEBUG) << "请求类型未知";
                }
                DelDescribe(rid);
            }
            bool Send(const BaseConnection::ptr &conn, const BaseMessage::ptr &msg, AsyncResponse &response)
            {
                RequestDescribe::ptr rd = NewDescribe(msg, RType::REQ_ASYNC);
                if (rd.get() == nullptr)
                    return false;
                LOG(LogLevel::DEBUG)<<"send";
                conn->Send(msg);
                LOG(LogLevel::DEBUG)<<"send sucessed";
                response = rd->response.get_future();
                return true;
            }
            bool Send(const BaseConnection::ptr &conn, const BaseMessage::ptr &req, BaseMessage::ptr &rsp)
            {
                AsyncResponse rsp_future;
                LOG(LogLevel::DEBUG)<<"send";
                bool ret = Send(conn, req, rsp_future);
                LOG(LogLevel::DEBUG)<<"send sucessed";
                if (ret == false)
                {
                    return false;
                }
                rsp = rsp_future.get();

                return true;
            }
            bool Send(const BaseConnection::ptr &conn, const BaseMessage::ptr &msg, const RequestCallback &cb)
            {
                RequestDescribe::ptr rd = NewDescribe(msg, RType::REQ_CALLBACK, cb);
                if (rd.get() == nullptr)
                    return false;
                conn->Send(msg); 
                return true;
            }

        private:
            RequestDescribe::ptr NewDescribe(const BaseMessage::ptr &req, RType rtype,
                                             const RequestCallback &cb = nullptr)
            {
                std::unique_lock<std::mutex> lock(__mutex);
                RequestDescribe::ptr rd = std::make_shared<RequestDescribe>();
                rd->request = req;
                rd->rtype = rtype;
                if (rtype == RType::REQ_CALLBACK && cb)
                {
                    rd->callback = cb;
                }

                _describes.insert(std::make_pair(rd->request->GetId(), rd));

                return rd;
            }
            RequestDescribe::ptr GetDescribe(const std::string &id)
            {
                std::unique_lock<std::mutex> lock(__mutex);
                auto it = _describes.find(id);
                if (it == _describes.end())
                    return RequestDescribe::ptr();
                return it->second;
            }

            void DelDescribe(const std::string &id)
            {
                std::unique_lock<std::mutex> lock(__mutex);
                _describes.erase(id);
            }

        private:
            std::mutex __mutex;
            std::unordered_map<std::string, RequestDescribe::ptr> _describes;
        };
    }
}