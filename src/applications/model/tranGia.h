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


#ifndef TRANGIA_H
#define TRANGIA_H

#include "ns3/application.h"
#include "ns3/event-id.h"
#include "ns3/ptr.h"
#include "ns3/ipv4-address.h"
#include "ns3/random-variable-stream.h"
#include <fstream>

namespace ns3 {

class Socket;
class Packet;


#define TG_HDRTCP_DUMMY   35
#define TG_HDRTLS_DUMMY  995
#define TG_HDRHTTP_DUMMY 345  // ns-3 3gpp request is 350 bytes = sizeof(http_Header_t)+345
#define TG_PARALLEL_FLOWS 8
#define TG_MAXBUFSIZE 10000


typedef enum {
    TGM_SEQ,
    TGM_PARALLEL,
    TGM_HTTP2
} tranGiaMode_e;

typedef enum {
    TGS_UNDEFINED,
    TGS_TCPCONNECT,
    TGS_CONNECT1,
    TGS_CONNECT2,
    TGS_MAINOBJECT,
    TGS_EMBOBJECT,
} tranGiaState_e;


typedef struct {
    uint8_t type;
    uint32_t payloadSize;// C->S size of object to be requested, S->C size of payload
} __attribute__((packed)) commonHeader_t;

typedef struct {
    commonHeader_t hdr;
    uint8_t dummy[TG_HDRTCP_DUMMY];
} __attribute__((packed)) tcpHeader_t;

typedef struct {
    commonHeader_t hdr;
    uint8_t dummy[TG_HDRTLS_DUMMY];
} __attribute__((packed)) tlsHeader_t;

typedef struct {
    commonHeader_t hdr;
    uint8_t dummy[TG_HDRHTTP_DUMMY];
} __attribute__((packed)) httpHeader_t;

typedef struct {
    Ptr<Socket> sock;       // Client and Server
    tranGiaState_e state;   // Client only
    bool pendingObject;     // Client only
    uint32_t pendingBytes;  // Client (receiving) and Server (sending)
} tranGiaFlow_t;





// This workload model (client and webserver) is based on:
//   Pries, R., Magyari, Z., Tran-Gia, P.: An HTTP web traffic model based
//   on the top one million visited web pages. NGI 2012. June 2012.
//   https://doi.org/10.1109/NGI.2012.6252145
class TranGia : public Application
{
public:
    void ErrorCloseCallback (Ptr<Socket> socket);

protected:
    tranGiaFlow_t flowArray[TG_PARALLEL_FLOWS];

    void InitFlowArray();
    int GetSocketIdx(Ptr<Socket> socket);

    //must be set during construction
    //changing the mode during runtime is not supported yet
    tranGiaMode_e  m_mode;

private:
    virtual void NormalCloseCallback (Ptr<Socket> socket) = 0;
};





// The client is started with StartApplication(), then one website is requested and received
// The next website can be requested again with StartApplication()
class TranGiaClient : public TranGia
{
public:
  static TypeId GetTypeId (void);

  TranGiaClient ();
  TranGiaClient (tranGiaMode_e tranGiaMode, Ipv4Address destIp, uint16_t destPort, std::string logFilename);

  virtual ~TranGiaClient ();

  Ptr<WeibullRandomVariable> m_sizeMainObjectsWeibull;
  Ptr<LogNormalRandomVariable> m_nrMainObjectsLogNormal;

  Ptr<LogNormalRandomVariable> m_sizeEmbObjectsLognormal;
  Ptr<ExponentialRandomVariable> m_nrEmbObjectsExp;

  uint32_t m_nrTotalObjects;
  uint32_t m_nrRequestedObjects;
  uint32_t m_nrReceivedObjects;

  virtual void StartApplication (void);
  virtual void StopApplication (void);

protected:
  virtual void DoDispose (void);

private:
  void NormalCloseCallback (Ptr<Socket> socket);
  void ConnectionSucceededCallback (Ptr<Socket> socket);
  void ReceivedDataCallback (Ptr<Socket> socket);

  Ipv4Address m_destIp;
  uint16_t m_destPort;

  std::list<uint32_t> objectSizes; // first entry is the size of the main object

  std::ofstream m_ofStat;
};





class TranGiaServer : public TranGia
{
public:
  static TypeId GetTypeId (void);

  TranGiaServer ();
  TranGiaServer (tranGiaMode_e tranGiaMode);

  virtual ~TranGiaServer ();

  virtual void StartApplication (void);
  virtual void StopApplication (void);

protected:
  virtual void DoDispose (void);

private:
  void NormalCloseCallback (Ptr<Socket> socket);
  void NewConnectionCreatedCallback (Ptr<Socket> socket, const Address &address);
  void ReceivedDataCallback (Ptr<Socket> socket);
  void SendCallback (Ptr<Socket> socket, uint32_t availableBufferSize);

  Ptr<Socket> m_listenSocket;
};


} // namespace ns3

#endif /* TRANGIA_H */
