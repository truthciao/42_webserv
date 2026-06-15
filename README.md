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

post 测试
- 测试：`curl -X POST -d "key=value" localhost:8080`
- 测试：`curl -X POST -H "Transfer-Encoding: chunked" -d @bigfile localhost:8080`
