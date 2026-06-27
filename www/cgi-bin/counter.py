#!/usr/bin/env python3
"""
counter.py - Session validation script.
Behavior:
  - Read session_id from HTTP_COOKIE.
  - Use session_id as the key for a temporary server-side visit counter file.
  - Print the visit count to verify returning visitor recognition.
"""
import os
import sys

cookie_str  = os.environ.get("HTTP_COOKIE", "")
session_id  = ""

# Parse cookies manually without a Python helper library.
for part in cookie_str.split(";"):
    part = part.strip()
    if part.startswith("session_id="):
        session_id = part.split("=", 1)[1].strip()
        break

# Store the visit count in a /tmp/webserv_session_<id> file.
count = 0
if session_id:
    counter_file = "/tmp/webserv_session_" + session_id
    try:
        with open(counter_file, "r") as f:
            count = int(f.read().strip())
    except Exception:
        count = 0
    count += 1
    try:
        with open(counter_file, "w") as f:
            f.write(str(count))
    except Exception:
        pass

# CGI response.
print("Content-Type: text/html\r")
print("\r")
print("<!DOCTYPE html>")
print("<html><head><meta charset='UTF-8'><title>Session Counter</title>")
print("<style>body{font-family:monospace;background:#111;color:#ccc;padding:2rem;}")
print(".box{background:#1e1e1e;border:1px solid #333;border-radius:8px;padding:1.5rem;max-width:480px;}")
print(".count{font-size:3rem;color:#7ec8e3;font-weight:bold;}")
print("</style></head><body>")
print("<div class='box'>")
print("<h2>🍪 Session Counter</h2>")

if session_id:
    print("<p>Session ID: <code>" + session_id[:8] + "...</code></p>")
    print("<p>You have visited this page <span class='count'>" + str(count) + "</span> time(s).</p>")
    if count == 1:
        print("<p style='color:#f0c040'>👋 Welcome, new visitor!</p>")
    else:
        print("<p style='color:#a3e6a3'>✅ Welcome back! We remember you.</p>")
else:
    print("<p style='color:#f08080'>⚠️ No session_id cookie found.</p>")
    print("<p>Try refreshing — the server should set a cookie on your first visit.</p>")

print("</div></body></html>")
