
#include "ns3/command-line.h"
#include "ns3/string.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/ssid.h"
#include "ns3/position-allocator.h"
#include "ns3/mobility-helper.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/on-off-helper.h"
#include "ns3/bulk-send-helper.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/packet-sink-helper.h"
#include "ns3/animation-interface.h"
#include "ns3/flow-monitor.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/ipv4-flow-classifier.h"
#include "ns3/core-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"
#include "ns3/lte-module.h"
#include "ns3/netanim-module.h"
#include "ns3/on-off-helper.h"
#include "ns3/bulk-send-helper.h"
#include <stdio.h>

/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2011-2018 Centre Tecnologic de Telecomunicacions de Catalunya (CTTC)
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
 *
 * Authors: Jaume Nin <jaume.nin@cttc.cat>
 *          Manuel Requena <manuel.requena@cttc.es>
 */



using namespace ns3;

/**
 * Sample simulation script for LTE+EPC. It instantiates 2 eNodeBs and a variant number of user entities,
 * attaches some of the UE per two eNodeB starts a flow for each of the UEs to and from a remote host.
 */

NS_LOG_COMPONENT_DEFINE ("LenaSimpleEpc");

int
main (int argc, char *argv[])
{
  uint16_t numNodePairs = 2;
  Time simTime = Seconds(20);

  Time interPacketInterval = MilliSeconds (100);
  bool useCa = false;
  bool disableDl = false;
  bool disableUl = false;
  bool disablePl = false;
  double distance = 1000.0;
  printf("Enter Number of User Entities: ");
  //int number;
  int numberOfUes;
  scanf("%d", &numberOfUes);

  // Command line arguments
  CommandLine cmd (__FILE__);
  cmd.AddValue ("numNodePairs", "Number of eNodeBs", numNodePairs);
  cmd.AddValue ("numNodePairs", "Number of User entities", numberOfUes);
  cmd.AddValue ("simTime", "Total duration of the simulation", simTime);
  cmd.AddValue ("distance", "Distance between eNBs [m]", distance);
  cmd.AddValue ("interPacketInterval", "Inter packet interval", interPacketInterval);
  cmd.AddValue ("useCa", "Whether to use carrier aggregation.", useCa);
  cmd.AddValue ("disableDl", "Disable downlink data flows", disableDl);
  cmd.AddValue ("disableUl", "Disable uplink data flows", disableUl);
  cmd.AddValue ("disablePl", "Disable data flows between peer UEs", disablePl);
  cmd.Parse (argc, argv);

  ConfigStore inputConfig;
  inputConfig.ConfigureDefaults ();

  // parse again so you can override default values from the command line
  cmd.Parse(argc, argv);

  if (useCa)
   {
     Config::SetDefault ("ns3::LteHelper::UseCa", BooleanValue (useCa));
     Config::SetDefault ("ns3::LteHelper::NumberOfComponentCarriers", UintegerValue (2));
     Config::SetDefault ("ns3::LteHelper::EnbComponentCarrierManager", StringValue ("ns3::RrComponentCarrierManager"));
   }
  

// Helpers 
  Ptr<LteHelper> lteHelper = CreateObject<LteHelper> ();
  Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper> ();
  lteHelper->SetEpcHelper (epcHelper);

  Ptr<Node> pgw = epcHelper->GetPgwNode ();

  // Create a single RemoteHost
  NodeContainer remoteHostContainer;
  remoteHostContainer.Create (1);
  Ptr<Node> remoteHost = remoteHostContainer.Get (0);
  InternetStackHelper internet;
  internet.Install (remoteHostContainer);

  // Create the Internet
  PointToPointHelper p2ph;
  p2ph.SetDeviceAttribute ("DataRate", DataRateValue (DataRate ("100Gb/s")));
  p2ph.SetDeviceAttribute ("Mtu", UintegerValue (1500));
  p2ph.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (10)));
  NetDeviceContainer internetDevices = p2ph.Install (pgw, remoteHost);
  Ipv4AddressHelper ipv4h;
  ipv4h.SetBase ("1.0.0.0", "255.0.0.0");
  Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign (internetDevices);
  
  // interface 0 is localhost, 1 is the p2p device
  Ipv4Address remoteHostAddr = internetIpIfaces.GetAddress (1);

  Ipv4StaticRoutingHelper ipv4RoutingHelper;
  Ptr<Ipv4StaticRouting> remoteHostStaticRouting = ipv4RoutingHelper.GetStaticRouting (remoteHost->GetObject<Ipv4> ());
  remoteHostStaticRouting->AddNetworkRouteTo (Ipv4Address ("7.0.0.0"), Ipv4Mask ("255.0.0.0"), 1);

  NodeContainer ueNodes;
  NodeContainer enbNodes;
  enbNodes.Create (numNodePairs);
  ueNodes.Create (numberOfUes); 

  // Install Mobility Model on ENodeB to be Constant and fixed within 1 km

  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  
  for (uint16_t i = 0; i < numNodePairs; i++)
    {
      positionAlloc->Add (Vector (1000, 500 + distance * i, 0));
    }
  MobilityHelper enb_mobility;
  enb_mobility.SetPositionAllocator(positionAlloc);
  enb_mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  enb_mobility.Install(enbNodes);
  
  //install mobility model on user entities 
  MobilityHelper ue_mobility; 
  ue_mobility.SetPositionAllocator ("ns3::RandomBoxPositionAllocator",
                                  "X", StringValue("ns3::UniformRandomVariable[Min=100|Max=2000]"),
                                  "Y", StringValue("ns3::UniformRandomVariable[Min=200|Max=2000]"), 
                                  "Z", StringValue("ns3::UniformRandomVariable[Min=0|Max=0]"));                   
                                 
  ue_mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                                "Mode", StringValue ("Time"),
                                "Time", StringValue ("1s"),
                                "Speed", StringValue ("ns3::ConstantRandomVariable[Constant=200.0]"),
                                "Bounds",StringValue ("0|2000|0|2000"));   
                                                       
  ue_mobility.Install(ueNodes);
  
  
  // Install LTE Devices to the nodes
  NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice (enbNodes);
  NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice (ueNodes);

  // Install the IP stack on the UEs
  internet.Install (ueNodes);
  Ipv4InterfaceContainer ueIpIface;
  ueIpIface = epcHelper->AssignUeIpv4Address (NetDeviceContainer (ueLteDevs));
 
  // Assign IP address to UEs, and install applications
  for (uint32_t u = 0; u < ueNodes.GetN (); ++u)
    {
      Ptr<Node> ueNode = ueNodes.Get (u);	
      // Set the default gateway for the UE
      Ptr<Ipv4StaticRouting> ueStaticRouting = ipv4RoutingHelper.GetStaticRouting (ueNode->GetObject<Ipv4> ());
      ueStaticRouting->SetDefaultRoute (epcHelper->GetUeDefaultGatewayAddress (), 1);
    }

  // Attach UE per eNodeB
  if (numberOfUes > 1)
  {
  for (uint16_t i = 0; i < numberOfUes; i++) 
    {
      lteHelper->Attach (ueLteDevs.Get(i), enbLteDevs.Get(0));   
      i += 1;   
    }
      for (uint16_t i = 0; i < numberOfUes; i++) 
    {
      lteHelper->Attach (ueLteDevs.Get(i), enbLteDevs.Get(1));
    }
    }
    else if(numberOfUes == 1)
    {
    lteHelper->Attach (ueLteDevs.Get(0), enbLteDevs.Get(0));
    }
    else
    {
    printf("No user entities! Nothing to be connected to the Base Stations");
    }

  // Install and start applications on UEs and remote host
  uint16_t dlPort = 1100;
  uint16_t ulPort = 2000;
  ApplicationContainer clientApps;
  ApplicationContainer serverApps;
  
      //--------------------BulkSendApplication ----- 
      /*
  for (uint32_t u = 0; u < ueNodes.GetN (); ++u)
    {
      if (!disableDl)
        {
          BulkSendHelper dlPacketSinkHelper ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), dlPort));

          serverApps.Add (dlPacketSinkHelper.Install (ueNodes.Get (u)));

          UdpClientHelper dlClient (ueIpIface.GetAddress (u), dlPort);
          dlClient.SetAttribute ("Interval", TimeValue (interPacketInterval));
          dlClient.SetAttribute ("MaxPackets", UintegerValue (1000000000));
          clientApps.Add (dlClient.Install (remoteHost));
        }

      if (!disableUl)
        {
          ++ulPort;
          BulkSendHelper ulPacketSinkHelper ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), ulPort));
          serverApps.Add (ulPacketSinkHelper.Install (remoteHost));

          UdpClientHelper ulClient (remoteHostAddr, ulPort);
          ulClient.SetAttribute ("Interval", TimeValue (interPacketInterval));
          ulClient.SetAttribute ("MaxPackets", UintegerValue (1000000000));
          clientApps.Add (ulClient.Install (ueNodes.Get(u)));
        }
    }  
    */
    //AnimationInterface anim ("Bulk.xml");
    
 //--------------------UDP client Server Helper
 /*
  for (uint32_t u = 0; u < ueNodes.GetN (); ++u)
    {
      if (!disableDl)
        {
        
          UdpServerHelper UDPServer (dlPort);

          serverApps.Add (UDPServer.Install (ueNodes.Get (u)));

          UdpClientHelper dlClient (ueIpIface.GetAddress (u), dlPort);
          dlClient.SetAttribute ("Interval", TimeValue (interPacketInterval));
          dlClient.SetAttribute ("MaxPackets", UintegerValue (1000000000));
          clientApps.Add (dlClient.Install (remoteHost));
        }

      if (!disableUl)
        {
          ++ulPort;
                  
          UdpServerHelper UDPServer (ulPort);

          serverApps.Add (UDPServer.Install (remoteHost));

          UdpClientHelper ulClient (remoteHostAddr, ulPort);
          ulClient.SetAttribute ("Interval", TimeValue (interPacketInterval));
          ulClient.SetAttribute ("MaxPackets", UintegerValue (1000000000));
          clientApps.Add (ulClient.Install (ueNodes.Get(u)));
        }
    }    
    */
     // AnimationInterface anim ("udp.xml");
      
    //----------------------------------On Off Application
    
 for (uint32_t u = 0; u < ueNodes.GetN (); ++u)
    {
      NS_LOG_INFO ("onoffApplications.");
      if (!disableDl)
        {
          OnOffHelper dlPacketSinkHelper ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), dlPort));
          serverApps.Add (dlPacketSinkHelper.Install (ueNodes.Get (u)));

          UdpClientHelper dlClient (ueIpIface.GetAddress (u), dlPort);
          dlClient.SetAttribute ("Interval", TimeValue (interPacketInterval));
          dlClient.SetAttribute ("MaxPackets", UintegerValue (1000000000));
          clientApps.Add (dlClient.Install (remoteHost));
        }

      if (!disableUl)
        {
          ++ulPort;
          OnOffHelper ulPacketSinkHelper ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), ulPort));
          serverApps.Add (ulPacketSinkHelper.Install (remoteHost));

          UdpClientHelper ulClient (remoteHostAddr, ulPort);
          ulClient.SetAttribute ("Interval", TimeValue (interPacketInterval));
          ulClient.SetAttribute ("MaxPackets", UintegerValue (1000000000));
          clientApps.Add (ulClient.Install (ueNodes.Get(u)));
        }
    }   
    
     AnimationInterface anim ("onoff.xml");


  serverApps.Start (MilliSeconds (500));
  clientApps.Start (MilliSeconds (500));
  lteHelper->EnableTraces ();



