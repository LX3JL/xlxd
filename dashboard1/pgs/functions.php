<?php

function sanitize_output($string) {
    if ($string === null) {
        return '';
    }

    return htmlspecialchars($string, ENT_QUOTES | ENT_HTML5, 'UTF-8');
}

function sanitize_attribute($string) {
    if ($string === null) {
        return '';
    }

    return htmlspecialchars($string, ENT_QUOTES | ENT_HTML5, 'UTF-8');
}

function validate_callsign($callsign) {
    $callsign = trim($callsign);

    if (preg_match('/^[A-Z0-9\-\/\s]{3,20}$/i', $callsign)) {
        return strtoupper($callsign);
    }

    return '';
}

function validate_module($module) {
    $module = trim(strtoupper($module));

    if (preg_match('/^[A-Z]$/', $module)) {
        return $module;
    }

    return '';
}

function validate_protocol($protocol) {
    $allowed = [
        'DPlus', 'DExtra', 'DCS', 'DMR', 'DMRplus',
        'DMRMmdvm', 'YSF', 'XLX', 'Terminal/AP',
        'IMRS', 'M17', 'none'
    ];

    $protocol = trim($protocol);

    return in_array($protocol, $allowed, true) ? $protocol : '';
}

function GetSystemUptime() {
    if (!file_exists('/proc/uptime')) {
        return 0;
    }

    $uptime = @file_get_contents('/proc/uptime');

    if ($uptime === false) {
        return 0;
    }

    $parts = explode(' ', $uptime);

    return isset($parts[0]) ? (int)$parts[0] : 0;
}

function Debug($message) {
    if (defined('DEBUG_MODE') && DEBUG_MODE === true) {
        echo '<br><hr><pre>';
        print_r($message);
        echo '</pre><hr><br>';
    }
}

function ParseTime($Input) {
    if (empty($Input) || !is_string($Input)) {
        return false;
    }

    $Input = strip_tags($Input);

    if (strpos($Input, '<') !== false) {
        $Input = substr($Input, 0, strpos($Input, '<'));
    }

    $tmp = preg_split('/\s+/', trim($Input));

    // Expected format:
    // Tuesday Tue Nov 17 14:23:22 2015
    if (count($tmp) < 6) {
        return false;
    }

    if (trim($tmp[3]) === '') {
        unset($tmp[3]);
        $tmp = array_values($tmp);
    }

    if (!isset($tmp[2], $tmp[3], $tmp[4], $tmp[5])) {
        return false;
    }

    $tmp1 = explode(':', $tmp[4]);

    if (count($tmp1) < 3) {
        return false;
    }

    $months = [
        'jan' => 1, 'feb' => 2, 'mar' => 3,
        'apr' => 4, 'may' => 5, 'jun' => 6,
        'jul' => 7, 'aug' => 8, 'sep' => 9,
        'oct' => 10, 'nov' => 11, 'dec' => 12
    ];

    $month = $months[strtolower($tmp[2])] ?? 1;

    return @mktime(
        (int)$tmp1[0],
        (int)$tmp1[1],
        (int)$tmp1[2],
        $month,
        (int)$tmp[3],
        (int)$tmp[5]
    );
}

function FormatSeconds($seconds) {
    $seconds = abs((int)$seconds);

    return sprintf(
        '%d days %02d:%02d:%02d',
        (int)($seconds / 86400),
        (int)(($seconds / 3600) % 24),
        (int)(($seconds / 60) % 60),
        $seconds % 60
    );
}

function CreateCode($laenge) {
    $zeichen = '1234567890abcdefghijklmnopqrstuvwyxzABCDEFGHIJKLMNAOPQRSTUVWYXZ';
    $out = '';

    for ($i = 1; $i <= $laenge; $i++) {
        $out .= $zeichen[mt_rand(0, strlen($zeichen) - 1)];
    }

    return $out;
}

function VNStatLocalize($str) {
    global $L;

    return isset($L[$str]) ? $L[$str] : $str;
}

