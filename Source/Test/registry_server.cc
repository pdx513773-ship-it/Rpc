#include "../Server/Rpc_Server.hpp"
#include "../Common/Detail.hpp"

using namespace Rpc;
using namespace Server;

int main()
{
    RegistryServer server(8081);
    server.Start();
    return 0;
}