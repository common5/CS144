#pragma once

#include <compare>
#include <queue>
#include <unordered_map>

#include "address.hh"
#include "ethernet_frame.hh"
#include "ipv4_datagram.hh"

// A "network interface" that connects IP (the internet layer, or network layer)
// with Ethernet (the network access layer, or link layer).

// This module is the lowest layer of a TCP/IP stack
// (connecting IP with the lower-layer network protocol,
// e.g. Ethernet). But the same module is also used repeatedly
// as part of a router: a router generally has many network
// interfaces, and the router's job is to route Internet datagrams
// between the different interfaces.

// The network interface translates datagrams (coming from the
// "customer," e.g. a TCP/IP stack or router) into Ethernet
// frames. To fill in the Ethernet destination address, it looks up
// the Ethernet address of the next IP hop of each datagram, making
// requests with the [Address Resolution Protocol](\ref rfc::rfc826).
// In the opposite direction, the network interface accepts Ethernet
// frames, checks if they are intended for it, and if so, processes
// the the payload depending on its type. If it's an IPv4 datagram,
// the network interface passes it up the stack. If it's an ARP
// request or reply, the network interface processes the frame
// and learns or replies as necessary.
class NetworkInterface
{
public:
  // An abstraction for the physical output port where the NetworkInterface sends Ethernet frames
  class OutputPort
  {
  public:
    virtual void transmit( const NetworkInterface& sender, const EthernetFrame& frame ) = 0;
    virtual ~OutputPort() = default;
  };

  // Construct a network interface with given Ethernet (network-access-layer) and IP (internet-layer)
  // addresses
  NetworkInterface( std::string_view name,
                    std::shared_ptr<OutputPort> port,
                    const EthernetAddress& ethernet_address,
                    const Address& ip_address );

  // Sends an Internet datagram, encapsulated in an Ethernet frame (if it knows the Ethernet destination
  // address). Will need to use [ARP](\ref rfc::rfc826) to look up the Ethernet destination address for the next
  // hop. Sending is accomplished by calling `transmit()` (a member variable) on the frame.
  void send_datagram( const InternetDatagram& dgram, const Address& next_hop );

  // Receives an Ethernet frame and responds appropriately.
  // If type is IPv4, pushes the datagram to the datagrams_in queue.
  // If type is ARP request, learn a mapping from the "sender" fields, and send an ARP reply.
  // If type is ARP reply, learn a mapping from the "sender" fields.
  void recv_frame( const EthernetFrame& frame );

  // Called periodically when time elapses
  void tick( size_t ms_since_last_tick );

  // 包装成以太网帧
  EthernetFrame make_eth_frame( uint16_t type, EthernetAddress dst, std::vector<std::string> payload );

  // 包装成ARP信息, 包含请求和回复两种
  ARPMessage make_arp_msg( uint16_t opcode, EthernetAddress target_eth, uint32_t target_ip );

  // Accessors
  const std::string& name() const { return name_; }
  const OutputPort& output() const { return *port_; }
  OutputPort& output() { return *port_; }
  std::queue<InternetDatagram>& datagrams_received() { return datagrams_received_; }

private:
  class EtherAddressWithTimer
  {
  private:
    EthernetAddress ethernet_addr_;
    size_t time_passed_ ;
  public:
    EtherAddressWithTimer( EthernetAddress e_addr ) : ethernet_addr_( std::move(e_addr) ), time_passed_{} {}
    // EtherAddressWithTimer& operator=(EthernetAddress e_addr);
    EthernetAddress get_eth_addr() const { return ethernet_addr_; }
    EtherAddressWithTimer& tick( size_t ms_since_last_tick ) noexcept;
    EtherAddressWithTimer& operator+=( size_t ms_since_last_tick ) noexcept { return tick( ms_since_last_tick ); }
    auto operator<=> (const size_t interval) const { return time_passed_ <=> interval; }
  };

  // Human-readable name of the interface
  std::string name_;

  // The physical output port (+ a helper function `transmit` that uses it to send an Ethernet frame)
  std::shared_ptr<OutputPort> port_;
  void transmit( const EthernetFrame& frame ) const { port_->transmit( *this, frame ); }

  // Ethernet (known as hardware, network-access-layer, or link-layer) address of the interface
  EthernetAddress ethernet_address_;

  // IP (known as internet-layer or network-layer) address of the interface
  Address ip_address_;

  // Datagrams that have been received
  std::queue<InternetDatagram> datagrams_received_ {};

  // 从ip映射到以太网地址的hash table
  std::unordered_map<uint32_t, EtherAddressWithTimer> ip_map2_eth_ {};

  // 记录某个ip发送ARP请求经过的时间
  std::unordered_map<uint32_t, size_t> ip_map2_arp_timer {};

  // 从目标ip地址映射到需要发送的数据包的multi hash table
  std::unordered_multimap<uint32_t, InternetDatagram> ip_map2_datagrams {};
};
