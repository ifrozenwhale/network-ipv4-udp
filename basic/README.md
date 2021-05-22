# 程序说明
在Linux上使用C语言在数据链路层上通信，实现IPv4和UDP协议以及应用层封装。
## 文件说明

文件目录结构如下

```
.
├── Makefile
├── chat.c
├── client.c
├── myhdr.h
├── utils.c
└── utils.h
1 directory, 6 files
```

- `Makefile`编译多文件。
- `chat.c`，用于展示聊天的内容（只输出了最终发送和收到的结果）
- `client.c`，用于展示帧的解析封装、分片等过程。
- `myhdr.h`，定义了自己的IP头，UDP头，以及其他一些数据结构和常量值。
- `utils.h`，声明了一些工具函数，主要有校验和计算，ip地址转换等。
- `utils.c`，工具函数的实现。

## 功能实现

### 数据链路层（Ethernet Frame）

1. 使用第一个project要求的帧结构`dst_mac(6) | src_mac(6) | protocal(2) | payload(46-1500) | fcs(4) `。
2. fcs使用crc32算法进行计算，接收端如果校验失败抛出异常。
3. payload长度不足46时，使用`bzero`进行0比特填充，长于1500则抛出异常。

### 网络层（IP packet）

1. 使用自定义的固定的IPv4头（20字节）。

2. 头检验和check使用16位半字相加取结果补码，接收端如果校验失败抛出异常。

3. IP分片

   发送时

   - 每一片都具有IP头，但是只有第一片有UDP头。
   - 偏移值offset使用8个字节作为偏移单位，因此每一片的长度为8的整数倍。

   接收时如如果MF（more frag)=1或者frag 0ff不为0表示分片

   - MF=1并且 offset偏移0，为第一片
   - MF=0，为最后一片
   - 其余情况，中间片

   根据IP的id进行分片重组。

### 运输层（UDP datagram）

1. 使用了自定义的UDP头（8字节）。
2. UDP伪首部 、UDP首部 、UDP的数据一起计算校验和，接收端如果校验失败抛出异常。

### 应用层（Chat）

1. 使用多线程库，分别创建读线程和发线程，用于双向的聊天。
2. 输出解析后的MAC帧结构信息、IP Packet结构信息，UDP datagram结构信息。

### 其他

1. 对于非unsigned char类型，使用`htons`和`ntohs`进行网络字节序的转化。
2. 使用`raw socket`进行数据链路层的通信。

# 展示

## 运行环境说明

- 使用了`raw socket`在linux环境中运行。这里使用了`Windows Subsystem for Linux(WSL)`作为linux环境。

- 需要修改`client.c`或者`chat.c`中前面定义的`dst_mac`数组为要发送的mac地址。

  ```c
  mac_addr dst_mac = {0x00, 0x15, 0x5d, 0xda, 0x50, 0x66};  // dst MAC 地址
  ```

- 编写了Makefile，用于编译多文件。Makefile文件内容如下：

  ```makefile
  project: client.c chat.c utils.o # 编译整个工程所有的文件
  	gcc utils.o client.c -o client -lpthread
  	gcc utils.o chat.c -o chat -lpthread
  	gcc -c utils.c 
  clientrun: client.c utils.c # 编译详细输出内容的程序，并运行
  	gcc -c utils.c
  	gcc utils.o client.c -o client -lpthread
  	sudo ./client
  chatrun: chat.c utils.c # 编译用于测试聊天的程序，并运行
  	gcc -c utils.c
  	gcc utils.o  chat.c -o chat -lpthread
  	sudo ./chat 
  clean: # 清除所有的文件
  	rm client
  	rm chat
  	rm utils.o
  ```

## 聊天形式（只显示数据内容）

（仅用于测试）发送端和接收端为同一个MAC地址

```bash
	make chatrun
```

