/**
 * @author 田润泽
 * @major 计算机科学与技术卓越
 * @course 计算机网络
 * @file client.c 发送端
 * 使用raw socket 进行frame封装、IP封装、分片、重组，UDP封装

 */
#include <arpa/inet.h>
#include <assert.h>
#include <ctype.h>
#include <malloc.h>
#include <net/if.h>            // struct ifreq
#include <netinet/ether.h>     // ETH_P_ALL
#include <netpacket/packet.h>  // struct sockaddr_ll
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>  // ioctl、SIOCGIFADDR
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>    // time()
#include <unistd.h>  // close

#include "myhdr.h"
#include "utils.h"

// #define UDP_PROTOCAL
/*
struct myiphdr {
    uint version;    // 版本
    uint ihl;        // IHL
    uchar tos;       // 区分服务
    ushot tot_len;   // 总长度
    ushot id;        // 标识
    ushot frag_off;  // 分段偏移量
    uchar ttl;       // 生存周期
    uchar protocol;  // 协议
    ushot check;     // 头校验和
    uint saddr;      // 源地址
    uint daddr;      // 目标地址
};
*/
mac_addr my_mac;  // 本机MAC地址
// mac_addr dst_mac = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};  // 广播src MAC地址
mac_addr dst_mac = {0x00, 0x15, 0x5d, 0x97, 0xC1, 0x6a};  // dst MAC 地址
int sock_raw_fd;
struct sockaddr_ll sll_send;           // the send socket address structure
struct sockaddr_ll sll_recv;           // the recv socket address structure
ipaddr dstip, srcip;                   // src IP, dst IP
char myipaddr_c[] = "172.18.170.252";  // 本机IP地址

typedef unsigned char checknum[4];           // checknum 字符数组
int frame_index;                             // frame index
char store[MAX_IP_PACKET_LEN];               // 用于存储分片的IP packet
int ip_packet_id = -1;                       // IP分片packet的第一片Id
int total_byte = MAC_FCS_LEN + MAC_HDR_LEN;  // IP分片的byte累加量
struct myiphdr *first_iphdr;                 // IP分片的第一片的IP header

// ------------------------------------------ //
void output_frame(char *frame, int frame_size);  // 输出frame结构信息
ushort calc_check_udphdr(char *udp_datageam, uint udplen, uint srcip,
                         uint dstip);  // udp头的校验
ushort make_frame(mac_addr *dst, mac_addr *src, ushort protocal, char *payload,
                  int payload_len, char *frame);  // 构造frame结构
void send_to_datalink_layer(char data[], int data_len,
                            int error);  // 发往数据链路层
void send_to_network_layer(uchar protocal, uint payload_len,
                           uchar *payload);          // 发往网络层
void send_data(uint sport, uint dport, char *data);  // 应用层发送数据
void output_mac(mac_addr p);                         // 输出MAC地址信息
int mac_equal(mac_addr x, mac_addr y);               // MAC匹配检查
int check_iphdr_error(struct myiphdr *p, int len);   // IP头校验
void read_from_network_layer(int nbyte, char *packet, struct myiphdr *iniphdr,
                             int frame_index);  // 从网络层读
void recv_data();
void send_thread();  // 发线程
void recv_thread();  // 发线程

int read_from_datalink_layer(char packet[], int nbyte);  // 从数据链路层读
void read_from_network_layer(int nbyte, char *packet, struct myiphdr *iniphdr,
                             int frame_index);  // 从网络层读
int frame_crc_check(uint cas_rec, char buffer_in[], int nbyte);  // Frame check

/**
 * create frame
 * @param dst char[], the dst mac address
 * @param src char[], the src mac address
 * @param protocal the protocal type
 * @param payload the data to be send in the frame
 * @param payload_len  the length of data
 * @param frame the frame to be made
 *
 * @return the length of frame
 */

ushort calc_check_udphdr(char *udp_datageam, uint udplen, uint srcip,
                         uint dstip) {
    char tmp[MAX_IP_PACKET_LEN];
    memset(tmp, 0, sizeof(tmp));
    memcpy(tmp, &srcip, 4);
    memcpy(tmp + 4, &dstip, 4);
    ushort protocal = 17;
    memcpy(tmp + 9, &protocal, 1);
    memcpy(tmp + 10, &udplen, 2);
    memcpy(tmp + 12, udp_datageam, udplen);
    ushort num = csum((short *)tmp, udplen + 12);
    return num;
}

