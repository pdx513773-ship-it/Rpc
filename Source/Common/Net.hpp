#pragma once

#include <muduo/net/TcpServer.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/TcpConnection.h>
#include <muduo/net/Buffer.h>
#include <muduo/base/CountDownLatch.h>
#include <muduo/net/EventLoopThread.h>
#include <muduo/net/TcpClient.h>
#include "Detail.hpp"
#include "Fields.hpp"
#include "Abstract.hpp"
#include "Message.hpp"
#include <mutex>
#include <unordered_map>

namespace Rpc
{
    class MuduoBuffer : public BaseBuffer
    {
    public:
        using ptr = std::shared_ptr<MuduoBuffer>;
        MuduoBuffer(muduo::net::Buffer *buf) : _buf(buf) {}
        virtual size_t ReadableSize() override
        {
            return _buf->readableBytes();
        }
        virtual int32_t PeekInt32() override
        {
            // muduo库是网络库，从缓冲区取出4字节整形，会进行网络字节序的转换，变成host字节序
            return _buf->peekInt32();
        }
        virtual void RetrieveInt32() override
        {
            _buf->retrieveInt32();
        }
        virtual int32_t ReadInt32() override
        {
            return _buf->readInt32();
        }
        virtual std::string RetrieveAsString(size_t len) override
        {
            return _buf->retrieveAsString(len);
        }

    private:
        // 这个指针的资源管理并不是由muduobuffer来进行的，muduobuffer目的是实现功能向外提供接口，并不负责资源的释放
        muduo::net::Buffer *_buf;
    };
    class BufferFactory
    {
    public:
        template <typename... Args>
        static BaseBuffer::ptr Create(Args &&...args)
        {
            return std::make_shared<MuduoBuffer>(std::forward<Args>(args)...);
        }
    };

    class LVProtocol : public BaseProtocol
    {
    public:
        using ptr = std::shared_ptr<LVProtocol>;

        // 判断缓冲区的数据是否够一条消息
        virtual bool IsProcessable(const BaseBuffer::ptr &buffer) override
        {
            if (buffer->ReadableSize() < lenFieldLength)
            {
                return false;
            }
            int32_t total_len = buffer->PeekInt32();
            if (buffer->ReadableSize() < total_len + lenFieldLength)
            {
                return false;
            }
            return true;
        }
        virtual bool OnMessage(const BaseBuffer::ptr &buffer, BaseMessage::ptr &msg) override
        {
            // 调用OnMessage默认是至少有一个完整的数据报文才会被处理，所以这里不需要判断数据是否够一条消息
            int32_t total_len = buffer->ReadInt32();    // 读取总长度
            MType mtype = (MType)(buffer->ReadInt32()); // 读取数据类型
            int32_t idlen = buffer->ReadInt32();        // 读取id长度
            int32_t body_len = total_len - idlen - mtypeFieldLength - lenFieldLength;
            std::string id = buffer->RetrieveAsString(idlen);      // 读取id
            std::string body = buffer->RetrieveAsString(body_len); // 读取body
            // msg = MessageFactory::CreateMessage(static_cast<MType>(mtype));
            msg = MessageFactory::CreateMessage(mtype);
            if (msg.get() == nullptr)
            {
                LOG(LogLevel::ERROR) << "message type error,creat message failed";
                return false;
            }
            bool ret = msg->Deserialize(body);
            if (!ret)
            {
                LOG(LogLevel::ERROR) << "deserialize message failed";
                return false;
            }
            msg->SetId(id);
            msg->SetType(mtype);

            return true;
        }
        virtual std::string Serialize(const BaseMessage::ptr &msg) override
        {
            // |--Len--|--mtype--|--idlen--|--id--|--body--|
            std::string body = msg->Serialize();
            std::string id = msg->GetId();
            auto mtype = htonl((int32_t)msg->GetType());
            int32_t idlen = htonl(id.size());
            int32_t h_total_len = mtypeFieldLength + idlenFieldLength + id.size() + body.size();
            int32_t n_total_len = htonl(h_total_len);

            std::string result;
            result.reserve(h_total_len);
            result.append((char *)&n_total_len, lenFieldLength);
            result.append((char *)&mtype, mtypeFieldLength);
            result.append((char *)&idlen, idlenFieldLength);
            result.append(id);
            result.append(body);
            return result;
        }

    private:
        const size_t lenFieldLength = 4;
        const size_t mtypeFieldLength = 4;
        const size_t idlenFieldLength = 4;
    };

    class ProtocolFactory
    {
    public:
        template <typename... Args>
        static BaseProtocol::ptr Create(Args &&...args)
        {
            return std::make_shared<LVProtocol>(std::forward<Args>(args)...);
        }
    };

