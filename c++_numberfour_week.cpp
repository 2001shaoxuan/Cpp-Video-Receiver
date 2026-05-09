// c++_numberfour_week.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//
#include <iostream>
#include <string>
#include <atomic>
#include <thread>
#include <vector>
#include"RTPHeader.h"
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib") // 告诉 Windows 链接网络库
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#define SOCKET int
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#endif

// 初始化：Windows 特有，Linux 下是空的
void init_network() {
#ifdef _WIN32
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
}


int TestUdp() {
    init_network();
    //创建邮筒
    //使用IPV4协议  AF_INET
    //使用数据包模式  udp  SOCK_DGRAM;
    SOCKET sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == INVALID_SOCKET) {
        std::cerr << "创建套接字失败" << std::endl;
        return 1;
    }
    //绑定地址和端口
    sockaddr_in local_addr;
    local_addr.sin_family = AF_INET;
    local_addr.sin_port = htons(8889); // 监听端口
    local_addr.sin_addr.s_addr = INADDR_ANY; // 监听所有接口

    if (bind(sock, (sockaddr*)&local_addr, sizeof(local_addr)) == SOCKET_ERROR) {
        std::cerr << "绑定失败" << std::endl;
        return 1;
    }
    std::cout << "UDP服务器已启动，等待消息..." << std::endl;

    std::atomic<bool> running{ true };

    //接收消息
    std::thread recv_thread([sock, &running]() {
        char buffer[1024];//接收缓冲区
        sockaddr_in client_addr;//客户端地址
        socklen_t addr_len = sizeof(client_addr);//地址长度


        // 增加：用于记录上一次收到的序列号，用来检测丢包
        uint16_t expected_seq = 0;
        bool is_first_packet = true;


        while (running.load()) {
            int recv_len = recvfrom(sock, buffer, sizeof(buffer) - 1, 0,
                (struct sockaddr*)&client_addr, &addr_len);// -1 代表错误，0 代表连接关闭

            // 1. 安全检查：收到的数据必须至少包含一个 RTP 头部（12字节）
            if (recv_len < sizeof(RTPHeader)) {
                std::cerr << "收到残缺包，直接丢弃" << std::endl;
                continue;
            }
            // 2. 暴力拆包！把 buffer 的前 12 字节强行当作 RTPHeader 来读取
            RTPHeader* header = (RTPHeader*)buffer;

         
            // 3. 提取序列号（从网络序转回本机序，用 ntohs）
            uint16_t received_seq = ntohs(header->seq_num);

			//4. 丢包检测：如果不是第一包，并且收到的序列号不等于预期的序列号，说明中间丢包了
			if (!is_first_packet && received_seq != expected_seq) {
                // 如果发现收到的包不是我们期望的序号，说明中间有包走丢了！
                std::cout << "\n[！！！网络波动！！！] 发生丢包！"
                    << " 期望包号: " << expected_seq
                    << " 实际收到包号: " << received_seq << "\n> " << std::flush;
            }
            expected_seq = received_seq + 1; // 期待下一个包
            is_first_packet = false;
            // 5. 提取真正的货物（Payload 数据）
            // 真实数据的长度 = 总接收长度 - 头部长度
            int payload_len = recv_len - sizeof(RTPHeader);
            if (payload_len > 0) {
                buffer[recv_len] = '\0'; // 确保字符串以 null 结尾

                // 跳过头部的 12 个字节，直接读取后面的字符串
                char* payload_data = buffer + sizeof(RTPHeader);

                char ip_str[INET_ADDRSTRLEN] = {};
#ifdef _WIN32
                InetNtopA(AF_INET, &client_addr.sin_addr, ip_str, INET_ADDRSTRLEN);
#else
                inet_ntop(AF_INET, &client_addr.sin_addr, ip_str, INET_ADDRSTRLEN);
#endif
               

                std::cout << "\n[RTP包 序号:" << received_seq << "]"
                    << " 来自 " << ip_str << ":" << ntohs(client_addr.sin_port)
                    << " 内容: " << payload_data << "\n> " << std::flush;
            }
            else if (recv_len == SOCKET_ERROR) {
                if (!running.load()) {
                    break;
                }
                std::cerr << "接收失败" << std::endl;
            }
        }
        });


    //设置发送目标地址
    sockaddr_in target_addr;
    target_addr.sin_family = AF_INET;
    target_addr.sin_port = htons(8888); // 目标端口
    inet_pton(AF_INET, "127.0.0.1", &target_addr.sin_addr); // 目标IP地址

    std::string message;
    //在进入while循环之前，定义一个本地的序列号计数器
    // uint32_t current_timestamp = 0; // 如果做真正的音视频，还需要时间戳，今天我们先专注 seq_num
	uint16_t current_seq = 0;


    while (true)
    {
        std::cout << ">";
        std::getline(std::cin, message);
        if (message == "exit") {
            break; // 输入 exit 退出程序
        }
        //1.组装RTP头部
		RTPHeader header = { 0 };
		header.version = 2;     // 固定为 2
		header.payload_type = 96;//随便定一个值，代表我们自定义的视频数据

        // 【核心重点】：本地是小端序，网络是大端序，必须用 htons 转换！
        header.seq_num = htons(current_seq);
        header.ssrc = htonl(123456); // 32位整数用 htonl (Host to Network Long)

        // 2. 准备一个足够大的缓冲区，把头部和数据“拼”在一起
    // 大小 = 头部大小 (12字节) + 字符串大小
        std::vector<char> send_buf(sizeof(RTPHeader) + message.size());

        //3.把头部拷贝到缓冲区最前面
		memcpy(send_buf.data(), &header, sizeof(RTPHeader));
		//4.把消息内容紧挨着头部拷贝到缓冲区
		memcpy(send_buf.data() + sizeof(RTPHeader), message.data(), message.size());

        // 5. 发送整个拼装好的包裹
        int send_len = sendto(sock, send_buf.data(), send_buf.size(), 0,
            (struct sockaddr*)&target_addr, sizeof(target_addr));

        if (send_len == SOCKET_ERROR) {
            std::cerr << "发送失败" << std::endl;
        }
        else {
			//发送成功后，本地的序列号计数器递增，为下一次发送做准备
			current_seq++;
        }
    }

    running.store(false);

#ifdef _WIN32
    closesocket(sock);
    WSACleanup();
#else
    close(sock);
#endif

    if (recv_thread.joinable()) {
        recv_thread.join();
    }
    return 0;
}


int main()
{
    //std::cout << "RTP Header 的大小: " << sizeof(RTPHeader) << " 字节" << std::endl;
    //std::cout << "Test 结构体的大小: " << sizeof(Test) << " 字节" << std::endl;
    //// 初始化一个头部看看
    //RTPHeader header = { 0 }; // 全部清零
    //header.version = 2;     // 固定为 2
    //header.payload_type = 96; // 假设 96 代表某种自定义视频数据
    //header.seq_num = 0;
    TestUdp();
}

// 运行程序: Ctrl + F5 或调试 >“开始执行(不调试)”菜单
// 调试程序: F5 或调试 >“开始调试”菜单

// 入门使用技巧: 
//   1. 使用解决方案资源管理器窗口添加/管理文件
//   2. 使用团队资源管理器窗口连接到源代码管理
//   3. 使用输出窗口查看生成输出和其他消息
//   4. 使用错误列表窗口查看错误
//   5. 转到“项目”>“添加新项”以创建新的代码文件，或转到“项目”>“添加现有项”以将现有代码文件添加到项目
//   6. 将来，若要再次打开此项目，请转到“文件”>“打开”>“项目”并选择 .sln 文件