/**
 * 构造帧结构
 * @param dst 目标MAC地址
 * @param src 源MAC地址
 * @param protocal 协议类型
 * @param payload 数据
 * @param payload_len 数据长度
 * @param frame 构造的帧结构
 * @return 帧的长度
 */
ushort make_frame(mac_addr *dst, mac_addr *src, ushort protocal, char *payload,
                  int payload_len, char *frame) {
    if (payload_len > 1500) return 0;  // length of payload must less than 4600

    memcpy(&frame[0], dst, 6);
    memcpy(&frame[6], src, 6);
    memcpy(&frame[12], &protocal, 2);
    memcpy(&frame[14], payload, payload_len);
    // fill 0 if payload length less than 46
    if (payload_len < MIN_FRAME_PAYLOAD_LEN) {
        char zeros[MIN_FRAME_PAYLOAD_LEN];
        bzero(zeros, sizeof(zeros));
        memcpy(&frame[MAC_HDR_LEN + payload_len], zeros,
               MIN_FRAME_PAYLOAD_LEN - payload_len);
    }
    int data_len = payload_len < MIN_FRAME_PAYLOAD_LEN ? MIN_FRAME_PAYLOAD_LEN
                                                       : payload_len;
    uint crc = crc32(frame, MAC_HDR_LEN + data_len);
    memcpy(&frame[MAC_HDR_LEN + data_len], &crc, sizeof(crc));

    // 测试ip
    // struct myiphdr hdr;

    // memcpy(&hdr, &frame[14], 20);
    // printf("test ip protocal%u\n", hdr.protocol);
    // printf("test ip saddr %u\n", hdr.daddr);

    // 测试 udp payload
    // char res[1500];
    // memcpy(res, &frame[14 + 20 + 8], payload_len - 20);
    // printf("test payload: %s\n", res);

    return MAC_HDR_LEN + MAC_FCS_LEN + data_len;
}

/**
 * 输出帧frame信息
 * @param frame 帧
 * @param frame_size 帧大小
 */
void output_frame(char *frame, int frame_size) {
    char *p = frame;
    int cnt = frame_size;
    int t = 0;
    while (cnt > 1) {
        printf("%hhx ", *p++);
        t++;
        if (t % 16 == 0) printf("\n");
        cnt--;
    }
    printf("\n");
}

/**
 * 从网络层发往数据链路层
 * @param 要发送的数据
 * @param 用于测试checksum. 0: normal, 1:crc check failed

 * @return
 */
void send_to_datalink_layer(char data[], int data_len, int error) {
    char frame[MAX_FRAME_LEN];

    int frame_size;
    // printf("send eth datalen: %d\n", data_len);
    // 测试时候为了过滤自己协议
    frame_size = make_frame(&dst_mac, &my_mac, htons(MY_ETH_PROTOCAL), data,
                            data_len, &frame[0]);

    // 实际ip协议

    if (!frame_size) {
        printf("[ERROR]:  data length must be less than 1500\n");
        return;
    }
    // printf("[INFO]: send data size %d\n", frame_size);
    if (error == 1)
        frame[14] = ~frame[14];
    else if (error == 2)
        frame[21] = ~frame[21];
    else if (error == 3)
        frame[36] = ~frame[36];
    // 输出frame 测试
    // output_frame(frame, frame_size);

    // if (cnt > 0) printf("%x", *(unsigned char *)p);
    int len = send(sock_raw_fd, frame, frame_size, 0);

    if (len == -1) {
        perror("sendto");
    } else {
        printf("[SUCCESS] send %d size data\n\n", frame_size);
    }
}

/**
 * 从运输层发往网络层
 * @param protocal 运输层的协议号
 * @param payload_len payload长度
 * @param payload
 */
