/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2011 Doreid AMMAR - INRIA RESO team, LIP laboratory
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
 * Author: Sharan Naribole <nsharan@rice.edu>
 * Extended from PPBP Application provided for older ns-3 version at
 * http://perso.ens-lyon.fr/thomas.begin/NS3-PPBP.zip
 */

#include "PPBP-application.h"
#include "ns3/log.h"
#include "ns3/node.h"
#include "ns3/random-variable-stream.h"
#include "ns3/socket.h"
#include "ns3/simulator.h"
#include "ns3/socket-factory.h"
#include "ns3/packet.h"
#include "ns3/uinteger.h"
#include "ns3/trace-source-accessor.h"
// #include "ns3/udp-socket-factory.h"
#include "ns3/tcp-socket-factory.h"
#include "ns3/tcp-socket-base.h"
#include "ns3/tcp-tx-buffer.h"
#include "ns3/double.h"
#include "ns3/pointer.h"
#include <fstream>
#include "ns3/exp-util.h"


using namespace std;

namespace ns3 {

	NS_LOG_COMPONENT_DEFINE ("PPBPApplication");
	
	NS_OBJECT_ENSURE_REGISTERED (PPBPApplication);

	TypeId
	PPBPApplication::GetTypeId (void)
	{
		static TypeId tid = TypeId ("ns3::PPBPApplication")
		.SetParent<Application> ()
		.AddConstructor<PPBPApplication> ()
		.AddAttribute ("BurstIntensity", "The data rate of each burst.",
					   DataRateValue (DataRate ("20Mb/s")),
					   MakeDataRateAccessor (&PPBPApplication::m_cbrRate),
					   MakeDataRateChecker ())
		.AddAttribute ("PacketSize", "The size of packets sent in on state",
					   UintegerValue (1470),
					   MakeUintegerAccessor (&PPBPApplication::m_pktSize),
					   MakeUintegerChecker<uint32_t> (1))
		.AddAttribute ("MeanBurstArrivals", "Mean Active Sources",
					   DoubleValue (5.0),
 					   MakeDoubleAccessor (&PPBPApplication::m_burstArrivals),
					   MakeDoubleChecker<uint32_t> (1))
		.AddAttribute ("MeanBurstTimeLength", "Pareto distributed burst durations",
					   DoubleValue (0.05),
					   MakeDoubleAccessor (&PPBPApplication::m_burstLength),
					   MakeDoubleChecker<double> ())
		.AddAttribute ("H", "Hurst parameter",
					   DoubleValue (0.7),
					   MakeDoubleAccessor (&PPBPApplication::m_h),
					   MakeDoubleChecker<double> ())
		.AddAttribute("Local",
			"The Address on which to Bind the rx socket.",
			AddressValue(),
			MakeAddressAccessor(&PPBPApplication::m_local),
			MakeAddressChecker())
		.AddAttribute ("Remote", "The address of the destination",
					   AddressValue (),
					   MakeAddressAccessor (&PPBPApplication::m_peer),
					   MakeAddressChecker ())
		.AddAttribute ("Protocol", "The type of protocol to use.",
					   TypeIdValue (TcpSocketFactory::GetTypeId ()),
					   MakeTypeIdAccessor (&PPBPApplication::m_protocolTid),
					   MakeTypeIdChecker ())
		.AddTraceSource ("Tx", "A new packet is created and is sent",
						 MakeTraceSourceAccessor (&PPBPApplication::m_txTrace),
						 "ns3::Packet::TracedCallback")
		.AddAttribute ("BaseLogsDir",
						"Base logging directory (flow logging will be placed here, i.e. logs_dir/flow_[flow id]_{progress, cwnd, rtt}.txt",
						StringValue (""),
						MakeStringAccessor (&PPBPApplication::m_baseLogsDir),
						MakeStringChecker ())
		.AddAttribute("Node", "The Node ID of PPBP application",
						UintegerValue(), 
						MakeUintegerAccessor(&PPBPApplication::m_node_id),
						MakeUintegerChecker<uint32_t>())
		;
		return tid;
	}

	PPBPApplication::PPBPApplication ()
	{
		NS_LOG_FUNCTION_NOARGS ();
		m_socket = 0;
		m_connected = false;
		m_lastStartTime = Seconds (0);
		m_totalBytes = 0;
		m_activebursts = 0;
		m_offPeriod = true;
		m_totalRx = 0;
	}

	PPBPApplication::~PPBPApplication()
	{
		NS_LOG_FUNCTION_NOARGS ();
	}

	uint32_t
	PPBPApplication::GetTotalBytes() const
	{
		return m_totalBytes;
	}

	void
	PPBPApplication::DoDispose (void)
	{
		NS_LOG_FUNCTION_NOARGS ();

		m_socket = 0;
		// chain up
		Application::DoDispose ();
	}

