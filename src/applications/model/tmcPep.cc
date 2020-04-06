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

#include <unistd.h>
#include <stdio.h>

#include "ns3/tmcPep.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("tmcPep");


//
// ns-3 class implementation
//
TmcApp::TmcApp ()
: m_devTer (0),
  m_devSat (0),
  m_thresSmallFlow (2000),
  m_thresTerSat (37500),
  m_linkUsedTer (false),
  m_linkUsedSat (false)
{
    for(int i = 0; i < TMC_CONNS; i++)
    {
        tmcConArray[i].status = TMC_STATUS_UNUSED;
        tmcConArray[i].pendBytes = 0;
        tmcConArray[i].potentialLink = LINKTYPE_UNDEFINED;
        tmcConArray[i].srcIp = 0;
        tmcConArray[i].dstIp = 0;
        tmcConArray[i].srcPort = 0;
        tmcConArray[i].dstPort = 0;
        tmcConArray[i].curPktId = 0;
        tmcConArray[i].expPktId = 0;
    }
}


TmcApp::~TmcApp()
{
}


void TmcApp::printTmcAppCallbackDev(Ptr<NetDevice> dev, std::string prefix)
{
    if(dynamic_cast<TmcAppRight*>(this) && (dev == m_devTer))
    {
        NS_LOG_INFO(prefix << " this is a TmcAppRight, callback from ter");
    }
    else if(dynamic_cast<TmcAppLeft*>(this) && (dev == m_devTer))
    {
        NS_LOG_INFO(prefix << " this is a TmcAppLeft, callback from ter");
    }
    else if(dynamic_cast<TmcAppRight*>(this) && (dev == m_devSat))
    {
        NS_LOG_INFO(prefix << " this is a TmcAppRight, callback from sat");
    }
    else if(dynamic_cast<TmcAppLeft*>(this) && (dev == m_devSat))
    {
        NS_LOG_INFO(prefix << " this is a TmcAppLeft, callback from sat");
    }
    else
    {
        NS_ASSERT(false);
    }
}


void TmcApp::printSfsHdr(sfsHdr_t* sfsHdr)
{
    NS_LOG_INFO("sfsHdr srcIp " << Ipv4Address(sfsHdr->srcIp) << ":" << sfsHdr->srcPort
                  << ", dstIp " << Ipv4Address(sfsHdr->dstIp) << ":" << sfsHdr->dstPort
                  << ", pktSize " << (int)sfsHdr->pktSize
                  << ", ctrl " << (int)sfsHdr->ctrl
                  << ", pktId " << sfsHdr->pktId);
}


void TmcApp::printTmcConArray()
{
    for(int k = 0; k < TMC_CONNS; k++)
    {
        Ipv4Address simSrcIp(tmcConArray[k].srcIp);
        Ipv4Address simDstIp(tmcConArray[k].dstIp);
        NS_LOG_INFO("tmcConArray isUsed " << tmcConArray[k].status << ", socket " << tmcConArray[k].sk
                << ", srcIp " << simSrcIp << ", srcPort " << tmcConArray[k].srcPort
                << ", dstIp " << simDstIp << ", dstPort" << tmcConArray[k].dstPort);
    }
}


int TmcApp::findTmcConArrayEntry(Ptr<Socket> socket)
{
    int sockIdx = 0;
    for(sockIdx = 0; sockIdx < TMC_CONNS; sockIdx++)
    {
        if(tmcConArray[sockIdx].sk == socket)
        {
            break;
        }
    }
    NS_ASSERT_MSG(sockIdx < TMC_CONNS, "tmcConArray does not contain HandleRead's ns-3 socket (was there a HandleAccept?)");

    return sockIdx;
}


// return unused tmcConArray entry
int TmcApp::allocateTmcCon(Ptr<Socket> socket, uint32_t srcIp, uint32_t dstIp, uint16_t srcPort, uint16_t dstPort)
{
    int fd_newClient = 0;
    for(fd_newClient = 0; fd_newClient < TMC_CONNS; fd_newClient++)
    {
        if(tmcConArray[fd_newClient].status == TMC_STATUS_UNUSED)
        {
            break;
        }
    }
    NS_ASSERT_MSG(fd_newClient < TMC_CONNS, "Unable to find free socket in tmcConArray (too many connections?)");

    tmcConArray[fd_newClient].status  = TMC_STATUS_USED;
    tmcConArray[fd_newClient].sk      = socket;
    tmcConArray[fd_newClient].srcIp   = srcIp;
    tmcConArray[fd_newClient].dstIp   = dstIp;
    tmcConArray[fd_newClient].srcPort = srcPort;
    tmcConArray[fd_newClient].dstPort = dstPort;
    tmcConArray[fd_newClient].curPktId = 0;
    tmcConArray[fd_newClient].expPktId = 1; // skip 0, which is TMC_CTRL_FLOW_INIT

    return fd_newClient;
}


