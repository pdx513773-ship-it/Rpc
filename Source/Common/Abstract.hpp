#pragma once
#include <memory>
#include <functional>
#include "Fields.hpp"

namespace Rpc
{
    class BaseMessage
    {
    public:
        using ptr = std::shared_ptr<BaseMessage>;   
        virtual ~BaseMessage() {}
        
        virtual void SetId(const std::string& id) { _rid = id; }
        virtual std::string GetId(){return _rid;}

        virtual void SetType(MType type) {_mytype = type; }
        virtual MType GetType() { return _mytype; }

        virtual std::string Serialize() = 0;
        virtual bool Deserialize(const std::string& data) = 0;
        virtual bool Check() = 0;

    private:
        MType _mytype;
        std::string _rid;
    };

    class BaseBuffer
    {
    public:
        using ptr = std::shared_ptr<BaseBuffer>;
        
        virtual size_t ReadableSize() = 0;
        virtual int32_t PeekInt32() = 0; //查看4字节数据
        virtual void  RetrieveInt32() = 0; //删除已经取出的4字节数据
        virtual int32_t ReadInt32() = 0; //读取4字节数据并且指向后面4字节
        virtual std::string RetrieveAsString(size_t len) = 0; //删除已经取出的len字节数据并返回字符串
    };

    class BaseProtocol
    {
    public:
        using ptr = std::shared_ptr<BaseProtocol>;

        virtual bool IsProcessable(const BaseBuffer::ptr& buffer) = 0;
        virtual bool OnMessage(const BaseBuffer::ptr& buffer, BaseMessage::ptr &msg) = 0;
        virtual std::string Serialize(const BaseMessage::ptr& msg) = 0;     
    };

    class BaseConnection
    {
    public:
        using ptr = std::shared_ptr<BaseConnection>;
       
        virtual void Send(const BaseMessage::ptr& msg) = 0;
        virtual bool Connected() = 0;
        virtual void Shutdown() = 0;

    private:
        BaseProtocol::ptr _protocol;
    };

    using ConnectionCallback = std::function<void(const BaseConnection::ptr&)>;
    using CloseCallback = std::function<void(const BaseConnection::ptr&)>;
    using MessageCallback = std::function<void(const BaseConnection::ptr&, const BaseMessage::ptr&)>;
    
    class BaseServer
    {
    public:
        using ptr = std::shared_ptr<BaseServer>;
        virtual ~BaseServer() {}
        virtual void Start() = 0;
        //virtual bool Stop() = 0;
        //virtual bool AddConnection(const BaseConnection::ptr& conn) = 0;
        //virtual bool RemoveConnection(const BaseConnection::ptr& conn) = 0;
        virtual void SetConnectionCallback(const ConnectionCallback& cb) { _on_connection = cb; }
        virtual void SetCloseCallback(const CloseCallback& cb) { _on_close = cb; }
        virtual void SetMessageCallback(const MessageCallback& cb) { _on_message = cb; }
    protected:
        ConnectionCallback _on_connection;
        CloseCallback _on_close;
        MessageCallback _on_message;
    };

    class BaseClient {
        public:
            using ptr = std::shared_ptr<BaseClient>;
            virtual void SetConnectionCallback(const ConnectionCallback& cb) {_on_connection = cb; }
            virtual void SetCloseCallback(const CloseCallback& cb) {_on_close = cb;}
            virtual void SetMessageCallback(const MessageCallback& cb) {_on_message = cb;}

            virtual void Connect() = 0;
            virtual void Shutdown() = 0;
            virtual bool Send(const BaseMessage::ptr&) = 0;
            virtual BaseConnection::ptr Connection() = 0;
            virtual bool Connected() = 0;
        protected:
            ConnectionCallback _on_connection;
            CloseCallback _on_close;
            MessageCallback _on_message;
    };
}
