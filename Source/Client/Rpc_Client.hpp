#include "../Common/Dispatcher.hpp"
#include "Requestor.hpp"
#include "Rpc_Registry.hpp"
#include "Rpc_Caller.hpp"
#include "Rpc_Topic.hpp"

#include <shared_mutex>
namespace Rpc
{
    namespace Client
    {
        class RegistryClient
        {
        public:
            using ptr = std::shared_ptr<RegistryClient>;
            RegistryClient(const std::string &ip, int port) : 
            _requestor(std::make_shared<Requestor>()),
            _provider(std::make_shared<Provider>(_requestor)),
            _dispatcher(std::make_shared<Dispatcher>())
            {
                auto rsp_cb = std::bind(&Requestor::OnResponse, _requestor.get(), std::placeholders::_1, std::placeholders::_2);
                _dispatcher->RegisterHandler<ServiceResponse>(MType::RSP_SERVICE, rsp_cb); // 注册响应处理函数

                auto msg_cb = std::bind(&Dispatcher::OnMessage, _dispatcher.get(), std::placeholders::_1, std::placeholders::_2);
                _client = ClientFactory::Create(ip, port);
                _client->SetMessageCallback(msg_cb); // 注册消息处理函数
                _client->Connect();
            }

            bool RegisterMethod(const std::string &method, const Address &host)
            {
                return _provider->RegistryMethod(_client->Connection(), method, host);
            }

        private:
            Requestor::ptr _requestor;
            Client::Provider::ptr _provider;
            Dispatcher::ptr _dispatcher;
            BaseClient::ptr _client;
        };

        class DiscoveryClient
        {
        public:
            using ptr = std::shared_ptr<DiscoveryClient>;
            DiscoveryClient(const std::string &ip, int port, 
                const Discoverer::OfflineCallback &offline_cb)
                : _requestor(std::make_shared<Requestor>()),
                  _discoverer(std::make_shared<Discoverer>(_requestor, offline_cb)),
                  _dispatcher(std::make_shared<Dispatcher>())
            {
                auto rsp_cb = std::bind(&Requestor::OnResponse, _requestor.get(),
                                        std::placeholders::_1, std::placeholders::_2);
                _dispatcher->RegisterHandler<ServiceResponse>(MType::RSP_SERVICE, rsp_cb); // 注册响应处理函数

                auto req_cb = std::bind(&Discoverer::OnServiceRequest, _discoverer.get(),
                                        std::placeholders::_1, std::placeholders::_2);
                _dispatcher->RegisterHandler<ServiceRequest>(MType::REQ_SERVICE, req_cb); // 注册响应处理函数
                
                auto msg_cb = std::bind(&Dispatcher::OnMessage, _dispatcher.get(), 
                std::placeholders::_1, std::placeholders::_2);
                
                _client = ClientFactory::Create(ip, port);
                _client->SetMessageCallback(msg_cb); // 注册消息处理函数
                _client->Connect();
            }

            bool ServiceDiscovery(const std::string &method, Address &host)
            {
                return _discoverer->ServiceDiscovery(_client->Connection(), method, host);
            }

        private:
            Requestor::ptr _requestor;
            Client::Discoverer::ptr _discoverer;
            Dispatcher::ptr _dispatcher;
            BaseClient::ptr _client;
        };

        class RpcClient
        {
        public:
            using ptr = std::shared_ptr<RpcClient>;

            RpcClient(bool enablediscovery, const std::string &ip, int port) : _enablediscovery(enablediscovery),
                                                                               _requestor(std::make_shared<Requestor>()),
                                                                               _dispatcher(std::make_shared<Dispatcher>()),
                                                                               _caller(std::make_shared<RpcCaller>(_requestor))
            {
                auto rsp_cb = std::bind(&Requestor::OnResponse, _requestor.get(), std::placeholders::_1, std::placeholders::_2);
                _dispatcher->RegisterHandler<RpcResponse>(MType::RSP_RPC, rsp_cb); // 注册响应处理函数

                if (_enablediscovery)
                {
                    auto offline_cb = std::bind(&RpcClient::DelClient, this, std::placeholders::_1);
                    _discovery_client = std::make_shared<DiscoveryClient>(ip, port, offline_cb);
                }
                else
                {
                    auto msg_cb = std::bind(&Dispatcher::OnMessage, _dispatcher.get(), std::placeholders::_1, std::placeholders::_2);
                    _rpc_client = ClientFactory::Create(ip, port);
                    _rpc_client->SetMessageCallback(msg_cb); // 注册消息处理函数
                    _rpc_client->Connect();
                }
            }

            bool Call(const std::string &method, const Json::Value &params, Json::Value &result)
            {
                BaseClient::ptr client = GetClient(method);
                if (client.get() == nullptr)
                {
                    return false;
                }

                return _caller->Call(client->Connection(), method, params, result);
            }

            bool Call(const std::string &method, const Json::Value &params, RpcCaller::JsonAsyncResponse &result)
            {
                BaseClient::ptr client = GetClient(method);
                if (client.get() == nullptr)
                {
                    return false;
                }

                return _caller->Call(client->Connection(), method, params, result);
            }
            bool Call(const std::string &method, const Json::Value &params, const RpcCaller::JsonResponseCallback &cb)
            {
                BaseClient::ptr client = GetClient(method);
                if (client.get() == nullptr)
                {
                    return false;
                }

                return _caller->Call(client->Connection(), method, params, cb);
            }

