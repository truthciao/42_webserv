*This project has been created as part of the 42 curriculum by yshi, hanwang.*

# Webserv

## Description

Webserv is a C++98 HTTP server developed for the 42 curriculum. The goal of the
project is to understand how a web server works internally by implementing the
core parts ourselves: socket setup, non-blocking I/O, request parsing, response
generation, routing, file serving, uploads, CGI execution, and configuration
parsing.

The server reads an nginx-inspired configuration file, opens one or more
listening sockets, accepts HTTP clients, parses HTTP/1.1 requests, and sends
responses from static files, generated directory listings, uploaded content, or
CGI scripts.

## Implemented Features

- HTTP/1.1 request parsing with request line, headers, and body handling.
- Support for `GET`, `POST`, and `DELETE` depending on location configuration.
- Static file serving with configurable `root` and `index`.
- Custom error pages.
- Client body size limits.
- Chunked transfer decoding.
- `multipart/form-data` upload parsing.
- Upload storage for configured locations.
- Directory listing through `autoindex`.
- HTTP redirections.
- CGI support for configured extensions, for example Python and PHP scripts.
- Multiple `server` blocks and multiple listening ports.
- Non-blocking event handling with `poll`.

## Instructions

### Compilation

Build the project from the repository root:

```sh
make
```

Useful Makefile targets:

```sh
make clean
make fclean
make re
make valgrind
```

The project is compiled with:

```sh
c++ -Wall -Wextra -Werror -std=c++98
```

### Execution

Run with the default configuration:

```sh
./webserv
```

This is equivalent to:

```sh
./webserv config/default.conf
```

Run with another configuration file:

```sh
./webserv path/to/config.conf
```
Stop the server with `Ctrl-C`.

### Test

```sh
siege -b -c 10 -t 10S http://127.0.0.1:8080/empty.html
siege -b -c 10 -t 30S --header="Connection: keep-alive" http://127.0.0.1:8080/empty.html

siege -b -c 50 -t 30S http://127.0.0.1:8080/empty.html

siege -b -c 100 -t 30S http://127.0.0.1:8080/empty.html

siege -b -c 25 -t 30S http://127.0.0.1:8080/cgi-bin/hello.py

```
```sh
ps aux | grep zombie

watch -n1 'ps aux | grep Z'
```

### Default Configuration

The default configuration listens on:

- `localhost:8080`
- `localhost:8081`

Some useful routes from `config/default.conf`:

- `GET /` serves `www/first/index.html`.
- `GET /listing/` shows an autoindex directory listing.
- `GET`, `POST`, `DELETE /upload/` tests upload and deletion behavior.
- `GET /old-site/` redirects to `/new-site/index.html`.
- `GET`, `POST /cgi-bin/` executes configured CGI scripts.

Example requests:

```sh
curl -i http://localhost:8080/
curl -i http://localhost:8080/listing/
curl -i -X POST -d "key=value" http://localhost:8080/
curl -i http://localhost:8080/cgi-bin/hello.py
```

Upload example:

```sh
curl -i -F "file=@README.md" http://localhost:8080/upload/
```

Delete example:

```sh
curl -i -X DELETE http://localhost:8080/upload/README.md
```

Generate a random file for upload or body-size tests:

```sh
dd if=/dev/urandom of=www/bigfile.bin bs=1M count=10
```

Chunked request example:

```sh
curl -i -X POST -H "Transfer-Encoding: chunked" -d @www/bigfile.bin http://localhost:8080/
```

## Configuration Overview

Configuration files use an nginx-like syntax:

```nginx
server {
    listen 8080;
    server_name localhost;
    client_max_body_size 10485760;

    error_page 404 /www/error_pages/404.html;

    cgi .py /usr/bin/python3;
    cgi .php /usr/bin/php-cgi;

    location / {
        allow_methods GET POST;
        root www/first;
        index index.html;
    }

    location /upload/ {
        allow_methods GET POST DELETE;
        root www/first/upload;
        autoindex on;
        upload_enable on;
        upload_store www/first/upload;
    }
}
```

Main directives used by this project:

- `listen`: port or host:port to bind.
- `server_name`: accepted host names.
- `client_max_body_size`: maximum request body size in bytes.
- `error_page`: custom file for an HTTP error status.
- `cgi`: maps a file extension to a CGI executable.
- `location`: route-specific configuration block.
- `allow_methods`: allowed HTTP methods for a location.
- `root`: filesystem directory used by the location.
- `index`: default file served for a directory.
- `autoindex`: enables or disables generated directory listing.
- `return`: redirects to another URL.
- `upload_enable`: enables upload handling.
- `upload_store`: directory where uploaded files are saved.

## Developer Notes

This section preserves protocol notes that are useful while working on the
server.

