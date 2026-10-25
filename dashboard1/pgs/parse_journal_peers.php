<?php
/**
 * parse_journal_peers.php — Shared journal parsing for peer reflector pages
 *
 * Reads connected gateway/repeater data from journalctl for a given systemd
 * unit.  Both YSFReflector and NXDNReflector log periodic "Currently linked"
 * snapshots in the same format; this function handles both.
 *
 * Returns an array of associative arrays:
 *   [ 'callsign' => string, 'ip' => string, 'port' => string ]
 *
 * Entries from 127.0.0.1 (local XLX bridge) are filtered out.
 */

function ParseJournalPeers(array $pageOpts): array
{
    $unit = isset($pageOpts['ServiceUnit']) ? (string)$pageOpts['ServiceUnit'] : '';

    // Whitelist the unit name to only safe systemd unit characters.
    if (!preg_match('/^[a-zA-Z0-9._@\-]+$/', $unit)) {
        error_log('parse_journal_peers: invalid ServiceUnit name');
        return [];
    }

    $unitArg  = escapeshellarg($unit);
    $sinceArg = escapeshellarg('5 min ago');
    $cmd = "journalctl -u {$unitArg} --no-pager --since {$sinceArg} 2>/dev/null";

    $output = shell_exec($cmd);
    if ($output === null || $output === '') {
        return [];
    }

    $lines = explode("\n", $output);

    // Find the LAST "Currently linked" block (most recent snapshot)
    $lastBlockStart = -1;
    foreach ($lines as $idx => $line) {
        if (strpos($line, 'Currently linked') !== false) {
            $lastBlockStart = $idx;
        }
    }

    if ($lastBlockStart === -1) {
        return [];
    }

    $peers = [];

    // Gateway/repeater lines immediately follow the marker.
    // Format after syslog prefix:
    //   M: YYYY-MM-DD HH:MM:SS.mmm     CALL      : IP:port N/N
    for ($i = $lastBlockStart + 1; $i < count($lines); $i++) {
        $line = $lines[$i];

        // Data lines contain the ": M: " syslog→reflector prefix
        if (strpos($line, ': M: ') === false) {
            break;
        }

        // Extract data after "M: YYYY-MM-DD HH:MM:SS.mmm " (27 chars from "M:")
        $mPos = strpos($line, ': M: ');
        if ($mPos === false) {
            break;
        }

        $dataStart = $mPos + 5 + 24;  // ": M: " (5) + timestamp (24)
        if (strlen($line) <= $dataStart) {
            continue;
        }

        $dataPart = substr($line, $dataStart);

        // Split on " : " to separate callsign from ip:port
        $colonPos = strpos($dataPart, ' : ');
        if ($colonPos === false) {
            continue;
        }

        $callsign  = trim(substr($dataPart, 0, $colonPos));
        $remainder = trim(substr($dataPart, $colonPos + 3));

        // First token is "IP:port"; ignore trailing counter
        $tokens  = explode(' ', $remainder);
        $ipPort  = isset($tokens[0]) ? $tokens[0] : '';

        $lastColon = strrpos($ipPort, ':');
        if ($lastColon === false) {
            continue;
        }
        $ip   = substr($ipPort, 0, $lastColon);
        $port = substr($ipPort, $lastColon + 1);

        // Validate callsign
        if (!preg_match('/^[A-Z0-9\-\/]{2,20}$/i', $callsign)) {
            continue;
        }

        // Validate IP
        if (!filter_var($ip, FILTER_VALIDATE_IP)) {
            continue;
        }

        // Filter out local XLX bridge
        if ($ip === '127.0.0.1') {
            continue;
        }

        $peers[] = [
            'callsign' => strtoupper($callsign),
            'ip'       => $ip,
            'port'     => $port,
        ];
    }

    return $peers;
}

/**
 * HasLocalPeer — true when xlxd.interlink has a peer of this type at 127.0.0.1
 *
 * These pages read the journal of a reflector on this host, so they only make
 * sense when xlxd peers with it locally. $type is 'YSF', 'NXDN' or 'P25', the
 * peer names are matched as xlxd does (YSF..., NX<digit>..., P25<digit>...).
 */
function HasLocalPeer(string $type, string $interlinkFile): bool
{
    static $cache = [];

    $patterns = ['YSF' => '/^YSF/', 'NXDN' => '/^NX[0-9]/', 'P25' => '/^P25[0-9]/'];
    if (!isset($patterns[$type])) {
        return false;
    }

    if (isset($cache[$type])) {
        return $cache[$type];
    }

    $cache[$type] = false;

    // The path is set in config.inc.php, wherever it lives only read a file named xlxd.interlink
    $realPath = realpath($interlinkFile);
    if ($realPath === false || !is_file($realPath)) {
        error_log('HasLocalPeer: interlink file not found: ' . $interlinkFile);
        return false;
    }
    if (basename($realPath) !== 'xlxd.interlink') {
        error_log('HasLocalPeer: not an xlxd.interlink file: ' . $realPath);
        return false;
    }
    if (!is_readable($realPath)) {
        error_log('HasLocalPeer: interlink file not readable: ' . $realPath);
        return false;
    }

    $lines = file($realPath, FILE_IGNORE_NEW_LINES);
    if ($lines === false) {
        return false;
    }

    foreach ($lines as $line) {
        $line = trim($line);
        if ($line === '' || $line[0] === '#') {
            continue;
        }

        // <name> <host[:port]> <modules>, split as xlxd does
        $tokens = preg_split('/[ ,\t]+/', $line);
        if (count($tokens) < 3 || !preg_match($patterns[$type], $tokens[0])) {
            continue;
        }

        $host = explode(':', $tokens[1])[0];
        $isLoopback = (filter_var($host, FILTER_VALIDATE_IP, FILTER_FLAG_IPV4) && strpos($host, '127.') === 0)
                   || strcasecmp($host, 'localhost') === 0;

        if ($isLoopback) {
            $cache[$type] = true;
            break;
        }
    }

    return $cache[$type];
}
?>
