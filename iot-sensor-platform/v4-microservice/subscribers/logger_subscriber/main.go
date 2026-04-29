package main

import (
	"fmt"
	"log"
	"os"
	"time"

	amqp "github.com/rabbitmq/amqp091-go"
)

const (
	exchange   = "amq.topic"
	queueName  = "logger_queue"
	bindingKey = "sensors.#"
)

func getEnv(key, fallback string) string {
	if v := os.Getenv(key); v != "" {
		return v
	}
	return fallback
}

func connectWithRetry(url string, attempts int, delay time.Duration) (*amqp.Connection, error) {
	var conn *amqp.Connection
	var err error
	for i := 0; i < attempts; i++ {
		conn, err = amqp.Dial(url)                                    // 連線到 RabbitMQ，包含登入驗證
		if err == nil {
			return conn, nil
		}
		log.Printf("[LOGGER_SUB] 連線失敗（第 %d 次），%v 後重試...", i+1, delay)
		time.Sleep(delay)
	}
	return nil, fmt.Errorf("連線失敗（已重試 %d 次）：%w", attempts, err)
}

func main() {
	host := getEnv("RABBITMQ_HOST", "rabbitmq")
	port := getEnv("RABBITMQ_PORT", "5672")
	user := getEnv("RABBITMQ_USER", "guest")
	pass := getEnv("RABBITMQ_PASS", "guest")
	logPath := getEnv("LOG_FILE", "/logs/sensor_log.txt")

	url := fmt.Sprintf("amqp://%s:%s@%s:%s/", user, pass, host, port)

	conn, err := connectWithRetry(url, 10, 3*time.Second)             // 連線重試 10 次，每次間隔 3 秒，等待 RabbitMQ 啟動
	if err != nil {
		log.Fatalf("[LOGGER_SUB] %v", err)
	}
	defer conn.Close()                                                // defer 不是「這一行直接執行」，而是「整個函式要結束、return 之前」才會執行，現在只是把「關 connection」排進佇列

	ch, err := conn.Channel()                                         // 開啟 channel
	if err != nil {
		log.Fatalf("[LOGGER_SUB] 開啟 channel 失敗：%v", err)
	}
	defer ch.Close()                                                  // defer 不是「這一行直接執行」，而是「整個函式要結束、return 之前」才會執行，現在只是把「關 channel」排進佇列

	_, err = ch.QueueDeclare(                                         // 宣告 Queue
		queueName,                                                    // queue name
		true,                                                         // durable
		false,                                                        // auto-delete
		false,                                                        // exclusive
		false,                                                        // no-wait
		nil,                                                          // 額外參數
	)
	if err != nil {
		log.Fatalf("[LOGGER_SUB] queue declare 失敗：%v", err)
	}

	err = ch.QueueBind(                                               // Queue 綁定
		queueName,                                                    // queue name
		bindingKey,                                                   // routing key
		exchange,                                                     // exchange name
		false,                                                        // no-wait
		nil                                                           // 額外參數
	)
	if err != nil {
		log.Fatalf("[LOGGER_SUB] queue bind 失敗：%v", err)
	}

	err = ch.Qos(                                                     // Qos 設定
		1,                                                            // prefetch count，告訴 broker「同一個 consumer 最多先塞 1 筆未 ack 的訊息」，處理完再給下一筆，比較好控負載與公平分派
		0,                                                            // prefetch size，不用「位元組數」限制 prefetch，0 表示這項不生效，只靠上面的「訊息筆數」限制
		false                                                         // global，false 表示只在這個 channel 生效，true 表示在這個 connection 的所有 channel 都生效
	)                                                                 // 每次只取一條，處理完才取下一條
	if err != nil {
		log.Fatalf("[LOGGER_SUB] Qos 設定失敗：%v", err)
	}

	msgs, err := ch.Consume(                                          // 開始消費
		queueName,                                                    // queue name
		"",                                                           // consumer tag（空 = 自動產生）
		false,                                                        // auto-ack（false = 手動 ACK）
		false,                                                        // exclusive
		false,                                                        // no-local
		false,                                                        // no-wait
		nil,                                                          // 額外參數
	)
	if err != nil {
		log.Fatalf("[LOGGER_SUB] consume 設定失敗：%v", err)
	}

	// 開啟 log 檔
	logFile, err := os.OpenFile(logPath, os.O_APPEND|os.O_CREATE|os.O_WRONLY, 0644)
	if err != nil {
		log.Fatalf("[LOGGER_SUB] 無法開啟 log 檔 %s：%v", logPath, err)
	}
	defer logFile.Close()
	logger := log.New(logFile, "", 0)

	log.Printf("[LOGGER_SUB] 等待訊息（binding: %s），寫入 %s ...", bindingKey, logPath)

	// Consume loop
	for msg := range msgs {
		timestamp := time.Now().UTC().Format("2006-01-02T15:04:05.000Z")
		line := fmt.Sprintf("[%s] %s", timestamp, string(msg.Body))

		logger.Println(line)
		log.Printf("[LOGGER_SUB] 已記錄：%s", line)

		msg.Ack(false)                                                // 手動 ACK，告知 RabbitMQ 訊息已處理完畢
	}

	log.Println("[LOGGER_SUB] 消費結束")
}