void TmcApp::recvFromHost(Ptr<Socket> socket, int sock)
{
    NS_LOG_FUNCTION(this << socket << sock);
    NS_ASSERT(tmcConArray[sock].status == TMC_STATUS_USED);

    uint8_t rxBuffer[BUF_SIZE];
    int received = 0;

    while(true)
    {
        memset(rxBuffer, 0, sizeof(rxBuffer));

        received = socket->Recv(rxBuffer, BUF_SIZE, 0);
        NS_ASSERT_MSG(received >= 0, "Recv() returned error");
        if(received == 0)
        {
            return;
        }

        //create sfsHdr
        sfsHdr_t hdr = {0};
        hdr.srcIp   = tmcConArray[sock].srcIp;
        hdr.dstIp   = tmcConArray[sock].dstIp;
        hdr.srcPort = tmcConArray[sock].srcPort;
        hdr.dstPort = tmcConArray[sock].dstPort;
        hdr.pktSize = received;
        hdr.ctrl    = TMC_CTRL_FLOW_REGULAR;
        hdr.pktId   = ++tmcConArray[sock].curPktId;

        uint8_t *data = (uint8_t*)malloc(hdr.pktSize);
        memcpy(data, rxBuffer, hdr.pktSize);

        printSfsHdr(&hdr);

        tmcConArray[sock].rxQueue.push_back({hdr,
            LINKTYPE_UNDEFINED, //linktype will be defined by checkQueues()
            getTime64(),
            data});

        checkQueues();
    }

}


void TmcApp::sentToBond (Ptr<NetDevice> dev)
{
    NS_LOG_FUNCTION(this);

    printTmcAppCallbackDev(dev, "sentToBond");

    if(dev == m_devTer)
    {
        m_linkUsedTer = false;
    }
    else if(dev == m_devSat)
    {
        m_linkUsedSat = false;
    }

    checkQueues();
}


