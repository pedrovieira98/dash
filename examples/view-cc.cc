#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include <iostream>

using namespace ns3;

int main(int argc, char *argv[])
{
  // Create a dummy TcpL4Protocol object to read the default attribute
  Ptr<TcpL4Protocol> tcp = CreateObject<TcpL4Protocol>();

  // Read the SocketType attribute (which is a TypeIdValue)
  TypeIdValue typeIdValue;
  tcp->GetAttribute("SocketType", typeIdValue);

  std::cout << "Default TCP Congestion Control: " << typeIdValue.Get().GetName() << std::endl;

  return 0;
}
