#!/usr/bin/env python3
"""
Child Friend AI - Raspberry Pi Server
WebSocket Server + ASR + ZeroClaw + TTS
"""

import asyncio
import json
import base64
import logging
import os
import uuid
import subprocess
from pathlib import Path
from typing import Set, Optional
import websockets
import numpy as np

logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger(__name__)


class ChildFriendServer:
    def __init__(self, host: str = "0.0.0.0", port: int = 8080):
        self.host = host
        self.port = port
        self.clients: Set[websockets.WebSocketServerProtocol] = set()
        self.audio_buffer = {}

        self.zeroclaw_process = None
        self.zeroclaw_url = "http://localhost:18789"

    async def initialize(self):
        logger.info("Initializing Child Friend AI Server...")
        await self.start_zeroclaw()

    async def start_zeroclaw(self):
        logger.info("Starting ZeroClaw...")

        zeroclaw_dir = Path.home() / ".zeroclaw"

        if not zeroclaw_dir.exists():
            logger.warning("ZeroClaw not configured, using fallback mode")
            return

        try:
            import requests

            response = requests.get(f"{self.zeroclaw_url}/health", timeout=5)
            if response.status_code == 200:
                logger.info("ZeroClaw is running")
                return
        except:
            pass

        logger.info("ZeroClaw not running, please run: zeroclaw gateway")

    async def handle_client(self, websocket: websockets.WebSocketServerProtocol, path: str):
        client_id = str(uuid.uuid4())
        self.clients.add(websocket)
        self.audio_buffer[client_id] = b""

        logger.info(f"Client connected: {client_id}")

        try:
            async for message in websocket:
                if isinstance(message, bytes):
                    await self.handle_audio(websocket, client_id, message)
                else:
                    await self.process_message(websocket, client_id, message)
        except websockets.exceptions.ConnectionClosed:
            logger.info(f"Client disconnected: {client_id}")
        finally:
            self.clients.remove(websocket)
            if client_id in self.audio_buffer:
                del self.audio_buffer[client_id]

    async def handle_audio(self, websocket, client_id: str, audio_data: bytes):
        try:
            pcm_data = self.decode_audio(audio_data)
            if pcm_data:
                self.audio_buffer[client_id] += pcm_data
        except Exception as e:
            logger.error(f"Error handling audio: {e}")

    def decode_audio(self, audio_data: bytes) -> Optional[bytes]:
        try:
            audio_json = json.loads(audio_data)
            if audio_json.get("type") == "audio":
                b64_data = audio_json.get("data", "")
                return base64.b64decode(b64_data)
        except:
            return audio_data
        return None

    async def process_message(self, websocket, client_id: str, message: str):
        try:
            data = json.loads(message)
            msg_type = data.get("type", "")

            if msg_type == "status":
                content = data.get("content", "")
                logger.info(f"Status: {content}")

                if content == "listening":
                    self.audio_buffer[client_id] = b""

            elif msg_type == "wakeword":
                detected = data.get("detected", False)
                logger.info(f"Wake word detected: {detected}")
                await self.send_status(websocket, "listening")

            elif msg_type == "audio_end":
                await self.process_speech(websocket, client_id)

            elif msg_type == "text":
                content = data.get("content", "")
                logger.info(f"Text response: {content}")

        except json.JSONDecodeError:
            logger.error(f"Invalid JSON: {message}")
        except Exception as e:
            logger.error(f"Error processing message: {e}")

    async def process_speech(self, websocket, client_id: str):
        audio_data = self.audio_buffer.get(client_id, b"")
        if not audio_data:
            await self.send_error(websocket, "No audio data")
            return

        await self.send_status(websocket, "processing")

        text = await self.recognize_speech(audio_data)
        if not text:
            await self.send_error(websocket, "Speech recognition failed")
            await self.send_status(websocket, "idle")
            return

        logger.info(f"Recognized: {text}")

        response = await self.generate_response(text)

        await self.send_text(websocket, response)

        audio = await self.synthesize_speech(response)
        if audio:
            await self.send_tts(websocket, audio)

        await self.send_status(websocket, "speaking")

        await asyncio.sleep(len(response) * 0.1)

        await self.send_status(websocket, "idle")

    async def recognize_speech(self, audio_data: bytes) -> Optional[str]:
        try:
            import requests

            b64_audio = base64.b64encode(audio_data).decode()

            response = requests.post(
                "http://localhost:5000/recognize",
                json={"audio": b64_audio},
                timeout=10
            )

            if response.status_code == 200:
                result = response.json()
                return result.get("text", "")

        except Exception as e:
            logger.warning(f"ASR error: {e}, using fallback")

        return "你好"

    async def generate_response(self, text: str) -> str:
        try:
            import requests

            payload = {
                "message": text,
                "stream": False
            }

            response = requests.post(
                f"{self.zeroclaw_url}/chat",
                json=payload,
                timeout=60
            )

            if response.status_code == 200:
                result = response.json()
                return result.get("content", {}).get("text", "你好呀！")

        except Exception as e:
            logger.warning(f"ZeroClaw error: {e}, using fallback")

        return "你好呀！有什么想和我聊的吗？"

    async def synthesize_speech(self, text: str) -> Optional[bytes]:
        try:
            process = subprocess.Popen(
                ["piper", "--model", str(Path.home() / "child-friend-ai" / "models" / "tts" / "en_US-lessac-medium.onnx"), "--output_file", "-"],
                stdin=subprocess.PIPE,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE
            )

            audio_bytes, _ = process.communicate(input=text.encode(), timeout=30)
            return audio_bytes
        except Exception as e:
            logger.warning(f"TTS error: {e}")
        return None

    async def send_status(self, websocket, status: str):
        message = json.dumps({"type": "status", "content": status})
        await websocket.send(message)

    async def send_text(self, websocket, text: str):
        message = json.dumps({
            "type": "text",
            "content": text,
            "speaker": "friend"
        })
        await websocket.send(message)

    async def send_tts(self, websocket, audio_data: bytes):
        b64_audio = base64.b64encode(audio_data).decode()
        message = json.dumps({
            "type": "tts",
            "data": b64_audio
        })
        await websocket.send(message)

    async def send_error(self, websocket, error: str):
        message = json.dumps({"type": "error", "message": error})
        await websocket.send(message)

    async def start(self):
        await self.initialize()

        logger.info(f"Server starting on {self.host}:{self.port}")

        async with websockets.serve(self.handle_client, self.host, self.port):
            await asyncio.Future()


async def main():
    server = ChildFriendServer(host="0.0.0.0", port=8080)
    await server.start()


if __name__ == "__main__":
    asyncio.run(main())
