// Network topology
//
//    SRC
//     | <=== Source Network
//     A-----B
//     |     |	                         Cost of different networks is as follows:
//     C-----D 	  	                     A to B : 5, B to D : 3, A to C : 2, C to D : 4
// 	         | <=== Destination Network       
//          DST
//
//
// A, B, C and D are RIPng routers.
// A and D are configured with static addresses.
// SRC and DST will exchange packets.
//
// After about 3 seconds, the topology is built, and Echo Reply will be received.
// After 30 seconds, the link between A and C will break, causing a route failure.
// After 31 seconds from the failure, the routers will recover from the failure.
// If "showPings" is enabled, the user will see:
// 1) if the ping has been acknowledged
// 2) if a Destination Unreachable has been received by the sender
// 3) nothing, when the Echo Request has been received by the destination but
//    the Echo Reply is unable to reach the sender.
// Examining the .pcap files with Wireshark can confirm this effect.

#include <fstream>
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/ipv6-routing-table-entry.h"

using namespace ns3;
//Define a logging component for this file which can later be enabled or disabled with the NS_LOG  environment variable
NS_LOG_COMPONENT_DEFINE ("RipNg-1");

//To tear down the link between any two interfaces
void TearDownLink (Ptr<Node> nodeA, Ptr<Node> nodeB, uint32_t interfaceA, uint32_t interfaceB)
{
  nodeA->GetObject<Ipv6> ()->SetDown (interfaceA);
  nodeB->GetObject<Ipv6> ()->SetDown (interfaceB);
}

int main (int argc, char **argv)
{
//Declare a verbose flag that can be set to true using the command line to enable log components
  bool verbose = false;
//Split horizon with Poison Reverse is used
  std::string SplitHorizon ("PoisonReverse");
  bool showPings = false;

//Creates an instance of CommandLine class to enable passsing command line arguments
  CommandLine cmd;
  cmd.AddValue ("verbose", "turn on log components", verbose);
  cmd.AddValue ("showPings", "Show Ping6 reception", showPings);
  cmd.AddValue ("splitHorizonStrategy", "Split Horizon strategy to use (NoSplitHorizon, SplitHorizon, PoisonReverse)", SplitHorizon);
//To parse the command line arguments
  cmd.Parse (argc, argv);

//Enables the Logging Components of all the levels above the level specified if verbose is set to true
  if (verbose)
    {
      LogComponentEnable ("RipNgSimpleRouting", LOG_LEVEL_INFO);
      LogComponentEnable ("RipNg", LOG_LEVEL_ALL);
      LogComponentEnable ("Icmpv6L4Protocol", LOG_LEVEL_INFO);
      LogComponentEnable ("Ipv6Interface", LOG_LEVEL_ALL);
      LogComponentEnable ("Icmpv6L4Protocol", LOG_LEVEL_ALL);
      LogComponentEnable ("NdiscCache", LOG_LEVEL_ALL);
      LogComponentEnable ("Ping6Application", LOG_LEVEL_ALL);
    }

  if (showPings)
    {
      LogComponentEnable ("Ping6Application", LOG_LEVEL_INFO);
    }

//Enables either Split Horizon, No Split horizon or poison Reverse depending on what is selected
  if (SplitHorizon == "NoSplitHorizon")
    {
      Config::SetDefault ("ns3::RipNg::SplitHorizon", EnumValue (RipNg::NO_SPLIT_HORIZON));
    }
  else if (SplitHorizon == "SplitHorizon")
    {
      Config::SetDefault ("ns3::RipNg::SplitHorizon", EnumValue (RipNg::SPLIT_HORIZON));
    }
  else
    {
      Config::SetDefault ("ns3::RipNg::SplitHorizon", EnumValue (RipNg::POISON_REVERSE));
    }

//To create the Nodes
  NS_LOG_INFO ("Creating nodes.");
  Ptr<Node> src = CreateObject<Node> ();
//Associates the node with the name provided in the first argument
  Names::Add ("SrcNode", src);
  Ptr<Node> dst = CreateObject<Node> ();
  Names::Add ("DstNode", dst);
  Ptr<Node> a = CreateObject<Node> ();
  Names::Add ("RouterA", a);
  Ptr<Node> b = CreateObject<Node> ();
  Names::Add ("RouterB", b);
  Ptr<Node> c = CreateObject<Node> ();
  Names::Add ("RouterC", c);
  Ptr<Node> d = CreateObject<Node> ();
  Names::Add ("RouterD", d);

//Defines a NodeContainer object that holds Devices
  NodeContainer net1 (src, a);
  NodeContainer net2 (a, b);
  NodeContainer net3 (a, c);
  NodeContainer net4 (b, d);
  NodeContainer net5 (c, d);
  NodeContainer net6 (d, dst);
  NodeContainer routers (a, b, c, d);
  NodeContainer nodes (src, dst);

//Creating CSMA channels
  NS_LOG_INFO ("Creating channels.");
  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", DataRateValue (5000000));
  csma.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (2)));

