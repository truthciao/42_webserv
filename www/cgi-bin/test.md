针对 CGI 错误处理的测试用例，覆盖以下几类场景：

---

## 1. 无限循环 CGI

**`cgi-bin/infinite_loop.py`**
```python
#!/usr/bin/env python3
import time

print("Content-Type: text/plain\r")
print("\r")
print("Starting infinite loop...\r")

while True:
    time.sleep(1)
```

**`cgi-bin/infinite_loop.sh`**
```bash
#!/bin/bash
echo "Content-Type: text/plain"
echo ""
while true; do
    sleep 1
done
```

---

## 2. 崩溃 / 异常退出

**`cgi-bin/crash.py`**
```python
#!/usr/bin/env python3
import os, sys

print("Content-Type: text/plain\r")
print("\r")

# 强制段错误
os.kill(os.getpid(), 11)  # SIGSEGV
```

**`cgi-bin/exit_nonzero.py`**
```python
#!/usr/bin/env python3
import sys

print("Content-Type: text/plain\r")
print("\r")
sys.exit(1)  # 非 0 退出码
```

---

## 3. 输出格式错误

**`cgi-bin/no_header.py`**
```python
#!/usr/bin/env python3
# 缺少 header，直接输出 body
print("This is body without any HTTP header")
```

**`cgi-bin/bad_header.py`**
```python
#!/usr/bin/env python3
# header 格式错误，缺少空行分隔
print("Content-Type text/plain")   # 缺少冒号
print("This is body")
```

**`cgi-bin/partial_header.py`**
```python
#!/usr/bin/env python3
import sys

# 只输出 header，没有空行，然后立即退出
sys.stdout.write("Content-Type: text/plain")
sys.stdout.flush()
# 没有 \r\n\r\n
```

---

## 4. 超大输出

**`cgi-bin/huge_output.py`**
```python
#!/usr/bin/env python3

print("Content-Type: text/plain\r")
print("\r")

# 输出 1GB 数据，测试服务器是否会 OOM / 超时
chunk = "A" * 1024 * 1024  # 1MB chunk
for i in range(1024):      # 共 1GB
    print(chunk)
```

---

## 5. 超长执行时间（慢启动）

**`cgi-bin/slow_start.py`**
```python
#!/usr/bin/env python3
import time

time.sleep(30)  # 延迟 30 秒才开始输出

print("Content-Type: text/plain\r")
print("\r")
print("Finally responded\r")
```

---

## 6. 文件权限问题

```bash
# 没有执行权限
chmod -x cgi-bin/no_permission.py

# 不存在的解释器
echo '#!/usr/bin/env python_not_exist' > cgi-bin/bad_interpreter.py
echo 'print("hello")' >> cgi-bin/bad_interpreter.py
chmod +x cgi-bin/bad_interpreter.py
```

---

## 7. 读取不存在的环境变量 / 写标准错误

**`cgi-bin/stderr_output.py`**
```python
#!/usr/bin/env python3
import sys

sys.stderr.write("This is an error on stderr\n")
sys.stderr.flush()

print("Content-Type: text/plain\r")
print("\r")
print("Body after stderr output\r")
```

---

## 测试脚本（自动化）

**`test_cgi_errors.sh`**
```bash
#!/bin/bash

HOST="localhost"
PORT="8080"
TIMEOUT=5

run_test() {
    local name="$1"
    local path="$2"
    local expected_code="$3"

    response=$(curl -s -o /dev/null -w "%{http_code}" \
        --max-time $TIMEOUT \
        "http://$HOST:$PORT$path")

    if [ "$response" = "$expected_code" ]; then
        echo "[ PASS ] $name -> HTTP $response"
    else
        echo "[ FAIL ] $name -> expected $expected_code, got $response"
    fi
}

echo "=== CGI Error Handling Tests ==="

run_test "Infinite Loop (timeout)"     "/cgi-bin/infinite_loop.py"    "504"
run_test "Crash / SIGSEGV"             "/cgi-bin/crash.py"            "500"
run_test "Non-zero exit"               "/cgi-bin/exit_nonzero.py"     "500"
run_test "No header output"            "/cgi-bin/no_header.py"        "500"
run_test "Bad header format"           "/cgi-bin/bad_header.py"       "500"
run_test "No execute permission"       "/cgi-bin/no_permission.py"    "403"
run_test "Bad interpreter"             "/cgi-bin/bad_interpreter.py"  "500"
run_test "Slow start (timeout)"        "/cgi-bin/slow_start.py"       "504"

echo "=== Done ==="
```

---

## 预期行为对照表

| 测试场景 | 预期 HTTP 状态 | 服务器应做的事 |
|---------|--------------|--------------|
| 无限循环 | `504 Gateway Timeout` | `kill` 子进程，返回超时 |
| 程序崩溃 | `500 Internal Server Error` | 检测 `waitpid` 信号退出 |
| 非 0 退出 | `500` | 检测 `exit code != 0` |
| 无 header | `500` | parse CGI 输出失败 |
| 无执行权限 | `403` | `execve` 失败 |
| 解释器不存在 | `500` | `execve` 失败 |
| 超大输出 | 正常或 `500` | 不能 OOM，要有 buffer 限制 |
| 慢启动 | `504` | 超时机制要覆盖「还没开始输出」 |