1. 分别在两个终端执行上述命令，启动程序

   ![image-20201226130034525](https://frozenwhale.oss-cn-beijing.aliyuncs.com/img/image-20201226130034525.png)

2. 两个终端互发消息，如下：

   ![image-20201226130341408](https://frozenwhale.oss-cn-beijing.aliyuncs.com/img/image-20201226130341408.png)



## 帧的解析和封装体现

```bash
	make clientrun
```

1. 短消息互发（不分片）

   ![image-20201226131056879](https://frozenwhale.oss-cn-beijing.aliyuncs.com/img/image-20201226131056879.png)

2. 长消息（分片，MTU设置为44，即有效数据长度为44-20=24）

   ![image-20201226131246044](https://frozenwhale.oss-cn-beijing.aliyuncs.com/img/image-20201226131246044.png)

   `My name is Tian Runze, and who are you? `长度为41，被分为3片。在最后一片中被正确的组装。

   回复消息，`Wow, I am Tian Runze, too!hhh`，长度为31，被分为2片，最后一片被正确组装打印出来。

   ![image-20201226131735408](https://frozenwhale.oss-cn-beijing.aliyuncs.com/img/image-20201226131735408.png)

## 抓包测试

> 由于前两组发消息，都是发往了本机MAC，因此并不能被wireshark抓包抓到。这里使用修改dst mac addr。
（仅用于测试）发送端和接收端为不同MAC地址。

1. 短消息

   ![image-20201226132414865](https://frozenwhale.oss-cn-beijing.aliyuncs.com/img/image-20201226132414865.png)

   被正确的解析为UDP协议

2. 长消息，被分为3片，且成功被wireshark解析到。

   ![image-20201226132758462](https://frozenwhale.oss-cn-beijing.aliyuncs.com/img/image-20201226132758462.png)



## 调试信息

### MAC地址、协议过滤

1. 在网卡模式为混杂模式下，会发现有许多发往非本机MAC的帧被监听到，因此可以首先使用MAC地址进行过滤，只捕捉发往本机MAC地址的帧。

   ```c
   // 根据MAC地址过滤，只读取发往本地MAC的帧
   int eq = mac_equal(dst_mac, my_mac);
   if (!eq) {
       // 取消以下注释，输出发往非本机MAC帧的MAC地址
       printf("[NOTE]: not frame to local. To ");
       output_mac(dst_mac);
       printf("\n");
       continue;
   }
   ```

   ![image-20201226135934272](https://frozenwhale.oss-cn-beijing.aliyuncs.com/img/image-20201226135934272.png)

2. 过滤掉发往非本机MAC的帧，发现仍有部分帧，但是这些帧的FCS校验错误。（没有具体探讨原因，程序运行在windows的子系统linux内核中，不清楚这些帧的来源和结构、协议）

   ![image-20201226140122349](https://frozenwhale.oss-cn-beijing.aliyuncs.com/img/image-20201226140122349.png)

3. 因此，实际检测自己的封装和解析的时候，通过自定义了protocol类型来过滤。这里使用学号的尾号4376来进行过滤。只监听自己实现的封装的帧。

### 校验和测试

由于实际发帧的时候很少出错，因此可以通过强制出错的方式来检测FCS校验等。

#### Mac Frame出错

修改`myhdr.h`中的宏定义`ERROR_TAKEN`为1，即可强制MAC帧出错。

![image-20201226140937442](https://frozenwhale.oss-cn-beijing.aliyuncs.com/img/image-20201226140937442.png)

### IP packet 出错

为了检测IP的校验和，修改`myhdr.h`中的宏定义`ERROR_TAKEN`为2，即可强制IP packet出错（但是此时MAC帧也是错的），这里仅为了测试，MAC帧出错时忽略（以模拟帧FCS没有校验出错误的情况）。

![image-20201226141728968](https://frozenwhale.oss-cn-beijing.aliyuncs.com/img/image-20201226141728968.png)

### UDP Datagram 出错

为了检测UDP的校验和，修改`myhdr.h`中的宏定义`ERROR_TAKEN`为2，即可强制UDP datagram出错（但是此时MAC帧也是错的），这里仅为了测试，MAC帧出错时忽略（以模拟帧FCS没有校验出错误的情况）。

![image-20201226142036807](https://frozenwhale.oss-cn-beijing.aliyuncs.com/img/image-20201226142036807.png)

以上，所有的功能和测试均实现。



### Git管理

使用了git进行版本管理，从帧的封装开始，到IP封装、UDP封装，然后做UDP的校验，IP分片重组，最后做多线程的聊天应用。

![image-20201226143057640](https://frozenwhale.oss-cn-beijing.aliyuncs.com/img/image-20201226143057640.png)
