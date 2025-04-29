#!/usr/bin/python3
# python3 cp3_checker.py 127.0.0.1 9999 requests/request_pipeline

from socket import *
import sys
import os
import time
import signal
import errno

def handle_timeout(signum, frame):
    """超时处理函数"""
    raise TimeoutError(os.strerror(errno.ETIME))

def test_week3(requests):
    """测试HTTP请求并验证响应"""
    TIMEOUT = 5
    signal.signal(signal.SIGALRM, handle_timeout)
    signal.alarm(TIMEOUT)

    cnt_success = 0
    cnt_recv = 0
    substr = 'HTTP/1.1'
    recv_strings = ""  # 初始化接收字符串

    try:
        buf_size = 2048
        # 打印请求内容
        print("=== 从请求文件读取的内容 ===")
        print(requests.decode('utf-8', errors='replace'))
        print("=" * 40)
        
        # 发送请求
        s.send(requests)
        
        # 打印提示信息
        print("\n=== 开始接收响应 ===")
        
        # 接收初始响应
        recv_data = s.recv(buf_size).decode('utf-8', errors='ignore')
        recv_strings += recv_data
        print("第一批响应数据:")
        print(recv_data)
        print("-" * 40)
        
        cnt_recv = recv_strings.count(substr)
        
        # 持续接收响应
        while cnt_recv < 27:
            recv_data = s.recv(buf_size).decode('utf-8', errors='ignore')
            if not recv_data:
                print("接收到空数据，连接可能已关闭")
                break
            
            recv_strings += recv_data
            print(f"接收到第{cnt_recv+1}批数据:")
            print(recv_data)
            print("-" * 40)
            cnt_recv += recv_data.count(substr)
            
    except TimeoutError:
        print("\nTimeout reached")
        print("已接收的完整响应:")
        print(recv_strings)
    except Exception as e:
        print(f"\n发生错误: {str(e)}")
        print("已接收的部分响应:")
        print(recv_strings)
    finally:
        signal.alarm(0)
        print("\n=== 响应接收结束 ===")

    # 统计成功数量
    responses = [res for res in recv_strings.split(substr) if res]
    responses = [substr + res for res in responses]
    
    for i in range(min(len(responses), len(RESPONSE))):
        if RESPONSE[i].lower() in responses[i].lower():
            cnt_success += 1
    
    return cnt_success

if __name__ == "__main__":
    # 参数检查
    if len(sys.argv) < 4:
        sys.stderr.write('Usage: %s <ip> <port> <request>\n' % (sys.argv[0]))
        sys.exit(1)

    # 启动测试服务器
    os.system('tmux new -s checker -d "./liso_server"')
    time.sleep(2)

    # 连接服务器
    serverHost = gethostbyname(sys.argv[1])
    serverPort = int(sys.argv[2])
    s = socket(AF_INET, SOCK_STREAM)
    s.connect((serverHost, serverPort))

    # 读取请求文件
    with open(sys.argv[3], "rb") as request_file:
        msg = request_file.read()

    # 预定义的期望响应列表
    RESPONSE = [
        'HTTP/1.1 200 OK',
        'HTTP/1.1 501 Not Implemented',
        'HTTP/1.1 200 OK',
        'HTTP/1.1 200 OK',
        'HTTP/1.1 505 HTTP Version not supported', # 先返回版本不支持 再返回not implemented 这分不要也罢
        'HTTP/1.1 200 OK',
        'HTTP/1.1 200 OK',
        'HTTP/1.1 501 Not Implemented',
        'HTTP/1.1 200 OK',
        'HTTP/1.1 505 HTTP Version not supported',# 到这里 前10个 对了9个 注意这里第九个发送文件的时候被第十个覆盖了 发送文件的内容的长度没有弄到response_buf_len和response_buf_ptr里面
        'HTTP/1.1 200 OK',
        'HTTP/1.1 400 Bad request'# 不对 这里返回了ok
        'HTTP/1.1 200 OK',
        'HTTP/1.1 200 OK',
        'HTTP/1.1 505 HTTP Version not supported',
        'HTTP/1.1 501 Not Implemented',
        'HTTP/1.1 501 Not Implemented',
        'HTTP/1.1 501 Not Implemented',
        'HTTP/1.1 200 OK',
        'HTTP/1.1 505 HTTP Version not supported',
        'HTTP/1.1 200 OK',
        'HTTP/1.1 200 OK', #到这里对了20个
        'HTTP/1.1 200 OK',
        'HTTP/1.1 200 OK',
        'HTTP/1.1 505 HTTP Version not supported',
        'HTTP/1.1 200 OK',
        'HTTP/1.1 501 Not Implemented',
        'HTTP/1.1 400 Bad request',
        'HTTP/1.1 400 Bad request',
    ]

    # 执行测试
    cnt_request = test_week3(msg)
    
    # 计算得分
    bound = [i for i in range(19, 0, -4)]
    scores = 0
    for cnt in bound:
        if cnt_request > cnt:
            scores = 5 * (cnt + 1)
            break
    
    # 输出结果
    print("success number: %d" % min(cnt_request, 20))
    s.close()
    os.system('tmux kill-session -t checker')
    print("{\"scores\": {\"lab3\": %.2f}}" % scores)