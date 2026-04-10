# Child Friend AI - 儿童陪伴 AI 机器人

## 1. 项目概述

### 1.1 项目背景

Child Friend AI 是一款基于 ESP32-S3 和树莓派的离线语音 AI 陪伴机器人，专为儿童设计，提供自然语言对话交互。

### 1.2 核心特性

- **唤醒词对话**：使用"你好小友"唤醒词激活对话（无需按钮）
- **离线运行**：完全本地化处理，保护儿童隐私
- **长期记忆**：支持 30 天跨会话对话记忆，语义搜索
- **多角色预设**：朋友、老师两种对话风格
- **语音交互**：自然语音对话，屏幕显示状态
- **低成本硬件**：ESP32-S3 + 树莓派 4B

### 1.3 技术选型

| 组件 | 技术方案 | 说明 |
|------|----------|------|
| 语音端 | ESP32-S3 | 语音采集/播放/显示 |
| 唤醒词 | ESP-SR WakeNet | 乐鑫官方硬件级唤醒词 |
| 语音识别 | Vosk | 离线中文 ASR |
| 语音合成 | Piper / eSpeak-ng | 离线 TTS |
| 对话引擎 | ZeroClaw | 记忆系统 + LLM |
| 本地 LLM | Ollama + Qwen2.5 | 离线推理 |

### 1.4 参考项目

| 项目 | 参考内容 | 地址 |
|------|----------|------|
| WalkieClaw | ESP32 音频流传输、WebSocket 通信协议 | github.com/slsah30/WalkieClaw |
| Cheeko | OpenClaw 插件集成、OTA 升级 | @cheeko-ai/esp32-voice |
| pizero-openclaw | 树莓派语音方案、LCD 显示 | OpenClaw 社区 |
| ZeroClaw 硬件外设 | 外设控制架构、GPIO/I2C/SPI | zeroclaw 官方文档 |

---

## 2. 系统架构

### 2.1 整体架构

```
┌─────────────────────────────────────────────────────────────┐
│                    树莓派 4B                               │
│                                                              │
│   ┌─────────────────────────────────────────────────────┐  │
│   │                   ZeroClaw Runtime                    │  │
│   │   ┌──────────┐  ┌──────────┐  ┌──────────────┐   │  │
│   │   │  SQLite  │  │  Ollama  │  │  Personality │   │  │
│   │   │  Memory  │  │ (Qwen2.5)│  │   Presets    │   │  │
│   │   └────┬─────┘  └────┬─────┘  └──────────────┘   │  │
│   │        │              │                             │  │
│   │   混合搜索          本地推理                       │  │
│   │   (向量+BM25)      (0.5B-1.5B)                   │  │
│   └───────────────────────┼─────────────────────────────┘  │
│                           │                                 │
│                    ┌──────┴──────┐                        │
│                    │ WebSocket   │                        │
│                    │  Server     │                        │
│                    └──────┬──────┘                        │
│                           │                                 │
└───────────────────────────┼─────────────────────────────────┘
                            │ WiFi
                            ▼
┌─────────────────────────────────────────────────────────────┐
│                   ESP32-S3 (Voice)                         │
│                                                              │
│   ┌─────────────────────────────────────────────────────┐  │
│   │                    firmware/                          │  │
│   │                                                      │  │
│   │   ┌──────────┐   ┌──────────┐   ┌──────────────┐  │  │
│   │   │   WiFi   │   │  Audio   │   │   Display    │  │  │
│   │   │ Manager  │   │ Service  │   │   (ST7735)   │  │  │
│   │   └────┬─────┘   └────┬─────┘   └──────────────┘  │  │
│   │        │              │                             │  │
│   │        └──────────────┼───────────────────────────  │  │
│   │                       │                            │  │
│   │               ┌────────┴────────┐                 │  │
│   │               │ WebSocket Client │                 │  │
│   │               └────────┬────────┘                 │  │
│   └───────────────────────┼─────────────────────────────┘  │
│                           │                                  │
│   ┌───────────────────────┴─────────────────────────────┐  │
│   │                     Hardware                          │  │
│   │                                                      │  │
│   │   INMP441 ◄─────────────► MAX98357A                │  │
│   │   (Mic I2S)              (Speaker I2S)              │  │
│   │                                                      │  │
│   │   ┌──────────────┐                                  │  │
│   │   │   ST7735    │                                  │  │
│   │   │   0.96"     │                                  │  │
│   │   │   160x80    │                                  │  │
│   │   └──────────────┘                                  │  │
│   └──────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────┘
```

### 2.2 数据流

