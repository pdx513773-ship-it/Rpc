// #include <iostream>
// #include "Message.hpp"

// using namespace Rpc;

// int main()
// {
//     std::cout << "=== RPC消息系统测试 ===" << std::endl << std::endl;

//     // 1. 测试RpcRequest
//     std::cout << "1. 测试RpcRequest" << std::endl;
//     RpcRequest::ptr rpcReq = MessageFactory::CreateMessage<RpcRequest>();
//     Json::Value params;
//     params["num1"] = 1;
//     params["num2"] = 2;
//     rpcReq->SetParams(params);
//     rpcReq->SetMethod("Add");

//     std::string rpcReqData = rpcReq->Serialize();
//     std::cout << "序列化: " << rpcReqData << std::endl;

//     RpcRequest::ptr rpcReq2 = MessageFactory::CreateMessage<RpcRequest>();
//     rpcReq2->Deserialize(rpcReqData);
//     std::cout << "方法名: " << rpcReq2->GetMethod() << std::endl;
//     std::cout << "参数: " << rpcReq2->GetParams().toStyledString() << std::endl;
//     std::cout << "检查结果: " << (rpcReq2->Check() ? "通过" : "失败") << std::endl << std::endl;

//     // 2. 测试RpcResponse
//     std::cout << "2. 测试RpcResponse" << std::endl;
//     RpcResponse::ptr rpcResp = MessageFactory::CreateMessage<RpcResponse>();
//     rpcResp->SetRcode(RCode::RCODE_OK);

//     Json::Value result;
//     result["sum"] = 3;
//     rpcResp->SetResult(result);

//     std::string rpcRespData = rpcResp->Serialize();
//     std::cout << "序列化: " << rpcRespData << std::endl;

//     RpcResponse::ptr rpcResp2 = MessageFactory::CreateMessage<RpcResponse>();
//     rpcResp2->Deserialize(rpcRespData);
//     std::cout << "状态码: " << static_cast<int>(rpcResp2->GetRcode()) << std::endl;
//     std::cout << "结果: " << rpcResp2->GetResult().toStyledString() << std::endl;
//     std::cout << "检查结果: " << (rpcResp2->Check() ? "通过" : "失败") << std::endl << std::endl;

//     // 3. 测试TopicRequest
//     std::cout << "3. 测试TopicRequest" << std::endl;
//     TopicRequest::ptr topicReq = MessageFactory::CreateMessage<TopicRequest>();
//     topicReq->SetTopicKey("temperature");
//     topicReq->SetOptype(TopicOptype::TOPIC_PUBLISH);

//     Json::Value topicMsg;
//     topicMsg["value"] = 25.5;
//     topicMsg["unit"] = "celsius";
//     topicReq->SetTopicMsg(topicMsg);

//     std::string topicReqData = topicReq->Serialize();
//     std::cout << "序列化: " << topicReqData << std::endl;

//     TopicRequest::ptr topicReq2 = MessageFactory::CreateMessage<TopicRequest>();
//     topicReq2->Deserialize(topicReqData);
//     std::cout << "主题键: " << topicReq2->GetTopicKey() << std::endl;
//     std::cout << "操作类型: " << static_cast<int>(topicReq2->GetOptype()) << std::endl;
//     std::cout << "消息: " << topicReq2->GetTopicMsg().toStyledString() << std::endl;
//     std::cout << "检查结果: " << (topicReq2->Check() ? "通过" : "失败") << std::endl << std::endl;

//     // 4. 测试TopicResponse
//     std::cout << "4. 测试TopicResponse" << std::endl;
//     TopicResponse::ptr topicResp = MessageFactory::CreateMessage<TopicResponse>();
//     topicResp->SetRcode(RCode::RCODE_OK);

//     std::string topicRespData = topicResp->Serialize();
//     std::cout << "序列化: " << topicRespData << std::endl;

//     TopicResponse::ptr topicResp2 = MessageFactory::CreateMessage<TopicResponse>();
//     topicResp2->Deserialize(topicRespData);
//     std::cout << "状态码: " << static_cast<int>(topicResp2->GetRcode()) << std::endl;
//     std::cout << "检查结果: " << (topicResp2->Check() ? "通过" : "失败") << std::endl << std::endl;

//     // 5. 测试ServiceRequest
//     std::cout << "5. 测试ServiceRequest" << std::endl;
//     ServiceRequest::ptr serviceReq = MessageFactory::CreateMessage<ServiceRequest>();
//     serviceReq->SetMethod("discovery");
//     serviceReq->SetOptype(TopicOptype::TOPIC_PUBLISH); // 注意类型可能不匹配

//     Address addr = {"192.168.1.100", 8080};
//     serviceReq->SetHost(addr);

//     Json::Value serviceParams;
//     serviceParams["service_type"] = "temperature";
//     serviceReq->SetParams(serviceParams);