void send_to_network_layer(uchar protocal, uint payload_len, uchar *payload) {
    // DEBUG UDP LEN
    struct myudphdr u;
    memcpy(&u, payload, 8);

    struct myiphdr iphdr;
    char ip_packet[MAX_FRAME_LEN * 2];
    bzero(ip_packet, sizeof(ip_packet));
    iphdr.version = 4;  // 我定义为ipv4
    iphdr.ihl = IPHDR_IHL;
    iphdr.tos = 0;
    iphdr.tot_len =
        htons(payload_len + (iphdr.ihl << 2));  // 这里是20个字节的固定长度

    iphdr.id = rand() % 10000;
    iphdr.frag_off = 0;
    iphdr.ttl = 128;
    iphdr.protocol = protocal;
    iphdr.daddr = dstip;
    // printf("in make iphdr: %u %u\n", saddr, daddr);
    iphdr.saddr = srcip;
    iphdr.check = 0;
    struct myiphdr test;
    // printf("check iphdr sizeof iphdr %ld\n", sizeof(*iphdr));

    uchar more_frag;
    if (payload_len + (iphdr.ihl << 2) > MTU) {
        // 首先确定需要分几片
        // 第一片带有IP hdr和 UDP hdr，后面的片没有 UDP hdr
        // 第一片的最大payload为1480
        int nfrag = payload_len / (MTU - IP_HDR_LEN);
        int last_frag_len = payload_len % (MTU - IP_HDR_LEN);
        nfrag = last_frag_len ? nfrag + 1 : nfrag;
        int each_size = MTU - IP_HDR_LEN;
        int left = payload_len;

        for (int i = 0; i < nfrag; i++) {
            iphdr.tot_len = 0;
            left -= each_size;
            more_frag = left > 0;
            iphdr.tot_len = htons(more_frag ? MTU : last_frag_len + IP_HDR_LEN);
            // calc frag off
            // 这里使用8个字节作为偏移单位，要求每个分片的长度是8字节的整数倍
            iphdr.frag_off = i * each_size / 8;
            // set tag 1
            iphdr.frag_off = iphdr.frag_off | 0x2000;

            if (more_frag) {
                memcpy(ip_packet + sizeof(iphdr), payload + i * each_size,
                       each_size);

            } else {
                iphdr.frag_off = iphdr.frag_off & 0xDFFF;  // more frag = 0
                // printf("last frag off %x\n", iphdr.frag_off);
                memcpy(ip_packet + sizeof(iphdr), payload + i * each_size,
                       each_size + left);
            }
            // iphdr.frag_off = htons(iphdr.frag_off);
            iphdr.check = 0;
            iphdr.frag_off = htons(iphdr.frag_off);
            iphdr.check = csum((ushort *)&iphdr, sizeof(iphdr));
            memcpy(ip_packet, &iphdr, sizeof(iphdr));

            // printf("frag off %x\n", iphdr.frag_off);
            send_to_datalink_layer(ip_packet, ntohs(iphdr.tot_len),
                                   ERROR_TAKEN);
        }
    } else {
        iphdr.check = csum((ushort *)&iphdr, sizeof(iphdr));

        memcpy(ip_packet, &iphdr, sizeof(iphdr));
        memcpy(ip_packet + sizeof(iphdr), payload, payload_len);
        send_to_datalink_layer(ip_packet, ntohs(iphdr.tot_len), ERROR_TAKEN);
    }
}
/**
 * 构造udp的header
 * @param sport 源端口
 * @param dport 目标端口
 * @param payload_len payload长度
 * @param udphdr udp的header结构
 */
void make_udphdr(ushort sport, ushort dport, ushort payload_len,
                 struct myudphdr *udphdr) {
    udphdr->sport = htons(sport);
    udphdr->dport = htons(dport);
    udphdr->len = htons(payload_len + UDP_HDR_LEN);
    udphdr->check = 0;
}

/**
 * 从应用层发往运输层
 * @param sport 源端口
 * @param dport 目标端口
 * @param payload payload结构
 * @param payload_len payload长度
 */
void send_to_tansport_layer(ushort sport, ushort dport, char *payload,
                            uint payload_len) {
    struct myudphdr hdr;
    char udp_datagram[MAX_IP_PACKET_LEN];
    make_udphdr(sport, dport, payload_len, &hdr);

    // check_udp();
    // printf("%d\n", sizeof(hdr));
    memcpy(udp_datagram, &hdr, sizeof(hdr));
    memcpy(udp_datagram + sizeof(hdr), payload, payload_len);
    // memcpy(udp_datagram + sizeof(hdr), payload, payload_len);

    uint udplen = payload_len + sizeof(hdr);
    // 伪首部 + UDP首部 + 数据一起计算校验和
    hdr.check = calc_check_udphdr(udp_datagram, udplen, srcip, dstip);
    // 更新udp头的check
    memcpy(udp_datagram, &hdr, sizeof(hdr));
    // printf("udphdr check: %x\n", hdr.check);
    send_to_network_layer(UDP_PROTOCAL, udplen, udp_datagram);
}

