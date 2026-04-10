package main

import (
	"encoding/json"
	"log"
	"net/http"
	"time"

	"github.com/gorilla/websocket"
)

var upgrader = websocket.Upgrader{
	ReadBufferSize:  1024,
	WriteBufferSize: 1024,
	CheckOrigin: func(r *http.Request) bool {
		return true
	},
}

type Message struct {
	Type    string `json:"type"`
	Content string `json:"content,omitempty"`
	Data    string `json:"data,omitempty"`
	Speaker string `json:"speaker,omitempty"`
	Detected bool  `json:"detected,omitempty"`
}

func main() {
	addr := "0.0.0.0:12020"
	log.Printf("Starting WebSocket echo server on %s", addr)

	http.HandleFunc("/", func(w http.ResponseWriter, r *http.Request) {
		conn, err := upgrader.Upgrade(w, r, nil)
		if err != nil {
			log.Printf("Upgrade error: %v", err)
			return
		}
		defer conn.Close()

		log.Printf("Client connected from %s", conn.RemoteAddr())

		for {
			messageType, message, err := conn.ReadMessage()
			if err != nil {
				if websocket.IsUnexpectedCloseError(err, websocket.CloseGoingAway, websocket.CloseAbnormalClosure) {
					log.Printf("Read error: %v", err)
				}
				break
			}

			if messageType == websocket.TextMessage {
				var msg Message
				if err := json.Unmarshal(message, &msg); err != nil {
					log.Printf("JSON parse error: %v", err)
					continue
				}

				log.Printf("Received: type=%s, content=%s", msg.Type, msg.Content)

				switch msg.Type {
				case "status":
					if msg.Content == "listening" {
						sendMessage(conn, Message{Type: "status", Content: "ready"})
					}

				case "wakeword":
					log.Printf("Wake word detected: %v", msg.Detected)
					sendMessage(conn, Message{Type: "status", Content: "listening"})

				case "text":
					resp := Message{
						Type:    "text",
						Content: "Echo: " + msg.Content,
						Speaker: "friend",
					}
					sendMessage(conn, resp)
					log.Printf("Sent: text response")

					time.Sleep(1 * time.Second)
					sendMessage(conn, Message{Type: "tts", Data: ""})
					log.Printf("Sent: tts response")

					time.Sleep(500 * time.Millisecond)
					sendMessage(conn, Message{Type: "status", Content: "idle"})
					log.Printf("Sent: idle")

				case "audio_end":
					log.Printf("Audio ended")
					sendMessage(conn, Message{Type: "status", Content: "processing"})
					log.Printf("Sent: processing")

					time.Sleep(2 * time.Second)
					sendMessage(conn, Message{
						Type:    "text",
						Content: "你好呀！有什么想和我聊的吗？",
						Speaker: "friend",
					})
					log.Printf("Sent: text response")

					time.Sleep(500 * time.Millisecond)
					sendMessage(conn, Message{Type: "status", Content: "idle"})
					log.Printf("Sent: idle")
				}
			}
		}

		log.Printf("Client disconnected")
	})

	if err := http.ListenAndServe(addr, nil); err != nil {
		log.Fatal("ListenAndServe error:", err)
	}
}

func sendMessage(conn *websocket.Conn, msg Message) {
	data, err := json.Marshal(msg)
	if err != nil {
		log.Printf("Marshal error: %v", err)
		return
	}
	if err := conn.WriteMessage(websocket.TextMessage, data); err != nil {
		log.Printf("Write error: %v", err)
	}
}