	// Application Methods
	void
	PPBPApplication::StartApplication() // Called at time specified by Start
	{
		NS_LOG_FUNCTION_NOARGS ();

		// Create the socket if not already
		if (!m_socket)
		{
			m_socket = Socket::CreateSocket (GetNode(), m_protocolTid);
			m_socket->Bind ();
			m_socket->Connect (m_peer);
		}

		if (!m_receive_socket) {
			m_receive_socket = Socket::CreateSocket(GetNode(), m_protocolTid);
			printf("  > created receive socket\n");
			if (m_receive_socket->Bind(m_local) == -1) {
				NS_FATAL_ERROR("Failed to bind socket");
			}
			m_receive_socket->Listen();
			m_receive_socket->ShutdownSend();


			    // Callbacks
    		m_receive_socket->SetRecvCallback(MakeCallback(&PPBPApplication::HandleRead, this));
			m_receive_socket->SetAcceptCallback(
					MakeNullCallback<bool, Ptr<Socket>,const Address &>(),
					MakeCallback(&PPBPApplication::HandleAccept, this)
			);
		}

		// Set up logging 
		std::ofstream ofs;
		ofs.open(m_baseLogsDir + format_string("/flow_%u_progress.txt", m_node_id));
		ofs.close();
		m_socket->TraceConnectWithoutContext ("HighestRxAck", MakeCallback (&PPBPApplication::HighestRxAckChange, this));
		ofs.open(m_baseLogsDir + format_string("/flow_%u_cwnd.txt", m_node_id));
		ofs.close();
		m_socket->TraceConnectWithoutContext ("CongestionWindow", MakeCallback (&PPBPApplication::CwndChange, this));
		ofs.open(m_baseLogsDir + format_string("/flow_%u_rtt.txt", m_node_id));
		ofs.close();
		m_socket->TraceConnectWithoutContext ("RTT", MakeCallback (&PPBPApplication::RttChange, this));

		// Insure no pending event
		CancelEvents ();
		ScheduleStartEvent();
	}

	void
	PPBPApplication::PPBP() // Poisson Pareto Burst
	{
		NS_LOG_FUNCTION_NOARGS ();

		double inter_burst_intervals;
		inter_burst_intervals = (double) 1/m_burstArrivals;

		//cout << "Mean = " << inter_burst_intervals << endl;

		Ptr<ExponentialRandomVariable> exp = CreateObject<ExponentialRandomVariable> ();
    	exp->SetAttribute ("Mean", DoubleValue (inter_burst_intervals));
		//exp->SetAttribute ("Bound", DoubleValue (3.0));
		Time t_poisson_arrival = Seconds (exp->GetValue());
		m_PoissonArrival = Simulator::Schedule(t_poisson_arrival,&PPBPApplication::PoissonArrival, this);

		// Pareto
		m_shape = 3 - 2 * m_h;
		m_timeSlot = Seconds((double) (m_shape - 1) * m_burstLength / m_shape);

		Ptr<ParetoRandomVariable> pareto = CreateObject<ParetoRandomVariable> ();
		pareto->SetAttribute ("Scale", DoubleValue (m_burstLength));
		pareto->SetAttribute ("Shape", DoubleValue (m_shape));

    double t_pareto = pareto->GetValue();
		//std::cout << "exp = " << t_poisson_arrival << std::endl;
		//std::cout << "pareto = " << t_pareto << std::endl;

		m_ParetoDeparture = Simulator::Schedule(t_poisson_arrival + Seconds (t_pareto),&PPBPApplication::ParetoDeparture, this);

		m_ppbp = Simulator::Schedule(t_poisson_arrival,&PPBPApplication::PPBP, this);
	}

	void PPBPApplication::PoissonArrival()
	{
		NS_LOG_FUNCTION_NOARGS ();
		++m_activebursts;
		if (m_offPeriod) ScheduleNextTx();
	}

	void
	PPBPApplication::ParetoDeparture()
	{
		NS_LOG_FUNCTION_NOARGS ();
		--m_activebursts;
	}

	void
	PPBPApplication::StopApplication() // Called at time specified by Stop
	{
		NS_LOG_FUNCTION_NOARGS ();

		CancelEvents ();
		if(m_socket != 0) m_socket->Close ();
		else NS_LOG_WARN("PPBPApplication found null socket to close in StopApplication");
	}

	void
	PPBPApplication::CancelEvents ()
	{
		NS_LOG_FUNCTION_NOARGS ();
		Simulator::Cancel(m_sendEvent);
		Simulator::Cancel(m_startStopEvent);

		Simulator::Cancel(m_ppbp);
		Simulator::Cancel(m_PoissonArrival);
		Simulator::Cancel(m_ParetoDeparture);
	}