bool TmcApp::recvFromBond (Ptr<NetDevice> dev, Ptr<const Packet> packet, uint16_t protocol, const Address & remote)
{
    NS_LOG_FUNCTION(this << packet);

    linkType_e bondRxLinkType = LINKTYPE_UNDEFINED;
    NS_ASSERT(packet->GetSize() <= sizeof(sfsHdr_t)+BUF_SIZE);

    uint8_t rxBuffer[sizeof(sfsHdr_t)+BUF_SIZE];
    packet->CopyData (rxBuffer, packet->GetSize());

    sfsHdr_t hdr = {0};

    //with direct p2p links we always get full packets (hdr+payload)
    memcpy(&hdr, rxBuffer, sizeof(sfsHdr_t));

    printTmcAppCallbackDev(dev, "recvFromBond received sfsHdr");
    printSfsHdr(&hdr);

    // simplification for simulation
    // ns-3 has no transparent TCP proxies, we simply forward all connections to the other side
    if(dynamic_cast<TmcAppRight*>(this))
    {
        Ipv4Address dstIpOriginal(hdr.dstIp);
        // the dstIp on the right side corresponds to the source node
        // e.g. 10.0.0.2 --> 10.1.0.2
        hdr.dstIp = hdr.srcIp + 0x00010000;

        NS_LOG_INFO("TmcAppRight TCP Proxy: changing hdr.dstIp from " << dstIpOriginal
                     << " to " << Ipv4Address(hdr.dstIp));
    }
    if(dynamic_cast<TmcAppLeft*>(this))
    {
        NS_LOG_INFO("TmcAppLeft TCP Proxy: changing hdr.dstIp from " << Ipv4Address(hdr.dstIp)
                     << " to " << Ipv4Address("10.0.0.1"));

        // the only dstIp we used on the left side
        hdr.dstIp = Ipv4Address("10.0.0.1").Get();
    }

    NS_ASSERT_MSG(hdr.dstPort == TMC_TPROXY_PORT, "Only dstPort TMC_TPROXY_PORT allowed in ns-3");
    NS_ASSERT(hdr.pktSize <= BUF_SIZE);

    if(hdr.ctrl == TMC_CTRL_FLOW_INIT)
    {
        NS_ASSERT(dynamic_cast<TmcAppRight*>(this));

        Ptr<Socket> dstSocket = Socket::CreateSocket (GetNode (), TcpSocketFactory::GetTypeId ());
        dstSocket->Connect(InetSocketAddress (Ipv4Address (hdr.dstIp), hdr.dstPort));

        dstSocket->SetRecvCallback (MakeCallback (&TmcApp::HandleRead, this));
        dstSocket->SetCloseCallbacks (MakeCallback (&TmcApp::NormalCloseCallback, this), //TmcAppRight
                                      MakeCallback (&TmcApp::ErrorCloseCallback, this)); //TmcAppRight

        int tmcConEntry = allocateTmcCon(dstSocket, hdr.srcIp, hdr.dstIp, hdr.srcPort, hdr.dstPort);

        NS_LOG_INFO("Flow not known, creating connection to " << Ipv4Address (hdr.dstIp) << ":" << hdr.dstPort
                    << " (tmcConEntry " << tmcConEntry << ")");

        return true;
    }

    // check if flow is known
    uint32_t entry = 0;
    for(entry = 0; entry < TMC_CONNS; entry++)
    {
        if(    tmcConArray[entry].srcIp   == hdr.srcIp
                && tmcConArray[entry].dstIp   == hdr.dstIp
                && tmcConArray[entry].srcPort == hdr.srcPort
                && tmcConArray[entry].dstPort == hdr.dstPort)
        {
            NS_ASSERT(tmcConArray[entry].status  == TMC_STATUS_USED);
            break;
        }
    }

    if(entry == TMC_CONNS)
    {
        printSfsHdr(&hdr);
        printTmcConArray();

        NS_ASSERT_MSG(false, "Flow not found (already terminated?)");
        return true;
    }

    NS_LOG_INFO("Received sfsHdr, flow found in tmcConArray entry " << entry << ", expPktId " << tmcConArray[entry].expPktId);

    // We got a valid packet, but not sure whether the order is correct
    NS_ASSERT(hdr.pktId >= tmcConArray[entry].expPktId);

    if(hdr.pktId > tmcConArray[entry].expPktId)
    {
        // a too large pktId should not close any gaps
        uint64_t expPktIdBackup = tmcConArray[entry].expPktId;
        sendPendingPkts(entry);
        NS_ASSERT_MSG(expPktIdBackup == tmcConArray[entry].expPktId, "sendPendingPkts(entry) did change expPktId");

        // larger than expected, add to list
        uint8_t *data = (uint8_t*)malloc(hdr.pktSize);
        memcpy(data, rxBuffer+sizeof(sfsHdr_t), hdr.pktSize);
        tmcConArray[entry].pendingPkts.push_back({hdr,
            bondRxLinkType, // link type of bond, for statistics only
            getTime64(),                // for statistics only
            data});
        NS_LOG_INFO("pktId " << hdr.pktId  << " too large, added to list");
        return true;
    }

    NS_ASSERT(hdr.pktId == tmcConArray[entry].expPktId);
    NS_LOG_INFO("pktId " << hdr.pktId << " == expPktId " << tmcConArray[entry].expPktId);

    uint64_t tsCurrent = getTime64();
    tmcConArray[entry].pktHistB2H.push_back({hdr.pktId,
        hdr.pktSize,
        bondRxLinkType, // link type of bond
        tsCurrent,
        tsCurrent});

    // - a CLOSE might also be stuck in the pendingPkts list, see below...
    // - every socket will receive its own CLOSE
    if(hdr.ctrl == TMC_CTRL_FLOW_CLOSE)
    {
        // In this simulation, we assume that only the left side actively closes
        // connections. In a real implementation, *both* sides might close a
        // socket simultaneously
        NS_ASSERT(dynamic_cast<TmcAppRight*>(this));
        NS_ASSERT(hdr.pktId == tmcConArray[entry].expPktId);

        NS_LOG_INFO("TMC_CTRL_FLOW_CLOSE tmcConArray[" << entry << "]");

        logConnectionCsvH2B(entry);

        tmcConArray[entry].status = TMC_STATUS_UNUSED;

        tmcConArray[entry].sk->SetRecvCallback(MakeNullCallback<void, Ptr<Socket> > ());
        int status = tmcConArray[entry].sk->Close();
        NS_ASSERT(status == 0);
        tmcConArray[entry].sk = 0;

        NS_ASSERT(tmcConArray[entry].rxQueue.empty());
        NS_ASSERT(tmcConArray[entry].pendBytes == 0);
        tmcConArray[entry].potentialLink = LINKTYPE_UNDEFINED;
        NS_ASSERT(tmcConArray[entry].pendingPkts.empty());
        tmcConArray[entry].pktHistH2B.clear();
        tmcConArray[entry].pktHistB2H.clear();

        printTmcConArray();

        return true;
    }

    // upon the next recv(), we expect the next PktId
    tmcConArray[entry].expPktId++;

    // connection already known
    int ret = tmcConArray[entry].sk->Send(rxBuffer+sizeof(sfsHdr_t), hdr.pktSize, 0);
    NS_ASSERT(ret == hdr.pktSize);

    NS_LOG_INFO("recvFromBond() did sent packet to host");
    printSfsHdr(&hdr);

    sendPendingPkts(entry);

    // there might be an buffered CLOSE
    if(tmcConArray[entry].pendingPkts.size() == 1
            && tmcConArray[entry].pendingPkts.front().hdr.ctrl == TMC_CTRL_FLOW_CLOSE
            && tmcConArray[entry].pendingPkts.front().hdr.pktId == tmcConArray[entry].expPktId)
    {
        NS_ASSERT_MSG(false, "TMC_CTRL_FLOW_CLOSE (buffered) should not happen in simulation");
        // compare TMC_CTRL_FLOW_CLOSE from above
    }
    return true;
}


