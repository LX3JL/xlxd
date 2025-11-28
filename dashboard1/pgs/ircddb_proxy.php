<?php
/**
 * Transparent proxy for live.ircddb.net
 *
 * This proxy allows the ircddb live page to be embedded in the dashboard
 * without CSP or mixed-content issues. It fetches content from the upstream
 * server and rewrites URLs to route back through this proxy.
 */

// Load config to get any overrides
if (file_exists(__DIR__ . "/config.inc.php")) {
    require_once(__DIR__ . "/config.inc.php");
}
if (file_exists(__DIR__ . "/../config.inc.php")) {
    require_once(__DIR__ . "/../config.inc.php");
}

// Defaults - can be overridden in config.inc.php
$ircddbBaseUrl = $PageOptions['IRCDDB']['URL'] ?? 'http://live.ircddb.net:8080';
$ircddbDefaultPage = $PageOptions['IRCDDB']['Page'] ?? 'ircddblive5.html';

// Security: Remove trailing slash from base URL
$ircddbBaseUrl = rtrim($ircddbBaseUrl, '/');

// Get requested path, default to the main page
$path = $_GET['path'] ?? $ircddbDefaultPage;

// Security: Sanitize path - only allow alphanumeric, dots, hyphens, underscores
// This prevents directory traversal and other path-based attacks
if (!preg_match('/^[a-zA-Z0-9._-]+$/', $path)) {
    http_response_code(400);
    die('Invalid path');
}

// Security: Block potentially dangerous file extensions
$blockedExtensions = ['php', 'phtml', 'php3', 'php4', 'php5', 'phps', 'phar'];
$extension = strtolower(pathinfo($path, PATHINFO_EXTENSION));
if (in_array($extension, $blockedExtensions)) {
    http_response_code(403);
    die('Forbidden');
}

// Build the upstream URL
$upstreamUrl = $ircddbBaseUrl . '/' . $path;

// Forward query parameters (except 'path')
$queryParams = $_GET;
unset($queryParams['path']);
if (!empty($queryParams)) {
    $upstreamUrl .= '?' . http_build_query($queryParams);
}

// Fetch content - try cURL first, fall back to file_get_contents
$body = false;
$contentType = 'text/html';
$httpCode = 200;
$error = '';

if (function_exists('curl_init')) {
    // Use cURL if available
    $ch = curl_init();

    curl_setopt_array($ch, [
        CURLOPT_URL => $upstreamUrl,
        CURLOPT_RETURNTRANSFER => true,
        CURLOPT_FOLLOWLOCATION => true,
        CURLOPT_MAXREDIRS => 3,
        CURLOPT_TIMEOUT => 15,
        CURLOPT_CONNECTTIMEOUT => 5,
        CURLOPT_USERAGENT => 'XLX-Dashboard-Proxy/1.0',
        CURLOPT_HEADER => true,
    ]);

    $response = curl_exec($ch);
    $error = curl_error($ch);
    $httpCode = curl_getinfo($ch, CURLINFO_HTTP_CODE);
    $headerSize = curl_getinfo($ch, CURLINFO_HEADER_SIZE);

    curl_close($ch);

    if ($response !== false && $httpCode > 0) {
        // Split headers and body
        $headers = substr($response, 0, $headerSize);
        $body = substr($response, $headerSize);

        // Parse Content-Type from headers
        $headerLines = explode("\r\n", $headers);
        foreach ($headerLines as $header) {
            if (stripos($header, 'Content-Type:') === 0) {
                $contentType = trim(substr($header, 13));
                break;
            }
        }
    }
} else {
    // Fallback to file_get_contents with stream context
    $context = stream_context_create([
        'http' => [
            'method' => 'GET',
            'header' => "User-Agent: XLX-Dashboard-Proxy/1.0\r\n",
            'timeout' => 15,
            'follow_location' => true,
            'max_redirects' => 3,
        ]
    ]);

    $body = @file_get_contents($upstreamUrl, false, $context);

    if ($body !== false && isset($http_response_header)) {
        // Parse response headers
        foreach ($http_response_header as $header) {
            // Get HTTP status code
            if (preg_match('/^HTTP\/\d+\.\d+\s+(\d+)/', $header, $matches)) {
                $httpCode = (int)$matches[1];
            }
            // Get Content-Type
            if (stripos($header, 'Content-Type:') === 0) {
                $contentType = trim(substr($header, 13));
            }
        }
    } else {
        $error = 'Failed to fetch upstream content';
    }
}

// Handle errors
if ($body === false) {
    http_response_code(502);
    die('Upstream server unavailable: ' . htmlspecialchars($error));
}

// Forward the HTTP status code
http_response_code($httpCode);

// Set content type
header('Content-Type: ' . $contentType);

// Determine if we need to rewrite URLs in this content
$rewriteContent = (
    stripos($contentType, 'text/html') !== false ||
    stripos($contentType, 'text/javascript') !== false ||
    stripos($contentType, 'application/javascript') !== false ||
    stripos($contentType, 'application/x-javascript') !== false
);

if ($rewriteContent && !empty($body)) {
    // Rewrite relative URLs to go through this proxy
    // Handle src="file.js", href="file.css", url("file"), etc.

    // Rewrite src attributes
    $body = preg_replace(
        '/src\s*=\s*"([^"\/][^"]*)"/',
        'src="ircddb_proxy.php?path=$1"',
        $body
    );
    $body = preg_replace(
        "/src\s*=\s*'([^'\/][^']*)'/",
        "src='ircddb_proxy.php?path=\$1'",
        $body
    );

    // Rewrite href attributes (for CSS, etc.)
    $body = preg_replace(
        '/href\s*=\s*"([^"\/][^"]*\.(css|ico))"/',
        'href="ircddb_proxy.php?path=$1"',
        $body
    );
    $body = preg_replace(
        "/href\s*=\s*'([^'\/][^']*\.(css|ico))'/",
        "href='ircddb_proxy.php?path=\$1'",
        $body
    );

    // Rewrite AJAX calls in JavaScript (the jj.yaws or jj3.yaws polling)
    // The original livelog.js uses: url: "jj.yaws", data: "p=" + lastNum
    // jQuery appends data as query string, so we need: url: "ircddb_proxy.php?path=jj.yaws"
    // Then jQuery will make it: ircddb_proxy.php?path=jj.yaws&p=123
    $body = preg_replace(
        '/url:\s*"(jj[0-9]*\.yaws)"/',
        'url: "ircddb_proxy.php?path=$1"',
        $body
    );
}

// Output the content
echo $body;
