/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright 2016 Technische Universitaet Berlin
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

// - TCP Stream server and user-defined number of clients connected with an AP
// - WiFi connection
// - Tracing of throughput, packet information is done in the client

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
#include "ns3/csma-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/tcp-stream-helper.h"
#include "ns3/tcp-stream-interface.h"
#include "ns3/packet-sink.h"
#include "ns3/packet-sink-helper.h"

template <typename T>
std::string ToString(T val)
{
    std::stringstream stream;
    stream << val;
    return stream.str();
}

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("TcpStreamExample");

Ptr<PacketSink> sink;                         /* Pointer to the packet sink application */
uint64_t lastTotalRx = 0;                     /* The value of the last total received bytes */

/*void
CalculateThroughput ()
{
  Time now = Simulator::Now ();                                         // Return the simulator's virtual time. 
  double cur = (sink->GetTotalRx () - lastTotalRx) * (double) 8 / 1e5;     // Convert Application RX Packets to MBits. 
  std::cout << now.GetSeconds () << "s: \t" << cur << " Mbit/s" << std::endl;
  lastTotalRx = sink->GetTotalRx ();
  Simulator::Schedule (MilliSeconds (100), &CalculateThroughput);
}*/

static void
CwndChange (Ptr<OutputStreamWrapper> stream, uint32_t oldCwnd, uint32_t newCwnd)
{
  std::cout << "Im here" << std::endl;
  NS_LOG_UNCOND(Simulator::Now().GetSeconds() << "\t" << newCwnd);
  std::cout << Simulator::Now().GetSeconds() << "\t" << oldCwnd << "\t" << newCwnd
                       << std::endl;
}

// Connect the CongestionWindow trace for every TCP socket created
static void
SocketCreated (Ptr<OutputStreamWrapper> stream, Ptr<Socket> socket)
{
  std::cout << "Im here 2" << std::endl;
  // Guard: only hook TCP sockets
  if (socket->GetInstanceTypeId().GetParent() == TcpSocket::GetTypeId())
    {
      socket->TraceConnectWithoutContext(
          "CongestionWindow",
          MakeBoundCallback(&CwndChange, stream));
    }
}