//--------------------------------------------------------------------------------------------------------
//------------------------------------Animation Part------------------------------------------------------
 
//Remote Host
anim.UpdateNodeDescription (remoteHostContainer.Get (0), "Host"); 
anim.UpdateNodeColor (remoteHostContainer.Get (0), 153, 204, 0);  //green

//E node B
anim.UpdateNodeDescription (enbNodes.Get (0), "ENodeB1"); 
anim.UpdateNodeColor (enbNodes.Get (0), 0, 140, 255);  //blue

anim.UpdateNodeDescription (enbNodes.Get (1), "ENodeB2");   
anim.UpdateNodeColor (enbNodes.Get (1), 0, 140, 255);  //blue    

// User entities 
for (uint32_t u = 0; u < ueNodes.GetN (); ++u)
    {
        anim.UpdateNodeDescription (ueNodes.Get (u), "User entity"); 
        anim.UpdateNodeColor (ueNodes.Get (u), 255, 153, 204);  //pink
    }

 // Flow monitoring
 FlowMonitorHelper flowmon;
 Ptr<FlowMonitor> monitor;
 monitor = flowmon.Install(ueNodes);
 monitor = flowmon.Install(remoteHost);
 
 //------------
  Simulator::Stop (simTime);
  Simulator::Run ();
  monitor -> SerializeToXmlFile("tota1.xml", true, true);

  
 Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
 int count = numberOfUes;

 std::map<FlowId, FlowMonitor::FlowStats> stats = monitor-> GetFlowStats();
 float throughput = 0;
 float jitter = 0;
 float lost = 0;