	// Event handlers
	void
	PPBPApplication::StartSending()
	{
		NS_LOG_FUNCTION_NOARGS ();
		m_lastStartTime = Simulator::Now();
		ScheduleNextTx();					// Schedule the send packet event
		ScheduleStopEvent();
	}

	void
	PPBPApplication::StopSending()
	{
		NS_LOG_FUNCTION_NOARGS ();
		CancelEvents();

		ScheduleStartEvent();
	}

	void
	PPBPApplication::ScheduleNextTx()
	{
		NS_LOG_FUNCTION_NOARGS ();
		uint32_t bits = (m_pktSize + 30) * 8;
		Time nextTime(Seconds (bits /
							   static_cast<double>(m_cbrRate.GetBitRate())));

		if (m_activebursts != 0)
		{
			m_offPeriod = false;
			double data_rate = (double) nextTime.GetSeconds() / m_activebursts;
			m_sendEvent = Simulator::Schedule(Seconds(data_rate),&PPBPApplication::SendPacket, this);
		}
		else
		{
			m_offPeriod = true;
		}

	}

	void
	PPBPApplication::ScheduleStartEvent()
	{
		NS_LOG_FUNCTION_NOARGS ();
		m_ppbp = Simulator::Schedule(Seconds(0.0), &PPBPApplication::PPBP, this);
		m_startStopEvent = Simulator::Schedule(Seconds(0.0), &PPBPApplication::StartSending, this);
	}

	void
	PPBPApplication::ScheduleStopEvent()
	{
		NS_LOG_FUNCTION_NOARGS ();
	}

	void
	PPBPApplication::SendPacket()
	{
		NS_LOG_FUNCTION_NOARGS ();
		Ptr<Packet> packet = Create<Packet> (m_pktSize);
		int actual = m_socket->Send (packet);
		// m_totalBytes += packet->GetSize();
		// std::cout <<" actual: "<<actual <<std::endl;
		if(actual > 0) {
			// std::cout<<"m_totalbytes before add actual: "<<m_totalBytes<<std::endl;
			m_totalBytes += actual;
			// std::cout<<"m_totalbytes after add actual: "<<m_totalBytes<<std::endl;
			m_txTrace (packet);
		}
		m_lastStartTime = Simulator::Now();
		ScheduleNextTx();
	}

	void
	PPBPApplication::HandleRead(Ptr<Socket> socket) 
	{
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

	void
	PPBPApplication::HandleAccept(Ptr<Socket> socket, const Address &from) {
		NS_LOG_FUNCTION(this << socket << from);
		socket->SetRecvCallback (MakeCallback (&PPBPApplication::HandleRead, this));
	}

	void
	PPBPApplication::ConnectionSucceeded(Ptr<Socket>)
	{
		NS_LOG_FUNCTION_NOARGS ();
		m_connected = true;
		ScheduleStartEvent();
	}

	void
	PPBPApplication::ConnectionFailed(Ptr<Socket>)
	{
		NS_LOG_FUNCTION_NOARGS ();
		cout << "PPBPApplication, Connection Failed" << endl;
	}

	void
	PPBPApplication::HighestRxAckChange(SequenceNumber<unsigned int, int> oldHighestRxAck, SequenceNumber<unsigned int, int> newHighestRxAck)
	{
		std::ofstream ofs;
		ofs.open (m_baseLogsDir + format_string("/flow_%u_progress.txt", m_node_id), std::ofstream::out | std::ofstream::app);
		// std::cout<<"m_totalbytes: "<<m_totalBytes<<" txbuffer size: "<<m_socket->GetObject<TcpSocketBase>()->GetTxBuffer()->Size()<<std::endl;
		ofs << "0" << "," << Simulator::Now ().GetNanoSeconds () << "," << (m_totalBytes - m_socket->GetObject<TcpSocketBase>()->GetTxBuffer()->Size()) << std::endl;
		ofs.close();
	}

	void
	PPBPApplication::CwndChange(uint32_t oldCwnd, uint32_t newCwnd)
	{
		std::ofstream ofs;
		ofs.open (m_baseLogsDir + format_string("/flow_%u_cwnd.txt", m_node_id), std::ofstream::out | std::ofstream::app);
		ofs << "0" << "," << Simulator::Now ().GetNanoSeconds () << "," << newCwnd << std::endl;
		ofs.close();
	}

	void
	PPBPApplication::RttChange (Time oldRtt, Time newRtt)
	{
		std::ofstream ofs;
		ofs.open (m_baseLogsDir + format_string("/flow_%u_rtt.txt", m_node_id), std::ofstream::out | std::ofstream::app);
		ofs << "0" << "," << Simulator::Now ().GetNanoSeconds () << "," << newRtt.GetNanoSeconds() << std::endl;
		ofs.close();
	}
} // Namespace ns3