//     std::string serviceReqData = serviceReq->Serialize();
//     std::cout << "序列化: " << serviceReqData << std::endl;

//     ServiceRequest::ptr serviceReq2 = MessageFactory::CreateMessage<ServiceRequest>();
//     serviceReq2->Deserialize(serviceReqData);
//     std::cout << "方法名: " << serviceReq2->GetMethod() << std::endl;
//     Address retrievedAddr = serviceReq2->GetHost();
//     std::cout << "主机地址: " << retrievedAddr.first << ":" << retrievedAddr.second << std::endl;
//     std::cout << "参数: " << serviceReq2->GetParams().toStyledString() << std::endl;
//     // 注意：ServiceRequest的Check方法可能有问题
//     std::cout << "检查结果: " << (serviceReq2->Check() ? "通过" : "失败") << std::endl << std::endl;

//     // 6. 测试ServiceResponse
//     std::cout << "6. 测试ServiceResponse" << std::endl;
//     ServiceResponse::ptr serviceResp = MessageFactory::CreateMessage<ServiceResponse>();
//     serviceResp->SetRcode(RCode::RCODE_OK);
//     serviceResp->SetOptype(ServiceOptype::SERVICE_DISCOVERY);
//     serviceResp->SetMethod("temperature_service");

//     std::vector<Address> addrs = {
//         {"192.168.1.101", 8080},
//         {"192.168.1.102", 8081},
//         {"192.168.1.103", 8082}
//     };
//     serviceResp->SetHost(addrs);

//     std::string serviceRespData = serviceResp->Serialize();
//     std::cout << "序列化: " << serviceRespData << std::endl;

//     ServiceResponse::ptr serviceResp2 = MessageFactory::CreateMessage<ServiceResponse>();
//     serviceResp2->Deserialize(serviceRespData);
//     std::cout << "状态码: " << static_cast<int>(serviceResp2->GetRcode()) << std::endl;
//     std::cout << "操作类型: " << static_cast<int>(serviceResp2->GetOptype()) << std::endl;
//     std::cout << "方法名: " << serviceResp2->GetMethod() << std::endl;

//     std::vector<Address> retrievedAddrs = serviceResp2->GetHosts();
//     std::cout << "主机列表:" << std::endl;
//     for (const auto& addr : retrievedAddrs) {
//         std::cout << "  " << addr.first << ":" << addr.second << std::endl;
//     }
//     std::cout << "检查结果: " << (serviceResp2->Check() ? "通过" : "失败") << std::endl << std::endl;

//     // 7. 测试MessageFactory通过MType创建
//     std::cout << "7. 测试MessageFactory通过MType创建" << std::endl;
//     BaseMessage::ptr messages[6];
//     MType types[6] = {
//         MType::REQ_RPC,
//         MType::RSP_RPC,
//         MType::REQ_TOPIC,
//         MType::RSP_TOPIC,
//         MType::REQ_SERVICE,
//         MType::RSP_SERVICE
//     };

//     const char* typeNames[6] = {
//         "REQ_RPC",
//         "RSP_RPC",
//         "REQ_TOPIC",
//         "RSP_TOPIC",
//         "REQ_SERVICE",
//         "RSP_SERVICE"
//     };

//     for (int i = 0; i < 6; i++) {
//         messages[i] = MessageFactory::CreateMessage(types[i]);
//         std::cout << "创建 " << typeNames[i] << ": "
//                   << (messages[i] ? "成功" : "失败") << std::endl;
//     }

//     std::cout << std::endl << "=== 测试完成 ===" << std::endl;

//     return 0;
// }

// #include "Message.hpp"
// #include "Net.hpp"
// #include "Dispatcher.hpp"

// using namespace Rpc;

// void OnRpcRequest(const BaseConnection::ptr &conn,const RpcRequest::ptr &msg)
// {
//     std::cout << "=== RPC消息测试 ===" << std::endl;
//     std::string body = msg->Serialize();
//     std::cout << "消息序列化: " << body << std::endl;
//     auto res = MessageFactory::CreateMessage<RpcResponse>();
//     res->SetId("111");

//     res->SetType(MType::RSP_RPC);
//     res->SetResult(33);
//     res->SetRcode(RCode::RCODE_OK);

//     conn->Send(res);
// }
// void OnTopicRequest(const BaseConnection::ptr &conn,const TopicRequest::ptr &msg)
// {
//     std::cout << "=== Topic消息测试 ===" << std::endl;
//     std::string body = msg->Serialize();
//     std::cout << "消息序列化: " << body << std::endl;
//     auto res = MessageFactory::CreateMessage<TopicResponse>();
//     res->SetId("111");

//     res->SetType(MType::RSP_TOPIC);

