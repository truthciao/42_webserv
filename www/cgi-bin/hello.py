#!/usr/bin/env python3
import os
import sys

method  = os.environ.get("REQUEST_METHOD", "GET")
query   = os.environ.get("QUERY_STRING", "")

# 读取请求 body
body = ""
if method == "POST":
    content_length = os.environ.get("CONTENT_LENGTH", "0")
    try:
        length = int(content_length)
        if length > 0:
            body = sys.stdin.read(length)
    except (ValueError, IOError):
        body = ""

print("Content-Type: text/html\r\n\r\n")
print("<html><body>")
print("<h1>Hello from CGI!</h1>")
print("<p>Method: " + method + "</p>")
print("<p>Query: "  + query  + "</p>")
print("<p>Body: "   + body   + "</p>")
print("</body></html>")