        private:
            BaseClient::ptr CreatClient(const Address &host)
            {
                // 第一重检查：无锁或共享锁快速路径
                {
                    std::shared_lock<std::shared_mutex> lock(_shared_mutex); // 使用共享锁，允许多个读线程
                    if (auto it = _rpc_clients.find(host); it != _rpc_clients.end())
                    {
                        return it->second;
                    }
                }

                // 只有未找到的线程会进入这个耗时区域，并且此时无锁，不会阻塞其他读线程
                auto client = ClientFactory::Create(host.first, host.second);
                client->SetMessageCallback(std::bind(&Dispatcher::OnMessage,
                                                     _dispatcher.get(), std::placeholders::_1, std::placeholders::_2));
                client->Connect();

                // 第二重检查：加独占锁，确保唯一性
                std::unique_lock<std::shared_mutex> lock(_shared_mutex);
                // 再次检查！可能在我们创建的过程中，已有其他线程创建并插入了
                if (auto it = _rpc_clients.find(host); it != _rpc_clients.end())
                {
                    // 如果已存在，我们的新client就是多余的，需要销毁清理。返回已存在的那个。
                    // client->Close(); // 必要时清理资源
                    return it->second;
                }
                // 确认我们是第一个也是唯一一个插入此host的线程
                _rpc_clients[host] = client;
                return client;
            }
            BaseClient::ptr GetClient(const Address &host)
            {
                std::unique_lock<std::mutex> lock(_mutex);
                auto it = _rpc_clients.find(host);
                if (it != _rpc_clients.end())
                {
                    return it->second;
                }
                return nullptr;
            }
            BaseClient::ptr GetClient(const std::string &method)
            {
                BaseClient::ptr client;
                // 找到服务提供者
                if (_enablediscovery)
                {
                    Address host;
                    if (_discovery_client->ServiceDiscovery(method, host) == false)
                    {
                        LOG(LogLevel::ERROR) << "service discovery failed";
                        return BaseClient::ptr();
                    }
                    client = GetClient(host);
                    if (client == nullptr)
                    {
                        // 没有已经实例化的客户端，创建一个新的客户端
                        client = CreatClient(host);
                    }
                }
                else
                {
                    client = _rpc_client;
                }
                return client;
            }
            void PutClient(const Address &host, const BaseClient::ptr &client)
            {
                std::unique_lock<std::mutex> lock(_mutex);
                _rpc_clients.insert(std::make_pair(host, client));
            }
            void DelClient(const Address &host)
            {
                std::unique_lock<std::mutex> lock(_mutex);
                auto it = _rpc_clients.find(host);
                if (it != _rpc_clients.end())
                {
                    _rpc_clients.erase(it);
                }
                else
                {
                    LOG(LogLevel::ERROR) << "rpc client not found";
                }
            }

        private:
            struct AddressHash
            {
                size_t operator()(const Address &host) const
                {
                    std::string addr = host.first + std::to_string(host.second);
                    return std::hash<std::string>{}(addr);
                }
            };
            bool _enablediscovery;
            Requestor::ptr _requestor;
            Dispatcher::ptr _dispatcher;
            DiscoveryClient::ptr _discovery_client;
            BaseClient::ptr _rpc_client;
            RpcCaller::ptr _caller;
            std::mutex _mutex;
            std::shared_mutex _shared_mutex;

            // 长连接，实例化rpc客户端  rpc客户端池
            std::unordered_map<Address, BaseClient::ptr, AddressHash> _rpc_clients;
        };


        class TopicClient
        {
        public:
            using ptr = std::shared_ptr<TopicClient>;
            TopicClient(const std::string &ip, int port) : 
            _requestor(std::make_shared<Requestor>()),
            _dispatcher(std::make_shared<Dispatcher>()),
            _topic_manager(std::make_shared<TopicManager>(_requestor))
            {
            
                auto rsp_cb = std::bind(&Requestor::OnResponse, _requestor.get(), std::placeholders::_1, std::placeholders::_2);
                _dispatcher->RegisterHandler<BaseMessage>(MType::RSP_TOPIC, rsp_cb); // 注册响应处理函数

                 auto req_cb = std::bind(&TopicManager::OnPublish,
                     _topic_manager.get(), std::placeholders::_1, std::placeholders::_2);

                _dispatcher->RegisterHandler<TopicRequest>(MType::REQ_TOPIC, req_cb); // 注册响应处理函数

                auto msg_cb = std::bind(&Dispatcher::OnMessage, _dispatcher.get(), std::placeholders::_1, std::placeholders::_2);
                _client = ClientFactory::Create(ip, port);
                _client->SetMessageCallback(msg_cb); // 注册消息处理函数
                _client->Connect();
            }
            bool Creat(const std::string &topic)
            {
                return _topic_manager->CreateTopic(_client->Connection(), topic);
            }
            bool Remove(const std::string &topic)
            {
                return _topic_manager->RemoveTopic(_client->Connection(), topic);
            }
            bool Publish(const std::string &topic, const std::string &msg)
            {
                return _topic_manager->Publish(_client->Connection(), topic, msg);
            }
            bool Subscribe(const std::string &topic, const TopicManager::SubCallback &cb)
            {
                return _topic_manager->Subscribe(_client->Connection(), topic, cb);
            }
            bool Cancelsubscribe(const std::string &topic)
            {
                return _topic_manager->CancelSubscribe(_client->Connection(), topic);
            }
            void Shutdown()
            {
                _client->Shutdown();
            }
        
        private:
            Requestor::ptr _requestor;
            TopicManager::ptr _topic_manager;
            Dispatcher::ptr _dispatcher;
            BaseClient::ptr _client;
            
          

        };
    }
}