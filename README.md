		case 200: return "OK";
		case 301: return "Moved Permanently";
		case 400: return "Bad Request";
		case 403: return "Fobidden";
		case 404: return "Not Found";
		case 405: return "Method Not Allowed";
		case 500: return "Internal Server Error";

HTTP-message = start-line *( header-field CRLF ) CRLF [ message-body ]
这个公式告诉你，一个报文由：起始行、零个或多个 Header、一个空行 (CRLF)，以及一个可选的 Body 组成。
request-line = method SP request-target SP HTTP-version CRLF
header-field = field-name ":" OWS field-value OWS

- Request Line 格式：`METHOD SP URI SP VERSION CRLF`
    - SP = Space（空格）
    - CRLF = Carriage Return Line Feed（`\r\n`）
- Header 格式：`Field-Name: Field-Value CRLF`


- Response 格式：`Status-Line\r\nHeaders\r\n\r\nBody`
- Status Line：`HTTP/1.1 200 OK\r\n`
- 必要 Headers：`Content-Type`、`Content-Length`、`Connection`


一个分块编码的 HTTP body

	<HTTP Headers>
	Transfer-Encoding: chunked  <-- 关键的请求头
	\r\n

	5\r\n        <-- 块大小 (十六进制的 5)
	Hello\r\n     <-- 块数据 (5个字节) + CRLF

	b\r\n        <-- 块大小 (十六进制的 11)
	world!\r\n   <-- 块数据 (11个字节) + CRLF

	0\r\n        <-- 零长度块，表示结束
	\r\n        <-- 最后的CRLF


生成随机大文件
	dd if=/dev/urandom of=www/bigfile.bin bs=1M count=10

multipart/form-data 和 boundary
- `Content-Type: multipart/form-data; boundary=----xxx`
- boundary 分隔符的作用
- 每个 part 的结构：headers + 空行 + 内容
- `Content-Disposition: form-data; name="file"; filename="photo.jpg"`
- 二进制文件在 body 中是原封不动的字节

    --boundary_string\r\n
    (Part 1 Headers)
    \r\n
    (Part 1 Body)
    \r\n
    --boundary_string\r\n
    (Part 2 Headers)
    ...
    --boundary_string--\r\n

    POST /upload HTTP/1.1
    Host: example.com
    Content-Type: multipart/form-data; boundary=----WebKitFormBoundary7MA4YWxkTrZu0gW

    ----WebKitFormBoundary7MA4YWxkTrZu0gW
    Content-Disposition: form-data; name="username"

    johndoe
    ----WebKitFormBoundary7MA4YWxkTrZu0gW
    Content-Disposition: form-data; name="file"; filename="photo.jpg"
    Content-Type: image/jpeg

    (这里是 photo.jpg 文件的原始二进制数据，一个字节不多，一个字节不少)
    ...
    ...
    ...
    ----WebKitFormBoundary7MA4YWxkTrZu0gW--

post 测试
- 测试：`curl -X POST -d "key=value" localhost:8080`
- 测试：`curl -X POST -H "Transfer-Encoding: chunked" -d @bigfile localhost:8080`


This project has been created as part of the 42 curriculum by yshi, hanwang.

webserv/
├── Makefile
├── config/
│   ├── default.conf            # 默认配置文件
│   └── test.conf               # 测试用配置
├── www/                         # 测试网站根目录
│   ├── index.html
│   ├── style.css
│   ├── 404.html
│   ├── uploads/                # 上传文件存放
│   └── cgi-bin/                # CGI 脚本
│       ├── hello.py
│       └── counter.py
├── includes/                    # 所有头文件
│   ├── Webserv.hpp             # 全局通用（宏、常量、公共 include）
│   ├── Server.hpp              # 核心引擎主类
│   ├── Client.hpp              # 单个连接的状态
│   ├── EventLoop.hpp           # poll 事件循环
│   ├── CgiHandler.hpp          # CGI 进程管理
│   ├── Config.hpp              # 配置数据结构
│   ├── ConfigParser.hpp        # 配置文件解析器
│   ├── Request.hpp             # HTTP 请求解析
│   ├── Response.hpp            # HTTP 响应构造
│   ├── Router.hpp              # 路由匹配
│   └── Session.hpp             # Cookie/Session（bonus）
└── srcs/
    ├── main.cpp                # 程序入口
    │
    ├── core/                   # ===== 人员 A 的领地 =====
    │   ├── Server.cpp          # socket/bind/listen + 多端口管理
    │   ├── EventLoop.cpp       # poll 循环 + 事件分发
    │   ├── Client.cpp          # 连接生命周期 + 读写缓冲区
    │   └── CgiHandler.cpp      # fork/pipe/execve/超时kill
    │
    ├── http/                   # ===== 人员 B 的领地 =====
    │   ├── Request.cpp         # 请求解析（request line + headers + body）
    │   ├── Response.cpp        # 响应构造（status + headers + body）
    │   ├── Router.cpp          # URI → location 匹配 → 决定行为
    │   └── Session.cpp         # Cookie 解析 + Session 管理（bonus）
    │
    └── config/                 # ===== 人员 B 的领地 =====
        ├── Config.cpp          # 配置数据结构方法
        └── ConfigParser.cpp    # tokenize + parse 配置文件