// Algorithm 1: TMC algorithm for heterogeneous links
// Iterating over tmcConArray that often is probably a real performance killer
void TmcApp::checkQueues()
{
    NS_LOG_FUNCTION(m_devTer << m_devSat << m_linkUsedTer << m_linkUsedSat);

    int i = 0;
    bool status;
    uint64_t totalPendingBytes = 0;

    if(    (m_linkUsedTer == true && m_linkUsedSat == true)
        || (m_devSat == 0 && m_linkUsedTer == true)  // only ter is allowed, but already in use
        || (m_devTer == 0 && m_linkUsedSat == true)) // only sat is allowed, but already in use
    {
        NS_LOG_DEBUG("no link available");
        return;
    }

    // calculate queue sizes
    for(i = 0; i < TMC_CONNS; i++)
    {
        if(tmcConArray[i].status == TMC_STATUS_UNUSED)
        {
            continue;
        }

        if(m_devTer != 0)
        {
            // only ter is allowed, potentialLink must be ter
            tmcConArray[i].potentialLink = LINKTYPE_TER;
        }
        else if(m_devSat != 0)
        {
            // only sat is allowed, potentialLink must be sat
            tmcConArray[i].potentialLink = LINKTYPE_SAT;
        }

        tmcConArray[i].pendBytes = 0;

        for(std::list<sfsPkt_t>::iterator entryIter = tmcConArray[i].rxQueue.begin();
                entryIter != tmcConArray[i].rxQueue.end();
                entryIter++)
        {
            tmcConArray[i].pendBytes += entryIter->hdr.pktSize;
            totalPendingBytes += entryIter->hdr.pktSize;
        }

        NS_LOG_INFO("tmcConArray[" << i << "].rxQueue has " << tmcConArray[i].pendBytes << " pendBytes");
    }

    // if both links are available, put suitable ones on satellite link, except very small ones
    if(m_devTer != 0 && m_devSat != 0)
    {
        NS_ASSERT(m_thresSmallFlow != 0 && m_thresTerSat != 0);

        for(i = 0; i < TMC_CONNS; i++)
        {
            if(tmcConArray[i].status == TMC_STATUS_UNUSED)
            {
                continue;
            }

            if(tmcConArray[i].pendBytes < m_thresSmallFlow || totalPendingBytes < m_thresTerSat)
            {
                tmcConArray[i].potentialLink = LINKTYPE_TER;
            }
            else
            {
                tmcConArray[i].potentialLink = LINKTYPE_SAT;
            }
        }
    }

    uint64_t lastUsedTs = UINT64_MAX;
    int      curEntry   = TMC_CONNS;

    // if ter is allowed and unused, always send a packet on it
    if(m_devTer != 0 && m_linkUsedTer == false)
    {
        lastUsedTs = UINT64_MAX;
        curEntry   = TMC_CONNS;

        //LINKTYPE_TER has priority
        for(i = 0; i < TMC_CONNS; i++)
        {
            if(tmcConArray[i].status != TMC_STATUS_UNUSED
                    && !tmcConArray[i].rxQueue.empty()
                    && tmcConArray[i].potentialLink == LINKTYPE_TER)
            {
                if(tmcConArray[i].rxQueue.back().tsRx < lastUsedTs)
                {
                    // we found a queue with was not served for longer time
                    lastUsedTs = tmcConArray[i].rxQueue.back().tsRx;
                    curEntry   = i;
                }
            }

        }

        //no packet for LINKTYPE_TER found, maybe there is a LINKTYPE_SAT which we take instead
        if(curEntry == TMC_CONNS)
        {
            for(i = 0; i < TMC_CONNS; i++)
            {
                if(tmcConArray[i].status != TMC_STATUS_UNUSED
                        && !tmcConArray[i].rxQueue.empty()
                        /* tmcConArray[i].potentialLink does not matter */)
                {
                    if(tmcConArray[i].rxQueue.back().tsRx < lastUsedTs)
                    {
                        // we found a queue with was not served for longer time
                        lastUsedTs = tmcConArray[i].rxQueue.back().tsRx;
                        curEntry   = i;
                    }
                }

            }
        }


        if(curEntry == TMC_CONNS)
        {
            NS_ASSERT(lastUsedTs == UINT64_MAX);
            NS_LOG_INFO("Terrestrial link is unused and there is no suitable packet for it");

            // no need to try the sat link
            return;
        }

        //send on ter
        NS_ASSERT(!tmcConArray[curEntry].rxQueue.empty());

        // recvFromHost does tmcConArray[sock].rxQueue.push_back(), i.e. new packets are added to the back
        sfsPkt_t pkt = tmcConArray[curEntry].rxQueue.front();
        tmcConArray[curEntry].rxQueue.pop_front();

        NS_LOG_INFO("Sending packet from queue tmcConArray[" << curEntry << "] via ter");
        printSfsHdr(&pkt.hdr);
        tmcConArray[curEntry].pktHistH2B.push_back({pkt.hdr.pktId,
            pkt.hdr.pktSize,
            LINKTYPE_TER,
            pkt.tsRx,
            getTime64()});

        uint8_t txBuffer[2*BUF_SIZE];
        memcpy(txBuffer, &pkt.hdr, sizeof(sfsHdr_t));
        memcpy(txBuffer+sizeof(sfsHdr_t), pkt.data, pkt.hdr.pktSize);
        Ptr<Packet> packet = Create<Packet> (txBuffer, sizeof(sfsHdr_t)+pkt.hdr.pktSize);
        Address address;
        status = m_devTer->Send(packet, address, 0x0800 /*IPv4*/);
        NS_ASSERT(status == true);
        m_linkUsedTer = true;

        // little bit of a hack :-/
        // see also below for sat
        if(pkt.hdr.ctrl == TMC_CTRL_FLOW_CLOSE)
        {
            NS_LOG_INFO("Sent TMC_CTRL_FLOW_CLOSE via ter, setting TMC_STATUS_UNUSED for tmcConArrayEntry " << curEntry);
            tmcConArray[curEntry].status = TMC_STATUS_UNUSED;
        }
    }


    // next try the sat link
    if(m_devSat != 0 && m_linkUsedSat == false)
    {
        lastUsedTs = UINT64_MAX;
        curEntry   = TMC_CONNS;

        for(i = 0; i < TMC_CONNS; i++)
        {
            if(    tmcConArray[i].status != TMC_STATUS_UNUSED
                    && !tmcConArray[i].rxQueue.empty()
                    && tmcConArray[i].potentialLink == LINKTYPE_SAT)
            {
                // may it happen, that a packet is penalized upon switch ter<->sat?
                if(tmcConArray[i].rxQueue.back().tsRx < lastUsedTs)
                {
                    // we found a queue with was not served for longer time
                    lastUsedTs = tmcConArray[i].rxQueue.back().tsRx;
                    curEntry   = i;
                }
            }
        }

        if(curEntry == TMC_CONNS)
        {
            NS_ASSERT(lastUsedTs == UINT64_MAX);
            NS_LOG_INFO("Satellite link is unused and there is no suitable packet for it");
            return;
        }

        //send on sat
        NS_ASSERT(!tmcConArray[curEntry].rxQueue.empty());

        // recvFromHost does tmcConArray[sock].rxQueue.push_back(), i.e. new packets are added to the back
        sfsPkt_t pkt = tmcConArray[curEntry].rxQueue.front();
        tmcConArray[curEntry].rxQueue.pop_front();

        NS_LOG_INFO("Sending packet from queue tmcConArray[" << curEntry << "] via sat");
        printSfsHdr(&pkt.hdr);
        tmcConArray[curEntry].pktHistH2B.push_back({pkt.hdr.pktId,
            pkt.hdr.pktSize,
            LINKTYPE_SAT,
            pkt.tsRx,
            getTime64()});

        uint8_t txBuffer[2*BUF_SIZE];
        memcpy(txBuffer, &pkt.hdr, sizeof(sfsHdr_t));
        memcpy(txBuffer+sizeof(sfsHdr_t), pkt.data, pkt.hdr.pktSize);
        Ptr<Packet> packet = Create<Packet> (txBuffer, sizeof(sfsHdr_t)+pkt.hdr.pktSize);
        Address address;
        status = m_devSat->Send(packet, address, 0x0800 /*IPv4*/);
        NS_ASSERT(status == true);
        m_linkUsedSat = true;

        // little bit of a hack :-/
        // see also above for ter
        if(pkt.hdr.ctrl == TMC_CTRL_FLOW_CLOSE)
        {
            NS_LOG_INFO("Sent TMC_CTRL_FLOW_CLOSE via sat, setting TMC_STATUS_UNUSED for tmcConArrayEntry " << curEntry);
            tmcConArray[curEntry].status = TMC_STATUS_UNUSED;
        }
    }
}


