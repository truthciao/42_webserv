#!/usr/bin/env python3
import os

method = os.environ.get("REQUEST_METHOD", "GET")
query  = os.environ.get("QUERY_STRING", "")

print("Content-Type: text/html\r\n\r\n")
print("<html><body>")
print("<h1>Hello from CGI!</h1>")
print("<p>Method: " + method + "</p>")
print("<p>Query: " + query + "</p>")
print("</body></html>")
