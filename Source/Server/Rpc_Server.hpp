#include "../Common/Dispatcher.hpp"
#include "../Client/Rpc_Client.hpp"
#include "Rpc_Router.hpp"
#include "Rpc_Registry.hpp"
#include "Rpc_Topic.hpp"
namespace Rpc
{
    namespace Server
    {
        class RegistryServer
        {
        public:
            using ptr = std::shared_ptr<RegistryServer>;
            RegistryServer(int port) : _pd_manager(std::make_shared<PDManager>()),
                                       _dispatcher(std::make_shared<Dispatcher>())
            {
                auto server_cb = std::bind(&PDManager::OnServiceRequest,
                                           _pd_manager, std::placeholders::_1, std::placeholders::_2);
                _dispatcher->RegisterHandler<ServiceRequest>(MType::REQ_SERVICE, server_cb);

                _server = ServerFactory::Create(port);
                auto message_cb = std::bind(&Dispatcher::OnMessage, _dispatcher,
                                            std::placeholders::_1, std::placeholders::_2);
                _server->SetMessageCallback(message_cb);
                auto close_cb = std::bind(&RegistryServer::OnConnShutdown, this, std::placeholders::_1);
                _server->SetCloseCallback(close_cb);
                _server->Start();
            }
            void Start()
            {
                _server->Start();
            }

        private:
            void OnConnShutdown(const BaseConnection::ptr &conn)
            {
                _pd_manager->OnConnShutdown(conn);
            }

        private:
            PDManager::ptr _pd_manager;
            Dispatcher::ptr _dispatcher;
            BaseServer::ptr _server;
        };

        class RpcServer
        {
        public:
            using ptr = std::shared_ptr<RpcServer>;

            RpcServer(const Address &access_addr, bool enableRegistry = false,
                      const Address &registry_addr = Address()) : _router(std::make_shared<RpcRouter>()),
                                                                  _dispatcher(std::make_shared<Dispatcher>()),
                                                                  _enableregistry(enableRegistry),
                                                                  _access_addr(access_addr)
            {
                if (enableRegistry)
                {
                    _reg_client = std::make_shared<Client::RegistryClient>(
                        registry_addr.first, registry_addr.second);
                }
                auto server_cb = std::bind(&RpcRouter::OnRpcRequest, _router,
                                           std::placeholders::_1, std::placeholders::_2);
                _dispatcher->RegisterHandler<RpcRequest>(MType::REQ_RPC, server_cb);

                _server = ServerFactory::Create(access_addr.second);
                auto message_cb = std::bind(&Dispatcher::OnMessage, _dispatcher,
                                            std::placeholders::_1, std::placeholders::_2);
                _server->SetMessageCallback(message_cb);
            }
            void RegisterMethod(const ServerDescribe::ptr &service)
            {
                if (_enableregistry)
                {
                    _reg_client->RegisterMethod(service->GetMethod(), _access_addr);
                }
                _router->RegisterMethod(service);
            }
            void Start()
            {
                _server->Start();
            }

        private:
            bool _enableregistry;
            Address _access_addr;
            RpcRouter::ptr _router;
            Client::RegistryClient::ptr _reg_client;
            Dispatcher::ptr _dispatcher;
            BaseServer::ptr _server;
        };

        class TopicServer
        {
        public:
            using ptr = std::shared_ptr<TopicServer>;
            TopicServer(int port) : _topic_manager(std::make_shared<TopicManager>()),
                                    _dispatcher(std::make_shared<Dispatcher>())
            {
                auto topic_cb = std::bind(&TopicManager::OnTopicRequest,
                                           _topic_manager, std::placeholders::_1, std::placeholders::_2);
                _dispatcher->RegisterHandler<TopicRequest>(MType::REQ_TOPIC, topic_cb); 

                _server = ServerFactory::Create(port);
                auto message_cb = std::bind(&Dispatcher::OnMessage, _dispatcher,
                                            std::placeholders::_1, std::placeholders::_2);
                _server->SetMessageCallback(message_cb);

                auto close_cb = std::bind(&TopicServer::OnConnShutdown, this, std::placeholders::_1);
                _server->SetCloseCallback(close_cb);
            }

            void OnTopicRequest(BaseConnection::ptr &conn, const TopicRequest::ptr &msg)
            {
            }
            void Start()
            {
                _server->Start();
            }

        private:
            void OnConnShutdown(const BaseConnection::ptr &conn)
            {
                _topic_manager->OnShutdown(conn);
            }

        private:
            TopicManager::ptr _topic_manager;
            Dispatcher::ptr _dispatcher;
            BaseServer::ptr _server;
        };

    }

}