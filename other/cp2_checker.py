#!/usr/bin/python
# python3 cp2_checker.py 127.0.0.1 9999 

from socket import *
import sys,random,os,time,signal,errno

def handle_timeout(signum, frame):
    raise TimeoutError(os.strerror(errno.ETIME))

if len(sys.argv) < 3:
    sys.stderr.write('Usage: %s <ip> <port>\n' % (sys.argv[0]))
    sys.exit(1)

os.system('tmux new -s checker -d "stdbuf -o0 ./liso_server > output.log"')
time.sleep(2)

serverHost = gethostbyname(sys.argv[1])
serverPort = int(sys.argv[2])

GOOD_REQUESTS = [
'\
GET / HTTP/1.1\r\nHost: www.cs.cmu.edu\r\nConnection: keep-alive\r\n\
Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8\
User-Agent: Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/39.0.2171.99 Safari/537.36\
Accept-Encoding: gzip, deflate, sdch\r\nAccept-Language: en-US,en;q=0.8\r\n\r\n',

'\
HEAD / HTTP/1.1\r\nHost: www.cs.cmu.edu\r\nConnection: keep-alive\r\n\
Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8\
User-Agent: Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/39.0.2171.99 Safari/537.36\
Accept-Encoding: gzip, deflate, sdch\r\nAccept-Language: en-US,en;q=0.8\r\n\r\n',

'\
POST / HTTP/1.1\r\nHost: www.cs.cmu.edu\r\nConnection: keep-alive\r\n\
Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8\
User-Agent: Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/39.0.2171.99 Safari/537.36\
Accept-Encoding: gzip, deflate, sdch\r\nAccept-Language: en-US,en;q=0.8\r\n\r\n'
]
BAD_REQUESTS = [ # 400
    'GET\r / HTTP/1.1\r\nUser-Agent: 441UserAgent/1.0.0\r\n\r\n',   # Extra CR
    'GET / HTTP/1.1\nUser-Agent: 441UserAgent/1.0.0\r\n\r\n',       # Missing CR
    'GET / HTTP/1.1\rUser-Agent: 441UserAgent/1.0.0\r\n\r\n',       # Missing LF
    'GET: / HTTP/1.1\r\nUser-Agent: 441UserAgent/1.0.0\r\n\r\n',    # Extra colon
    'GET / \r\nUser-Agent: 441UserAgent/1.0.0\r\n\r\n',             # Missing version 
    ' / HTTP/1.1\r\nUser-Agent: 441UserAgent/1.0.0\r\n\r\n',        # Missing method
    'GET  HTTP/1.1\r\nUser-Agent: 441UserAgent/1.0.0\r\n\r\n',      # Missing uri
    'GET: / HTTP/1.1\r\n: 441UserAgent/1.0.0\r\n\r\n',              # Missig header name
    'GET: / HTTP/1.1\r\nUser-Agent:\r\n\r\n',                       # Missing header value
    '  GET: / HTTP/1.1\r\nUser-Agent: 441UserAgent/1.0.0\r\n\r\n',  # Extra spaces
]
REQUESTS_404 = [
'\
GET /~prs/15-441-F15/ HTTP/1.1\r\nHost: www.cs.cmu.edu\r\nConnection: keep-alive\r\n\
Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8\
User-Agent: Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/39.0.2171.99 Safari/537.36\
Accept-Encoding: gzip, deflate, sdch\r\nAccept-Language: en-US,en;q=0.8\r\n\r\n',

'\
GET /~prs/15-441-F15/ HTTP/1.1\r\nHost: www.cs.cmu.edu\r\nConnection: keep-alive\r\n\
Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8\
User-Agent: Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/39.0.2171.99 Safari/537.36\
Accept-Encoding: gzip, deflate, sdch\r\nAccept-Language: en-US,en;q=0.8\r\n\r\n',

'\
GET /~prs/15-441-F15/ HTTP/1.1\r\nHost: www.cs.cmu.edu\r\nConnection: keep-alive\r\n\
Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8\
User-Agent: Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/39.0.2171.99 Safari/537.36\
Accept-Encoding: gzip, deflate, sdch\r\nAccept-Language: en-US,en;q=0.8\r\n\r\n'
]
UNIMPLEMENT_REQUESTS = ['HELLO / HTTP/1.1\r\nUser-Agent: 441UserAgent/1.0.0\r\n\r\n',
'HELLO / HTTP/1.1\r\nUser-Agent: 441UserAgent/1.0.0\r\n\r\n',
'HELLO / HTTP/1.1\r\nUser-Agent: 441UserAgent/1.0.0\r\n\r\n'
]
REQUESTS_505 = [
'\
HEAD / HTTP/1.10\r\nHost: www.cs.cmu.edu\r\nConnection: keep-alive\r\n\
Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8\
User-Agent: Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/39.0.2171.99 Safari/537.36\
Accept-Encoding: gzip, deflate, sdch\r\nAccept-Language: en-US,en;q=0.8\r\n\r\n',

'\
HEAD / HTTP/1.10\r\nHost: www.cs.cmu.edu\r\nConnection: keep-alive\r\n\
Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8\
User-Agent: Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/39.0.2171.99 Safari/537.36\
Accept-Encoding: gzip, deflate, sdch\r\nAccept-Language: en-US,en;q=0.8\r\n\r\n',

'\
HEAD / HTTP/1.10\r\nHost: www.cs.cmu.edu\r\nConnection: keep-alive\r\n\
Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8\
User-Agent: Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/39.0.2171.99 Safari/537.36\
Accept-Encoding: gzip, deflate, sdch\r\nAccept-Language: en-US,en;q=0.8\r\n\r\n'
]