```
用户说话 → INMP441 麦克风 → I2S 采集 → ESP32
                                          │
                                          ▼ WebSocket (二进制音频)
                                    树莓派 ZeroClaw
                                          │
                                          ▼
                              Vosk ASR → LLM → TTS
                                          │
                                          ▼ WebSocket (二进制音频)
                                    ESP32 → I2S 播放 → MAX98357A → 扬声器
                                          │
                                          ▼
                                    ST7735 屏幕显示状态和对话内容
```

---

## 3. 硬件设计

### 3.1 硬件清单

| 组件 | 型号 | 数量 | 说明 |
|------|------|------|------|
| 主控 | ESP32-S3-N16R8 | 1 | 16MB Flash + 8MB PSRAM |
| 麦克风 | INMP441 | 1 | I2S 数字 MEMS 麦克风 |
| 功放 | MAX98357A | 1 | I2S D 类功放 |
| 屏幕 | ST7735 | 1 | 0.96" 160x80 RGB LCD |
| 存储 | SPI Flash | 1 | 可选，用于离线模型 |

### 3.2 ESP32-S3 引脚分配

#### I2S 音频接口

| 功能 | GPIO | 说明 |
|------|------|------|
| I2S BCK | 12 | 音频时钟 |
| I2S WS | 14 | 字选通 |
| I2S DIN (Mic) | 15 | 麦克风输入 |
| I2S DOUT (Spk) | 13 | 扬声器输出 |

#### SPI 屏幕接口 (ST7735)

| 功能 | GPIO | 说明 |
|------|------|------|
| LCD CS | 5 | 片选 |
| LCD DC | 2 | 命令/数据 |
| LCD RST | 4 | 复位 |
| LCD SCK | 6 | 时钟 |
| LCD MOSI | 7 | 数据 |
| LCD BLK | 8 | 背光 |

#### 电源和状态

| 功能 | GPIO | 说明 |
|------|------|------|
| POWER EN | 21 | 电源使能 |
| LED | 48 | 状态指示灯 |

### 3.3 硬件接线图

```
ESP32-S3
┌─────────────────────────────────────────┐
│                                         │
│  GPIO12 ──────── I2S BCK               │
│  GPIO14 ──────── I2S WS                │
│  GPIO15 ──────── I2S DIN (INMP441 SD) │
│  GPIO13 ──────── I2S DOUT (MAX98357)  │
│                                         │
│  GPIO5  ──────── LCD CS                │
│  GPIO2  ──────── LCD DC                │
│  GPIO4  ──────── LCD RST               │
│  GPIO6  ──────── LCD SCK               │
│  GPIO7  ──────── LCD MOSI              │
│  GPIO8  ──────── LCD BLK               │
│                                         │
│  GPIO48 ──────── Status LED            │
│                                         │
└─────────────────────────────────────────┘

INMP441 (麦克风)          MAX98357A (功放)
┌──────────────┐          ┌──────────────┐
│ VDD   ───3.3V│          │ VIN   ───5V  │
│ GND   ───GND │          │ GND   ───GND │
│ SCK   ──GPIO12│          │ BCLK  ──GPIO12│
│ WS    ──GPIO14│          │ LRC    ──GPIO14│
│ SD    ──GPIO15│          │ DIN    ──GPIO13│
│ LR    ──GND   │          │ GAIN   ──GND   │
└──────────────┘          └──────────────┘

ST7735 (屏幕)
┌──────────────┐
│ VCC   ───3.3V│
│ GND   ───GND │
│ CS    ──GPIO5 │
│ DC    ──GPIO2 │
│ RST   ──GPIO4 │
│ SCK   ──GPIO6 │
│ MOSI  ──GPIO7 │
│ BLK   ──GPIO8 │
└──────────────┘
```

---

## 4. 软件设计

### 4.1 ESP32 固件模块

```
firmware/
├── main/
│   ├── main.c              # 入口程序
│   ├── Kconfig.projbuild  # menuconfig 配置
│   ├── board.c/h          # 硬件抽象层
│   ├── state_machine.c/h  # 设备状态机
│   ├── websocket_client.c/h # WebSocket 客户端
│   ├── audio_codec.c/h    # I2S 音频编解码
│   ├── audio_service.c/h  # 音频服务 (唤醒词/VAD)
│   └── display.c/h        # ST7735 屏幕驱动
├── components/
│   └── st7735s/          # ST7735 屏幕组件
├── CMakeLists.txt
├── sdkconfig
└── partitions.csv
```

### 4.2 状态机

```
STARTING ──► IDLE ──► LISTENING ──► CONNECTING ──► SPEAKING
                    │              │              │
                    │              │              │
                    └──────────────┴──────────────┘
```

