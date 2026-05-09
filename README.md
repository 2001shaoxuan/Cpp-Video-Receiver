# 🚀 抗弱网模拟视频接收引擎 (Anti-Jitter Video Receiver Engine)

![C++](https://img.shields.io/badge/C++-11/14/17-blue.svg)
![Platform](https://img.shields.io/badge/Platform-Windows%20%7C%20Linux-lightgrey.svg)
![Protocol](https://img.shields.io/badge/Protocol-UDP%20%7C%20RTP-green.svg)

## 📖 项目简介
本项目是一个基于 C++ 开发的底层音视频数据流接收引擎，主要用于模拟 WebRTC 等实时通信场景下的核心接收端逻辑。项目完全脱离高层网络框架，**从零手写底层 Socket 通信、二进制协议拆包以及多线程并发调度的全过程**。

通过实现基于生产者-消费者模型的有界抖动缓冲队列（Jitter Buffer），本项目能够有效应对弱网环境，并具备精准的丢包检测与跨平台运行能力。

## ✨ 核心特性与技术栈

### 1. 🌐 纯手工底层网络协议解析 (RTP/UDP)
* **无棉花内存对齐**：使用 `#pragma pack(push, 1)` 严谨控制结构体内存布局，完美贴合 RFC 3550 协议规定的 12 字节 RTP Header，杜绝网络传输中的内存碎片与脏数据。
* **零拷贝报文解包**：摒弃低效的字符串拼接，采用 `reinterpret_cast` 指针偏移直接切片内存，实现 O(1) 复杂度的网络数据流到结构体的零拷贝映射。
* **大小端字节序转换**：熟练运用 `ntohs`/`htons` 处理跨端序的网络字节解析，保证跨架构通信的精确性。

### 2. ⚡ 高性能多线程并发控制
* **基于条件变量的 Jitter Buffer**：使用 `std::mutex` 与 `std::condition_variable` 实现线程安全的有界缓冲队列，严格处理**虚假唤醒 (Spurious Wakeup)** 问题。
* **无损移动语义**：利用 C++11 `std::move` 实现底层 VideoFrame 数据在各个线程队列间的流转，避免大块 Payload（如视频帧数据）的深度拷贝操作。
* **无死锁的优雅退出机制 (Graceful Shutdown)**：通过 `std::atomic` 原子变量与条件变量的精准唤醒，实现主控信令下发后，接收线程与渲染线程瞬间且安全的资源回收。

### 3. 🛡️ 弱网环境抵抗与检测
* 实时校验网络包的 Sequence Number (序列号)。
* 针对 UDP 不可靠传输特性，在渲染消费者侧实现精准的乱序、跳帧与丢包预警逻辑。

---

## 🏗️ 架构设计

项目采用经典的 **Producer-Consumer (生产者-消费者)** 架构划分网络层与渲染层：

```text
[发送端 (Sender)] 
      | (UDP 数据流 / 模拟弱网)
      v
+-----------------------+     +-------------------------+     +-----------------------+
| Network Thread        |     | Thread-Safe JitterBuffer|     | Render Thread         |
| (Producer)            |     | (std::mutex + cond_var) |     | (Consumer)            |
|-----------------------|     |-------------------------|     |-----------------------|
| 1. recvfrom 监听      |====>| 容量: Max Frames        |====>| 1. 取出 VideoFrame    |
| 2. reinterpret_cast   |     | push() / pop()          |     | 2. Seq Number 丢包检测|
| 3. ntohs 端序转换     |     | stop() 唤醒所有等待     |     | 3. 模拟视频帧渲染展示 |
+-----------------------+     +-------------------------+     +-----------------------+