// send pending packets, which have been delayed by any link
void TmcApp::sendPendingPkts(uint32_t entry)
{
    int ret;

    uint32_t resendCtr = 0;
    uint64_t prevPktId = 0;

    // check if list contains missing packets
    for(std::list<sfsPkt_t>::iterator pktIter = tmcConArray[entry].pendingPkts.begin();
            pktIter != tmcConArray[entry].pendingPkts.end();
            /*pktIter++*/)
    {
        // pending packets should be monotonously increasing
        // sanity check for development purpose
        NS_ASSERT(pktIter->hdr.pktId > prevPktId);
        prevPktId = pktIter->hdr.pktId;

        if(    pktIter->hdr.pktId == tmcConArray[entry].expPktId
            && pktIter->hdr.ctrl != TMC_CTRL_FLOW_CLOSE) // FLOW_CLOSE will be handled later
        {
            tmcConArray[entry].pktHistB2H.push_back({pktIter->hdr.pktId,
                pktIter->hdr.pktSize,
                pktIter->usedLinkType,
                pktIter->tsRx,
                getTime64()});

            // send to host
            NS_LOG_INFO("Trying to send " << pktIter->hdr.pktSize << " bytes, socket has available " << tmcConArray[entry].sk->GetTxAvailable());
            ret = tmcConArray[entry].sk->Send(pktIter->data, pktIter->hdr.pktSize, 0);
            NS_ASSERT(ret == pktIter->hdr.pktSize);

            resendCtr++;
            NS_LOG_INFO("Did send pendingPkt, resendCtr " << resendCtr);
            printSfsHdr(&pktIter->hdr);

            free(pktIter->data);
            pktIter = tmcConArray[entry].pendingPkts.erase(pktIter);

            tmcConArray[entry].expPktId++;
        }
        else
        {
            // just to finish the loop
            // there shouldn't be a wrongly ordered packet which is checked by assertion above
            pktIter++;
        }
    }
}


