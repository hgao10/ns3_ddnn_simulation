/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright 2007 University of Washington
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
 * Author: Simon
 * Adapted from PacketSink by:
 * Author:  Tom Henderson (tomhend@u.washington.edu)
 */
#include "ns3/address.h"
#include "ns3/address-utils.h"
#include "ns3/log.h"
#include "ns3/inet-socket-address.h"
#include "ns3/inet6-socket-address.h"
#include "ns3/node.h"
#include "ns3/socket.h"
#include "ns3/udp-socket.h"
#include "ns3/simulator.h"
#include "ns3/socket-factory.h"
#include "ns3/packet.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/udp-socket-factory.h"
#include "flow-sink.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("FlowSink");

NS_OBJECT_ENSURE_REGISTERED (FlowSink);

TypeId
FlowSink::GetTypeId(void) {
    static TypeId tid = TypeId("ns3::FlowSink")
            .SetParent<Application>()
            .SetGroupName("Applications")
            .AddConstructor<FlowSink>()
            .AddAttribute("Local",
                          "The Address on which to Bind the rx socket.",
                          AddressValue(),
                          MakeAddressAccessor(&FlowSink::m_local),
                          MakeAddressChecker())
            .AddAttribute("Protocol",
                          "The type id of the protocol to use for the rx socket.",
                          TypeIdValue(UdpSocketFactory::GetTypeId()),
                          MakeTypeIdAccessor(&FlowSink::m_tid),
                          MakeTypeIdChecker());
    return tid;
}

FlowSink::FlowSink() {
    NS_LOG_FUNCTION(this);
    m_socket = 0;
    m_totalRx = 0;
}

FlowSink::~FlowSink() {
    NS_LOG_FUNCTION(this);
}

void FlowSink::DoDispose(void) {
    NS_LOG_FUNCTION(this);
    m_socket = 0;
    m_socketList.clear();

    // chain up
    Application::DoDispose();
}


void FlowSink::StartApplication() { // Called at time specified by Start
    NS_LOG_FUNCTION(this);

    // Create a socket which is always in LISTEN state
    // As soon as it processes a SYN in ProcessListen(),
    // it forks itself into a new socket, which
    // keeps the accept and close callbacks
    if (!m_socket) {
        m_socket = Socket::CreateSocket(GetNode(), m_tid);
        if (m_socket->Bind(m_local) == -1) {
            NS_FATAL_ERROR("Failed to bind socket");
        }
        m_socket->Listen();
        m_socket->ShutdownSend();
        if (addressUtils::IsMulticast(m_local)) {
            throw std::runtime_error("No support for UDP here");
        }
    }

    // Callbacks
    m_socket->SetRecvCallback(MakeCallback(&FlowSink::HandleRead, this));
    m_socket->SetAcceptCallback(
            MakeNullCallback<bool, Ptr<Socket>,const Address &>(),
            MakeCallback(&FlowSink::HandleAccept, this)
    );
    m_socket->SetCloseCallbacks(
            MakeCallback(&FlowSink::HandlePeerClose, this),
            MakeCallback(&FlowSink::HandlePeerError, this)
    );

}

void FlowSink::StopApplication() {  // Called at time specified by Stop
    NS_LOG_FUNCTION(this);
    while (!m_socketList.empty()) {
        Ptr <Socket> socket = m_socketList.front();
        m_socketList.pop_front();
        socket->Close();
    }
    if (m_socket) {
        m_socket->Close();
    }
}

void FlowSink::HandleAccept(Ptr<Socket> socket, const Address &from) {
    NS_LOG_FUNCTION(this << socket << from);
    socket->SetRecvCallback (MakeCallback (&FlowSink::HandleRead, this));
    m_socketList.push_back(socket);
}

void FlowSink::HandleRead(Ptr<Socket> socket) {
    NS_LOG_FUNCTION (this << socket);

    // Immediately from the socket drain all the packets it has received
    Ptr<Packet> packet;
    Address from;
    while ((packet = socket->RecvFrom(from))) {
        if (packet->GetSize() == 0) { // EOFs
            break;
        }
        m_totalRx += packet->GetSize ();

        // Other fields that could be useful here if actually did something:
        // Size: packet->GetSize()
        // Source IP: InetSocketAddress::ConvertFrom(from).GetIpv4 ()
        // Source port: InetSocketAddress::ConvertFrom (from).GetPort ()
        // Our own IP / port: Address localAddress; socket->GetSockName (localAddress);

    }
}

void FlowSink::HandlePeerClose(Ptr<Socket> socket) {
    NS_LOG_FUNCTION(this << socket);
    CleanUp(socket);
}

void FlowSink::HandlePeerError(Ptr<Socket> socket) {
    NS_LOG_FUNCTION(this << socket);
    CleanUp(socket);
}

void FlowSink::CleanUp(Ptr<Socket> socket) {
    NS_LOG_FUNCTION(this << socket);
    bool found = false;
    std::list<Ptr<Socket>>::iterator it;
    for (it = m_socketList.begin(); it != m_socketList.end(); ++it) {
        if (*it == socket) {
            m_socketList.erase(it);
            found = true;
            break;
        }
    }
    if (!found) {
        throw std::runtime_error("When trying to clean up socket, could not find the socket.");
    }
}

} // Namespace ns3