//Install CSMA Between the nodes
  NetDeviceContainer ndc1 = csma.Install (net1);
  NetDeviceContainer ndc2 = csma.Install (net2);
  NetDeviceContainer ndc3 = csma.Install (net3);
  NetDeviceContainer ndc4 = csma.Install (net4);
  NetDeviceContainer ndc5 = csma.Install (net5);
  NetDeviceContainer ndc6 = csma.Install (net6);

  NS_LOG_INFO ("Configuring RIPng on the Routers");
//Defines an RipNgHelper object that helps configure RipNg
  RipNgHelper ripNgRouting;

//To exclude RIPng at the specified interface
//Interfaces start from 0 and are added sequentially
//However Interface 0 is for loopback address
  ripNgRouting.ExcludeInterface (a, 1);
  ripNgRouting.ExcludeInterface (d, 3);

//Sets the interface metric for the specified links
  ripNgRouting.SetInterfaceMetric (a, 2, 5);
  ripNgRouting.SetInterfaceMetric (a, 3, 2);
  ripNgRouting.SetInterfaceMetric (b, 1, 5);
  ripNgRouting.SetInterfaceMetric (b, 2, 3);
  ripNgRouting.SetInterfaceMetric (c, 1, 2);
  ripNgRouting.SetInterfaceMetric (c, 2, 4);
  ripNgRouting.SetInterfaceMetric (d, 1, 3);
  ripNgRouting.SetInterfaceMetric (d, 2, 4);

  Ipv6ListRoutingHelper listRH;
  listRH.Add (ripNgRouting, 0);

  InternetStackHelper internetv6;
  internetv6.SetIpv4StackInstall (false);
  internetv6.SetRoutingHelper (listRH);
  internetv6.Install (routers);

  InternetStackHelper internetv6Nodes;
  internetv6Nodes.SetIpv4StackInstall (false);
  internetv6Nodes.Install (nodes);

//Assigning IPv6 addresses
//Source and Destination have Global Addresses
//Rest of the routers need only link-local addresses for routing within the network
  NS_LOG_INFO ("Assign IPv6 Addresses.");
  Ipv6AddressHelper ipv6;

  ipv6.SetBase (Ipv6Address ("2001:1::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer iic1 = ipv6.Assign (ndc1);
  iic1.SetForwarding (1, true);
  iic1.SetDefaultRouteInAllNodes (1);

  ipv6.SetBase (Ipv6Address ("2001:0:1::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer iic2 = ipv6.Assign (ndc2);
  iic2.SetForwarding (0, true);
  iic2.SetForwarding (1, true);

  ipv6.SetBase (Ipv6Address ("2001:0:2::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer iic3 = ipv6.Assign (ndc3);
  iic3.SetForwarding (0, true);
  iic3.SetForwarding (1, true);

  ipv6.SetBase (Ipv6Address ("2001:0:3::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer iic4 = ipv6.Assign (ndc4);
  iic4.SetForwarding (0, true);
  iic4.SetForwarding (1, true);

  ipv6.SetBase (Ipv6Address ("2001:0:4::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer iic5 = ipv6.Assign (ndc5);
  iic5.SetForwarding (0, true);
  iic5.SetForwarding (1, true);

  ipv6.SetBase (Ipv6Address ("2001:2::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer iic6 = ipv6.Assign (ndc6);
  iic6.SetForwarding (0, true);
  iic6.SetDefaultRouteInAllNodes (0);

  NS_LOG_INFO ("Create Applications.");
  uint32_t packetSize = 1024;
  uint32_t maxPacketCount = 100;
  Time interPacketInterval = Seconds (1.0);
  Ping6Helper ping6;

//Set the source and destination address
  ping6.SetLocal (iic1.GetAddress (0, 1));
  ping6.SetRemote (iic6.GetAddress (1, 1));
  ping6.SetAttribute ("MaxPackets", UintegerValue (maxPacketCount));
  ping6.SetAttribute ("Interval", TimeValue (interPacketInterval));
  ping6.SetAttribute ("PacketSize", UintegerValue (packetSize));
  ApplicationContainer apps = ping6.Install (src);
  apps.Start (Seconds (1.0));
  apps.Stop (Seconds (110.0));

  AsciiTraceHelper ascii;
//To create the trace file
  csma.EnableAsciiAll (ascii.CreateFileStream ("ripng-1.tr"));
//Enables generation of .pcap files that can be examined using wireshark
  csma.EnablePcapAll ("ripng-1", true);

//Call the function defined above to tear down the link between two nodes
  Simulator::Schedule (Seconds (30), &TearDownLink, a, c, 3, 1);

/* Now, do the actual simulation. */
  NS_LOG_INFO ("Run Simulation.");
  Simulator::Stop (Seconds (120));
  Simulator::Run ();
  Simulator::Destroy ();
  NS_LOG_INFO ("Done.");
}
