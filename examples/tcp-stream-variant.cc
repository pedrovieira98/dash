#include "ns3/point-to-point-helper.h"
#include <fstream>
#include "ns3/core-module.h"
#include "ns3/applications-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include <ns3/buildings-module.h>
#include "ns3/building-position-allocator.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include "ns3/flow-monitor-module.h"
#include "ns3/tcp-stream-helper.h"
#include "ns3/tcp-stream-interface.h"
#include "ns3/csma-module.h"

template <typename T>
std::string ToString(T val)
{
    std::stringstream stream;
    stream << val;
    return stream.str();
}

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("TcpStreamExample");

int
main (int argc, char *argv[])
{
  uint64_t segmentDuration;
  uint32_t simulationId;
  uint32_t numberOfClients;
  std::string adaptationAlgo;
  std::string segmentSizeFilePath;
  std::string tcpVariant;
  std::string linkRate = "3Mbps";
  std::string delay = "5ms";

  CommandLine cmd;
  cmd.Usage ("Simulation of streaming with DASH.\n");
  cmd.AddValue ("simulationId", "The simulation's index (for logging purposes)", simulationId);
  cmd.AddValue ("numberOfClients", "The number of clients", numberOfClients);
  cmd.AddValue ("segmentDuration", "The duration of a video segment in microseconds", segmentDuration);
  cmd.AddValue ("adaptationAlgo", "The adaptation algorithm that the client uses for the simulation", adaptationAlgo);
  cmd.AddValue ("segmentSizeFile", "The relative path (from ns-3.x directory) to the file containing the segment sizes in bytes", segmentSizeFilePath);
  cmd.AddValue ("tcpVariant", "TCP Congestion Control Algorithm", tcpVariant);
  cmd.Parse (argc, argv);

   tcpVariant = std::string ("ns3::") + tcpVariant;
   // Select TCP variant
   if (tcpVariant.compare ("ns3::TcpWestwoodPlus") == 0)
     {
       // TcpWestwoodPlus is not an actual TypeId name; we need TcpWestwood here
       Config::SetDefault ("ns3::TcpL4Protocol::SocketType", TypeIdValue (TcpWestwood::GetTypeId ()));
       // the default protocol type in ns3::TcpWestwood is WESTWOOD
       Config::SetDefault ("ns3::TcpWestwood::ProtocolType", EnumValue (TcpWestwood::WESTWOODPLUS));
     }
   else
     {
       TypeId tcpTid;
       NS_ABORT_MSG_UNLESS (TypeId::LookupByNameFailSafe (tcpVariant, &tcpTid), "TypeId " << tcpVariant << " not found");
       Config::SetDefault ("ns3::TcpL4Protocol::SocketType", TypeIdValue (TypeId::LookupByName (tcpVariant)));
     }

  Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue (1446));
  Config::SetDefault("ns3::TcpSocket::SndBufSize", UintegerValue (5524288));
  Config::SetDefault("ns3::TcpSocket::RcvBufSize", UintegerValue (5524288));

   // create folder so we can log the positions of the clients
  const char * mylogsDir = dashLogDirectory.c_str();
  mkdir (mylogsDir, 0775);
  std::string algodirstr (dashLogDirectory +  adaptationAlgo );  
  const char * algodir = algodirstr.c_str();
  mkdir (algodir, 0775);
  std::string dirstr (dashLogDirectory + adaptationAlgo + "/" + ToString (numberOfClients) + "/");
  const char * dir = dirstr.c_str();
  mkdir(dir, 0775);

    
  NS_LOG_INFO("Create nodes.");
  
  // Create nodes for routers and clients
  NodeContainer serverRouter, router1, router2, csmaNodes;
  serverRouter.Create(1);  // Server router
  router1.Create(1);       // First router (between server and second router)
  router2.Create(1);       // Second router (between first router and client LAN)
  
  csmaNodes.Add(router2.Get(0)); // Add router2 to client LAN
  csmaNodes.Create(numberOfClients); // Clients

  // Point-to-point connection between server and router1
  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue (linkRate));
  pointToPoint.SetChannelAttribute ("Delay", StringValue (delay));
  NetDeviceContainer serverRouterDevices, router1Devices;
  serverRouterDevices = pointToPoint.Install(serverRouter.Get(0), router1.Get(0));

  // Point-to-point connection between router1 and router2
  NetDeviceContainer router1Router2Devices;
  router1Router2Devices = pointToPoint.Install(router1.Get(0), router2.Get(0));

  // CSMA network for clients
  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", StringValue ("100Mbps"));
  csma.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (6560)));
  NetDeviceContainer csmaDevices;
  csmaDevices = csma.Install (csmaNodes);

  // Install internet stack on all routers and clients
  InternetStackHelper stack;
  stack.Install (serverRouter.Get(0));
  stack.Install (router1.Get(0));
  stack.Install (router2.Get(0));
  stack.Install (csmaNodes);

  // Assign IP addresses
  Ipv4AddressHelper address;
  
  // Server to Router1
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer serverRouterInterfaces = address.Assign(serverRouterDevices);
  
  // Router1 to Router2
  address.SetBase ("10.2.1.0", "255.255.255.0");
  Ipv4InterfaceContainer router1Router2Interfaces = address.Assign(router1Router2Devices);

  // Router2 to Client LAN
  address.SetBase ("10.3.1.0", "255.255.255.0");
  Ipv4InterfaceContainer csmaInterfaces = address.Assign(csmaDevices);

  // Set up the server
  TcpStreamServerHelper serverHelper(80);
  serverHelper.SetAttribute ("SegmentDuration", UintegerValue (segmentDuration));
  serverHelper.SetAttribute ("SegmentSizeFilePath", StringValue (segmentSizeFilePath));

  ApplicationContainer serverApp = serverHelper.Install (serverRouter.Get (0));
  serverApp.Start (Seconds (0));

  // Set up the client applications
  std::vector<std::pair<Ptr<Node>, std::string>> clients;
  for (NodeContainer::Iterator i = csmaNodes.Begin (); i != csmaNodes.End (); ++i)
  {
      std::pair <Ptr<Node>, std::string> client (*i, adaptationAlgo);
      clients.push_back (client);
  }

  TcpStreamClientHelper clientHelper(router2.Get(0)->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal(), 80);  // Update client IP address to Router2's address
  clientHelper.SetAttribute ("SegmentDuration", UintegerValue (segmentDuration));
  clientHelper.SetAttribute ("SegmentSizeFilePath", StringValue (segmentSizeFilePath));
  clientHelper.SetAttribute ("NumberOfClients", UintegerValue (numberOfClients));
  clientHelper.SetAttribute ("SimulationId", UintegerValue (simulationId));
  ApplicationContainer clientApps = clientHelper.Install (clients);


  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  NS_LOG_INFO ("Run Simulation.");
  Simulator::Stop (Seconds(4000));
  Simulator::Run ();
  Simulator::Destroy ();
  NS_LOG_INFO ("Done.");
}
