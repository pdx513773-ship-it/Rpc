#pragma once
#include "../Common/Message.hpp"
#include "../Common/Net.hpp"

#include <unordered_set>

namespace Rpc
{
    namespace Server
    {
        class TopicManager
        {
        public:
            using ptr = std::shared_ptr<TopicManager>;
            TopicManager() {}
            void OnTopicRequest(const BaseConnection::ptr &conn, const TopicRequest::ptr &msg)
            {
                // 主题的创建
                // 主题的删除
                // 主题的订阅
                // 主题的取消订阅
                // 主题消息的发布
                TopicOptype topic_optype = msg->GetOptype();
                bool ret = true;
                switch (topic_optype)
                {
                case TopicOptype::TOPIC_CREATE:
                    TopicCreat(conn, msg);
                    break;
                case TopicOptype::TOPIC_REMOVE:
                    TopicRemove(conn, msg);
                    break;
                case TopicOptype::TOPIC_SUBSCRIBE:
                    ret = TopicSubscribe(conn, msg);
                    break;
                case TopicOptype::TOPIC_CANCEL:
                    TopicCancel(conn, msg);
                    break;
                case TopicOptype::TOPIC_PUBLISH:
                    TopicPublish(conn, msg);
                    break;
                default:
                    return ErrorResponse(conn, msg, RCode::RCODE_INVALID_OPTYPE);

                }
                if(!ret) return ErrorResponse(conn, msg, RCode::RCODE_NOT_FOUND_TOPIC);
                return topicResponse(conn, msg);
            }

            void OnShutdown(const BaseConnection::ptr &conn)
            {
                std::vector<Topic::ptr> topics;
                Subscriber::ptr subscribers;
                {
                    std::unique_lock<std::mutex> lock(_mutex);
                    auto it = _subscribers.find(conn);
                    if(it == _subscribers.end())
                        return;
                    subscribers = it->second;
                    for(auto &topic_name : it->second->topics)
                    {
                        auto topic_it = _topics.find(topic_name);
                        if(topic_it == _topics.end())
                            continue;
                        topics.push_back(topic_it->second);
                    }
                    _subscribers.erase(it);
                }
                for(auto &topic : topics)
                {
                    topic->RemoveSubscriber(subscribers);
                }
            }


            struct Subscriber
            {
                using ptr = std::shared_ptr<Subscriber>;
                std::unordered_set<std::string> topics;
                std::mutex mutex;
                BaseConnection::ptr conn;
                Subscriber(const BaseConnection::ptr &conn) : conn(conn) {}
                void AppendTopic(const std::string &topic_name)
                {
                    std::unique_lock<std::mutex> lock(mutex);
                    topics.insert(topic_name);
                }
                void RemoveTopic(const std::string &topic_name)
                {
                    std::unique_lock<std::mutex> lock(mutex);
                    topics.erase(topic_name);
                }
            };

            struct Topic
            {
                using ptr = std::shared_ptr<Topic>;
                std::string topic_name;
                std::mutex mutex;
                std::unordered_set<Subscriber::ptr> subscribers;
                Topic(const std::string &topic_name) : topic_name(topic_name) {}

                void AppendSubscriber(const Subscriber::ptr &_subscriber)
                {
                    std::unique_lock<std::mutex> lock(mutex);
                    subscribers.insert(_subscriber);
                }
                void RemoveSubscriber(const Subscriber::ptr &_subscriber)
                {
                    std::unique_lock<std::mutex> lock(mutex);
                    subscribers.erase(_subscriber);
                }

                void Publish(const BaseMessage::ptr &msg)
                {
                    std::unique_lock<std::mutex> lock(mutex);
                    for (auto &subscriber : subscribers)
                    {
                        for (auto &topic_name : subscriber->topics)
                        {
                            if (topic_name == this->topic_name)
                            {
                                subscriber->conn->Send(msg);
                            }
                        }
                    }
                }
            };