### Common HTTP Status Messages

```cpp
case 200: return "OK";
case 201: return "Created";
case 204: return "No Content";
case 301: return "Moved Permanently";
case 302: return "Found";
case 400: return "Bad Request";
case 403: return "Forbidden";
case 404: return "Not Found";
case 405: return "Method Not Allowed";
case 413: return "Request Entity Too Large";
case 415: return "Unsupported Media Type";
case 500: return "Internal Server Error";
```

### HTTP Message Format

An HTTP message is made of a start line, zero or more headers, an empty line,
and an optional body:

```text
HTTP-message = start-line *( header-field CRLF ) CRLF [ message-body ]
request-line = method SP request-target SP HTTP-version CRLF
header-field = field-name ":" OWS field-value OWS
```

Request line format:

```text
METHOD SP URI SP VERSION CRLF
```

Header format:

```text
Field-Name: Field-Value CRLF
```

Response format:

```text
Status-Line\r\nHeaders\r\n\r\nBody
```

Example status line:

```text
HTTP/1.1 200 OK\r\n
```

Important response headers:

- `Content-Type`
- `Content-Length`
- `Connection`

### Chunked Transfer Encoding

Chunked bodies are announced with:

```http
Transfer-Encoding: chunked
```

Example structure:

```text
<HTTP Headers>
Transfer-Encoding: chunked
\r\n

5\r\n
Hello\r\n

b\r\n
hello world\r\n

0\r\n
\r\n
```

Each chunk starts with its size in hexadecimal, followed by `CRLF`, the chunk
data, another `CRLF`, and finally a zero-sized chunk.

### Multipart Form Data

File uploads usually use:

```http
Content-Type: multipart/form-data; boundary=----xxx
```

The boundary separates each part of the body. Each part contains its own headers,
an empty line, and then the part content. For file uploads, the body bytes must
be preserved exactly.

Typical part header:

```http
Content-Disposition: form-data; name="file"; filename="photo.jpg"
```

General structure:

```text
--boundary_string\r\n
Part 1 Headers
\r\n
Part 1 Body
\r\n
--boundary_string\r\n
Part 2 Headers
\r\n
Part 2 Body
\r\n
--boundary_string--\r\n
```

Example:

```http
POST /upload HTTP/1.1
Host: example.com
Content-Type: multipart/form-data; boundary=----WebKitFormBoundary7MA4YWxkTrZu0gW

------WebKitFormBoundary7MA4YWxkTrZu0gW
Content-Disposition: form-data; name="username"

johndoe
------WebKitFormBoundary7MA4YWxkTrZu0gW
Content-Disposition: form-data; name="file"; filename="photo.jpg"
Content-Type: image/jpeg

<raw binary bytes of photo.jpg>
------WebKitFormBoundary7MA4YWxkTrZu0gW--
```

## Resources

Classic references used or useful for this project:

- [RFC 9110: HTTP Semantics](https://www.rfc-editor.org/rfc/rfc9110)
- [RFC 9112: HTTP/1.1](https://www.rfc-editor.org/rfc/rfc9112)
- [RFC 3875: The Common Gateway Interface, CGI Version 1.1](https://www.rfc-editor.org/rfc/rfc3875)
- [RFC 7578: Returning Values from Forms: multipart/form-data](https://www.rfc-editor.org/rfc/rfc7578)
- [MDN Web Docs: HTTP](https://developer.mozilla.org/en-US/docs/Web/HTTP)
- [MDN Web Docs: HTTP messages](https://developer.mozilla.org/en-US/docs/Web/HTTP/Messages)
- [MDN Web Docs: Transfer-Encoding](https://developer.mozilla.org/en-US/docs/Web/HTTP/Headers/Transfer-Encoding)
- [MDN Web Docs: POST](https://developer.mozilla.org/en-US/docs/Web/HTTP/Reference/Methods/POST)
- [Beej's Guide to Network Programming](https://beej.us/guide/bgnet/)
- [Linux manual page: poll(2)](https://man7.org/linux/man-pages/man2/poll.2.html)
- [nginx Beginner's Guide](https://nginx.org/en/docs/beginners_guide.html)
- [nginx Configuration File's Structure](https://nginx.org/en/docs/beginners_guide.html#conf_structure)

### AI Usage

AI was used as a support tool for documentation and learning-oriented tasks:

- Organizing the README structure to match the required 42 format.
- Translating and integrating personal HTTP notes into English.
- Summarizing protocol concepts such as HTTP message format, chunked transfer
  encoding, and multipart form data.
- Suggesting manual `curl` commands for testing server behavior.

AI was not used as an authority for final project validation. The implementation,
configuration choices, tests, and corrections remain the responsibility of the
project authors.
