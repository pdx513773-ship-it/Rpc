#include "../../Server/Rpc_Server.hpp"
#include "../../Common/Detail.hpp"

using namespace Rpc;
using namespace Server;

int main()
{
    TopicServer server(8080);
    server.Start();
    return 0;
}