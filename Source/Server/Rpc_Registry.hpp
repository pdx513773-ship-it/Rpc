#pragma once
#include "../Common/Message.hpp"
#include "../Common/Net.hpp"

#include <set>

namespace Rpc
{
    namespace Server
    {
        class ProviderManager
        {
        public:
            using ptr = std::shared_ptr<ProviderManager>;
            struct Provider
            {
                using ptr = std::shared_ptr<Provider>;
                BaseConnection::ptr conn;
                std::mutex mutex;
                Address host;
                std::vector<std::string> methods;
                Provider(const BaseConnection::ptr &conn, const Address &host) : conn(conn), host(host) {}
                void AppendMethod(const std::string &method)
                {
                    std::lock_guard<std::mutex> lock(mutex);
                    methods.emplace_back(method);
                }
            };
                // 新的服务提供服务注册时调用
                void AddProvider(const BaseConnection::ptr &conn, const Address &host, const std::string &method)
                {
                    Provider::ptr provider;
                    // 查找连接所关联的服务提供者，找到则获取，找不到则创建，并建立关联
                    {
                        std::unique_lock<std::mutex> lock(_mutex);

                        auto it = _conns.find(conn);
                        if (it != _conns.end())
                            provider = it->second;
                        else
                        {
                            provider = std::make_shared<Provider>(conn, host);
                            _conns.insert(std::make_pair(conn, provider));
                        }
                        //???是否合理？？如果是第一个注册这个方法的客户端，那么返回的是一个
                        // 空的provider数组类型，然后我在后面插入了一个provider，这时候provider数组就有了元素，
                        // 如果是一个第二个注册这个方法的客户端，那么返回的就是要管理这个方法的数组，然后插入进去一起管理起来
                        // 没问题
                        auto &provider_list = _providers[method];
                        provider_list.insert(provider);
                    }
                    // method方法要多出一个
                    provider->AppendMethod(method); //?????是否要检查这个方法已经添加过了？
                }
                // 当一个服务提供者断开连接后，获取他的服务 -- 用于提供服务下线通知
                Provider::ptr GetProvider(const BaseConnection::ptr &conn)
                {
                    std::unique_lock<std::mutex> lock(_mutex);

                    auto it = _conns.find(conn);
                    if (it != _conns.end())
                        return it->second;
                    else
                        return nullptr;
                }
                // 当一个服务提供者断开连接后，删除他的关联信息
                void DelProvider(const BaseConnection::ptr &conn)
                {
                    std::unique_lock<std::mutex> lock(_mutex);
                    auto it = _conns.find(conn);
                    if (it == _conns.end())
                        return;
                    for (auto &method : it->second->methods)
                    {
                        auto &provider_list = _providers[method];
                        provider_list.erase(it->second);
                    }
                    _conns.erase(it);
                }
                std::vector<Address> GetProviders(const std::string &method)
                {
                    std::unique_lock<std::mutex> lock(_mutex);
                    auto it = _providers.find(method);
                    if (it == _providers.end())
                        return std::vector<Address>();
                    else
                    { 
                        std::vector<Address> hosts;
                        for(auto &provider : it->second)
                        {
                            hosts.emplace_back(provider->host);
                        }
                        return hosts;
                        //std::vector<Address> result(it->second.begin(), it->second.end());
                    }
                }

            

            private:
                std::mutex _mutex;
                // 管理一个连接到服务提供者的列表
                std::unordered_map<BaseConnection::ptr, Provider::ptr> _conns;
                // 管理一个方法到服务提供者的映射
                std::unordered_map<std::string, std::set<Provider::ptr>> _providers;
            };

            class DisCovererManager
            {
            public:
                using ptr = std::shared_ptr<DisCovererManager>;
                struct Discoverer
                {
                    using ptr = std::shared_ptr<Discoverer>;
                    BaseConnection::ptr conn;
                    std::mutex mutex;
                    Address host;
                    std::vector<std::string> methods;
                    Discoverer(BaseConnection::ptr conn) : conn(conn) {}
                    void AppendMethod(const std::string &method)
                    {
                        std::unique_lock<std::mutex> lock(mutex);
                        methods.emplace_back(method);
                    }
                };
                // 新的服务发现者注册时调用 服务发现的时候发现新的发现者
                Discoverer::ptr AddDiscoverer(const BaseConnection::ptr &conn, const std::string &method)
                {
                    Discoverer::ptr discoverer;
                    {
                        std::unique_lock<std::mutex> lock(_mutex);

                        auto it = _conns.find(conn);
                        if (it != _conns.end())
                        {
                            discoverer = it->second;
                        }
                        else
                        {
                            discoverer = std::make_shared<Discoverer>(conn);
                            _conns.insert(std::make_pair(conn, discoverer));
                        }
                        auto discoverers_list = _discoverers[method];
                        discoverers_list.insert(discoverer);
                    }
                    discoverer->AppendMethod(method);
                    return discoverer;
                }
                // 当一个服务发现者断开连接后，删除他的关联信息
                void DelDiscoverer(BaseConnection::ptr conn)
                {
                    std::unique_lock<std::mutex> lock(_mutex);
                    
                    auto it = _conns.find(conn);
                    if (it == _conns.end())
                        return;
                    for(auto &method : it->second->methods)
                    {
                        auto &discoverers_list = _discoverers[method];
                        discoverers_list.erase(it->second);
                    }
                    _conns.erase(conn);
                }

