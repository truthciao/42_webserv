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