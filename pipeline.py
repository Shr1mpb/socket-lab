import socket

def send_pipelined_requests(host='127.0.0.1', port=9999):
    # 构造符合RFC标准的请求序列（保留原始请求中的错误用于测试）
    requests = [
        # 有效请求
        "HEAD / HTTP/1.1\r\nHost: www.cs.cmu.edu\r\nConnection: keep-alive\r\n"
        "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8\r\n"
        "User-Agent: Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/39.0.2171.99 Safari/537.36\r\n"
        "Accept-Encoding: gzip, deflate, sdch\r\nAccept-Language: en-US,en;q=0.8\r\n\r\n",

        # 非法方法测试
        "DELETE /~prs/15-441-F15/ HTTP/1.1\r\nHost: www.cs.cmu.edu\r\nConnection: keep-alive\r\n"
        "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8\r\n"
        "User-Agent: Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/39.0.2171.99 Safari/537.36\r\n"
        "Accept-Encoding: gzip, deflate, sdch\r\nAccept-Language: en-US,en;q=0.8\r\n\r\n",

        # 非法协议版本测试
        "HEAD / HTTP/1.5\r\nHost: www.cs.cmu.edu\r\nConnection: keep-alive\r\n"
        "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8\r\n"
        "User-Agent: Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/39.0.2171.99 Safari/537.36\r\n"
        "Accept-Encoding: gzip, deflate, sdch\r\nAccept-Language: en-US,en;q=0.8\r\n\r\n",

        # 非法方法测试
        "HOHO /~prs/15-441-F15/ HTTP/1.1\r\nHost: www.cs.cmu.edu\r\nConnection: keep-alive\r\n"
        "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8\r\n"
        "User-Agent: Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/39.0.2171.99 Safari/537.36\r\n"
        "Accept-Encoding: gzip, deflate, sdch\r\nAccept-Language: en-US,en;q=0.8\r\n\r\n",

        # 有效GET请求
        "GET /~prs/15-441-F15/ HTTP/1.1\r\nHost: www.cs.cmu.edu\r\nConnection: keep-alive\r\n"
        "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8\r\n"
        "User-Agent: Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/39.0.2171.99 Safari/537.36\r\n"
        "Accept-Encoding: gzip, deflate, sdch\r\nAccept-Language: en-US,en;q=0.8\r\n\r\n"
    ]

    try:
        # 创建TCP套接字并建立连接（参考网页5/6的实现）
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
            sock.connect((host, port))
            
            # 发送所有请求数据（参考网页1的管道实现）
            full_request = "".join(requests)
            sock.sendall(full_request.encode('utf-8'))

            # 接收响应数据（持续读取直到连接关闭）
            response_data = b""
            while True:
                chunk = sock.recv(4096)
                if not chunk:
                    break
                response_data += chunk

            # 输出原始响应（包含所有响应报文）
            print("Received response:\n", response_data.decode('utf-8', errors='ignore'))

    except Exception as e:
        print(f"Connection error: {str(e)}")

if __name__ == "__main__":
    send_pipelined_requests()