//     res->SetRcode(RCode::RCODE_OK);

//     conn->Send(res);
// }

// int main()
// {

//     auto dispatcher = std::make_shared<Dispatcher>();
//     dispatcher->RegisterHandler<RpcRequest>(MType::REQ_RPC, OnRpcRequest); // 注册响应处理函数
//     dispatcher->RegisterHandler<TopicRequest>(MType::REQ_TOPIC,OnTopicRequest); // 注册响应处理函数

//     auto server = ServerFactory::Create(8082);
//     auto message_cb = std::bind(&Dispatcher::OnMessage, dispatcher.get(), std::placeholders::_1, std::placeholders::_2);
//     server->SetMessageCallback(message_cb);

//     server->Start();

//     return 0;
// }

// #include "../Server/Rpc_Router.hpp"
// #include "../Client/Rpc_Caller.hpp"
// #include "../Client/Requestor.hpp"
// #include "Message.hpp"
// #include "Net.hpp"
// #include "Dispatcher.hpp"

// using namespace Rpc;
// using namespace Server;



// void Add(const Json::Value &req, Json::Value &rsp)
// {
//     int num1 = req["num1"].asInt();
//     int num2 = req["num2"].asInt();
//     rsp = num1 + num2;
// }

// int main()
// {

//     auto dispatcher = std::make_shared<Dispatcher>();
//     auto router = std::make_shared<RpcRouter>();
//     std::unique_ptr<SDescribeFactory> desc_factory(new SDescribeFactory);
//     desc_factory->SetMethod("Add");
//     desc_factory->SetParamsDesc("num1", VType::INTEGRAL);
//     desc_factory->SetParamsDesc("num2", VType::INTEGRAL);
//     desc_factory->SetVType(VType::INTEGRAL);
//     desc_factory->SetCallback(Add);
//     router->RegisterMethod(desc_factory->Build());

//     auto cb = std::bind(&RpcRouter::OnRpcRequest, router.get(),
//                         std::placeholders::_1, std::placeholders::_2);
//     dispatcher->RegisterHandler<RpcRequest>(MType::REQ_RPC, cb); // 注册响应处理函数

//     auto server = ServerFactory::Create(8082);
//     auto message_cb = std::bind(&Dispatcher::OnMessage, dispatcher.get(), std::placeholders::_1, std::placeholders::_2);
//     server->SetMessageCallback(message_cb);

//     server->Start();

//     return 0;
// }


// #include "../Common/Detail.hpp"
// #include "../Server/Rpc_Server.hpp"


// using namespace Rpc;
// using namespace Server;



// void Add(const Json::Value &req, Json::Value &rsp)
// {
//     int num1 = req["num1"].asInt();
//     int num2 = req["num2"].asInt();
//     rsp = num1 + num2;
// }

// int main()
// {

//     std::unique_ptr<SDescribeFactory> desc_factory(new SDescribeFactory);
//     desc_factory->SetMethod("Add");
//     desc_factory->SetParamsDesc("num1", VType::INTEGRAL);
//     desc_factory->SetParamsDesc("num2", VType::INTEGRAL);
//     desc_factory->SetVType(VType::INTEGRAL);
//     desc_factory->SetCallback(Add);
    

//     RpcServer server(Address("127.0.0.1",8080));
//     server.RegisterMethod(desc_factory->Build());
//     server.Start();
//     return 0;
// }





// using namespace Rpc;
// using namespace Client;



// void Add(const Json::Value &req, Json::Value &rsp)
// {
//     int num1 = req["num1"].asInt();
//     int num2 = req["num2"].asInt();
//     rsp = num1 + num2;
// }

// int main()
// {

//     std::unique_ptr<SDescribeFactory> desc_factory(new SDescribeFactory);
//     desc_factory->SetMethod("Add");
//     desc_factory->SetParamsDesc("num1", VType::INTEGRAL);
//     desc_factory->SetParamsDesc("num2", VType::INTEGRAL);
//     desc_factory->SetVType(VType::INTEGRAL);
//     desc_factory->SetCallback(Add);
    

//     RpcServer server(Address("127.0.0.1",8080),true,Address("127.0.0.1",8081));
//     server.RegisterMethod(desc_factory->Build());
//     server.Start();
//     return 0;
// }
#include "../../Client/Rpc_Client.hpp"
#include <thread>

using namespace Rpc;

using namespace Client;
int main()
{
    auto client = std::make_shared<TopicClient>("127.0.0.1",8080);


    bool ret = client->Creat("test_topic");

    if(!ret)
    {
        std::cout << "创建主题失败" << std::endl;
        return 0;
    }
    for(int i = 0;i<10;i++)
    {
        client->Publish("test_topic",std::to_string(i));
    }


    std::this_thread::sleep_for(std::chrono::seconds(11));

    client->Shutdown();

}