function VNStatGetData($iface, $vnstat_bin) {
    $empty = [
        [], [], [], [], [], [], []
    ];

    if (!is_string($iface) ||
        !preg_match('/^[a-zA-Z0-9_-]+$/', $iface)) {
        return $empty;
    }

    if (!is_string($vnstat_bin) || !is_executable($vnstat_bin)) {
        return $empty;
    }

    /*
     * Read daily data.
     */
    $command = escapeshellarg($vnstat_bin)
        . ' --json d 30 -i '
        . escapeshellarg($iface)
        . ' 2>/dev/null';

    $json = @shell_exec($command);
    $data = json_decode($json, true);

    if (!is_array($data) ||
        !isset($data['interfaces'][0]['traffic'])) {
        return $empty;
    }

    $interfaceData = $data['interfaces'][0];
    $traffic = $interfaceData['traffic'];

    $day = [];
    $month = [];
    $hour = [];
    $top = [];
    $summary = [];

    $now = time();
    $today = date('Y-n-j', $now);
    $currentYear = (int)date('Y', $now);
    $currentMonth = (int)date('n', $now);

    /*
     * The database creation time matters when collection
     * started partway through the current day or month.
     */
    $created = (int)(
        $interfaceData['created']['timestamp'] ?? $now
    );

    $startOfDay = strtotime(date('Y-m-d 00:00:00', $now));
    $startOfCaptureToday = max($created, $startOfDay);

    $todayHours = max(
        ($now - $startOfCaptureToday) / 3600,
        1 / 60
    );

    foreach (($traffic['day'] ?? []) as $entry) {
        $rx = (int)($entry['rx'] ?? 0);
        $tx = (int)($entry['tx'] ?? 0);
        $date = $entry['date'] ?? [];

        $year = (int)($date['year'] ?? $currentYear);
        $monthNumber = (int)($date['month'] ?? $currentMonth);
        $dayNumber = (int)($date['day'] ?? date('j', $now));

        /*
         * Use local noon for display timestamps to avoid
         * date shifts around timezone boundaries.
         */
        $timestamp = mktime(
            12, 0, 0,
            $monthNumber,
            $dayNumber,
            $year
        );

        $entryDate = "$year-$monthNumber-$dayNumber";
        $isToday = ($entryDate === $today);

        /*
         * Today's average starts at the later of midnight
         * and database creation. Previous days use 24 hours.
         */
        $hours = $isToday ? $todayHours : 24;

        $day[] = [
            'time' => $timestamp,
            'rx'   => $rx,
            'tx'   => $tx,
            'rx2'  => (int)($rx / $hours),
            'tx2'  => (int)($tx / $hours),
            'act'  => 1
        ];
    }

    /*
     * Read monthly data.
     */
    $command = escapeshellarg($vnstat_bin)
        . ' --json m 12 -i '
        . escapeshellarg($iface)
        . ' 2>/dev/null';

    $json = @shell_exec($command);
    $monthlyData = json_decode($json, true);

    $monthlyInterface = $monthlyData['interfaces'][0] ?? [];
    $monthlyTraffic = $monthlyInterface['traffic']['month'] ?? [];

    /*
     * Calculate the elapsed hours for the current month.
     * Use the same database creation timestamp as for daily data.
     */
    $startOfMonth = strtotime(date('Y-m-01 00:00:00', $now));
    $startOfCaptureThisMonth = max($created, $startOfMonth);

    $currentMonthHours = max(
        ($now - $startOfCaptureThisMonth) / 3600,
        1 / 60
    );

    foreach ($monthlyTraffic as $entry) {
        $rx = (int)($entry['rx'] ?? 0);
        $tx = (int)($entry['tx'] ?? 0);
        $date = $entry['date'] ?? [];

        $year = (int)($date['year'] ?? $currentYear);
        $monthNumber = (int)($date['month'] ?? $currentMonth);

        $timestamp = mktime(
            12, 0, 0,
            $monthNumber,
            1,
            $year
        );

        $isCurrentMonth = (
            $year === $currentYear &&
            $monthNumber === $currentMonth
        );

        if ($isCurrentMonth) {
            $hours = $currentMonthHours;
        } else {
            $daysInMonth = cal_days_in_month(
                CAL_GREGORIAN,
                $monthNumber,
                $year
            );

            $hours = $daysInMonth * 24;
        }

        $hours = max($hours, 1 / 60);

        $month[] = [
            'time' => $timestamp,
            'rx'   => $rx,
            'tx'   => $tx,
            'rx2'  => (int)($rx / $hours),
            'tx2'  => (int)($tx / $hours),
            'act'  => 1
        ];
    }

    /*
     * Newest day and month first.
     */
    usort($day, function ($a, $b) {
        return $b['time'] <=> $a['time'];
    });

    usort($month, function ($a, $b) {
        return $b['time'] <=> $a['time'];
    });

    /*
     * Keep the existing seven-element return structure
     * for compatibility with the dashboard.
     */
    return [
        $day,
        $month,
        $hour,
        $day,
        $month,
        $top,
        $summary
    ];
}

function kbytes_to_string($bytes) {
    $units = ['B', 'KB', 'MB', 'GB', 'TB'];
    $size = (float)$bytes;
    $i = 0;

    while ($size >= 1024 && $i < count($units) - 1) {
        $size /= 1024;
        $i++;
    }

    return sprintf('%.2f %s', $size, $units[$i]);
}

?>
