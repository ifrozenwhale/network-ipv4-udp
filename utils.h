
#define POLYNOMIAL 0xEDB88320  // used for crc32
unsigned short csum(unsigned short *src, int count);
unsigned int ipstr2addr(const char *c_ipaddr);
void ipaddr2str(unsigned int in, char result[]);
unsigned int crc32(char *data, int len);