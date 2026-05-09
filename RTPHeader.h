#pragma once
#include <cstdint>
#pragma pack(push, 1) // 告诉编译器：接下来的结构体必须 1 字节对齐，绝对不许塞任何“棉花”！
//如果不是1字节对齐，编译器会在结构体成员之间插入一些“棉花”，
// 以满足默认的对齐要求。这会导致结构体的实际大小大于其成员大小之和，从而影响数据的正确解析。
struct RTPHeader
{
	//第一个字节
	//个人电脑是小端序，网络传输是大端序，所以在网络传输时需要将多字节数据转换为大端序。
	uint8_t csrc_count : 4;  // CC: CSRC 计数
	uint8_t extension : 1; //扩展标志，占1位
	uint8_t padding : 1; //填充标志，占1位
	uint8_t version : 2; //V 版本号


	//第二个字节
	uint8_t payload_type : 7; //PT: 负载类型（比如传视频可以随便定一个 96，传音频定 8）
	uint8_t marker : 1; //M: 标记位（通常用来表示一帧视频的最后一个包）

	// --- 第 3-4 个字节 ---
	uint16_t seq_num;        // 序列号（每发一个包 +1，用来检测丢包）

	// --- 第 5-8 个字节 ---
	uint32_t timestamp;      // 时间戳（用来做音视频同步播放）

	// --- 第 9-12 个字节 ---
	uint32_t ssrc;           // 同步源（随机生成一个数，代表你的电脑）

};
#pragma pack(pop) // 恢复编译器默认的对齐方式，以免影响其他正常代码

struct Test {
	char a;
	int b;
};