    class MuduoConnection : public BaseConnection
    {
    public:
        using ptr = std::shared_ptr<MuduoConnection>;

        MuduoConnection(const muduo::net::TcpConnectionPtr &conn,
                        const BaseProtocol::ptr &protocol) : _conn(conn), _protocol(protocol)
        {
        }

        virtual void Send(const BaseMessage::ptr &msg) override
        {
            LOG(LogLevel::DEBUG)<<"发送数据包";
            LOG(LogLevel::DEBUG)<<"发送数据包";
            LOG(LogLevel::DEBUG)<<"发送数据包";

            std::string body = _protocol->Serialize(msg);
            _conn->send(body);
        }
        virtual bool Connected() override
        {
            return _conn->connected();
        }
        virtual void Shutdown() override
        {
            _conn->shutdown();
        }

    private:
        BaseProtocol::ptr _protocol;
        muduo::net::TcpConnectionPtr _conn;
    };

    class ConnectionFactory
    {
    public:
        template <typename... Args>
        static BaseConnection::ptr Create(Args &&...args)
        {
            return std::make_shared<MuduoConnection>(std::forward<Args>(args)...);
        }
    };

    class MuduoServer : public BaseServer
    {
    public:
        using ptr = std::shared_ptr<MuduoServer>;
        MuduoServer(int port) : _server(&_baseloop, muduo::net::InetAddress("0,0,0,0", port), "MuduoServer", muduo::net::TcpServer::kReusePort),
                                _protocol(ProtocolFactory::Create()) {}
        virtual void Start()
        {
            _server.setConnectionCallback(std::bind(&MuduoServer::onConnection, this, std::placeholders::_1));
            _server.setMessageCallback(std::bind(&MuduoServer::onMessage, this,
                                                 std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
            _server.start();
            _baseloop.loop();
        }

    private:
        void onConnection(const muduo::net::TcpConnectionPtr &conn)
        {
            if (conn->connected())
            {
                std::cout << "连接建立" << std::endl;
                auto muduo_conn = ConnectionFactory::Create(conn, _protocol);

                {
                    std::unique_lock<std::mutex> lock(_mutex);
                    _conns[conn] = muduo_conn;
                }
                if (_on_connection)
                    _on_connection(muduo_conn);
            }
            else
            {
                std::cout << "连接断开" << std::endl;
                BaseConnection::ptr muduo_conn;
                {
                    std::unique_lock<std::mutex> lock(_mutex);
                    auto it = _conns.find(conn);
                    if (it != _conns.end())
                        return;
                    muduo_conn = it->second;
                    _conns.erase(conn);
                }
                if (_on_close)
                    _on_close(muduo_conn);
            }
        }
        void onMessage(const muduo::net::TcpConnectionPtr &conn, muduo::net::Buffer *buf, muduo::Timestamp)
        {
            LOG(LogLevel::DEBUG) << "有数据到来";
            // 明白这个函数是干嘛用的才能编写
            //  1.首先检查缓冲区的数据是否是可处理的
            //  2.再交给协议进行一个反序列化的处理
            //  3.根据反序列化的结果，创建消息对象，并交给回调函数处理
            auto base_buf = BufferFactory::Create(buf);
            while (1)
            {
                if (_protocol->IsProcessable(base_buf) == false)
                {
                    if (base_buf->ReadableSize() > maxDataSize)
                    {
                        LOG(LogLevel::ERROR) << "数据包太大";
                        conn->shutdown();
                        return;
                    }
                    LOG(LogLevel::DEBUG) << "数据包不完整";
                    break;
                }
                BaseMessage::ptr msg;
                bool ret = _protocol->OnMessage(base_buf, msg);
                if (ret == false)
                {
                    LOG(LogLevel::ERROR) << "数据包解析失败";
                    conn->shutdown();
                    return;
                }
                BaseConnection::ptr base_conn;
                {
                    std::unique_lock<std::mutex> lock(_mutex);
                    auto it = _conns.find(conn);
                    if (it == _conns.end())
                    {
                        conn->shutdown();
                        return;
                    }
                    base_conn = it->second;
                }
                if (_on_message)
                    _on_message(base_conn, msg);
            }
        }

    private:
        const size_t maxDataSize = (1 << 16);
        BaseProtocol::ptr _protocol;
        muduo::net::EventLoop _baseloop;
        muduo::net::TcpServer _server;
        std::mutex _mutex;
        std::unordered_map<muduo::net::TcpConnectionPtr, BaseConnection::ptr> _conns;
    };

    class ServerFactory
    {
    public:
        template <typename... Args>
        static BaseServer::ptr Create(Args &&...args)
        {
            return std::make_shared<MuduoServer>(std::forward<Args>(args)...);
        }
    };

    class MuduoClient : public BaseClient
    {
    public:
        using ptr = std::shared_ptr<MuduoClient>;
        MuduoClient(const std::string &ip, int port) : _downlatch(1),
                                                       _protocol(ProtocolFactory::Create()),
                                                       _baseloop(_loopthread.startLoop()),
                                                       _client(_baseloop, muduo::net::InetAddress(ip, port), "MuduoClient") {}

        virtual void Connect() override
        {
            LOG(LogLevel::DEBUG) << "设置回调函数";
            _client.setConnectionCallback(std::bind(&MuduoClient::onConnection, this, std::placeholders::_1));
            _client.setMessageCallback(std::bind(&MuduoClient::onMessage, this, std::placeholders::_1,
                                                 std::placeholders::_2, std::placeholders::_3));

            _client.connect();
            _downlatch.wait();
            LOG(LogLevel::DEBUG) << "连接服务器成功";
        }

        virtual bool Send(const BaseMessage::ptr &msg) override
        {
            // 获取连接的本地副本并加锁
            BaseConnection::ptr conn;
            bool is_connected = false;

            {
                std::lock_guard<std::mutex> lock(_conn_mutex);
                if (!_conn || !_conn->Connected())
                {
                    LOG(LogLevel::ERROR) << "连接已断开";
                    return false;
                }
                conn = _conn;
                is_connected = true;
            }

            // 只有在确认连接有效后才尝试发送
            if (!is_connected)
            {
                LOG(LogLevel::ERROR) << "连接已断开";
                return false;
            }

            try
            {
                std::string serialized_data = msg->Serialize();
                LOG(LogLevel::DEBUG) << "消息序列化成功，长度: " << serialized_data.length();

                conn->Send(msg);
                LOG(LogLevel::DEBUG) << "消息发送成功";
                return true;
            }
            catch (const std::exception &e)
            {
                LOG(LogLevel::ERROR) << "发送消息时发生异常: " << e.what();

                // 发送失败可能意味着连接已断开，更新状态
                std::lock_guard<std::mutex> lock(_conn_mutex);
                if (_conn == conn) // 确保不会错误地重置新连接
                {
                    _conn.reset();
                }
                return false;
            }
        }

        virtual void Shutdown() override
        {
            return _client.disconnect();
        }
        virtual BaseConnection::ptr Connection() override
        {
            return _conn;
        }
        virtual bool Connected() override
        {
            return (_conn && _conn->Connected());
        }

    private:
        void onConnection(const muduo::net::TcpConnectionPtr &conn)
        {
            if (conn->connected())
            {
                LOG(LogLevel::DEBUG) << "连接建立";
                _conn = ConnectionFactory::Create(conn, _protocol);
                _downlatch.countDown(); // 计数--，为0时唤醒阻塞
            }
            else
            {
                LOG(LogLevel::DEBUG) << "连接断开";
                _conn.reset();
            }
        }
        void onMessage(const muduo::net::TcpConnectionPtr &conn, muduo::net::Buffer *buf, muduo::Timestamp)
        {
            LOG(LogLevel::DEBUG) << "有数据到来";
            auto base_buf = BufferFactory::Create(buf);
            while (1)
            {
                if (_protocol->IsProcessable(base_buf) == false)
                {
                    if (base_buf->ReadableSize() > maxDataSize)
                    {
                        LOG(LogLevel::ERROR) << "数据包太大";
                        conn->shutdown();
                        return;
                    }
                   
                    LOG(LogLevel::DEBUG) << "数据包不完整";
                    break;
                }
                BaseMessage::ptr msg;
                bool ret = _protocol->OnMessage(base_buf, msg);
                if (ret == false)
                {
                    LOG(LogLevel::ERROR) << "数据包解析失败";
                    conn->shutdown();
                    return;
                }
                if (_on_message)
                    _on_message(_conn, msg);
            }
        }

    private:
        std::mutex _conn_mutex;
        std::size_t maxDataSize = (1 << 16);
        BaseProtocol::ptr _protocol;

        BaseConnection::ptr _conn;
        muduo::CountDownLatch _downlatch;
        muduo::net::EventLoopThread _loopthread;
        muduo::net::EventLoop *_baseloop;
        muduo::net::TcpClient _client;
    };

    class ClientFactory
    {
    public:
        template <typename... Args>
        static BaseClient::ptr Create(Args &&...args)
        {
            return std::make_shared<MuduoClient>(std::forward<Args>(args)...);
        }
    };
}
