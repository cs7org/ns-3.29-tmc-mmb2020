/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2019 University of Erlangen-Nuernberg
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
 * Author: Joerg Deutschmann <joerg.deutschmann@fau.de>
 *
 * This work has been funded by the Federal Ministry of Economics and
 * Technology of Germany in the project Transparent Multichannel IPv6
 * (FKZ 50YB1705).
 */

#include "ns3/boolean.h"
#include "ns3/config.h"
#include "ns3/inet-socket-address.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/node.h"
#include "ns3/node-container.h"
#include "ns3/packet.h"
#include "ns3/pointer.h"
#include "ns3/simulator.h"
#include "ns3/string.h"
#include "ns3/test.h"
#include "ns3/uinteger.h"
#include "ns3/simple-net-device.h"
#include "ns3/simple-channel.h"
#include "ns3/simple-net-device-helper.h"
#include "ns3/socket-factory.h"
#include "ns3/udp-socket-factory.h"

#include "ns3/core-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-module.h"

#include "ns3/command-line.h"
#include "ns3/internet-module.h"
#include "ns3/traffic-control-helper.h"

#include "ns3/tmcPep.h"
#include "ns3/tranGia.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("TMCv4_mmb2020");

// TMC in a nutshell:
// Client <-----------> PEP <==============> PEP <----------> Server
//
// tgc                tmcPepL      sat     tmcPepR            tgs
// 10.0.0.2 <-------> 10.0.0.1 <=========> 10.1.0.1 <-------> 10.1.0.2
//                                 dsl
//
// ns-3 does not provide a transparent TCP proxy, therefore all connections
// from the client are destined for the tmcPepLeft.
// tmcPepLeft forwards all connections to tmcPepRight.
// tmcPepRight then creates corresponding connections on the right side.
// The mapping is based on the source IP, e.g. 10.0.0.2 --> 10.1.0.2
// See TmcPep for more details.

