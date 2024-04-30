#include "tcp_sender.hh"
#include "tcp_config.hh"

#include <fstream>

using namespace std;

TCPTimer& TCPTimer::activate()
{
  this->activated_ = true;
  return *this;
}
TCPTimer& TCPTimer::reset()
{
  this->time_passed_ = 0;
  return *this;
}
TCPTimer& TCPTimer::backoff()
{
  this->RTO_ <<= 1;
  return *this;
}
TCPTimer& TCPTimer::ticked( uint64_t time_ticked_ms_ )
{
  // (void)time_ticked_ms_;
  this->time_passed_ += time_ticked_ms_ * this->activated_;
  return *this;
}

uint64_t TCPSender::sequence_numbers_in_flight() const
{
  // Your code here.
  return cnt_seq_in_flight_;
}

uint64_t TCPSender::consecutive_retransmissions() const
{
  // Your code here.
  return cnt_consecutive_retransmission_;
}

void TCPSender::push( const TransmitFunction& transmit )
{
  // Your code here.
  (void)transmit;
  auto& rdr = input_.reader(); // 由于不能使用reader(), 因为需要用到pop(), pop不是一个常函数
  read_fin_ |= rdr.is_finished();
  // 特判处理
  if ( sent_fin_ ) {
    return;
  }
  const uint16_t wnd_size
      = window_size_ == 0 ? 1 : window_size_; // 当窗口大小为0的时候, 需要将假装是1, 否则永远无法知道接收方的状态
  // 循环执行: 1.组装报文到大小上限, 2.发送报文, 循环退出条件: 已发送但未确认的包的大小总和达到窗口上限,
  // 或者别的什么错误
  for ( std::string payload {}; cnt_seq_in_flight_ < wnd_size; payload.clear() ) {
    std::string_view content = rdr.peek();
    if ( content.empty() && sent_syn_ && (!read_fin_ || sent_fin_ ) ) {

      break; // input_流空了, 并且已经发送过syn, 并且input_未结束, 那么就啥也不用组装, 直接break
    }
    const size_t upper_size = TCPConfig::MAX_PAYLOAD_SIZE;
    const uint64_t syn_size
      = sent_syn_ ? 0 : 1; // 如果没有发送过syn_flag, 那么计算报文大小的时候则需要将syn_flag的大小计入
    // 报文大小= payload大小 + syn_flag + fin_flag, 但由于fin_flag比较特殊, 所以将fin_flag放到后面处理
    // 报文大小应该小于TCPConfig::MAX_PAYLOAD_SIZE, 同时总未达到包的大小应该小于window_size_
    // 组装报文

    while ( cnt_seq_in_flight_ + payload.size() + syn_size < wnd_size && payload.size() < upper_size ) {
      uint64_t expected_size
        = min( wnd_size - syn_size - payload.size() - cnt_seq_in_flight_, upper_size - payload.size() );
        
      if ( content.empty() || read_fin_ ) {
        break;
      }
      if ( content.size() > expected_size ) {
        content.remove_suffix( content.size() - expected_size );
      }
      payload.append( content );
      rdr.pop( content.size() );
      read_fin_ |= rdr.is_finished();
      if(input_.writer().is_closed())
      {
        fstream f( "/home/common5/cs144/CS144/writeups/1.txt", ios::out | ios::app );
        f << Wrap32::wrap(next_seqno_, isn_).get_raw() << " b " << input_.writer().available_capacity() << " " << input_.writer().capacity() << " " << rdr.is_finished() << endl;
      }
      content = rdr.peek();
    }
    bool fin_flag = false;
    if ( read_fin_ ) {
              // getchar();
      // 如果流已经结束, 那么需要判断能否在当前包中附加fin_flag
      if ( payload.size() + !sent_syn_ + cnt_seq_in_flight_ + 1 <= wnd_size ) {
        fin_flag = true;
      }
      // f << Wrap32::wrap(next_seqno_, isn_).get_raw() << " c " << payload.size() + !sent_syn_ + 1 << " " << payload.size() + !sent_syn_ + cnt_seq_in_flight_ + 1 << " " << wnd_size << endl;
    }
    auto msg = make_message( Wrap32::wrap( next_seqno_, isn_ ), !sent_syn_, std::move( payload ), fin_flag );
    buffer_.emplace( msg );
    transmit( msg );
    cnt_seq_in_flight_ += msg.sequence_length();
    next_seqno_ += msg.sequence_length();
    sent_syn_ = true; // 只要发送了报文, 那么sent_syn_一定会变为true
    // fstream f("/home/common5/cs144/CS144/writeups/1.txt", ios::out|ios::app);
    // f << read_fin_ << endl;
    if ( fin_flag ) {
      // f << "sent_fin_" << endl;
      // getchar();
      sent_fin_ = true;
    }
    if ( msg.sequence_length() > 0 ) {
      timer_.activate();
    }
  }
}

TCPSenderMessage TCPSender::make_message( Wrap32 seqno, bool syn, std::string payload, bool fin ) const
{
  return { .seqno = seqno, .SYN = syn, .payload = std::move( payload ), .FIN = fin, .RST = input_.has_error() };
}

TCPSenderMessage TCPSender::make_empty_message() const
{
  // Your code here.
  return make_message( Wrap32::wrap( next_seqno_, isn_ ), false, {}, false );
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
  // Your code here.
  // (void)msg;
  if ( !msg.ackno.has_value() ) {
    window_size_ = msg.window_size;
    if(msg.RST)
    {
      input_.reader().set_error();
    }
    return;
  }
  window_size_ = msg.window_size;
  bool rst = msg.RST;
  if ( rst ) {
    input_.reader().set_error();
    return;
  }
  uint64_t expected_seqno = msg.ackno->unwrap( isn_, next_seqno_ );
  if ( expected_seqno > next_seqno_ ) // 显然不合法的字节号
  {
    return;
  }
  bool acked_first_pck = false;
  while ( !buffer_.empty() ) {
    const uint64_t seqno = buffer_.front().seqno.unwrap( isn_, next_seqno_ );
    // 当且仅当整条信息完全被确认, 才将信息pop出去
    if ( expected_seqno < seqno || expected_seqno - seqno < buffer_.front().sequence_length() )
      break;
    acked_first_pck = true;
    cnt_seq_in_flight_ -= buffer_.front().sequence_length();
    buffer_.pop();
  }
  if ( acked_first_pck ) {
    if ( buffer_.empty() ) {
      timer_ = ( TCPTimer( initial_RTO_ms_ ) );
    } else {
      timer_ = std::move( TCPTimer( initial_RTO_ms_ ) ).activate();
    }
    cnt_consecutive_retransmission_ = 0; // 因为确认了一个完整包, 所以重传计数重置
  }
}

void TCPSender::tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit )
{
  // Your code here.
  (void)ms_since_last_tick;
  (void)transmit;
  (void)initial_RTO_ms_;
  timer_.ticked( ms_since_last_tick );
  // 超时重传
  if ( timer_.is_expired() ) {
    transmit( buffer_.front() );
    // 当且仅当实际窗口大小不为0的时候，采用exponential backoff策略并记录连续重传次数
    if(window_size_ != 0)
    {
      timer_.backoff().reset().activate();
      cnt_consecutive_retransmission_++;
    }
    else
    {
      timer_.reset().activate();
    }
  }
}
