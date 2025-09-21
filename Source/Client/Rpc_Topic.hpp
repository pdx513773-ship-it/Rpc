#pragma once
#include "Requestor.hpp"
#include <unordered_set>

namespace Rpc
{
    namespace Client
    {
        class TopicManager
        {
        public:
            using SubCallback = std::function<void(const std::string &topic_name, const std::string &msg)>;
            using ptr = std::shared_ptr<TopicManager>;
            TopicManager(const Requestor::ptr &requestor) : _requestor(requestor) {}
            bool CreateTopic(const BaseConnection::ptr &conn, const std::string &topic_name)
            {
                LOG(LogLevel::DEBUG)<< "CreateTopic " << topic_name;
                Request(conn, topic_name, TopicOptype::TOPIC_CREATE);
                return true;
            }
            bool RemoveTopic(const BaseConnection::ptr &conn, const std::string &topic_name)
            {
                Request(conn, topic_name, TopicOptype::TOPIC_REMOVE);
                return true;
            }
            bool Subscribe(const BaseConnection::ptr &conn, const std::string &topic_name, const SubCallback &callback)
            {
                AppendSubscribers(topic_name, callback);
                bool ret = Request(conn, topic_name, TopicOptype::TOPIC_SUBSCRIBE);
                if (!ret)
                    return false;

                return true;
            }
            bool CancelSubscribe(const BaseConnection::ptr &conn, const std::string &topic_name)
            {
                RemoveSubscribers(topic_name);
                bool ret = Request(conn, topic_name, TopicOptype::TOPIC_CANCEL);
                if (!ret)
                    return false;

                return true;
            }
            bool Publish(const BaseConnection::ptr &conn, const std::string &topic_name, const std::string &msg)
            {
                return Request(conn, topic_name, TopicOptype::TOPIC_PUBLISH, msg);
            }
            void OnPublish(const BaseConnection::ptr &conn, const TopicRequest::ptr &msg)
            {
                auto type = msg->GetOptype();
                if (type != TopicOptype::TOPIC_PUBLISH)
                    return;
                auto topic_name = msg->GetTopicKey();
                auto topic_msg = msg->GetTopicMsg();
                auto callback = GetSubscribers(topic_name);
                if (!callback)
                    return;
                return callback(topic_name, topic_msg.asString());
            }

        private:
            void AppendSubscribers(const std::string &topic_name, const SubCallback &cb)
            {
                std::unique_lock<std::mutex> lock(_mutex);
                _topic_callbacks[topic_name] = cb;
            }
            void RemoveSubscribers(const std::string &topic_name)
            {
                std::unique_lock<std::mutex> lock(_mutex);
                _topic_callbacks.erase(topic_name);
            }
            const SubCallback GetSubscribers(const std::string &topic_name)
            {
                std::unique_lock<std::mutex> lock(_mutex);
                auto it = _topic_callbacks.find(topic_name);
                if (it != _topic_callbacks.end())
                {
                    return it->second;
                }
                return SubCallback();
            }
            bool Request(const BaseConnection::ptr &conn, const std::string topic_name,
                         TopicOptype type, const std::string &msg = "")
            {
                auto msg_req = MessageFactory::CreateMessage<TopicRequest>();
                msg_req->SetId(UUID::Uuid());
                msg_req->SetTopicKey(topic_name);
                msg_req->SetOptype(type);
                msg_req->SetType(MType::REQ_TOPIC);
                if (type == TopicOptype::TOPIC_PUBLISH)
                {
                    msg_req->SetTopicMsg(msg);
                }
                BaseMessage::ptr msg_rsp;
                LOG(LogLevel::DEBUG)<<"send";
                bool ret = _requestor->Send(conn, msg_req, msg_rsp);
                LOG(LogLevel::DEBUG)<<"send sucessed";

                if (!ret)
                {
                    LOG(LogLevel::ERROR) << "Request topic failed";
                    return false;
                }
                auto topic_rsp_msg = std::dynamic_pointer_cast<TopicResponse>(msg_rsp);
                if (!topic_rsp_msg)
                {
                    LOG(LogLevel::DEBUG) << "dynamic error";
                    return false;
                }
                if (topic_rsp_msg->GetRcode() != RCode::RCODE_OK)
                {
                    LOG(LogLevel::DEBUG) << "主题操作请求出错 " << "ErrReason(topic_rsp_msg->RCode()";
                    return false;
                }
                return true;
            }

        private:
            std::mutex _mutex;
            Requestor::ptr _requestor;
            std::unordered_map<std::string, SubCallback> _topic_callbacks;
        };
    }
}
