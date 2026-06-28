#!/usr/bin/env php-cgi
<?php
$method = getenv("REQUEST_METHOD") ?: "GET";
$query  = getenv("QUERY_STRING") ?: "";

// 读取 POST body
$body = "";
if ($method === "POST") {
    $body = file_get_contents("php://stdin");
}

echo "Content-Type: text/html\r\n\r\n";
?>
<html>
<body>
  <h1>Hello from PHP CGI!</h1>
  <p>Method: <?php echo htmlspecialchars($method); ?></p>
  <p>Query: <?php echo htmlspecialchars($query); ?></p>
  <?php if ($body !== ""): ?>
  <p>Body: <?php echo htmlspecialchars($body); ?></p>
  <?php endif; ?>
</body>
</html>