/**
 * 应用层发送数据
 * @param sport 源端口
 * @param dport 目标端口
 * @param data 要发送的数据
 */
void send_data(uint sport, uint dport, char *data) {
    // printf("%s %ld\n", data, strlen(data));
    send_to_tansport_layer(sport, dport, data, strlen(data));
}

// ------------------------------- server -----------------------------//

/**
 * 打印MAC地址
 * @param p mac地址
 */
void output_mac(mac_addr p) {
    printf("mac %02X:%02X:%02X:%02X:%02X:%02x ", p[0], p[1], p[2], p[3], p[4],
           p[5]);
}

/**
 * judge whether dst mac address is local address
 * @param x mac addr1
 * @param y mac addr2
 *
 * @return 1 if equal else 0
 */
int mac_equal(mac_addr x, mac_addr y) {
    for (int i = 0; i < 6; i++)
        if (x[i] != y[i]) return 0;
    return 1;
}

/**
 * IP头的校验
 * @param p ip头
 * @param len ip头长度
 * @return 1表示有错误，0表示正确
 */
int check_iphdr_error(struct myiphdr *p, int len) {
    // printf("p.check: %x res: %x\n", p->check, csum((ushort *)p, len));
    return csum((ushort *)p, len) != 0;
}

/**
 * 从网络层读取数据
 * 处理后直接是应用层显示了
 * @param nbyte 收到的总的字节数
 * @param packet ip packet
 * @param iniphdr ip 头
 * @param frame_index 帧序号（仅用来打印）
 */
void read_from_network_layer(int nbyte, char *packet, struct myiphdr *iniphdr,
                             int frame_index) {
    // 解析udp

    char udp_datagram[MAX_IP_PACKET_LEN];
    struct myudphdr inudphdr;
    char data[MAX_IP_PACKET_LEN];
    ushort leftlen = nbyte - MAC_HDR_LEN - MAC_FCS_LEN - IP_HDR_LEN;

    memcpy(udp_datagram, packet + IP_HDR_LEN, leftlen);

    udp_datagram[leftlen] = '\0';

    memcpy(&inudphdr, &udp_datagram, 8);
    // 先进行udp的检验
    ushort udphdr_eror = calc_check_udphdr(udp_datagram, ntohs(inudphdr.len),
                                           iniphdr->saddr, iniphdr->daddr);
    if (udphdr_eror) {
        printf(
            "[ERROR]: frame[%d] takes error caused by failed UDP "
            "checksum.\n",
            frame_index);
        return;
    }
    printf("[UDP]    source port %u | dest port %u\n", ntohs(inudphdr.sport),
           ntohs(inudphdr.dport));
    // printf("UDP len: %d\n", ntohs(inudphdr.len));
    strcpy(data, &udp_datagram[UDP_HDR_LEN]);
    printf("[DATA]   %s\n", data);
}

/**
 * 发数据线程
 */
void send_thread() {
    // port
    uint sport = 5000;
    uint dport = 8080;
    printf("send thread start...\n");
    // init long data
    // char long_data[5000];
    // for (int i = 0; i < 4000; i++) long_data[i] = 'a';
    // send_data(sport, dport, long_data);

    while (1) {
        char data[MAX_IP_PACKET_LEN];
        fgets(data, MAX_IP_PACKET_LEN, stdin);
        send_data(sport, dport, data);
    }
}

/**
 * 帧Frame的校验
 * @param cas_rec 接收方重新计算的cas值
 * @param buffer_in 收到的数据
 * @param nbyte 数据长度
 * @return -1表示有错误
 */
int frame_crc_check(uint cas_rec, char buffer_in[], int nbyte) {
    char cas_rec_str[4];  // 将crc32的数值转化为字符串数组
    memcpy(&cas_rec_str, &cas_rec, sizeof(cas_rec));
    if (cas_rec_str[0] != buffer_in[nbyte - 4] ||
        cas_rec_str[1] != buffer_in[nbyte - 3] ||
        cas_rec_str[2] != buffer_in[nbyte - 2] ||
        cas_rec_str[3] != buffer_in[nbyte - 1]) {
        printf("[ERROR]: frame[%d] cas check fail.\n", frame_index);
        return -1;
    }
    return 0;
}
/**
 * 从数据链路层读取数据
 * @param packet IP包
 * @param nbyte 字节数
 * @return -1表示IP头校验失败
 */