        private:
            void ErrorResponse(const BaseConnection::ptr &conn, const TopicRequest::ptr &msg,const RCode rcode)
            {
                auto msg_rsp = MessageFactory::CreateMessage<TopicResponse>();
                msg_rsp->SetId(msg->GetId());
                msg_rsp->SetType(MType::RSP_TOPIC);
                msg_rsp->SetRcode(rcode);
                
                conn->Send(msg_rsp);
            }

            void topicResponse(const BaseConnection::ptr &conn, const TopicRequest::ptr &msg)
            {
                auto msg_rsp = MessageFactory::CreateMessage<TopicResponse>();
                msg_rsp->SetId(msg->GetId());
                msg_rsp->SetType(MType::RSP_TOPIC);
                
                msg_rsp->SetRcode(RCode::RCODE_OK);
                conn->Send(msg_rsp);
            }
            bool TopicSubscribe(const BaseConnection::ptr &conn, const TopicRequest::ptr &msg)
            {
                // 1.先找出主题对象 和 订阅者的信息 如果没有订阅者，我就为这个连接也就是这个客户端所建立一个 subscriber 对象
                // 2.将这个连接加入到订阅者的集合中
                // 3.将这个主题对象加入到订阅者的主题集合中
                Topic::ptr topic;
                Subscriber::ptr subscriber;
                {
                    std::unique_lock<std::mutex> lock(_mutex);
                    auto it = _topics.find(msg->GetTopicKey());
                    if (it == _topics.end())
                        return false;
                    topic = it->second;
                    auto sub_it = _subscribers.find(conn);
                    if (sub_it == _subscribers.end())
                    {
                        subscriber = std::make_shared<Subscriber>(conn);
                    }
                    else
                    {
                        subscriber = sub_it->second;
                    }
                    _subscribers.insert(std::make_pair(conn, subscriber));
                }

                topic->AppendSubscriber(subscriber);
                subscriber->AppendTopic(msg->GetTopicKey());
                return true;
            }

            void TopicCancel(const BaseConnection::ptr &conn, const TopicRequest::ptr &msg)
            {
                Topic::ptr topic;
                Subscriber::ptr subscriber;
                {
                    std::unique_lock<std::mutex> lock(_mutex);
                    auto it = _topics.find(msg->GetTopicKey());
                    if (it != _topics.end())
                        topic = it->second;

                    auto sub_it = _subscribers.find(conn);
                    if (sub_it != _subscribers.end())
                        subscriber = sub_it->second;
                }
                if (subscriber)
                    subscriber->RemoveTopic(msg->GetTopicKey());
                if (topic && subscriber)
                    topic->RemoveSubscriber(subscriber);
            }

            void TopicCreat(const BaseConnection::ptr &conn, const TopicRequest::ptr &msg)
            {
                std::unique_lock<std::mutex> lock(_mutex);
                auto topic_name = msg->GetTopicKey();
                auto topic = std::make_shared<Topic>(topic_name);
                _topics.insert(std::make_pair(topic_name, topic));
            }

            void TopicRemove(const BaseConnection::ptr &conn, const TopicRequest::ptr &msg)
            {
                std::string topic_name = msg->GetTopicKey();
                std::unordered_set<Subscriber::ptr> sub;
                {
                    std::unique_lock<std::mutex> lock(_mutex);
                    auto it = _topics.find(topic_name);
                    if (it == _topics.end())
                        return;
                    sub = it->second->subscribers;
                    _topics.erase(it);
                }
                for (auto &subscriber : sub)
                {
                    subscriber->RemoveTopic(topic_name);
                }
            }
            bool TopicPublish(const BaseConnection::ptr &conn, const TopicRequest::ptr &msg)
            {
                Topic::ptr topic;
                Subscriber::ptr subscriber;
                {
                    std::unique_lock<std::mutex> lock(_mutex);
                    auto it = _topics.find(msg->GetTopicKey());
                    if (it == _topics.end())
                        return false;
                    topic = it->second;
                }
                topic->Publish(msg);
                return true;
            }

        private:
            std::unordered_map<std::string, Topic::ptr> _topics;
            std::unordered_map<BaseConnection::ptr, Subscriber::ptr> _subscribers;
            std::mutex _mutex;
        };
    }
}