void TmcApp::logConnectionCsvH2B(uint32_t entry)
{
    NS_LOG_FUNCTION(this);

    NS_ASSERT(tmcConArray[entry].status == TMC_STATUS_USED);

    uint32_t totalTer = 0;
    uint32_t totalSat = 0;
    uint32_t totalPending = 0;

    m_ofStat << entry << "," << Ipv4Address(tmcConArray[entry].srcIp)
                                  << "," << Ipv4Address(tmcConArray[entry].dstIp)
                                  << "," << tmcConArray[entry].srcPort
                                  << "," << tmcConArray[entry].dstPort << ",";

    //ter and sat
    for(std::list<pktHistHost2Bond_t>::iterator pktIter = tmcConArray[entry].pktHistH2B.begin();
            pktIter != tmcConArray[entry].pktHistH2B.end();
            pktIter++)
    {
        if(pktIter->usedLinkType == LINKTYPE_TER)
        {
            totalTer += pktIter->pktSize;
        }
        else if(pktIter->usedLinkType == LINKTYPE_SAT)
        {
            totalSat += pktIter->pktSize;
        }
        else
        {
            NS_ASSERT_MSG(false, "Neither LINKTYPE_TER nor LINKTYPE_SAT");
        }
    }
    m_ofStat << totalTer << "," << totalSat << "," << totalTer+totalSat << ",";

    //pending packets
    for(std::list<sfsPkt_t>::iterator pktIter = tmcConArray[entry].pendingPkts.begin();
            pktIter != tmcConArray[entry].pendingPkts.end();
            pktIter++)
    {
        totalPending += pktIter->hdr.pktSize;
    }
    m_ofStat << totalPending << std::endl;
}


void
TmcApp::DoDispose (void)
{
    NS_LOG_FUNCTION (this);
    m_ofStat.close();
    Application::DoDispose ();
}


void TmcApp::ConnectionFailed (Ptr<Socket> socket)
{
    NS_LOG_FUNCTION (this << socket);
    NS_ASSERT_MSG(false, "ConnectionFailed()");
}


void TmcApp::ErrorCloseCallback (Ptr<Socket> socket)
{
    NS_LOG_FUNCTION (this << socket);
    NS_ASSERT_MSG(false, "ErrorCloseCallback()");
}






TmcAppLeft::TmcAppLeft ()
{
    NS_ASSERT_MSG(false, "Default constructor not implemented");
}

TmcAppLeft::TmcAppLeft (std::string logFilename)
: m_socketTproxy (0)
{
    // not needed for MMB paper
//    m_ofStat.open(logFilename);
//    m_ofStat << "connId,srcIp,dstIp,srcPort,dstPort,sizeTer,sizeSat,sizeTotal,sizePending" << std::endl;
}

TmcAppLeft::~TmcAppLeft()
{
    m_socketTproxy = 0;
}


