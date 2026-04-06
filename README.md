# Child Friend AI

儿童陪伴 AI 机器人 - 基于 ESP32-S3 + 树莓派 + ZeroClaw

## 项目结构

```
child-friend-ai/
├── firmware/           # ESP32 固件
│   ├── main/          # 主程序源码
│   ├── CMakeLists.txt
│   ├── sdkconfig
│   └── partitions.csv
├── server/            # 树莓派服务端
│   ├── src/
│   │   └── server.py  # WebSocket 服务
│   └── requirements.txt
└── docs/
    └── DESIGN.md      # 设计文档
```

## 快速开始

### 1. 树莓派部署

```bash
# 安装依赖
pip install -r server/requirements.txt

# 安装 Ollama (本地 LLM)
curl -fsSL https://ollama.ai/install.sh | sh
ollama pull qwen2.5:0.5b

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

```bash
cd firmware
idf.py set-target esp32s3
idf.py menuconfig
# 配置 WiFi 和服务器 IP
idf.py build
idf.py flash monitor
```

## 通信协议

详见 [DESIGN.md](docs/DESIGN.md)

## 参考项目

- WalkieClaw
- Cheeko ESP32 Voice
- ZeroClaw
