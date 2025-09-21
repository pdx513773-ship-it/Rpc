#pragma once

#include "Net.hpp"
#include "Message.hpp"

namespace Rpc
{
    class Callback
    {
    public:
        using ptr = std::shared_ptr<Callback>;
        virtual void onMessage(const BaseConnection::ptr& conn, const BaseMessage::ptr& msg) = 0;
    };
    
    template<typename T>
    class CallBackT : public Callback
    {
    public:
        using ptr = std::shared_ptr<CallBackT<T>>;
        using MessageCallback = std::function<void (const BaseConnection::ptr& conn, std::shared_ptr<T>& msg)>;
        CallBackT(const MessageCallback &handler) : _handler(handler) {}
        void onMessage(const BaseConnection::ptr& conn,const BaseMessage::ptr& msg) override
        {
            //调用完父类指针后，转换为子类指针
            auto tmsg = std::dynamic_pointer_cast<T>(msg);
            _handler(conn, tmsg);
        }
        
    private:
        MessageCallback _handler;
    };


    //为了更好的注册方法，void OnRpcResponse(const BaseConnection::ptr &conn, const BaseMessage::ptr &msg)例如这个
    //想把里面的BaseMessage直接写成RpcMeseeage，因为父类无法直接拿子类的方法 还需要用到dynamical_cast，所以这里用CallBackT来包装一下
    //CallBackT<RpcMessage>
    //然后在注册的时候，用CallBackT<RpcMessage>::ptr来注册
    //这样就能更好的注册方法，并且可以直接拿到子类的方法了

    //我们为什么用父类继承体系来封装这个CallBackT因为在一个数据结构里面只能有一种类型，所以用父类继承体系来封装
    //但是还有一个问题，就是在使用模版后dispatcher->RegisterHandler<RpcResponse>(MType::RSP_RPC, OnRpcResponse);得这样子写才比较合理
    //但是呢在只做了上述的事情后，这样子写无法编译，为什么？因为我们的<RpcResponse>只是消息的类型
    //OnRpcResponse是一个函数，按道理来讲我们需要传递这个函数的类型，但是没办法，因为我们不知道这个函数的类型，所以我们只能用父类来封装
    class Dispatcher
    {
    public:
        using ptr = std::shared_ptr<Dispatcher>;
        //首先需要向外部提供接口，能注册自己类型的方法
        template<typename T>
        void RegisterHandler(MType mtype, const typename CallBackT<T>::MessageCallback  &handler)
        {
            std::unique_lock<std::mutex> lock(_mutex);
            auto cb = std::make_shared<CallBackT<T>>(handler);
            _handlers.insert(std::make_pair(mtype, cb));      
        }

        void OnMessage(const BaseConnection::ptr& conn, const BaseMessage::ptr& msg)
        {
            std::unique_lock<std::mutex> lock(_mutex);
            //找到该消息类型的回调函数，传参
            auto it = _handlers.find(msg->GetType());
            if (it!= _handlers.end())
            {
                it->second->onMessage(conn, msg);
            }
            else
            {
                auto type = int(msg->GetType());
                printf("未注册的消息类型：%d\n", type);
                LOG(LogLevel::DEBUG)<<"收到未知类型消息";
                conn->Shutdown();
            }
        }
 
    private:
        std::mutex _mutex;
        std::unordered_map<MType,Callback::ptr> _handlers;
    };
}