int read_from_datalink_layer(char packet[], int nbyte) {
    struct myiphdr iniphdr;
    ipaddr inipaddr;
    memcpy(&iniphdr, packet, IP_HDR_LEN);

    char dstip_c[16], srcip_c[16];
    ipaddr2str(iniphdr.daddr, dstip_c);
    ipaddr2str(iniphdr.saddr, srcip_c);
    // iniphdr.check = 0;
    // csum
    int iphdr_error = check_iphdr_error(&iniphdr, sizeof(iniphdr));

    if (iphdr_error) {
        printf(
            "[ERROR]: frame[%d] takes error caused by failed IP "
            "checksum.\n",
            frame_index);
        return -1;
    }
    iniphdr.tot_len = ntohs(iniphdr.tot_len);
    printf(
        "[IP]     from %s to %s | version %u | ihl %2u | tos %u | total "
        "len "
        "%4u | id %6u | "
        "ttl %4u | protocal %2u | check %x\n",
        srcip_c, dstip_c, iniphdr.version, iniphdr.ihl, iniphdr.tos,
        (iniphdr.tot_len), iniphdr.id, iniphdr.ttl, iniphdr.protocol,
        iniphdr.check);

    // 检查是否分片
    iniphdr.frag_off = ntohs(iniphdr.frag_off);
    if ((iniphdr.frag_off & 0x2000) || iniphdr.frag_off) {
        // 如果more frag 或者 frag 0ff 表示分片
        // 1. MF=1 and no offset 第一片
        // 2. MF=0 最后一片
        // 3. 其余情况 中间片

        int islast = !((iniphdr.frag_off & 0x2000) >> 13);
        ushort addroff = 8 * (iniphdr.frag_off & 0x1FFF);
        int isfirst = !islast && !addroff;
        // printf("addr off: %x\n", addroff);
        if (isfirst) {
            printf("\n(first fragment)\n");
            printf("----------------------------------------------\n");

            ip_packet_id = iniphdr.id;
            memcpy(store, packet, iniphdr.tot_len);
            first_iphdr = &iniphdr;
            total_byte += iniphdr.tot_len;

            // DEBUG UDP LEN
            struct myudphdr u;
            memcpy(&u, packet + IP_HDR_LEN, 8);
            // printf("udplen in first: %d\n", ntohs(u.len));

        } else if (islast) {
            memcpy(store + IP_HDR_LEN + addroff, packet + IP_HDR_LEN,
                   iniphdr.tot_len - IP_HDR_LEN);

            total_byte += iniphdr.tot_len - IP_HDR_LEN;
            // 进行重组

            read_from_network_layer(total_byte, store, first_iphdr,
                                    frame_index);
            // 清空
            ip_packet_id = -1;
            first_iphdr = NULL;
            bzero(store, sizeof(store));
            total_byte = MAC_FCS_LEN + MAC_HDR_LEN;
            printf("\n(last fragment)\n");
            printf("----------------------------------------------\n");
        } else if (ip_packet_id == iniphdr.id) {
            memcpy(store + IP_HDR_LEN + addroff, packet + IP_HDR_LEN,
                   iniphdr.tot_len - IP_HDR_LEN);
            total_byte += iniphdr.tot_len - IP_HDR_LEN;
        }
    } else {
        read_from_network_layer(nbyte, packet, &iniphdr, frame_index);
    }
}

/**
 * 接收数据
 */
