#include <iostream>
#include <fstream>

#include "arp_message.hh"
#include "exception.hh"
#include "network_interface.hh"

using namespace std;

NetworkInterface::EtherAddressWithTimer& NetworkInterface::EtherAddressWithTimer::tick(
  size_t ms_since_last_tick ) noexcept
{
  time_passed_ += ms_since_last_tick;
  return *this;
}

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface( string_view name,
                                    shared_ptr<OutputPort> port,
                                    const EthernetAddress& ethernet_address,
                                    const Address& ip_address )
  : name_( name )
  , port_( notnull( "OutputPort", move( port ) ) )
  , ethernet_address_( ethernet_address )
  , ip_address_( ip_address )
{
  cerr << "DEBUG: Network interface has Ethernet address " << to_string( ethernet_address ) << " and IP address "
       << ip_address.ip() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but
//! may also be another host if directly connected to the same network as the destination) Note: the Address type
//! can be converted to a uint32_t (raw 32-bit IP address) by using the Address::ipv4_numeric() method.
void NetworkInterface::send_datagram( const InternetDatagram& dgram, const Address& next_hop )
{
  // Your code here.
  (void)dgram;
  (void)next_hop;
  uint32_t numeric_ip = next_hop.ipv4_numeric();
  auto it = ip_map2_eth_.find( numeric_ip );
  if ( it == ip_map2_eth_.end() ) {
    ip_map2_datagrams.emplace(numeric_ip, dgram);
    if(ip_map2_arp_timer.find(numeric_ip) == ip_map2_arp_timer.end())
    {
      transmit(make_eth_frame(EthernetHeader::TYPE_ARP, ETHERNET_BROADCAST, serialize(make_arp_msg(ARPMessage::OPCODE_REQUEST, {}, numeric_ip))));
      ip_map2_arp_timer.emplace(numeric_ip, 0);
    }
  } else {
    transmit( make_eth_frame( EthernetHeader::TYPE_IPv4, it->second.get_eth_addr(), serialize( dgram ) ) );
  }
}

//! \param[in] frame the incoming Ethernet frame
void NetworkInterface::recv_frame( const EthernetFrame& frame )
{
  // Your code here.
  if ( frame.header.dst != ETHERNET_BROADCAST && frame.header.dst != ethernet_address_ ) {
    return;
  }
  fstream f("../writeups/1.txt", ios::app);
  f << "\n";
  for(auto i:frame.header.dst)
  {
    f << hex << uint32_t(i) << " ";
  }
  f << "\n";
  if ( frame.header.type == EthernetHeader::TYPE_IPv4 ) {
    InternetDatagram ip_dgram;
    if ( not parse( ip_dgram, frame.payload ) ) // 解析出错时直接返回
    {
      return;
    }
    datagrams_received_.push( std::move( ip_dgram ) );
  } else {
    ARPMessage arp_msg;
    if ( not parse( arp_msg, frame.payload ) ) // 解析出错时直接返回
    {
      return;
    }
    // 只要是合法arp消息, 就应该记录到ip-ethaddr映射表
    ip_map2_eth_.insert_or_assign( arp_msg.sender_ip_address, arp_msg.sender_ethernet_address );
    if ( arp_msg.opcode == ARPMessage::OPCODE_REQUEST ) {
      // auto msg = make_arp_msg(ARPMessage::OPCODE_REPLY, arp_msg.sender_ethernet_address,
      // arp_msg.sender_ip_address);
      transmit( make_eth_frame( EthernetHeader::TYPE_ARP,
                                arp_msg.sender_ethernet_address,
                                serialize( make_arp_msg( ARPMessage::OPCODE_REPLY,
                                                         arp_msg.sender_ethernet_address,
                                                         arp_msg.sender_ip_address ) ) ) );
    } else if ( arp_msg.opcode == ARPMessage::OPCODE_REPLY ) {
      auto [st, ed] = ip_map2_datagrams.equal_range(arp_msg.sender_ip_address);
      for(auto it = st; it != ed; it++)
      {
        transmit(make_eth_frame(EthernetHeader::TYPE_IPv4, arp_msg.sender_ethernet_address, serialize(it->second)));
      }
      if(st != ed)
      {
        ip_map2_datagrams.erase(st, ed);// 全卅喽
      }
    }
  }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick( const size_t ms_since_last_tick )
{
  // Your code here.
  // (void)ms_since_last_tick;
  const size_t arp_interval = 5000, eth_interval = 30000;
  auto ticker = [&](auto& tbl, size_t itrvl)
  {
    for(auto it = tbl.begin(); it!=tbl.end(); )
    {
      if((it->second += ms_since_last_tick) >= itrvl)
      {
        it = tbl.erase(it);
      }
      else
      {
        it++;
      }
    }
  };
  ticker(ip_map2_arp_timer, arp_interval);
  ticker(ip_map2_eth_, eth_interval);
}

EthernetFrame NetworkInterface::make_eth_frame( uint16_t type,
                                                EthernetAddress dst,
                                                std::vector<std::string> payload )
{
  return { .header { .dst = dst, .src = ethernet_address_, .type = type }, .payload = std::move( payload ) };
}

ARPMessage NetworkInterface::make_arp_msg( uint16_t opcode, EthernetAddress target_eth, uint32_t target_ip )
{
  return { .opcode = opcode,
           .sender_ethernet_address = ethernet_address_,
           .sender_ip_address = ip_address_.ipv4_numeric(),
           .target_ethernet_address = std::move( target_eth ),
           .target_ip_address = target_ip };
}