void TmcAppLeft::StartApplication (void)
{
    NS_LOG_INFO("TmcAppLeft now started!");

    NS_LOG_FUNCTION (this);
    // Create the socket if not already
    if (!m_socketTproxy)
    {
        m_socketTproxy = Socket::CreateSocket (GetNode (), TcpSocketFactory::GetTypeId ());
        if (m_socketTproxy->Bind (InetSocketAddress (Ipv4Address::GetAny (), TMC_TPROXY_PORT)) == -1) //m_local
        {
            NS_FATAL_ERROR ("Failed to bind socket");
        }
        m_socketTproxy->Listen ();
    }

    m_socketTproxy->SetRecvCallback (MakeCallback (&TmcAppLeft::HandleRead, this));
    m_socketTproxy->SetAcceptCallback (
            MakeNullCallback<bool, Ptr<Socket>, const Address &> (),
            MakeCallback (&TmcAppLeft::HandleAccept, this));
}


// Identical to TmcAppRight::HandleRead, except GetPort () Local/Remote
void TmcAppLeft::HandleRead (Ptr<Socket> socket)
{
    NS_LOG_FUNCTION (this << socket);
    Ptr<Packet> packet;
    Address sockLocal, sockRemote;

    socket->GetSockName(sockLocal);
    socket->GetPeerName(sockRemote);
    NS_LOG_INFO("sockLocal  ip " << InetSocketAddress::ConvertFrom (sockLocal).GetIpv4 ()  << "   port " << InetSocketAddress::ConvertFrom (sockLocal).GetPort ());
    NS_LOG_INFO("sockRemote ip " << InetSocketAddress::ConvertFrom (sockRemote).GetIpv4 () << "   port " << InetSocketAddress::ConvertFrom (sockRemote).GetPort ());

    if(InetSocketAddress::ConvertFrom (sockLocal).GetPort () == TMC_TPROXY_PORT)
    {
        NS_LOG_INFO("TmcAppLeft Received from host (port " << TMC_TPROXY_PORT << ")");
        int tmcConArrayEntry = findTmcConArrayEntry(socket);
        this->recvFromHost(socket, tmcConArrayEntry);
    }
    else
    {
        NS_ASSERT_MSG(false, "Received packet from unexpected port");
    }
}


// On the left side, we see a HandleAccept for every new connection
void TmcAppLeft::HandleAccept (Ptr<Socket> s, const Address& from)
{
    NS_LOG_FUNCTION (this << s << from);

    Address sockLocal;
    s->GetSockName(sockLocal);

    uint32_t newSrcIp = InetSocketAddress::ConvertFrom(from).GetIpv4 ().Get ();
    uint32_t newDstIp = InetSocketAddress::ConvertFrom(sockLocal).GetIpv4 ().Get ();
    uint32_t newSrcPort = InetSocketAddress::ConvertFrom(from).GetPort ();
    uint32_t newDstPort = InetSocketAddress::ConvertFrom(sockLocal).GetPort ();

    int tmcConEntry = allocateTmcCon(s, newSrcIp, newDstIp, newSrcPort, newDstPort);

    sfsHdr_t hdr;
    hdr.srcIp = newSrcIp;
    hdr.dstIp = newDstIp;
    hdr.srcPort = newSrcPort;
    hdr.dstPort = newDstPort;
    hdr.ctrl    = TMC_CTRL_FLOW_INIT;
    hdr.pktId   = 0;
    hdr.pktSize = 0;

    NS_LOG_INFO("Receivd a new connection and created tmcConEntry " << tmcConEntry);
    printSfsHdr(&hdr);

    tmcConArray[tmcConEntry].rxQueue.push_back(
       {hdr,
        LINKTYPE_UNDEFINED, // received from host, linktype not defined yet, is done by checkQueues()
        getTime64(), // not used, tsBondRx does not make sense, could be the rx timestamp from the host
        0});

    checkQueues();

    s->SetRecvCallback (MakeCallback (&TmcAppLeft::HandleRead, this));
    s->SetCloseCallbacks (MakeCallback (&TmcAppLeft::NormalCloseCallback, this),
                          MakeCallback (&TmcAppLeft::ErrorCloseCallback, this));
}

void TmcAppLeft::ConnectionSucceeded (Ptr<Socket> socket)
{
    NS_LOG_FUNCTION (this << socket);
    NS_LOG_LOGIC ("TmcLgwApp Connection succeeded");
    NS_ASSERT(false);

    Address sockLocal, sockRemote;
    socket->GetSockName(sockLocal);
    socket->GetPeerName(sockRemote);

    // In the Lgw, we see a ConnectionSucceeded twice
    NS_LOG_LOGIC ("TmcAppLeft ConnectionSucceeded() new connection (should be ter or sat) "
            << InetSocketAddress::ConvertFrom(sockLocal).GetIpv4 () << ":"
            << InetSocketAddress::ConvertFrom(sockLocal).GetPort () << " -> "
            << InetSocketAddress::ConvertFrom(sockRemote).GetIpv4 () << ":"
            << InetSocketAddress::ConvertFrom(sockRemote).GetPort ());
}


