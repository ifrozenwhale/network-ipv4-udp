#include "utils.h"

#include <ctype.h>

#include "stdio.h"
char *int_to_binary(int n) {
    static char str[100] = "";
    int temp;
    temp = n % 2;
    n = n >> 1;

    if (n != 0) {
        int_to_binary(n);
    }
    char num_str[2];               //字符串
    sprintf(num_str, "%d", temp);  //数字转字符串
    strcat(str, num_str);

    return str;
}
/**
 * checking algorithm  - crc32
 * refer to the network
 * @param data char*, string to be check
 * @param len  the length of string
 *
 * @return crc32 result
 */
unsigned int crc32(char *data, int len) {
    unsigned int table[256];
    for (int i = 0; i < 256; i++) {
        table[i] = i;
        for (int j = 0; j < 8; j++) {
            table[i] = (table[i] >> 1) ^ ((table[i] & 1) ? POLYNOMIAL : 0);
        }
    }
    unsigned int crc = 0;
    crc = ~crc;
    for (int i = 0; i < len; i++)
        crc = (crc >> 8) ^ table[(crc ^ data[i]) & 0xff];
    return ~crc;
}
unsigned short csum(unsigned short *src, int count) {
    register unsigned short *buffer = src;
    register long sum = 0;
    while (count > 1) {
        // This is the inner loop
        sum += *buffer++;
        // printf("%x ", *(buffer - 1));
        // printf("%x sum %x |", *(buffer - 1), (unsigned int)sum);
        count -= 2;
    }
    // printf("\n");
    // Add left-over byte, if any
    if (count > 0) sum += *(unsigned char *)buffer;
    // Fold 32-bit sum to 16 bits
    while (sum >> 16) sum = (sum & 0xffff) + (sum >> 16);
    return ~sum;
}
unsigned int ipstr2addr(const char *c_ipaddr) {
    unsigned int u_ipaddr = 0, u_tmp = 0;
    int i_base = 10, i_shift = 0, i_recycle = 0;
    char c = *c_ipaddr;
    while (1) {
        u_tmp = 0;
        while (1) {
            if (isdigit(c)) {
                u_tmp = (u_tmp * i_base) + (c - 0x30);
                c = *++c_ipaddr;
            } else
                break;
        }
        //字节移位，注意网络字节序是大端模式
        i_shift = 8 * i_recycle++;
        u_tmp <<= i_shift;
        u_ipaddr += u_tmp;
        //对点(.)符号的处理
        if (c == '.') {
            c = *++c_ipaddr;
        } else
            break;
    }
    // printf("ipstr: %u\n", u_ipaddr);
    return u_ipaddr;
}
void ipaddr2str(unsigned int in, char result[]) {
    unsigned char bytes[4];
    // 这里不能定义为char 调了一晚上
    bytes[0] = (in >> 24) & 0xFF;
    bytes[1] = (in >> 16) & 0xFF;
    bytes[2] = (in >> 8) & 0xFF;
    bytes[3] = in & 0xFF;
    sprintf(result, "%d.%d.%d.%d", bytes[3], bytes[2], bytes[1], bytes[0]);
}
