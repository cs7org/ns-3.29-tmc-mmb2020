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

#ifndef TMCCOMMON_H_
#define TMCCOMMON_H_

#include <netinet/in.h>
#include <assert.h>

#include "ns3/core-module.h"
#include "ns3/socket.h"
#include "ns3/tcp-socket-factory.h"
#include "ns3/application.h"
#include "ns3/data-rate.h"


#define TMC_TPROXY_PORT  80
#define TMC_CONNS 10 //max. number of flows handled by tmcPep
#define BUF_SIZE 1448


namespace ns3 {


//
// Typedefs
//
typedef enum
{
    DEBUG_LEVEL_SILENT = 0,
    DEBUG_LEVEL_ERR    = 1,
    DEBUG_LEVEL_WARN   = 2,
    DEBUG_LEVEL_INFO   = 3,
} debugLevel_e;

typedef enum {
    TMC_CTRL_FLOW_REGULAR = 0,
    TMC_CTRL_FLOW_INIT = 1,
    TMC_CTRL_FLOW_CLOSE = 2,
} flowCtrl_e;

typedef enum {
    TMC_STATUS_UNUSED = 0,
    TMC_STATUS_USED = 1,
} tmcStatus_e;

typedef enum
{
    LINKTYPE_UNDEFINED,
    LINKTYPE_TER,
    LINKTYPE_SAT,
} linkType_e;

typedef struct {
    uint32_t srcIp;
    uint32_t dstIp;
    uint16_t srcPort;
    uint16_t dstPort;
    uint16_t pktSize;
    uint16_t ctrl;
    uint64_t pktId;
} __attribute__((packed)) sfsHdr_t;

typedef struct {
    sfsHdr_t hdr;
    linkType_e usedLinkType; // for statistics only, needed for later pktHist
    uint64_t tsRx;           // txHostRx (used for RR scheduling) or txBondRx (pktHist for statistics)
    uint8_t *data;
} sfsPkt_t;

typedef struct
{
    uint64_t pktId;
    uint16_t pktSize;
    linkType_e usedLinkType;
    uint64_t tsHostRx;
    uint64_t tsBondTx;
} pktHistHost2Bond_t;

typedef struct
{
    uint64_t pktId;
    uint16_t pktSize;
    linkType_e usedLinkType;
    uint64_t tsBondRx;
    uint64_t tsHostTx;
} pktHistBond2Host_t;

typedef struct {
    tmcStatus_e status;

    Ptr<Socket> sk;
    std::list<sfsPkt_t> rxQueue;
    uint32_t pendBytes;       // only used in checkQueues()
    linkType_e potentialLink; // only used in checkQueues()

    uint32_t srcIp;
    uint32_t dstIp;
    uint16_t srcPort;
    uint16_t dstPort;
    uint64_t curPktId; // has been a stack-local variable in linux, which does not work in ns-3
    uint64_t expPktId;
    std::list<sfsPkt_t> pendingPkts;

    std::list<pktHistHost2Bond_t> pktHistH2B; //pktId, pktSize, linkType, tsHostRx, tsBondTx
    std::list<pktHistBond2Host_t> pktHistB2H; //pktId, pktSize, linkType, tsBondRx, tsHostTx
} tmcCon_t;



//return the time in millisec
static inline uint64_t getTime64()
{
    return Simulator::Now().GetMilliSeconds();
}

static inline uint64_t calcThresTerSat(std::string rDsl, std::string dDsl, std::string rSat, std::string dSat)
{
    double delayTerSat = Time(dSat).GetSeconds() - Time(dDsl).GetSeconds();
    double ratesTerSat = (8/(double)DataRate(rDsl).GetBitRate()) - (8/(double)DataRate(rSat).GetBitRate());
    double thresTerSat = delayTerSat / ratesTerSat;

    NS_LOG_UNCOND("rDsl: " << DataRate(rDsl).GetBitRate());
    NS_LOG_UNCOND("dDsl: " << Time(dDsl).GetSeconds());
    NS_LOG_UNCOND("rSat: " << DataRate(rSat).GetBitRate());
    NS_LOG_UNCOND("dSat: " << Time(dSat).GetSeconds());
    NS_LOG_UNCOND("thresTerSat: " << (uint64_t)thresTerSat);
    return (uint64_t)thresTerSat;
}



//
// ns-3 classes
//
class TmcApp : public Application
{
public:
    TmcApp ();
    virtual ~TmcApp();

    void printTmcAppCallbackDev(Ptr<NetDevice> dev, std::string prefix);
    void printSfsHdr(sfsHdr_t* sfsHdr);
    void printTmcConArray();
    int findTmcConArrayEntry(Ptr<Socket> socket);
    int allocateTmcCon(Ptr<Socket> socket, uint32_t srcIp, uint32_t dstIp, uint16_t srcPort, uint16_t dstPort);

    void recvFromHost(Ptr<Socket> socket, int sock);
    void sentToBond (Ptr<NetDevice> dev);
    bool recvFromBond (Ptr<NetDevice>, Ptr<const Packet>, uint16_t, const Address &);

    virtual void HandleRead (Ptr<Socket> socket) = 0;

    void checkQueues();
    void sendPendingPkts(uint32_t entry);
    void logConnectionCsvH2B(uint32_t entry);

    tmcCon_t tmcConArray[TMC_CONNS];

    Ptr<NetDevice> m_devTer;
    Ptr<NetDevice> m_devSat;

    std::ofstream m_ofStat;

    uint64_t m_thresSmallFlow;
    uint64_t m_thresTerSat;

protected:
    virtual void DoDispose (void);

    void ConnectionFailed (Ptr<Socket> socket);
    void ErrorCloseCallback (Ptr<Socket> socket);

    // Stuff for bond
    bool m_linkUsedTer;
    bool m_linkUsedSat;

private:
    virtual void NormalCloseCallback (Ptr<Socket> socket) = 0;
};


class TmcAppLeft : public TmcApp
{
public:
  TmcAppLeft ();
  TmcAppLeft (std::string logFilename);
  virtual ~TmcAppLeft();

  void HandleRead (Ptr<Socket> socket);

private:
  virtual void StartApplication (void);

  void HandleAccept (Ptr<Socket> socket, const Address& from);

  void ConnectionSucceeded (Ptr<Socket> socket);
  void NormalCloseCallback (Ptr<Socket> socket);

  // Stuff for host
  // In the case of TCP, each socket accept returns a new socket, so the
  // listening socket is stored separately from the accepted sockets
  Ptr<Socket> m_socketTproxy; //listening socket
};


class TmcAppRight : public TmcApp
{
public:
  TmcAppRight ();
  TmcAppRight (std::string logFilename);
  virtual ~TmcAppRight ();

  void HandleRead (Ptr<Socket> socket);

private:
  virtual void StartApplication (void);

  void ConnectionSucceeded (Ptr<Socket> socket);
  void NormalCloseCallback (Ptr<Socket> socket);
};



  } //namespace ns-3




#endif /* TMCCOMMON_H_ */
