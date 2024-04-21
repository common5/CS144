#include "tcp_receiver.hh"

using namespace std;

void TCPReceiver::receive( TCPSenderMessage message )
{
  // Your code here.
  // (void)message;
  if ( message.RST ) {
    reassembler_.reader().set_error();
    return;
  }
  const uint64_t checkpoint = reassembler_.writer().bytes_pushed(); // expected stream index
  if ( !ISN_.has_value() ) {
    if ( message.SYN )
      ISN_ = message.seqno;
    else
      return;
  } else {
    if ( checkpoint <= ( static_cast<uint64_t>( 1 ) << 32 )
         && message.seqno == *ISN_ ) // 当且仅当checkpoint <= 2^32的时候, message.seqno才可能等于ISN, 即非法
    {
      return;
    }
  }
  reassembler_.insert( message.seqno.unwrap( *ISN_, checkpoint ) - ( *ISN_ != message.seqno ),
                       std::move( message.payload ),
                       message.FIN );
}

TCPReceiverMessage TCPReceiver::send() const
{
  // Your code here.
  // return {};
  std::optional<Wrap32> seqno = std::nullopt;
  if ( ISN_.has_value() ) {
    // getchar();
    seqno = Wrap32::wrap( reassembler_.writer().bytes_pushed(), *ISN_ ) + ( 1 + reassembler_.writer().is_closed() );
  }
  return {
    seqno,
    static_cast<uint16_t>( min( reassembler_.writer().available_capacity(), static_cast<uint64_t>( 65535 ) ) ),
    reassembler_.writer().has_error() };
}
