// #include "Log.hpp"
// #include "Message.hpp"
// #include "Net.hpp"
// #include "Dispatcher.hpp"

// #include <thread>
// using namespace Rpc;
// using namespace LogModule;

// void OnRpcResponse(const BaseConnection::ptr &conn, const RpcResponse::ptr &msg)
// {
//     LOG(LogLevel::DEBUG) << "收到RPC响应";
//     std::string body = msg->Serialize();
//     std::cout << "消息序列化: " << body << std::endl;
// }
// void OnTopicResponse(const BaseConnection::ptr &conn, const TopicResponse::ptr &msg)
// {
//     LOG(LogLevel::DEBUG) << "收到Topic响应";
//     std::string body = msg->Serialize();
//     std::cout << "消息序列化: " << body << std::endl;
// }


// int main()
// {
    
              
//     auto dispatcher = std::make_shared<Dispatcher>();
//     dispatcher->RegisterHandler<RpcResponse>(MType::RSP_RPC, OnRpcResponse); // 注册响应处理函数
//     dispatcher->RegisterHandler<TopicResponse>(MType::RSP_TOPIC,OnTopicResponse);
//     auto client = ClientFactory::Create("111.230.252.14", 8082);
//     auto message_cb = std::bind(&Dispatcher::OnMessage, dispatcher.get(), std::placeholders::_1, std::placeholders::_2);
//     client->SetMessageCallback(message_cb);
//     client->Connect();
//     auto req = MessageFactory::CreateMessage<RpcRequest>();
//     req->SetId("111");
//     req->SetType(MType::REQ_RPC);
//     req->SetMethod("add");
//     Json::Value params;
//     params["num1"] = 1;
//     params["num2"] = 2;
//     req->SetParams(params);
//     client->Send(req);


//     auto topic_req = MessageFactory::CreateMessage<TopicRequest>();
//     topic_req->SetId("222222");
//     topic_req->SetType(MType::REQ_TOPIC);
//     topic_req->SetOptype(TopicOptype::TOPIC_CREATE);
//     topic_req->SetTopicKey("test");
   
//     client->Send(topic_req);

//     std::this_thread::sleep_for(std::chrono::seconds(10));
//     client->Shutdown();

//     return 0;
// }



// #include "../Server/Rpc_Router.hpp"
// #include "../Client/Rpc_Caller.hpp"
// #include "../Client/Requestor.hpp"

// #include "Message.hpp"
// #include "Net.hpp"
// #include "Dispatcher.hpp"

// #include <thread>
// using namespace Rpc;
// using namespace LogModule;
// using namespace Client;


// void callback(const Json::Value& result)
// {
//     LOG(LogLevel::DEBUG)<<"回调请求成功 "<<result.asInt();
// }


// int main()
// {
//     auto rpquestor = std::make_shared<Requestor>();
//     auto caller = std::make_shared<RpcCaller>(rpquestor);
//     auto rsp_cb = std::bind(&Requestor::OnResponse, rpquestor.get(), 
//     std::placeholders::_1, std::placeholders::_2);
//     auto dispatcher = std::make_shared<Dispatcher>();

//     dispatcher->RegisterHandler<RpcResponse>(MType::RSP_RPC, rsp_cb); // 注册响应处理函数
    


//     auto client = ClientFactory::Create("111.230.252.14", 8082);
//     auto message_cb = std::bind(&Dispatcher::OnMessage, dispatcher.get(), std::placeholders::_1, std::placeholders::_2);
//     client->SetMessageCallback(message_cb);



//     client->Connect();

//     auto conn = client->Connection();
//     Json::Value params,result;
//     params["num1"] = 11;
//     params["num2"] = 89;
//     bool ret = caller->Call(conn,"Add",params,result);
//     if(!ret)
//     {
//         LOG(LogLevel::ERROR)<<"请求失败"<<result.asInt();
//         return 0;
//     }
//     LOG(LogLevel::DEBUG)<<"请求成功 "<< result.asInt();
    



//     RpcCaller::JsonAsyncResponse result_promise;
//     params["num1"] = 66;
//     params["num2"] = 99;
//     ret = caller->Call(conn,"Add",params,result_promise);
//     if(ret)
//     {
//         result = result_promise.get();
//         std::cout<<"异步请求成功 "<<result.asInt()<<std::endl;
//     }


//     params["num1"] = 55;
//     params["num2"] = 11;
//     ret = caller->Call(conn,"Add",params,callback);
//     std::this_thread::sleep_for(std::chrono::seconds(11));

//     client->Shutdown();

//     return 0;
// }



#include "../Common/Detail.hpp"
#include "../Client/Rpc_Client.hpp"

#include <thread>
using namespace Rpc;

using namespace Client;


void callback(const Json::Value& result)
{
    LOG(LogLevel::DEBUG)<<"回调请求成功 "<<result.asInt();
}


int main()
{
    RpcClient client(false,"111.230.252.14", 8080);
    
    Json::Value params,result;
    params["num1"] = 11;
    params["num2"] = 89;
    bool ret = client.Call("Add",params,result);
    if(!ret)
    {
        LOG(LogLevel::ERROR)<<"请求失败"<<result.asInt();
        return 0;
    }
    LOG(LogLevel::DEBUG)<<"请求成功 "<< result.asInt();
    



    RpcCaller::JsonAsyncResponse result_promise;
    params["num1"] = 66;
    params["num2"] = 99;
    ret = client.Call("Add",params,result_promise);
    if(ret)
    {
        result = result_promise.get();
        std::cout<<"异步请求成功 "<<result.asInt()<<std::endl;
    }


    params["num1"] = 55;
    params["num2"] = 11;
    ret = client.Call("Add",params,callback);
    std::this_thread::sleep_for(std::chrono::seconds(11));


    return 0;
}