//----------------Lists for the flows ----------------------

float throughput_lst[count]= {0} , lostPackets_lst[count] = {0},  Jitter_lst[count] = {0};
  int i = 0;
 for(std::map<FlowId, FlowMonitor::FlowStats>::const_iterator iter = stats.begin (); iter != stats.end (); ++iter)
 {
  Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(iter->first);
  if (t.sourceAddress == Ipv4Address("1.0.0.2"))
  {
    throughput += iter->second.rxBytes * 8.0 / (iter->second.timeLastRxPacket.GetSeconds()-iter->second.timeFirstTxPacket.GetSeconds()) / 1024;
    jitter += iter->second.jitterSum.GetSeconds();
    lost += iter->second.lostPackets;
    //----------
    throughput_lst[i] = iter->second.rxBytes * 8.0 / (iter->second.timeLastRxPacket.GetSeconds()-iter->second.timeFirstTxPacket.GetSeconds()) / 1024;
    lostPackets_lst[i] = iter->second.lostPackets;
    Jitter_lst[i] = iter->second.jitterSum.GetSeconds();
    i += 1;
  } 
 }
  NS_LOG_UNCOND (numberOfUes << " " << throughput/count);
  NS_LOG_UNCOND (numberOfUes << " " << jitter/count);
  NS_LOG_UNCOND (numberOfUes << " " << lost/count);
  //NS_LOG_UNCOND (numberOfUes << " " << throughput);
  //NS_LOG_UNCOND (numberOfUes << " " << jitter);
  //NS_LOG_UNCOND (numberOfUes << " " << lost);


//-----------------------------File ----------------------------//
FILE * fPtr;
fPtr = fopen("thro5.txt", "w");
    for(int i = 0 ; i<count ;i++) 
    {
      fprintf(fPtr, "<Flow flowId=%d  Throughput=%f>", i+1,throughput_lst[i]); 
    }  
fclose(fPtr);
//-----------

FILE * fPtr2;
fPtr2 = fopen("jitt5.txt", "w");
    for(int i = 0 ; i<count ;i++) 
    {
      fprintf(fPtr2, "<Flow flowId=%d  Jitter=%f>", i+1,Jitter_lst[i]);
    }  
fclose(fPtr2);

//--------------
FILE * fPtr3;
fPtr3 = fopen("lost5.txt", "w");
    for(int i = 0 ; i<count ;i++) 
    {
      fprintf(fPtr3, "<Flow flowId=%d  lostPackets=%f>", i+1,lostPackets_lst[i]);   
    }
       
fclose(fPtr3);

//--------------------------------
  Simulator::Destroy ();
  return 0;
}







