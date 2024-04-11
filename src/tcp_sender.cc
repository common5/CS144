#include "tcp_sender.hh"
#include "tcp_config.hh"

using namespace std;

uint64_t TCPSender::sequence_numbers_in_flight() const
{
  // Your code here.
  return {};
}

uint64_t TCPSender::consecutive_retransmissions() const
{
  // Your code here.
  return {};
}

void TCPSender::push( const TransmitFunction& transmit )
{
  // Your code here.
  // (void)transmit;
  if(sent_fin_)
  {
    return;//发送过fin信号就单方停止push
  }
  const uint16_t window_size = window_size_ == 0 ? 1 : window_size_; // window_size_为0的时候也要假装宽度为1
  const size_t max_size = TCPConfig::MAX_PAYLOAD_SIZE;
  read_fin_ |= input_.reader().is_finished();
  for(string payload {}; !read_fin_ && cnt_seq_in_flight_ < window_size; )
  {
    // 持续从input_中读取sv并装载到payload, 直到payload长度达到上限, 或者窗口满载
    while(payload.size() < max_size && payload.size() + (!sent_syn_) + cnt_seq_in_flight_ < window_size)
    {
      string_view info = input_.reader().peek();
      if(info.empty() || read_fin_)
      {
        break; // 已经读空了, 退出
      }
      // 注意截断问题
      if(payload.size() + info.size() > max_size || payload.size() + info.size() + (!sent_syn_) + cnt_seq_in_flight_ > window_size)
      {
        const auto sz = min(max_size - payload.size(), window_size - (info.size() + (!sent_syn_) + cnt_seq_in_flight_));
        if(sz > 0)
        {
          info.remove_suffix(info.size() - sz);
        }
      }
      payload.append(info);// 拼接
      input_.reader().pop(info.size());
      read_fin_ |= input_.reader().is_finished();
    }
    // auto fin_flag_ = false;
    // // 读取到fin并且空间足够加一个FIN字节的时候就可以直接添加FIN字节
    // if(read_fin_ && payload.size() + (!sent_syn_) + cnt_seq_in_flight_ + 1 <= window_size)
    // {
    //   fin_flag_ = true;
    // }
    // // 发包
    // const auto& msg = make_message(next_seqno_, !sent_syn_, std::move(payload), fin_flag_);
    // transmit(msg);
    // cnt_seq_in_flight_ += msg.sequence_length();
    // next_seqno_ += msg.sequence_length();
    // if(fin_flag_)
    // {
    //   sent_fin_ = true;
    // }
    auto fin_flag = false;
    if(read_fin_ && payload.size() + (!sent_syn_) + cnt_seq_in_flight_ + 1 <= window_size)
    {
      fin_flag = true;
      sent_fin_ = true; // 能发送fin则一定会发送fin, 直接将sent_fin_置为true
    }
    const auto& msg = make_message(Wrap32::wrap(next_seqno_, isn_), !sent_syn_, std::move(payload), fin_flag_);
    buffer_.emplace(msg);
    transmit(msg);
    cnt_seq_in_flight_ += msg.sequence_length();
    next_seqno_ += msg.sequence_length();
    sent_syn_ = true;

  }
}

TCPSenderMessage TCPSender::make_message(Wrap32 seqno, bool syn, std::string payload, bool fin) const
{
  return { 
    .seqno = seqno,
    .SYN = syn,
    .payload = std::move(payload),
    .FIN = fin,
    .RST = input_.reader().has_error()
  };
}

TCPSenderMessage TCPSender::make_empty_message() const
{
  // Your code here.
  return {};
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
  // Your code here.
  (void)msg;
}

void TCPSender::tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit )
{
  // Your code here.
  (void)ms_since_last_tick;
  (void)transmit;
  (void)initial_RTO_ms_;

}