# 实际上他们response回来不一定是这样的
GOOD_RESPONSE = [ # GET查有网页，HEAD查没有网页
    '<!DOCTYPE html> \n<html xml:lang="en" lang="en" xmlns="http://www.w3.org/1999/xhtml">\n    <head>\n        <meta http-equiv="content-type" content="text/html; charset=utf-8" />\n        <title>Liso the Friendly Web Server</title>\n        <link rel="stylesheet" type="text/css" href="style.css" /> \n    </head>\n\n    <body>\n        <div id="page">\n            <img src="images/liso_header.png" alt="Liso the Friendly Web Server"></img>\n            <p>Welcome to the Liso 1.0 web server.  If you see this page then\n               congratulations!  Your Liso web server is up and running now!</p>\n            <p>This Liso web server was implemented by: xxxxxxx\n               &#60;<a href="mailto:xxxxx@andrew.cmu.edu">xxxxx@andrew.cmu.edu\n                    </a>&#62;</p>\n        </div>\n    </body>\n\n</html>\n',
    'HTTP/1.1 200 OK\r\nServer: liso/1.0\r\nDate: Sun, 20 Feb 2022 12:22:57 UTC\r\nContent-Length: 802\r\nContent-type: text/html\r\nLast-modified: Sun, 20 Feb 2022 08:16:59 GMT\r\nConnection: keep-alive\r\n\r\n',
]
RESPONSE_200 = "HTTP/1.1 200 OK\r\n"
RESPONSE_400 = "HTTP/1.1 400 Bad request\r\n\r\n"
RESPONSE_404 = "HTTP/1.1 404 Not Found\r\n\r\n"
RESPONSE_501 = "HTTP/1.1 501 Not Implemented\r\n\r\n"
RESPONSE_505 = "HTTP/1.1 505 HTTP Version not supported\r\n\r\n"
def test_week2(request, mtype):
    TIMEOUT=1
    s = socket(AF_INET, SOCK_STREAM)
    s.connect(('localhost', 9999))

    signal.signal(signal.SIGALRM, handle_timeout)
    signal.alarm(TIMEOUT)
    cnt_success = 0
    if mtype=='get':
        compare_string = GOOD_RESPONSE[0]
    elif mtype=='head':
        compare_string = GOOD_RESPONSE[1]
    elif mtype=='post':
        compare_string = request
    elif mtype=='400':
        compare_string = RESPONSE_400
    elif mtype=='404':
        compare_string = RESPONSE_404
    elif mtype=='501':
        compare_string = RESPONSE_501
    elif mtype=='505':
        compare_string = RESPONSE_505
    send_size = len(compare_string)
    try:
        s.send(request.encode('utf-8'))
        recv_string = s.recv(send_size).decode('utf-8')
        # import pdb;pdb.set_trace()
        while True:
            # if mtype=='get' or mtype=='head': pass # get和head请求不限制接收长度，用超时来限制
            if compare_string.lower() in recv_string.lower(): break
            recv_string += s.recv(send_size).decode('utf-8', errors='ignore')
    except TimeoutError:
        # print("warning: ",mtype," timeout reached")
        pass
    signal.alarm(0)

    if mtype=='get':
        if recv_string.find(RESPONSE_200)<0: cnt_success = 0 # 响应中不包含200
        elif recv_string.find('<!DOCTYPE html>')<0: cnt_success = 0 
        else: cnt_success = 1
    elif mtype=='head':
        if recv_string.find(RESPONSE_200)<0: cnt_success = 0 # 响应中不包含200
        elif recv_string.find('<!DOCTYPE html>')>0: cnt_success = 0 # 接收到网页
        else: cnt_success = 1
    elif compare_string.lower() in recv_string.lower():
        cnt_success = 1
    else:
        if mtype=='post' and len(compare_string)==len(RESPONSE_400): pass
        else: print(mtype+" Error: Data received is unexpected! \n")
    
    s.shutdown(2)
    s.close()
    return cnt_success

