import os
import grpc
import requests
import logging
from concurrent import futures

import alert_pb2
import alert_pb2_grpc

logging.basicConfig(
    level=logging.INFO,
    format="[%(asctime)s][NOTIFY] %(message)s",
    datefmt="%H:%M:%S",
)

SLACK_WEBHOOK_URL = os.environ.get("SLACK_WEBHOOK_URL", "")                  # Slack Webhook URL
GRPC_PORT = os.environ.get("GRPC_PORT", "50051")                             # notification_service 的 port


class NotificationServicer(alert_pb2_grpc.NotificationServiceServicer):
    def SendAlert(self, request, context):                                   # 實作 SendAlert 方法，收到 AlertRequest 後，發送 Slack 通知
        message = (
            f"*IoT Alert* | sensor={request.sensor_id} "
            f"type={request.type} value={request.value:.2f}\n"
            f"> {request.message}"
        )
        logging.info("收到 gRPC SendAlert: %s", request.message)

        if not SLACK_WEBHOOK_URL:
            # 沒有設定 Webhook URL 時只印出，方便開發測試
            logging.warning("SLACK_WEBHOOK_URL 未設定，僅印出警報")
            logging.info("[ALERT] %s", message)
            return alert_pb2.AlertResponse(success=True)

        try:
            resp = requests.post(
                SLACK_WEBHOOK_URL,
                json={"text": message},
                timeout=5,
            )
            if resp.status_code == 200:
                logging.info("Slack 通知成功")
                return alert_pb2.AlertResponse(success=True)
            else:
                logging.error("Slack 回應非 200：%d", resp.status_code)
                return alert_pb2.AlertResponse(success=False)
        except Exception as e:
            logging.error("Slack 請求失敗：%s", e)
            return alert_pb2.AlertResponse(success=False)


def serve():
    server = grpc.server(futures.ThreadPoolExecutor(max_workers=4))         # 建立 gRPC server，使用 ThreadPoolExecutor，最多 4 個執行緒
    alert_pb2_grpc.add_NotificationServiceServicer_to_server(               # 註冊 NotificationServiceServicer 到 server
        NotificationServicer(), server
    )
    addr = f"[::]:{GRPC_PORT}"                                              # 監聽 port
    server.add_insecure_port(addr)                                          # 新增 insecure port
    server.start()                                                          # 啟動 server
    logging.info("gRPC server 啟動，監聽 %s", addr)                         
    server.wait_for_termination()                                           # 等待 server 停止


if __name__ == "__main__":
    serve()
