#include "wrapping_integers.hh"

using namespace std;

Wrap32 Wrap32::wrap( uint64_t n, Wrap32 zero_point )
{
  // Your code here.
  // (void)n;
  // (void)zero_point;
  return zero_point + (n & 0xffffffff);
}

uint64_t Wrap32::unwrap( Wrap32 zero_point, uint64_t checkpoint ) const
{
  // Your code here.
  // (void)zero_point;
  // (void)checkpoint;
  // return {};
  const uint64_t mod = static_cast<uint64_t>(1) << 32;
  uint32_t dist = raw_value_ - wrap(checkpoint, zero_point).raw_value_;
  if(dist <= (static_cast<uint32_t>(1) << 31) || checkpoint + dist < mod)
  {
    return dist + checkpoint;
  }
  return dist + checkpoint - mod;
}