void TmcAppLeft::NormalCloseCallback (Ptr<Socket> socket)
{
    NS_LOG_FUNCTION (this << socket);

    if (socket->GetErrno () != Socket::ERROR_NOTERROR)
    {
        NS_LOG_ERROR (this << " Connection has been terminated,"
                << " error code: " << socket->GetErrno () << ".");
    }

    int entry = findTmcConArrayEntry(socket);

    sfsHdr_t hdr;
    hdr.srcIp   = tmcConArray[entry].srcIp;
    hdr.dstIp   = tmcConArray[entry].dstIp;
    hdr.srcPort = tmcConArray[entry].srcPort;
    hdr.dstPort = tmcConArray[entry].dstPort;
    hdr.pktSize = 0;
    hdr.ctrl    = TMC_CTRL_FLOW_CLOSE;
    hdr.pktId   = ++tmcConArray[entry].curPktId;

    tmcConArray[entry].rxQueue.push_back({hdr,
        LINKTYPE_UNDEFINED, // received from host, linktype not defined yet, is done by checkQueues()
        getTime64(),
        0});

    NS_LOG_INFO("TmcAppLeft puts TMC_CTRL_FLOW_CLOSE for sock " << entry << " into rxQueue. hdr.pktId " << hdr.pktId);

    checkQueues();
}











TmcAppRight::TmcAppRight ()
{
    NS_ASSERT_MSG(false, "Default constructor not implemented");
}

TmcAppRight::TmcAppRight (std::string logFilename)
{
    m_ofStat.open(logFilename);
    m_ofStat << "connId,srcIp,dstIp,srcPort,dstPort,sizeTer,sizeSat,sizeTotal,sizePending" << std::endl;
}

TmcAppRight::~TmcAppRight()
{
}


void TmcAppRight::StartApplication (void)
{
    NS_LOG_INFO("TmcAppRight now started!");

    NS_LOG_FUNCTION (this);
}

// Identical to TmcAppLeft::HandleRead, except GetPort () Local/Remote
void TmcAppRight::HandleRead (Ptr<Socket> socket)
{
    NS_LOG_FUNCTION (this << socket);
    Ptr<Packet> packet;
    Address sockLocal, sockRemote;

    socket->GetSockName(sockLocal);
    socket->GetPeerName(sockRemote);
    NS_LOG_INFO("sockLocal  ip " << InetSocketAddress::ConvertFrom (sockLocal).GetIpv4 ()  << "   port " << InetSocketAddress::ConvertFrom (sockLocal).GetPort ());
    NS_LOG_INFO("sockRemote ip " << InetSocketAddress::ConvertFrom (sockRemote).GetIpv4 () << "   port " << InetSocketAddress::ConvertFrom (sockRemote).GetPort ());

    if(InetSocketAddress::ConvertFrom (sockRemote).GetPort () == TMC_TPROXY_PORT)
    {
        // RecvFromHost
        NS_LOG_INFO("TmcAppRight Received from host (port " << TMC_TPROXY_PORT << ")");
        int tmcConArrayEntry = findTmcConArrayEntry(socket);
        this->recvFromHost(socket, tmcConArrayEntry);
    }
    else
    {
        NS_ASSERT_MSG(false, "Received packet from unexpected port");
    }
}


void TmcAppRight::ConnectionSucceeded (Ptr<Socket> socket)
{
    NS_LOG_FUNCTION (this << socket);
    NS_ASSERT(false);

    Address sockLocal, sockRemote;
    socket->GetSockName(sockLocal);
    socket->GetPeerName(sockRemote);

    // in the Rgw, we see a ConnectionSucceeded for every new connection
    NS_LOG_LOGIC ("TmcRgwApp ConnectionSucceeded() new connection "
            << InetSocketAddress::ConvertFrom(sockLocal).GetIpv4 () << ":"
            << InetSocketAddress::ConvertFrom(sockLocal).GetPort () << " -> "
            << InetSocketAddress::ConvertFrom(sockRemote).GetIpv4 () << ":"
            << InetSocketAddress::ConvertFrom(sockRemote).GetPort ());
}


void TmcAppRight::NormalCloseCallback (Ptr<Socket> socket)
{
    NS_LOG_FUNCTION (this << socket);

    if (socket->GetErrno () != Socket::ERROR_NOTERROR)
    {
        NS_LOG_ERROR (this << " Connection has been terminated,"
                << " error code: " << socket->GetErrno () << ".");
    }
}








} // namespace ns3