| 状态 | 描述 | 屏幕显示 |
|------|------|----------|
| STARTING | 启动中 | 显示 Logo |
| IDLE | 空闲，等待唤醒 | "Say Hi!" |
| LISTENING | 正在听 | 聆听动画 |
| CONNECTING | 处理中 | "Thinking..." |
| SPEAKING | 正在说话 | 说话动画 |

### 4.3 通信协议

#### 4.3.1 传输层

- **协议**：WebSocket (ws://)
- **连接**：ESP32 主动连接树莓派
- **端口**：12020
- **重连机制**：自动重连，间隔 3s

#### 4.3.2 报文格式

全部使用 JSON 格式，便于调试和扩展。

##### ESP32 → 树莓派

```json
// 唤醒词检测
{"type": "wakeword", "detected": true}

// 状态消息
{"type": "status", "content": "listening"}
{"type": "status", "content": "processing"}
{"type": "status", "content": "speaking"}

// 音频数据 (Base64 编码)
{"type": "audio", "data": "base64-encoded-pcm"}

// 音频结束
{"type": "audio_end"}
```

##### 树莓派 → ESP32

```json
// 文本响应
{"type": "text", "content": "你好呀！今天过得怎么样？", "speaker": "friend"}

// TTS 音频数据 (Base64 编码)
{"type": "tts", "data": "base64-encoded-pcm"}

// 状态更新
{"type": "status", "content": "idle"}

// 错误消息
{"type": "error", "message": "ASR failed"}
```

#### 4.3.3 数据规范

| 字段 | 类型 | 说明 |
|------|------|------|
| audio | PCM | 16kHz, 16bit, mono |
| base64 | 标准 | 无换行、无 padding |
| encoding | UTF-8 | JSON 文本编码 |

---

## 5. 树莓派服务端设计

### 5.1 目录结构

```
server/
├── src/
│   └── server.py          # WebSocket 转发服务
├── models/
│   ├── vosk/             # ASR 模型
│   └── tts/              # TTS 模型
├── workspace/             # ZeroClaw 工作区
│   ├── AGENTS.md
│   ├── PERSONALITY.md
│   └── memory/            # SQLite 记忆库
├── requirements.txt
└── README.md
```

### 5.2 ZeroClaw 配置

```yaml
# ~/.zeroclaw/config.yaml
provider:
  type: ollama
  model: qwen2.5:0.5b-instruct-q4_K_M

memory:
  backend: sqlite
  path: ./workspace/memory.db
  hybrid_search: true
  retention_days: 30

channel:
  type: websocket
  port: 12020  # WebSocket 服务端口
```

### 5.3 预置人格

#### 朋友模式 (Friend)

```markdown
你是小朋友的朋友，名叫"小友"。
- 友好、亲切、有耐心
- 用简单的语言交流
- 可以聊学校、玩具、动画等话题
- 适当鼓励和夸奖
```

#### 老师模式 (Teacher)

```markdown
你是知识渊博的老师，名叫"智多星"。
- 善于解答问题
- 用生动的例子解释知识
- 鼓励好奇心
- 适当引导思考
```

---

## 6. 部署指南

### 6.1 树莓派部署

```bash
# 1. 安装 ZeroClaw
curl -fsSL https://get.zeroclaw.dev | sh

# 2. 安装 Ollama
curl -fsSL https://ollama.com/install.sh | sh
ollama pull qwen2.5:0.5b-instruct-q4_K_M

# 3. 配置人格
mkdir -p ~/.zeroclaw/workspace
cp personality_friend.md ~/.zeroclaw/workspace/AGENTS.md

# 4. 启动 ZeroClaw
zeroclaw start

# 5. 启动 WebSocket 服务
cd server
pip install -r requirements.txt
python src/server.py
```

### 6.2 ESP32 固件构建

```bash
cd firmware
idf.py set-target esp32s3
idf.py menuconfig
# 配置 WiFi 和服务器地址
idf.py build
idf.py flash monitor
```

---

## 7. 功能特性清单

- [x] 语音唤醒
- [x] 语音识别 (Vosk)
- [x] 对话生成 (ZeroClaw + Ollama)
- [x] 语音合成 (Piper/eSpeak)
- [x] 长期记忆 (30天)
- [x] 语义搜索
- [x] 多角色切换
- [x] 屏幕显示
- [x] 对话记录查看

---

## 8. 未来扩展

- [ ] 在线 API 对接 (Claude/OpenAI)
- [ ] 敏感词过滤
- [ ] 家长监控
- [ ] 多语言支持
- [ ] 儿歌/故事播放
