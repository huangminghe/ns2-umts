#include "packet.h"

typedef int32_t nsaddr_t; 
typedef int32_t nsmask_t; 
/* 32-bit addressing support */
struct ns_addr_t {
 int32_t addr_;
 int32_t port_;
};
struct hdr_ip {
 /* common to IPv{4,6} */
 ns_addr_t src_;
 ns_addr_t dst_;
 int  ttl_;
 /* Monarch extn */
  u_int16_t sport_;
  u_int16_t dport_;
 
 /* IPv6 */
 int  fid_; /* flow id */
 int  prio_;
 static int offset_;
 inline static int& offset() { return offset_; }
 /* per-field member acces functions */
 ns_addr_t& src() { return (src_); }
 nsaddr_t& saddr() { return (src_.addr_); }
        int32_t& sport() { return src_.port_;}
 ns_addr_t& dst() { return (dst_); }
 nsaddr_t& daddr() { return (dst_.addr_); }
        int32_t& dport() { return dst_.port_;}
 int& ttl() { return (ttl_); }
 /* ipv6 fields */
 int& flowid() { return (fid_); }
 int& prio() { return (prio_); }
};
int main(int argc, char* argv[])
{
    printf("ip: %d\n", sizeof(hdr_ip));
    printf("tora: %d\n", sizeof(hdr_tora));
    getch();
     return 0;
}
