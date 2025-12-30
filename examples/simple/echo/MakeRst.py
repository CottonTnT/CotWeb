import socket
import select
import time
import os

def send_and_rst(host, port, message=b"HELLO", wait_time=1):
    # 创建 TCP socket
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.setblocking(0)

    try:
        s.connect_ex((host, port))
        print(f"[+] Connecting to {host}:{port}")
        time.sleep(0.2)  # 等待连接建立

        # 发送数据
        s.sendall(message)
        print(f"[+] Sent data: {message}")

        # 等待一段时间
        time.sleep(wait_time)

        # 检查是否有数据可读
        rlist, _, _ = select.select([s], [], [], 0)
        if rlist:
            print("[!] Data available to read — closing fd to trigger RST")
            fd = s.fileno()
            os.close(fd)  # 直接关闭底层文件描述符，跳过优雅关闭
            return

        print("[*] No data received, closing normally")
        s.close()

    except Exception as e:
        print(f"[!] Error: {e}")
        s.close()

if __name__ == "__main__":
    # 示例：向本地 12345 端口发送
    send_and_rst("127.0.0.1", 2007)
