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

// ajustar valor das sementes, colocar manual -->> Feito!
// ajustar taxa do trecho sem fio, setar mcs do sem fio -->> Feito!

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
#include "ns3/propagation-module.h"
#include <sys/stat.h>
#include "ns3/output-stream-wrapper.h"
#include <sys/types.h>
#include <errno.h>
#include <iomanip>
#include "ns3/flow-monitor-module.h"
#include "ns3/tcp-stream-helper.h"
#include "ns3/tcp-stream-interface.h"
#include <fstream>
#include "ns3/stats-module.h"


template <typename T>
std::string ToString(T val)
{
    std::stringstream stream;
    stream << val;
    return stream.str();
}

using namespace ns3;

static void
CwndChange (Ptr<OutputStreamWrapper> stream, uint32_t oldCwnd, uint32_t newCwnd)
{
  NS_LOG_UNCOND (Simulator::Now ().GetSeconds () << "\t" << newCwnd);
  *stream->GetStream () << Simulator::Now ().GetSeconds () << "\t" << oldCwnd << "\t" << newCwnd << std::endl;
}


NS_LOG_COMPONENT_DEFINE ("TcpStreamExample");

int
main (int argc, char *argv[])
{
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
  uint16_t count;
  std::string tcpVariant;
  std::string adaptationAlgo;
  std::string segmentSizeFilePath;

  bool shortGuardInterval = true;

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
  
    //TypeIdValue socketType;
    //GlobalValue::GetValueByNameFailSafe("ns3::TcpL4Protocol::SocketType", socketType);
    //std::cout << "TCP variant: " << socketType.Get().GetName() << std::endl;
    std::cout << "Adaptation Algorithm: " << adaptationAlgo << std::endl;

  //add while loopn here !!!

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


    //Set constant bitrate at mcs
    WifiHelper wifiHelper;
    wifiHelper.SetStandard (WIFI_PHY_STANDARD_80211n_5GHZ);
    wifiHelper.SetRemoteStationManager ("ns3::ConstantRateWifiManager", "DataMode",StringValue ("HtMcs0"), 
    "ControlMode", StringValue ("HtMcs0"));


    /* Set up Legacy Channel */
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
    // We do not set an explicit propagation loss model here, we just use the default ones that get applied with the building model.

    /* Setup Physical Layer */
    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
    wifiPhy.SetPcapDataLinkType (YansWifiPhyHelper::DLT_IEEE802_11_RADIO);
    wifiPhy.Set ("TxPowerStart", DoubleValue (20.0));//
    wifiPhy.Set ("TxPowerEnd", DoubleValue (20.0));//
    wifiPhy.Set ("TxPowerLevels", UintegerValue (1));//
    wifiPhy.Set ("TxGain", DoubleValue (0));//
    wifiPhy.Set ("RxGain", DoubleValue (0));//
    wifiPhy.SetErrorRateModel ("ns3::YansErrorRateModel");//
    wifiPhy.SetChannel (wifiChannel.Create ());
    wifiPhy.Set("ShortGuardEnabled", BooleanValue(shortGuardInterval));
    wifiPhy.Set ("Antennas", UintegerValue (4));
    // wifiPhy.Set ("RxAntennas", UintegerValue (4));

    /* Create Nodes */
    NodeContainer networkNodes;
    networkNodes.Create (numberOfClients + 2);

    /* Determin access point and server node */
    Ptr<Node> apNode = networkNodes.Get (0);
    Ptr<Node> serverNode = networkNodes.Get (1);

    /* Configure clients as STAs in the WLAN */
    NodeContainer staContainer;
    /* Begin at +2, because position 0 is the access point and position 1 is the server */
    for (NodeContainer::Iterator i = networkNodes.Begin () + 2; i != networkNodes.End (); ++i)
      {
        staContainer.Add (*i);
      }

    /* Determin client nodes for object creation with client helper class */
    std::vector <std::pair <Ptr<Node>, std::string> > clients;
    for (NodeContainer::Iterator i = networkNodes.Begin () + 2; i != networkNodes.End (); ++i)
      {
        std::pair <Ptr<Node>, std::string> client (*i, adaptationAlgo);
        clients.push_back (client);
        
      }


    /* Set up WAN link between server node and access point*/
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute ("DataRate", StringValue ("1000kb/s")); // This must not be more than the maximum throughput in 802.11n
    p2p.SetDeviceAttribute ("Mtu", UintegerValue (1500));
    p2p.SetChannelAttribute ("Delay", StringValue ("45ms"));
    NetDeviceContainer wanIpDevices;
    wanIpDevices = p2p.Install (serverNode, apNode);

    /* create MAC layers */
    WifiMacHelper wifiMac;
    /* WLAN configuration */
    Ssid ssid = Ssid ("network");
    /* Configure STAs for WLAN*/

    wifiMac.SetType ("ns3::StaWifiMac",
                      "Ssid", SsidValue (ssid));
    NetDeviceContainer staDevices;
    staDevices = wifiHelper.Install (wifiPhy, wifiMac, staContainer);

    /* Configure AP for WLAN*/
    wifiMac.SetType ("ns3::ApWifiMac",
                      "Ssid", SsidValue (ssid));
    NetDeviceContainer apDevice;
    apDevice = wifiHelper.Install (wifiPhy, wifiMac, apNode);

    Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/ChannelWidth", UintegerValue (40));

    /* Determin WLAN devices (AP and STAs) */
    NetDeviceContainer wlanDevices;
    wlanDevices.Add (staDevices);
    wlanDevices.Add (apDevice);

    /* Internet stack */
    InternetStackHelper stack;
    stack.Install (networkNodes);

    Ptr<Node> someNode = networkNodes.Get(0); // Pick any node you installed InternetStack on
    Ptr<TcpL4Protocol> tcp = someNode->GetObject<TcpL4Protocol>();


    if (tcp)
    {
      TypeIdValue typeIdValue;
      tcp->GetAttribute("SocketType", typeIdValue);
      std::cout << "TCP variant in use: " << typeIdValue.Get().GetName() << std::endl;
    }
    else
    {
      std::cout << "TcpL4Protocol not found on node!" << std::endl;
    }


    /* Assign IP addresses */
    Ipv4AddressHelper address;

    /* IPs for WAN */
    address.SetBase ("76.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer wanInterface = address.Assign (wanIpDevices);
    Address serverAddress = Address(wanInterface.GetAddress (0));

    /* IPs for WLAN (STAs and AP) */
    address.SetBase ("192.168.1.0", "255.255.255.0");
    address.Assign (wlanDevices);

    /* Populate routing table */
    Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
    uint16_t port = 9;

    Vector posAp = Vector ( 1.0, 1.0, 1.0);
    // give the server node any position, it does not have influence on the simulation, it has to be set though,
    // because when we do: mobility.Install (networkNodes);, there has to be a position as place holder for the server
    // because otherwise the first client would not get assigned the desired position.
    Vector posServer = Vector (1.5, 1.5, 1.5);

    /* Set up positions of nodes (AP and server) */
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
    positionAlloc->Add (posAp);
    positionAlloc->Add (posServer);

    // create folder so we can log the positions of the clients
    const char * mylogsDir = dashLogDirectory.c_str();
    mkdir (mylogsDir, 0775);
    std::string algodirstr (dashLogDirectory +  adaptationAlgo );  
    const char * algodir = algodirstr.c_str();
    mkdir (algodir, 0775);
    std::string dirstr (dashLogDirectory + adaptationAlgo + "/" + ToString (numberOfClients) + "/");
    const char * dir = dirstr.c_str();
    mkdir(dir, 0775);

  // allocate clients to positions
    for (uint i = 0; i < numberOfClients; i++)
      {
        Vector pos = Vector (2.0, 2.0, 2.0);
        positionAlloc->Add (pos);

      }

  /* Mobility model */
    MobilityHelper mobility;
    mobility.SetPositionAllocator (positionAlloc);
    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    mobility.Install (networkNodes);

    // if logging of the packets between AP---Server or AP and the STAs is wanted, these two lines can be activated

    // p2p.EnablePcapAll ("p2p-", true);
    // wifiPhy.EnablePcapAll ("wifi-", true);



    /* Install TCP Receiver on the access point */
    TcpStreamServerHelper serverHelper (port);
    ApplicationContainer serverApp = serverHelper.Install (serverNode);
    serverApp.Start (Seconds (0.0));
    /* Install TCP/UDP Transmitter on the station */
    TcpStreamClientHelper clientHelper (serverAddress, port);
    clientHelper.SetAttribute ("SegmentDuration", UintegerValue (segmentDuration));
    clientHelper.SetAttribute ("SegmentSizeFilePath", StringValue (segmentSizeFilePath));
    clientHelper.SetAttribute ("NumberOfClients", UintegerValue(numberOfClients));
    clientHelper.SetAttribute ("SimulationId", UintegerValue (simulationId));
    ApplicationContainer clientApps = clientHelper.Install (clients);
    for (uint i = 0; i < clientApps.GetN (); i++)
      {
        double startTime = ((i * 2));
        clientApps.Get (i)->SetStartTime (Seconds (startTime));
      }

    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream ("tcp-cwnd.tr");

    Config::ConnectWithoutContext ("/NodeList/*/$ns3::TcpL4Protocol/SocketList/*/CongestionWindow",
                               MakeBoundCallback (&CwndChange, stream));



    Ptr<Node> someNode2 = networkNodes.Get(2); // Pick any node you installed InternetStack on
    Ptr<TcpL4Protocol> tcp2 = someNode2->GetObject<TcpL4Protocol>();

    if (tcp2)
    {
      TypeIdValue typeIdValue2;
      tcp2->GetAttribute("SocketType", typeIdValue2);
      std::cout << "TCP variant in use: " << typeIdValue2.Get().GetName() << std::endl;
    }
    else
    {
      std::cout << "TcpL4Protocol not found on node!" << std::endl;
    }



    NS_LOG_INFO ("Run Simulation.");
    NS_LOG_INFO ("Sim: " << simulationId << "Clients: " << numberOfClients);
    Simulator::Run ();
    Simulator::Destroy ();
    NS_LOG_INFO ("Done.");

    simulationId += 1;
    seed += 1;

  }

}
