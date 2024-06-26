#pragma once

#include "byte_stream.hh"
#include "tcp_receiver_message.hh"
#include "tcp_sender_message.hh"

#include <cstdint>
#include <functional>
#include <list>
#include <memory>
#include <optional>
#include <queue>

class TCPTimer
{
private:
  uint64_t RTO_;
  uint64_t time_passed_ {};
  bool activated_ { false };

public:
  TCPTimer( uint64_t RTO ) : RTO_( RTO ) {}
  TCPTimer& activate(); // 返回本身, 流控制
  TCPTimer& reset();
  TCPTimer& backoff();
  TCPTimer& ticked( uint64_t time_ticked_ms_ );
  bool is_expired() const { return activated_ && time_passed_ >= RTO_; }
  bool is_activated() const { return activated_; }
};

class TCPSender
{
public:
  /* Construct TCP sender with given default Retransmission Timeout and possible ISN */
  TCPSender( ByteStream&& input, Wrap32 isn, uint64_t initial_RTO_ms )
    : input_( std::move( input ) ), isn_( isn ), initial_RTO_ms_( initial_RTO_ms ), timer_( initial_RTO_ms )
  {}

  /* Generate an empty TCPSenderMessage */
  TCPSenderMessage make_empty_message() const;

  /* 生成message */
  TCPSenderMessage make_message( Wrap32 seqno, bool syn, std::string payload, bool fin ) const;

  /* Receive and process a TCPReceiverMessage from the peer's receiver */
  void receive( const TCPReceiverMessage& msg );

  /* Type of the `transmit` function that the push and tick methods can use to send messages */
  using TransmitFunction = std::function<void( const TCPSenderMessage& )>;

  /* Push bytes from the outbound stream */
  void push( const TransmitFunction& transmit );

  /* Time has passed by the given # of milliseconds since the last time the tick() method was called */
  void tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit );

  // Accessors
  uint64_t sequence_numbers_in_flight() const;  // How many sequence numbers are outstanding?
  uint64_t consecutive_retransmissions() const; // How many consecutive *re*transmissions have happened?
  Writer& writer() { return input_.writer(); }
  const Writer& writer() const { return input_.writer(); }

  // Access input stream reader, but const-only (can't read from outside)
  const Reader& reader() const { return input_.reader(); }

private:
  // Variables initialized in constructor
  ByteStream input_;
  Wrap32 isn_;
  uint64_t initial_RTO_ms_;
  TCPTimer timer_;

  uint16_t window_size_ { 1 };
  uint64_t cnt_seq_in_flight_ {};
  uint64_t cnt_consecutive_retransmission_ {};
  uint64_t next_seqno_ {};

  bool read_fin_ { false };
  bool read_syn_ { false };
  bool sent_fin_ { false };
  bool sent_syn_ { false };

  std::queue<TCPSenderMessage> buffer_ {};
};