void recv_data() {
    int nbyte;
    char buffer_in[1536];  // buffer to read from socket

    mac_addr src_mac;  // src mac address
    mac_addr dst_mac;  // dst mac address
    frame_index = 0;

    while (1) {
        buffer_in[0] = '\0';
        socklen_t addr_length = sizeof(struct sockaddr_ll);
        nbyte = recvfrom(sock_raw_fd, buffer_in, 1536, 0,
                         (struct sockaddr *)&sll_recv,
                         &addr_length);  // read from socket
        if (nbyte < MAC_HDR_LEN + MAC_FCS_LEN) {
            continue;
        }
        ushort protocal;  // protocal type
        // 解析mac帧
        memcpy(&dst_mac, &buffer_in, 6);       // get dst addr
        memcpy(&src_mac, &buffer_in[6], 6);    // get src addr
        memcpy(&protocal, &buffer_in[12], 2);  // get protocal type

        // 根据MAC地址过滤，只读取发往本地MAC的帧
        int eq = mac_equal(dst_mac, my_mac);
        if (!eq) {
            // 取消以下注释，输出发往非本机MAC帧的MAC地址
            // printf("[NOTE]: not frame to local. To ");
            // output_mac(dst_mac);
            // printf("\n");
            continue;
        }
        // 这里为了测试，自定义协议protocol为4376（学号尾号），过滤，只监听此协议的MAC帧
        if (protocal != ntohs(MY_ETH_PROTOCAL)) {
            continue;
        }

        // 更新帧号
        frame_index++;
        printf("\n");

        // 解析MAC帧的内容

        // 使用crc32进行FCS校验
        uint cas_rec = crc32(buffer_in, nbyte - 4);
        int check_res = frame_crc_check(cas_rec, buffer_in, nbyte);
        // 如果checksum出错，丢弃该帧
        if (check_res == -1) continue;

        // 输出MAC帧信息
        printf(
            "[MAC]    from MAC %02X:%02X:%02X:%02X:%02X:%02x | mac protocal %x "
            "| receive %d byte data\n",
            src_mac[0], src_mac[1], src_mac[2], src_mac[3], src_mac[4],
            src_mac[5], ntohs(protocal), nbyte);

        // 解析IP包

        // char *packet = (char *)malloc(2 * nbyte - MAC_FCS_LEN - MAC_HDR_LEN);
        char packet[MAX_FRAME_LEN];
        memcpy(packet, &buffer_in[MAC_HDR_LEN],
               nbyte - MAC_FCS_LEN - MAC_HDR_LEN);

        read_from_datalink_layer(packet, nbyte);
    }
}
/**
 * 读数据线程
 */
void recv_thread() {
    printf("recv thread start...\n");
    recv_data();
}

int main(int argc, char *argv[]) {
    // init random seed (for random packet id)
    srand(time(NULL));
    /**
     * create the raw socket
     * type is PF_PACKET, representing the data link layer
     * use sock_raw to send
     */
    sock_raw_fd = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL));

    struct ifreq req;  // network interface address

    // strncpy(req.ifr_name, "ens33", IFNAMSIZ);  // the network card name
    // on wsl:
    strncpy(req.ifr_name, "eth0", IFNAMSIZ);  // the network card name
    if (ioctl(sock_raw_fd, SIOCGIFHWADDR, &req) == 0) {
        memcpy(my_mac, &req.ifr_addr.sa_data, sizeof(my_mac));
    }
    output_mac(my_mac);
    printf("\n");
    // get network interface
    if (-1 == ioctl(sock_raw_fd, SIOCGIFINDEX, &req)) {
        perror("ioctl");
        close(sock_raw_fd);
        exit(-1);
    }

    // Assigns a value to the original socket address structure
    bzero(&sll_send, sizeof(sll_send));
    sll_send.sll_family = AF_PACKET;
    sll_send.sll_protocol =
        htons(ETH_P_ALL);  // specifies the frame protocol type
    sll_send.sll_halen = ETH_ALEN;
    sll_send.sll_ifindex =
        req.ifr_ifindex;  // the network card to send frames from

    // bind raw socket
    if (bind(sock_raw_fd, (struct sockaddr *)&sll_send, sizeof(sll_send)) ==
        -1) {
        perror("bind error\n");
        exit(1);
    }
    // init ip config

    char dstip_c[] = "172.17.0.5";
    dstip = ipstr2addr(dstip_c);
    srcip = ipstr2addr(myipaddr_c);

    // 启动多线程
    pthread_t send_id, recv_id;
    // 发数据线程
    if (pthread_create(&send_id, NULL, (void *)send_thread, NULL) != 0) {
        printf("Create pthread error!\n");
        exit(1);
    }
    // 读数据线程
    if (pthread_create(&recv_id, NULL, (void *)recv_thread, NULL) != 0) {
        printf("Create pthread error!\n");
        exit(1);
    }
    pthread_join(send_id, NULL);
    pthread_join(recv_id, NULL);
    return 0;
}