                void OnlineNotify(const std::string &method, const Address &host)
                {
                   return Notify(method,host,ServiceOptype::SERVICE_ONLINE);   
                }

                void OfflineNotify(const std::string &method, const Address &host)
                {
                    return Notify(method,host,ServiceOptype::SERVICE_OFFLINE);
                }
            private:
                void Notify(const std::string &method, const Address &host, ServiceOptype optype)
                {
                    std::unique_lock<std::mutex> lock(_mutex);
                    auto it = _discoverers.find(method);
                    if(it == _discoverers.end())
                        return;

                    auto msg_req = MessageFactory::CreateMessage<ServiceRequest>();
                    msg_req->SetId(UUID::Uuid());
                    msg_req->SetMethod(method);
                    msg_req->SetHost(host);
                    msg_req->SetType(MType::REQ_SERVICE);
                    msg_req->SetOptype(optype);
                     
                    for(auto &discoverer : it->second)
                    {
                        discoverer->conn->Send(msg_req);
                    }
                }

            private:
                std::mutex _mutex;
                // 管理一个连接到服务发现者的列表
                std::unordered_map<BaseConnection::ptr, Discoverer::ptr> _conns;
                // 管理一个方法到服务发现者的映射
                std::unordered_map<std::string, std::set<Discoverer::ptr>> _discoverers;
            };

            class PDManager
            {
            public:
                using ptr = std::shared_ptr<PDManager>;
                PDManager() : 
                _providers(std::make_shared<ProviderManager>()), 
                _discovers(std::make_shared<DisCovererManager>()) {}
                void OnServiceRequest(const BaseConnection::ptr &conn,const ServiceRequest::ptr &msg)
                {
                    //服务注册，服务发现
                    //1.服务注册 2.服务上线的通知
                    ServiceOptype optype = msg->GetOptype();
                    if(optype == ServiceOptype::SERVICE_REGISTRY)
                    {
                        _providers->AddProvider(conn, msg->GetHost(), msg->GetMethod());
                        _discovers->OnlineNotify(msg->GetMethod(), msg->GetHost());
                        return RegistryResponse(conn, msg);
                    }
                    else if(optype == ServiceOptype::SERVICE_DISCOVERY)
                    {
                        _discovers->AddDiscoverer(conn, msg->GetMethod());
                        return DiscoverResponse(conn, msg);
                    }
                    else LOG(LogLevel::ERROR)<<"收到位置的服务操作请求";
                    return ErrorResponse(conn, msg);
                }
                void OnConnShutdown(BaseConnection::ptr conn)
                {
                    auto provider = _providers->GetProvider(conn);
                    if(provider.get() != nullptr)
                    {
                        for(auto &method : provider->methods)
                        {
                            _discovers->OfflineNotify(method, provider->host);
                        }
                        _providers->DelProvider(conn);
                    }
                    _discovers->DelDiscoverer(conn);
                }
            private:
                void ErrorResponse(const BaseConnection::ptr &conn, const ServiceRequest::ptr &msg) {
                    auto msg_rsp = MessageFactory::CreateMessage<ServiceResponse>();
                    msg_rsp->SetId(msg->GetId());
                    msg_rsp->SetType(MType::RSP_SERVICE);
                    msg_rsp->SetRcode(RCode::RCODE_INVALID_OPTYPE);
                    msg_rsp->SetOptype(ServiceOptype::SERVICE_UNKNOW);
                    conn->Send(msg_rsp);
                }    

                void RegistryResponse(const BaseConnection::ptr &conn,const ServiceRequest::ptr &msg)
                {
                    auto msg_rsp = MessageFactory::CreateMessage<ServiceResponse>();
                    msg_rsp->SetId(msg->GetId());
                    msg_rsp->SetType(MType::RSP_SERVICE);
                    msg_rsp->SetOptype(ServiceOptype::SERVICE_REGISTRY);
                    msg_rsp->SetRcode(RCode::RCODE_OK);
                    conn->Send(msg_rsp);
                }

                void DiscoverResponse(const BaseConnection::ptr &conn,const ServiceRequest::ptr &msg)
                {
                    auto msg_rsp = MessageFactory::CreateMessage<ServiceResponse>();
                    std::vector<Address> hosts = _providers->GetProviders(msg->GetMethod());
                    msg_rsp->SetId(msg->GetId());
                    msg_rsp->SetType(MType::RSP_SERVICE);
                    msg_rsp->SetOptype(ServiceOptype::SERVICE_DISCOVERY);
                    if(hosts.empty())
                    {
                        msg_rsp->SetRcode(RCode::RCODE_NOT_FOUND_SERVICE);
                        return conn->Send(msg_rsp);
                    }
                    
                    msg_rsp->SetRcode(RCode::RCODE_OK);
                    msg_rsp->SetHost(hosts);   
                    msg_rsp->SetMethod(msg->GetMethod());

                    
                    conn->Send(msg_rsp);
                }
            private:
                std::mutex _mutex;
                ProviderManager::ptr _providers;
                DisCovererManager::ptr _discovers;
            };
        }
}


