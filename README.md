# Child Friend AI

儿童陪伴 AI 机器人 - 基于 ESP32-S3 + 树莓派 + ZeroClaw

## 项目简介

Child Friend AI 是一款专为儿童设计的陪伴 AI 机器人，采用完全离线架构保护孩子隐私。ESP32-S3 负责音频采集/播放和屏幕显示，树莓派 4B 处理语音识别、语音合成和 AI 对话。

## 硬件配置

- **ESP32-S3-N16R8** - 主控芯片
- **INMP441** - 麦克风模块 (I2S)
- **MAX98357** - 音频放大器 (I2S)
- **ST7735** - 1.8寸 TFT 屏幕 (SPI)
- **树莓派 4B** - 服务端处理 ASR/TTS/LLM

## 项目结构

```
child-friend-ai/
├── firmware/           # ESP32 固件
│   ├── main/          # 主程序源码
│   │   ├── main.c              # 入口
│   │   ├── audio_service.c/h   # 音频服务 (INMP441 + MAX98357)
│   │   ├── board.c/h           # 板级配置
│   │   ├── display.c/h         # ST7735 屏幕驱动
│   │   ├── font.c/h            # 8x16 点阵字体
│   │   ├── state_machine.c/h   # 状态机
│   │   ├── websocket_client.c/h # WebSocket 客户端
│   │   ├── wifi_init.c/h       # WiFi 初始化
│   │   └── wifi_provisioning.c/h # WiFi 配网
│   ├── CMakeLists.txt
│   ├── sdkconfig
│   └── partitions.csv
├── server/            # 树莓派服务端
│   ├── src/
│   │   └── server.py  # WebSocket 服务 + ZeroClaw
│   └── requirements.txt
└── docs/
    ├── DESIGN.md           # 设计文档
    └── BUILD_EXPERIENCE.md # 构建经验
```

## 快速开始

### 1. 树莓派部署

```bash
# 安装依赖
pip install -r server/requirements.txt

# 安装 Vosk (语音识别)
cd ~/
mkdir -p child-friend-ai/models/vosk
cd child-friend-ai/models/vosk
wget https://alphacephei.com/vosk/models/vosk-model-cn-0.22.zip
unzip vosk-model-cn-0.22.zip
mv vosk-model-cn-0.22 model

# 安装 Piper (语音合成)
wget https://github.com/rhasspy/piper/releases/download/v1.2.0/piper_linux_x86_64.tar.gz
tar -xzf piper_linux_x86_64.tar.gz

# 启动服务
cd child-friend-ai/server/src
python3 server.py
```

### 2. ESP32 固件构建

#### 环境要求
- ESP-IDF v5.5.4
- macOS / Linux

#### 构建步骤

```bash
# 激活 ESP-IDF 环境
# source ~/.espressif/v6.0/esp-idf/export.sh
# 进入固件目录
cd child-friend-ai/firmware && rm -rf build sdkconfig/build sdkconfig.old
# 设置目标芯片
idf.py set-target esp32s3
# 配置选项 (可选)
idf.py menuconfig
# 编译
idf.py build
# 烧录
idf.py -p /dev/ttyUSB0 flash
# 监控输出
idf.py -p /dev/ttyUSB0 monitor
```

#### 固件配置

在 `sdkconfig` 中可配置：
- `CONFIG_WIFI_SSID` - WiFi 名称
- `CONFIG_WIFI_PASSWORD` - WiFi 密码

## 通信协议

ESP32 与树莓派通过 WebSocket 通信，JSON 格式报文：

### ESP32 -> 树莓派

```json
{"type": "wakeword", "data": {"detected": true}}
{"type": "audio", "data": {"chunk": "base64..."}}
{"type": "status", "data": {"state": "listening"}}
```

### 树莓派 -> ESP32

```json
{"type": "tts", "data": {"text": "你好呀！"}}
{"type": "emoji", "data": {"face": "😊"}}
{"type": "state", "data": {"state": "idle"}}
```

详见 [DESIGN.md](docs/DESIGN.md)

## 功能特性

- [x] 硬件级唤醒词检测 (ESP-SR)
- [x] 语音活动检测 (VAD)
- [x] I2S 麦克风采集 (INMP441)
- [x] I2S 音频播放 (MAX98357)
- [x] ST7735 屏幕显示
- [x] 文本点阵字体渲染
- [x] WebSocket 实时通信
- [x] 完全离线语音对话
- [x] 上下文记忆

## 固件 (ESP32-S3 N16R8 16MB Flash)

- bootloader.bin: ~20KB
- app.bin: ~950KB
- 剩余空间: ~15.1MB

## 参考项目

- WalkieClaw
- Cheeko ESP32 Voice
- ZeroClaw
- ESP-SR (乐鑫语音框架)
