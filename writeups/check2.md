代码量很少的一集, 也不需要像前两个lab嗯抠数据结构, 主要是对于前两个checkpoint中完成的Reassembler的应用, 完成TCP的接收与回复功能
在Reassembler中, 我们使用一个初值为0的64位非负整型的index, 标记流的首字节索引, 然而在TCP中, 为了节约空间, TCP序列号(`sequence number`)采用32位非负整型, 同时为了安全性(防止被猜到序列号从而插入恶意包), TCP序列号的初值是一个32位整型随机值. 在将数据片段送入Reassembler的时候, 需要将TCP序列号转换为Reassembler可以接受的从零开始的64位整型索引. 为了避免转换过程导致的奇怪错误, lab贴心的抽象了一个Wrap32类方便我们编写转换逻辑并测试. Wrap32内部需要实现uint64_t的打包(转换为uint32_t)和uint32_t的解包(转换为uint64_t), 过程不复杂, 需要注意的点在于unwrap, 对于一个大于$2^{32}$的checkpoint, 存在两个可能的结果, 需要确定两个结果哪个更接近checkpoint
`receive()`和`send()`实现不复杂, 但是需要注意对错误情况的处理, 可以根据通过测试来确认需要错误处理的情况
比较奇怪的事是这个checkpoint中, 允许SYN和FIN在同一个片段中同时置1的畸形报文, 与RFC793的规定有出入
RFC793原文:
`eighth, check the FIN bit,

      Do not process the FIN if the state is CLOSED, LISTEN or SYN-SENT
      since the SEG.SEQ cannot be validated; drop the segment and
      return.

      If the FIN bit is set, signal the user "connection closing" and
      return any pending RECEIVEs with same message, advance RCV.NXT
      over the FIN, and send an acknowledgment for the FIN.  Note that
      FIN implies PUSH for any segment text not yet delivered to the
      user.
`
