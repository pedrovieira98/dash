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
#include "ns3/netanim-module.h"
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
//
// Users may find it convenient to turn on explicit debugging
// for selected modules; the below lines suggest how to do this
//
// #if 1
//   LogComponentEnable ("TcpStreamExample", LOG_LEVEL_INFO);
//   LogComponentEnable ("TcpStreamClientApplication", LOG_LEVEL_INFO);
//   LogComponentEnable ("TcpStreamServerApplication", LOG_LEVEL_INFO);
// #endif
uint64_t segmentDuration;
// The simulation id is used to distinguish log file results from potentially multiple consequent simulation runs.
uint32_t simulationId;
uint32_t numberOfClients;
std::string adaptationAlgo;
std::string tcpVariant;
std::string segmentSizeFilePath;
// bool shortGuardInterval = true;
CommandLine cmd;
cmd.Usage ("Simulation of streaming with DASH.\n");
cmd.AddValue ("simulationId", "The simulation's index (for logging purposes)", simulationId);
cmd.AddValue ("numberOfClients", "The number of clients", numberOfClients);
cmd.AddValue ("segmentDuration", "The duration of a video segment in microseconds", segmentDuration);
cmd.AddValue ("adaptationAlgo", "The adaptation algorithm that the client uses for the simulation", adaptationAlgo);
cmd.AddValue ("segmentSizeFile", "The relative path (from ns-3.x directory) to the file containing the segment sizes in bytes", segmentSizeFilePath);
cmd.AddValue("tcpVariant",
                "Transport protocol to use: TcpNewReno, "
                "TcpHybla, TcpHighSpeed, TcpHtcp, TcpVegas, TcpScalable, TcpVeno, "
                "TcpBic, TcpYeah, TcpIllinois, TcpWestwood, TcpWestwoodPlus, TcpLedbat, TcpBBR ",
                tcpVariant);
cmd.Parse (argc, argv);
tcpVariant = std::string("ns3::") + tcpVariant;
 
// Select TCP variant
TypeId tcpTid;
NS_ABORT_MSG_UNLESS(TypeId::LookupByNameFailSafe(tcpVariant, &tcpTid),
                       "TypeId " << tcpVariant << " not found");
Config::SetDefault("ns3::TcpL4Protocol::SocketType",
                    TypeIdValue(TypeId::LookupByName(tcpVariant)));
Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue (1446));
Config::SetDefault("ns3::TcpSocket::SndBufSize", UintegerValue (524288));
Config::SetDefault("ns3::TcpSocket::RcvBufSize", UintegerValue (524288));
 
/* Create Nodes */
 
NodeContainer serverNodes;
serverNodes.Create (1);
 
NodeContainer routerNodes;
routerNodes.Create(2);
 
NodeContainer clientNodes;
clientNodes.Create (numberOfClients);
 
// Create the point-to-point link helpers
PointToPointHelper p2p;
p2p.SetDeviceAttribute  ("DataRate", StringValue ("3Mbps"));
p2p.SetChannelAttribute ("Delay", StringValue ("0ms"));
 
NetDeviceContainer serverRouter = p2p.Install(serverNodes.Get(0), routerNodes.Get(0));
NetDeviceContainer routerRouter = p2p.Install(routerNodes.Get(0), routerNodes.Get(1));
 
CsmaHelper csma;
csma.SetChannelAttribute("DataRate", StringValue("10Mbps"));
csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));
 
NetDeviceContainer csmaClients;
csmaClients = csma.Install(clientNodes);
NetDeviceContainer routerClients = p2p.Install(routerNodes.Get(1), clientNodes.Get(0));
 
/* Internet stack */
InternetStackHelper stack;
stack.Install (serverNodes);
stack.Install (routerNodes);
stack.Install (clientNodes);
 
/* Assign IP addresses */
Ipv4AddressHelper address;
 
address.SetBase("10.1.1.0", "255.255.255.0");
Ipv4InterfaceContainer serverIp = address.Assign(serverRouter);
 
address.SetBase("10.1.2.0", "255.255.255.0");
Ipv4InterfaceContainer routerIp = address.Assign(routerRouter);
 
