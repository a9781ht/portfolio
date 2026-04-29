import os
import json
import grpc
import pika
import logging

import alert_pb2
import alert_pb2_grpc

logging.basicConfig(
    level=logging.INFO,
    format="[%(asctime)s][ALERT_SUB] %(message)s",
    datefmt="%H:%M:%S",
)

RABBITMQ_HOST = os.environ.get("RABBITMQ_HOST", "rabbitmq")
RABBITMQ_PORT = int(os.environ.get("RABBITMQ_PORT", "5672"))
RABBITMQ_USER = os.environ.get("RABBITMQ_USER", "guest")
RABBITMQ_PASS = os.environ.get("RABBITMQ_PASS", "guest")

NOTIFICATION_HOST = os.environ.get("NOTIFICATION_HOST", "notification_service")  # notification_service 的 host name
NOTIFICATION_PORT = os.environ.get("NOTIFICATION_PORT", "50051")                 # notification_service 的 port

EXCHANGE    = "amq.topic"
QUEUE_NAME  = "alert_queue"
BINDING_TEMP = "sensors.temperature"
BINDING_HUM  = "sensors.humidity"

TEMP_THRESHOLD = 35.0
HUM_THRESHOLD  = 80.0


def send_grpc_alert(stub, sensor_id: str, type_: str, value: float, msg: str):
    try:
        resp = stub.SendAlert(alert_pb2.AlertRequest(                          # 呼叫 notification_service 的 SendAlert 方法，發送 Slack 通知
            sensor_id=sensor_id,
            type=type_,
            value=value,
            message=msg,
        ))
        if resp.success:
            logging.info("gRPC SendAlert 成功")
        else:
            logging.warning("gRPC SendAlert 回報失敗")
    except grpc.RpcError as e:
        logging.error("gRPC 呼叫失敗：%s", e)


def on_message(ch, method, properties, body):
    payload = body.decode("utf-8")
    try:
        data = json.loads(payload)
        type_      = data["type"]
        sensor_id  = data["sensor_id"]
        value      = float(data["value"])

        alert_msg = None
        if type_ == "temperature" and value > TEMP_THRESHOLD:
            alert_msg = f"高溫警報！溫度 {value:.2f}°C 超過閾值 {TEMP_THRESHOLD}°C"
            logging.info("[ALERT] %s sensor=%s", alert_msg, sensor_id)
        elif type_ == "humidity" and value > HUM_THRESHOLD:
            alert_msg = f"高濕警報！濕度 {value:.2f}% 超過閾值 {HUM_THRESHOLD}%"
            logging.info("[ALERT] %s sensor=%s", alert_msg, sensor_id)

        # alert_subscriber (client) 透過 gRPC 呼叫 notification_service (server) 發送 Slack 通知
        if alert_msg:
            channel_grpc = grpc.insecure_channel(                              # 建立 gRPC channel，連到 notification_service
                f"{NOTIFICATION_HOST}:{NOTIFICATION_PORT}"
            )
            stub = alert_pb2_grpc.NotificationServiceStub(channel_grpc)        # 建立 gRPC stub，用來呼叫 notification_service
            send_grpc_alert(stub, sensor_id, type_, value, alert_msg)          # 呼叫 send_grpc_alert 函式，發送 Slack 通知

    except (KeyError, ValueError, json.JSONDecodeError) as e:
        logging.error("訊息處理失敗：%s | payload=%s", e, payload)

    ch.basic_ack(delivery_tag=method.delivery_tag)                             # 手動 ACK，告知 RabbitMQ 訊息已處理完畢


def main():
    credentials = pika.PlainCredentials(RABBITMQ_USER, RABBITMQ_PASS)          # 建立 credentials，包含使用者名稱與密碼
    params = pika.ConnectionParameters(                                        # 建立 connection parameters
        host=RABBITMQ_HOST,
        port=RABBITMQ_PORT,
        credentials=credentials,
        connection_attempts=10,                                                # 連線重試 10 次
        retry_delay=3,                                                         # 每次間隔 3 秒
    )
    connection = pika.BlockingConnection(params)                               # 建立 blocking connection
    channel = connection.channel()                                             # 建立 channel

    channel.queue_declare(queue=QUEUE_NAME, durable=True)                      # 宣告 Queue，durable=True 表示持久化，即使 RabbitMQ 重啟，Queue 也不會消失
    
    channel.queue_bind(queue=QUEUE_NAME, exchange=EXCHANGE, routing_key=BINDING_TEMP) # 綁定 Queue 到 exchange，routing key=BINDING_TEMP 表示溫度
    channel.queue_bind(queue=QUEUE_NAME, exchange=EXCHANGE, routing_key=BINDING_HUM)  # 綁定 Queue 到 exchange，routing key=BINDING_HUM 表示濕度
    
    channel.basic_qos(prefetch_count=1)                                        # 每次只取一條，處理完才取下一條
    channel.basic_consume(queue=QUEUE_NAME, on_message_callback=on_message)    # 註冊消費者 callback 函式，當有訊息時，就呼叫 on_message 函式

    logging.info(
        "等待訊息（binding: %s + %s）...",
        BINDING_TEMP, BINDING_HUM,
    )
    channel.start_consuming()                                                  # 開始消費


if __name__ == "__main__":
    main()
