#pragma once
#include "Requestor.hpp"
#include <unordered_set>
namespace Rpc
{
    namespace Client
    {
        class Provider
        {
        public:
            using ptr = std::shared_ptr<Provider>;
            Provider(const Requestor::ptr &requestor) : _requestor(requestor) {}
            bool RegistryMethod(const BaseConnection::ptr &conn, const std::string &method, const Address &host)
            {
                auto msg_req = MessageFactory::CreateMessage<ServiceRequest>();
                msg_req->SetId(UUID::Uuid());
                msg_req->SetMethod(method);
                msg_req->SetHost(host);
                msg_req->SetType(MType::REQ_SERVICE);
                msg_req->SetOptype(ServiceOptype::SERVICE_REGISTRY);
                BaseMessage::ptr msg_rsp;
                bool ret = _requestor->Send(conn, msg_req, msg_rsp);
                if (ret == false)
                {
                    LOG(LogLevel::ERROR) << "注册失败";
                    return false;
                }
                LOG(LogLevel::INFO) << "注册成功";
                auto service_rsp = std::dynamic_pointer_cast<ServiceResponse>(msg_rsp);
                if (service_rsp.get() == nullptr)
                {
                    LOG(LogLevel::ERROR) << "服务响应消息类型向下转换失败";
                    return false;
                }
                if (service_rsp->GetRcode() != RCode::RCODE_OK)
                {
                    LOG(LogLevel::ERROR) << "服务注册失败 " << ErrReason(service_rsp->GetRcode());
                    return false;
                }
                return true;
            }

        private:
            Requestor::ptr _requestor;
        };

        class MethodHost
        {
        public:
            using ptr = std::shared_ptr<MethodHost>;
            MethodHost() : _index(0) {}
            MethodHost(const std::vector<Address> &hosts): 
            _index(0),
            _hosts(hosts.begin(), hosts.end()) {};
            void AppendHost(const Address &host)
            {
                // 中途有服务上线了，添加进来
                std::unique_lock<std::mutex> lock(_mutex);
                _hosts.push_back(host);
            }
            Address ChooseHost()
            {
                std::unique_lock<std::mutex> lock(_mutex);
                size_t pos = _index++ % _hosts.size();
                return _hosts[pos];
            }
            bool Empty() const { return _hosts.empty(); };
            void RemoveHost(const Address &host)
            {
                std::unique_lock<std::mutex> lock(_mutex);
                auto it = std::find(_hosts.begin(), _hosts.end(), host);
                if (it != _hosts.end())
                {
                    _hosts.erase(it);
                }
            }

        private:
            std::mutex _mutex;
            size_t _index;
            std::vector<Address> _hosts;
        };
        class Discoverer
        {
        public:
            using OfflineCallback = std::function<void(const Address&)>;
            using ptr = std::shared_ptr<Discoverer>;
            Discoverer(const Requestor::ptr &requestor,const OfflineCallback &offline_callback) :
            _requestor(requestor),_offline_callback(offline_callback) {}

            bool ServiceDiscovery(const BaseConnection::ptr &conn, const std::string &method, Address &host)
            {
                // 在缓存中存在客服端发现的服务，直接从表中调用，使用轮询算法
                {  
                    std::unique_lock<std::mutex> lock(_mutex);
                    auto it = _method_hosts.find(method);
                    if (it != _method_hosts.end())
                    {
                        if(!it->second->Empty())
                        {
                            host = it->second->ChooseHost();
                            return true;
                        }
                    }
                   
                    // 后续实现一下缓存检查问题
                    //
                    //
                    //
                }
                // 缓冲中没有这个服务，那么就需要去中间服务器发现
                auto msg_req = MessageFactory::CreateMessage<ServiceRequest>();
                msg_req->SetId(UUID::Uuid());
                msg_req->SetMethod(method);
                msg_req->SetType(MType::REQ_SERVICE);
                msg_req->SetOptype(ServiceOptype::SERVICE_DISCOVERY);
                BaseMessage::ptr msg_rsp;
                bool ret = _requestor->Send(conn, msg_req, msg_rsp);
                if (ret == false)
                {
                    LOG(LogLevel::ERROR) << "服务发现失败";
                }
                auto msg = std::dynamic_pointer_cast<ServiceResponse>(msg_rsp);
                if (msg.get() == nullptr)
                {
                    LOG(LogLevel::ERROR) << "服务响应消息类型向下转换失败";
                    return false;
                }
                if (msg->GetRcode() != RCode::RCODE_OK)
                {
                    LOG(LogLevel::ERROR) << "服务发现失败 " << ErrReason(msg->GetRcode());
                    return false;
                }
                // 走到这里说明服务发现成功，将服务信息缓存起来
                
                auto method_hosts = std::make_shared<MethodHost>(msg->GetHosts());
                if (method_hosts->Empty())
                {
                    LOG(LogLevel::ERROR) << "服务发现失败，没有发现服务";
                    return false;
                }
                
                //缓存服务发现的服务
                {
                    std::unique_lock<std::mutex> lock(_mutex);
                    _method_hosts[method] = method_hosts;
                }
                auto hosts = msg->GetHosts();
                if (!hosts.empty())
                {
                    host = method_hosts->ChooseHost();
                    return true;
                }
                return false;
            }

            void OnServiceRequest(const BaseConnection::ptr &conn, const ServiceRequest::ptr &msg)
            {
                //判断是什么请求
                auto optype = msg->GetOptype();
                auto method = msg->GetMethod();
                std::unique_lock<std::mutex> lock(_mutex);
                if(optype == ServiceOptype::SERVICE_REGISTRY)
                {
                    auto it = _method_hosts.find(method);
                    if(it == _method_hosts.end())
                    {
                        auto method_hosts = std::make_shared<MethodHost>();
                        method_hosts->AppendHost(msg->GetHost());
                        _method_hosts[method] = method_hosts;
                    }
                    else
                    {
                        it->second->AppendHost(msg->GetHost());
                    }
                }
                else if(optype == ServiceOptype::SERVICE_OFFLINE)
                {
                    auto it = _method_hosts.find(method);
                    if(it!= _method_hosts.end())
                    {
                        it->second->RemoveHost(msg->GetHost());
                        _offline_callback(msg->GetHost());
                    }
                    else
                        return;
                }
            }

        private:
            OfflineCallback _offline_callback;
            std::mutex _mutex;
            std::unordered_map<std::string, MethodHost::ptr> _method_hosts;
            Requestor::ptr _requestor;
        };
    }
}