# import pdb;pdb.set_trace()
cnt_request_get = test_week2(GOOD_REQUESTS[0], 'get')
cnt_request_get += test_week2(GOOD_REQUESTS[0], 'get')
cnt_request_get += test_week2(GOOD_REQUESTS[0], 'get')
if cnt_request_get < 1: print('get: response error!')
else: 
    print('get: correct response')
    cnt_request_get = 1

cnt_request_head = test_week2(GOOD_REQUESTS[1], 'head')
cnt_request_head += test_week2(GOOD_REQUESTS[1], 'head')
cnt_request_head += test_week2(GOOD_REQUESTS[1], 'head')
if cnt_request_head < 1: print('head: response error!')
else: 
    print('head: correct response')
    cnt_request_head = 1

cnt_request_post = test_week2(GOOD_REQUESTS[2], 'post')
cnt_request_post += test_week2(GOOD_REQUESTS[2], 'post')
cnt_request_post += test_week2(GOOD_REQUESTS[2], 'post')
if cnt_request_post:
    if test_week2('POST/1.1 400 Bad request\r\n\r\n', 'post'): # 返回400才对
        cnt_request_post = 0
        print("post: didn't parsing, error!")
    else:
        cnt_request_post = 1
        print('post: correct response')

cnt_request_400 = test_week2(BAD_REQUESTS[random.randint(0,9)], '400')
cnt_request_400 += test_week2(BAD_REQUESTS[random.randint(0,9)], '400')
cnt_request_400 += test_week2(BAD_REQUESTS[random.randint(0,9)], '400')
if cnt_request_400:
    cnt_request_400 = 1
    print('400: correct response')
else:
    print('400 error: data received is unexpected! \n')

cnt_request_404 = test_week2(REQUESTS_404[0], '404')
cnt_request_404 += test_week2(REQUESTS_404[0], '404')
cnt_request_404 += test_week2(REQUESTS_404[0], '404')
if cnt_request_404:
    cnt_request_404 = 1
    print('404: correct response')
else:
    print('404 error: data received is unexpected! \n')

cnt_request_501 = test_week2(UNIMPLEMENT_REQUESTS[0], '501')
cnt_request_501 += test_week2(UNIMPLEMENT_REQUESTS[0], '501')
cnt_request_501 += test_week2(UNIMPLEMENT_REQUESTS[0], '501')
if cnt_request_501:
    cnt_request_501 = 1
    print('501: correct response')
else:
    print('501 error: data received is unexpected! \n')

cnt_request_505 = test_week2(REQUESTS_505[0], '505')
cnt_request_505 += test_week2(REQUESTS_505[0], '505')
cnt_request_505 += test_week2(REQUESTS_505[0], '505')
if cnt_request_505:
    cnt_request_505 = 1
    print('505: correct response')
else:
    print('505 error: data received is unexpected! \n')

scores = 20 * (cnt_request_get + cnt_request_head + cnt_request_post) +\
         10 * (cnt_request_400 + cnt_request_404 + cnt_request_501 + cnt_request_505)

os.system('tmux kill-session -t checker')

os.system('cat output.log')
print("{\"scores\": {\"lab2\": %.2f}}"%scores)