address.SetBase("10.1.3.0", "255.255.255.0");
Ipv4InterfaceContainer lanIp = address.Assign(csmaClients);
Ipv4InterfaceContainer clientIp = address.Assign(routerClients);
 
  std::vector <std::pair <Ptr<Node>, std::string> > clients;
  for (NodeContainer::Iterator i = clientNodes.Begin () + 1; i != clientNodes.End (); ++i)
    {
      std::pair <Ptr<Node>, std::string> client (*i, adaptationAlgo);
      clients.push_back (client);
    }
 
// Create Applications
 
uint16_t port = 8080;
UdpEchoServerHelper echoServer(port);
ApplicationContainer serverApps = echoServer.Install(serverNodes.Get(0));
serverApps.Start(Seconds(1.0));
serverApps.Stop(Seconds(31.0));
UdpEchoClientHelper echoClient(serverIp.GetAddress(0), port);
echoClient.SetAttribute("MaxPackets", UintegerValue(1));
echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
echoClient.SetAttribute("PacketSize", UintegerValue(1024));
 
// First client starts at 1s
ApplicationContainer clientApp1 = echoClient.Install(clientNodes.Get(0));
clientApp1.Start(Seconds(1.0));
clientApp1.Stop(Seconds(31.0));
 
// Second client starts at 11s
ApplicationContainer clientApp2 = echoClient.Install(clientNodes.Get(1));
clientApp2.Start(Seconds(11.0));
clientApp2.Stop(Seconds(31.0));
 
// Third client starts at 22s
ApplicationContainer clientApp3 = echoClient.Install(clientNodes.Get(2));
clientApp3.Start(Seconds(21.0));
clientApp3.Stop(Seconds(31.0));
 
// Enable Routing
Ipv4GlobalRoutingHelper::PopulateRoutingTables();
 
// Enable NetAnim
AnimationInterface anim("tcp-stream.xml");
anim.SetConstantPosition(serverNodes.Get(0), 0, 0);
anim.SetConstantPosition(routerNodes.Get(0), 50, 0);
anim.SetConstantPosition(routerNodes.Get(1), 100, 0);
anim.SetConstantPosition(clientNodes.Get(0), 150, 0);
anim.SetConstantPosition(clientNodes.Get(1), 150, 10);
anim.SetConstantPosition(clientNodes.Get(2), 150, 20);
 
// create folders for logs
const char * mylogsDir = dashLogDirectory.c_str();
mkdir (mylogsDir, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
std::string temp = dashLogDirectory + "/SimID_" + ToString (simulationId); 
const char * dir = temp.c_str();
mkdir(dir, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
 
std::ofstream clientPosLog;
std::string clientPos = dashLogDirectory + "/" + adaptationAlgo + "/" + ToString (numberOfClients) + "/" + "sim" + ToString (simulationId) + "_"  + "clientPos.txt";
clientPosLog.open (clientPos.c_str());
NS_ASSERT_MSG (clientPosLog.is_open(), "Couldn't open clientPosLog file");
 
  /* Install TCP Receiver on the access point */
  TcpStreamServerHelper serverHelper (port);
  ApplicationContainer serverApp = serverHelper.Install (serverNode);
  serverApp.Start (Seconds (1.0));
 
  /* Install TCP/UDP Transmitter on the station */
  TcpStreamClientHelper clientHelper (serverAddress, port);
  clientHelper.SetAttribute ("SegmentDuration", UintegerValue (segmentDuration));
  clientHelper.SetAttribute ("SegmentSizeFilePath", StringValue (segmentSizeFilePath));
  clientHelper.SetAttribute ("NumberOfClients", UintegerValue(numberOfClients));
  clientHelper.SetAttribute ("SimulationId", UintegerValue (simulationId));
  ApplicationContainer clientApps = clientHelper.Install (clients);
  for (uint i = 0; i < clientApps.GetN (); i++)
    {
      double startTime = 2.0 + ((i * 3) / 100.0);
      clientApps.Get (i)->SetStartTime (Seconds (startTime));
    }
 
 
NS_LOG_INFO ("Run Simulation.");
NS_LOG_INFO ("Sim ID: " << simulationId << "Clients: " << numberOfClients);
Simulator::Run ();
Simulator::Destroy ();
NS_LOG_INFO ("Done.");
}