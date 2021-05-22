
typedef unsigned int uint;
typedef unsigned char uchar;
typedef unsigned short int ushort;
typedef uint ipaddr;
#define IPHDR_IHL 5                     // IP header 长
#define UDP_HDR_LEN 8                   // UDP header 长
#define IP_HDR_LEN 20                   // IP header 长度
#define MAC_HDR_LEN 14                  // MAC 头长度
#define MAC_FCS_LEN 4                   // MAC 尾长度
#define MIN_FRAME_PAYLOAD_LEN 46        // 最小frame payload长度
const uchar UDP_PROTOCAL = (uchar)17U;  // UDP 协议号
#define MAX_FRAME_LEN 1518              // max frame length
typedef uchar mac_addr[6];
typedef uchar checknum[4];
#define ETH_PROTOCAL 0x0800     // IPv4 协议号
#define ERROR_TAKEN 0           // 错误发生类型（调试）
#define MY_ETH_PROTOCAL 0x0800  // 自定义协议号
// #define MTU 1460
#define MTU 44                   // 为了测试，使用较小的MTU
#define MAX_IP_PACKET_LEN 65536  // 最大的IP packet长度
struct myiphdr {
    uchar ihl : 4, version : 4;
    // 版本 0.5个字节 // IHL 0.5字节
    uchar tos;        // 区分服务 1
    ushort tot_len;   // 总长度 2
    ushort id;        // 标识 2
    ushort frag_off;  // 分段偏移量 2
    uchar ttl;        // 生存周期 1
    uchar protocol;   // 协议 1
    ushort check;     // 头校验和 2
    uint saddr;       // 源地址 4
    uint daddr;       // 目标地址 4
};

struct myudphdr {
    ushort sport;
    ushort dport;
    ushort len;
    ushort check;
};