int 
main (int argc, char *argv[])
{
    std::string rDsl;
    std::string dDsl;
    std::string rSat;
    std::string dSat;
    char tranGiaModeCmd;
    tranGiaMode_e tranGiaMode = TGM_SEQ;
    uint32_t runNumber = 0;
    uint32_t nrIterations = 1000;

    CommandLine cmd;
    cmd.AddValue ("rDsl",  "Rate of DSL link (default 1 Mbps)", rDsl);
    cmd.AddValue ("dDsl",  "Delay of DSL link (default 15 ms)", dDsl);
    cmd.AddValue ("rSat",  "Rate of Sat link (default 20 Mbps)", rSat);
    cmd.AddValue ("dSat",  "Delay of Sat link (default 300 ms)", dSat);
    cmd.AddValue ("tranGiaMode",  "TranGia mode. s=SEQ p=PARALLEL h=HTTP2", tranGiaModeCmd);
    cmd.AddValue ("runNumber", "runNumber", runNumber);
    cmd.AddValue ("nrIterations", "nrIterations", nrIterations);
    cmd.Parse (argc, argv);

    if (!rDsl.empty()) { NS_ASSERT(!dDsl.empty()); }
    if (!dDsl.empty()) { NS_ASSERT(!rDsl.empty()); }
    if (!rSat.empty()) { NS_ASSERT(!dSat.empty()); }
    if (!dSat.empty()) { NS_ASSERT(!rSat.empty()); }
    NS_ASSERT(!rDsl.empty() || !rSat.empty());

    if(tranGiaModeCmd == 's') {tranGiaMode = TGM_SEQ;}
    else if(tranGiaModeCmd == 'p') {tranGiaMode = TGM_PARALLEL;}
    else if(tranGiaModeCmd == 'h') {tranGiaMode = TGM_HTTP2;}
    else {NS_ASSERT_MSG(false, "TranGia mode. s=SEQ p=PARALLEL h=HTTP2");}

    RngSeedManager::SetRun (runNumber);

    // Nodes
    Ptr<Node> nodeClient   = CreateObject<Node> ();
    Ptr<Node> nodePepLeft  = CreateObject<Node> ();
    Ptr<Node> nodePepRight = CreateObject<Node> ();
    Ptr<Node> nodeServer   = CreateObject<Node> ();

    NodeContainer nodesAll = NodeContainer (nodeClient, nodePepLeft, nodePepRight, nodeServer);

    InternetStackHelper internet;
    internet.Install (nodesAll);


    // P2P links
    PointToPointHelper p2pLeft, p2pRight, p2pSat, p2pTer;
    p2pLeft.SetDeviceAttribute ("DataRate", StringValue ("100Mbps"));
    p2pLeft.SetChannelAttribute ("Delay", StringValue ("5ms"));
    p2pRight.SetDeviceAttribute ("DataRate", StringValue ("100Mbps"));
    p2pRight.SetChannelAttribute ("Delay", StringValue ("5ms"));

    if(!rDsl.empty())
    {
        p2pTer.SetDeviceAttribute ("DataRate", StringValue (rDsl));
        p2pTer.SetChannelAttribute ("Delay", StringValue (dDsl));
        p2pTer.SetQueue("ns3::DropTailQueue", "MaxSize", StringValue ("1p"));
    }

    if(!rSat.empty())
    {
        p2pSat.SetDeviceAttribute ("DataRate", StringValue (rSat));
        p2pSat.SetChannelAttribute ("Delay", StringValue (dSat));
        p2pSat.SetQueue("ns3::DropTailQueue", "MaxSize", StringValue ("1p"));
    }

    NetDeviceContainer devicesLeft  = p2pLeft.Install (nodePepLeft, nodeClient);
    NetDeviceContainer devicesRight = p2pRight.Install (nodePepRight, nodeServer);
    NetDeviceContainer devicesSat   = p2pSat.Install (nodePepLeft, nodePepRight);
    NetDeviceContainer devicesTer   = p2pTer.Install (nodePepLeft, nodePepRight);

    // Add IP addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase ("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer ipv4Left = ipv4.Assign (devicesLeft);
    ipv4.SetBase ("10.1.0.0", "255.255.255.0");
    Ipv4InterfaceContainer ipv4Right = ipv4.Assign (devicesRight);


    // TMC model
    std::stringstream lgwLogFilename;
    lgwLogFilename << "mmb2020_link_dsl" << rDsl << dDsl << "_sat" << rSat << dSat
                   << "_mode" << tranGiaModeCmd << "_tmcPepLeft_run" << runNumber << ".csv";
    Ptr<TmcAppLeft> tmcPepL = CreateObject<TmcAppLeft> (lgwLogFilename.str());
    nodePepLeft->AddApplication (tmcPepL);
    tmcPepL->SetStartTime (Seconds (0.0));

    std::stringstream rgwLogFilename;
    rgwLogFilename << "mmb2020_link_dsl" << rDsl << dDsl << "_sat" << rSat << dSat
                   << "_mode" << tranGiaModeCmd << "_tmcPepRight_run" << runNumber << ".csv";
    Ptr<TmcAppRight> tmcPepR = CreateObject<TmcAppRight> (rgwLogFilename.str());
    nodePepRight->AddApplication (tmcPepR);
    tmcPepR->SetStartTime (Seconds (0.0));

    if(!rDsl.empty() && !rSat.empty())
    {
        uint64_t thresTerSat = calcThresTerSat(rDsl, dDsl, rSat, dSat);
        tmcPepL->m_thresSmallFlow = 2000;
        tmcPepL->m_thresTerSat = thresTerSat;
        tmcPepR->m_thresSmallFlow = 2000;
        tmcPepR->m_thresTerSat = thresTerSat;
    }


    // provide direct access to p2p link for bond and set up callbacks
    if(!rDsl.empty())
    {
        tmcPepL->m_devTer = devicesTer.Get(0);
        tmcPepR->m_devTer = devicesTer.Get(1);
        DynamicCast<PointToPointNetDevice>(tmcPepL->m_devTer)->m_transmitCompleteCb = MakeCallback (&TmcApp::sentToBond, tmcPepL);
        DynamicCast<PointToPointNetDevice>(tmcPepR->m_devTer)->m_transmitCompleteCb = MakeCallback (&TmcApp::sentToBond, tmcPepR);
        DynamicCast<PointToPointNetDevice>(tmcPepL->m_devTer)->SetReceiveCallback(MakeCallback (&TmcApp::recvFromBond, tmcPepL));
        DynamicCast<PointToPointNetDevice>(tmcPepR->m_devTer)->SetReceiveCallback(MakeCallback (&TmcApp::recvFromBond, tmcPepR));
    }

    if(!rSat.empty())
    {
        tmcPepL->m_devSat = devicesSat.Get(0);
        tmcPepR->m_devSat = devicesSat.Get(1);
        DynamicCast<PointToPointNetDevice>(tmcPepL->m_devSat)->m_transmitCompleteCb = MakeCallback (&TmcApp::sentToBond, tmcPepL);
        DynamicCast<PointToPointNetDevice>(tmcPepR->m_devSat)->m_transmitCompleteCb = MakeCallback (&TmcApp::sentToBond, tmcPepR);
        DynamicCast<PointToPointNetDevice>(tmcPepL->m_devSat)->SetReceiveCallback(MakeCallback (&TmcApp::recvFromBond, tmcPepL));
        DynamicCast<PointToPointNetDevice>(tmcPepR->m_devSat)->SetReceiveCallback(MakeCallback (&TmcApp::recvFromBond, tmcPepR));
    }


    // Workload model
    std::stringstream tgcLogFilename;
    tgcLogFilename << "mmb2020_link_dsl" << rDsl << dDsl << "_sat" << rSat << dSat
                   << "_mode" << tranGiaModeCmd << "_tranGiaClient_run" << runNumber << ".csv";
    TranGiaClient tgc(tranGiaMode, Ipv4Address("10.0.0.1"), 80, tgcLogFilename.str());
    nodeClient->AddApplication(&tgc);

    TranGiaServer tgs(tranGiaMode);
    nodeServer->AddApplication(&tgs);

    //Specific number of websites, therefore no need for Simulator::Stop()
    for(uint32_t iter = 1; iter < nrIterations; iter++)
    {
        Simulator::Schedule(Seconds(iter*1000), &TranGiaClient::StartApplication, &tgc);
    }


    // Set up tracing if desired
    if (false)
    {
        p2pLeft.EnablePcapAll ("tmcv4_left", false);
        p2pRight.EnablePcapAll ("tmcv4_right", false);
        p2pSat.EnablePcapAll ("tmcv4_sat", false);
        p2pTer.EnablePcapAll ("tmcv4_ter", false);
    }


    //
    // Now, do the actual simulation.
    //
    NS_LOG_INFO ("Run Simulation.");
    Simulator::Run ();
    Simulator::Destroy ();
    NS_LOG_INFO ("Done.");

}