int
main (int argc, char *argv[])
{

srand(time(NULL));   // Initialization, should only be called once.
int r = rand();      // Returns a pseudo-random integer between 0 and RAND_MAX.
SeedManager::SetSeed(r); // Set the seed to the desired number

//
// Users may find it convenient to turn on explicit debugging
// for selected modules; the below lines suggest how to do this
//
// #if 1
//   LogComponentEnable ("TcpStreamExample", LOG_LEVEL_INFO);
//   LogComponentEnable ("TcpStreamClientApplication", LOG_LEVEL_INFO);
//   LogComponentEnable ("TcpStreamServerApplication", LOG_LEVEL_INFO);
// #endif

  uint64_t segmentDuration;
  // The simulation id is used to distinguish log file results from potentially multiple consequent simulation runs.
  uint32_t simulationId;
  uint32_t numberOfClients;
  uint32_t seed;
  uint32_t count;
  double startTime;
  std::string adaptationAlgo;
  std::string segmentSizeFilePath;
  std::string tcpVariant, save_tcpVariant;
  std::string linkrate;

  CommandLine cmd;
  cmd.Usage ("Simulation of streaming with DASH.\n");
  cmd.AddValue("seed", "Seed of the random events", seed);
  cmd.AddValue ("simulationId", "The simulation's index (for logging purposes)", simulationId);
  cmd.AddValue ("numberOfClients", "The number of clients", numberOfClients);
  cmd.AddValue ("segmentDuration", "The duration of a video segment in microseconds", segmentDuration);
  cmd.AddValue ("adaptationAlgo", "The adaptation algorithm that the client uses for the simulation", adaptationAlgo);
  cmd.AddValue ("segmentSizeFile", "The relative path (from ns-3.x directory) to the file containing the segment sizes in bytes", segmentSizeFilePath);
  cmd.AddValue ("tcpVariant", "TCP Congestion Control Algorithm", tcpVariant);
  cmd.AddValue ("count", "Count the number of rounds", count);
  cmd.Parse (argc, argv);

  // ./waf --run="tcp-stream-pel-uerj --tcpVariant=NewReno --simulationId=899 --count=1 --seed=400  --numberOfClients=1 --adaptationAlgo=panda --segmentDuration=2000000 --segmentSizeFile=contrib/dash/segmentSizes.txt"

  save_tcpVariant = tcpVariant;
  tcpVariant = std::string ("ns3::Tcp") + tcpVariant;
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

  count = count + simulationId;

  while (simulationId < count){
    //seed = simulationId;
    std::cout << "Starting round: " << simulationId << std::endl;
    std::cout << "This seed: " << seed << std::endl;

    // srand(time(NULL));   // Initialization, should only be called once.
    // int r = rand();      // Returns a pseudo-random integer between 0 and RAND_MAX.
    // SeedManager::SetSeed(r); // Set the seed to the desired number
    ns3::RngSeedManager::SetSeed(seed);

    Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue (1446));
    Config::SetDefault("ns3::TcpSocket::SndBufSize", UintegerValue (524288));
    Config::SetDefault("ns3::TcpSocket::RcvBufSize", UintegerValue (524288));

    NS_LOG_INFO("Create nodes.");
      
    NodeContainer p2pNodes;
    p2pNodes.Create (2);

    NodeContainer csmaNodes;
    csmaNodes.Add (p2pNodes.Get (1));
    csmaNodes.Create (numberOfClients);	

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("100Mbps"));
    pointToPoint.SetChannelAttribute ("Delay", StringValue ("45ms"));

    NetDeviceContainer p2pDevices;
    p2pDevices = pointToPoint.Install (p2pNodes);

    // linkrate = std::to_string(numberOfClients*1) + "Mbps"; //link of 1Mbps per client

    CsmaHelper csma;
    csma.SetChannelAttribute ("DataRate", StringValue ("100Mbps"));
    csma.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (6560)));

    NetDeviceContainer csmaDevices;
    csmaDevices = csma.Install (csmaNodes);

    InternetStackHelper stack;
    stack.Install (p2pNodes.Get (0));
    stack.Install (csmaNodes);
      
    Ipv4AddressHelper address;
    address.SetBase ("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer p2pInterfaces;
    p2pInterfaces = address.Assign (p2pDevices);

    address.SetBase ("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer csmaInterfaces;
    csmaInterfaces = address.Assign (csmaDevices);

    // create folder so we can log the positions of the clients
    const char * mylogsDir = dashLogDirectory.c_str();
    mkdir (mylogsDir, 0775);
    std::string algodirstr (dashLogDirectory +  adaptationAlgo + "/" + save_tcpVariant);  
    const char * algodir = algodirstr.c_str();
    mkdir (algodir, 0775);
    std::string dirstr (dashLogDirectory + adaptationAlgo + "/" + save_tcpVariant + "/" + ToString (numberOfClients) + "/");
    const char * dir = dirstr.c_str();
    mkdir(dir, 0775);

    std::vector <std::pair <Ptr<Node>, std::string> > clients;
    for (NodeContainer::Iterator i = csmaNodes.Begin () + 1; i != csmaNodes.End (); ++i)
      {
        std::pair <Ptr<Node>, std::string> client (*i, adaptationAlgo);
        clients.push_back (client);
      }

    TcpStreamServerHelper serverHelper (80);
    //serverHelper.SetAttribute ("SegmentDuration", UintegerValue (segmentDuration));
    //serverHelper.SetAttribute ("SegmentSizeFilePath", StringValue (segmentSizeFilePath));
    //serverHelper.SetAttribute ("Chunk", UintegerValue (chunk));
    //serverHelper.SetAttribute ("Cmaf", UintegerValue (cmaf));
    ApplicationContainer serverApp = serverHelper.Install (p2pNodes.Get (0));
    serverApp.Start (Seconds (0));
      
    TcpStreamClientHelper clientHelper (p2pInterfaces.GetAddress(0), 80);
    clientHelper.SetAttribute ("SegmentDuration", UintegerValue (segmentDuration));
    clientHelper.SetAttribute ("SegmentSizeFilePath", StringValue (segmentSizeFilePath));
    clientHelper.SetAttribute ("NumberOfClients", UintegerValue(numberOfClients));
    clientHelper.SetAttribute ("SimulationId", UintegerValue (simulationId));
    //if(playbackStart > 0) {
      //clientHelper.SetAttribute ("PlaybackStart", UintegerValue (playbackStart));
    //}
    //clientHelper.SetAttribute ("Chunk", UintegerValue (chunk));
    //clientHelper.SetAttribute ("Cmaf", UintegerValue (cmaf));
    //clientHelper.SetAttribute ("LogLevel", UintegerValue (logLevel));
    //clientHelper.SetAttribute ("AbrParameter", UintegerValue (ABR_parameter));
    ApplicationContainer clientApps = clientHelper.Install (clients);
    for (uint i = 0; i < clientApps.GetN (); i++)
    {
      startTime = (i * 20);
      clientApps.Get (i)->SetStartTime (Seconds (startTime));
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
    //Simulator::Schedule (Seconds (startTime), &CalculateThroughput);

    std::cout << "Im here 3" << std::endl;

    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream ("seventh.cwnd");
    Config::ConnectWithoutContext("/NodeList/*/$ns3::TcpL4Protocol/SocketList/*",
      MakeBoundCallback(&SocketCreated, stream));

    std::cout << "Im here 4" << std::endl;


    NS_LOG_INFO ("Run Simulation.");
    NS_LOG_INFO ("Sim: " << simulationId << "Clients: " << numberOfClients);
    Simulator::Run ();
    Simulator::Destroy ();
    NS_LOG_INFO ("Done.");

    simulationId += 1;
    seed += 1;

    }
}
