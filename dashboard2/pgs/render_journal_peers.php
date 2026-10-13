<?php
/**
 * render_journal_peers.php — Shared Bootstrap table renderer for journal peer pages (dashboard2)
 *
 * Expects the calling page to set these variables before including:
 *   $__peers     — array of peer entries, each: [ 'callsign' => string, 'ip' => string, 'port' => string ]
 *   $__peerLabel — column header label, e.g. "YSF Gateway"
 *   $__pageSlug  — URL slug used for filter/paging links, e.g. "ysfpeers"
 *   $ipModus     — IP display mode (mirrors $PageOptions['RepeatersPage']['IPModus'])
 *   $maskChar    — masquerade character (mirrors $PageOptions['RepeatersPage']['MasqueradeCharacter'])
 *
 * Implicit dependencies (provided by dashboard2's index.php bootstrap):
 *   $Reflector   — xReflector instance (for GetFlag(); LoadFlags() called by the page file)
 *   $PageOptions — global config; reads PeerPage.LimitTo and PageRefreshDelay
 *
 * HTML conventions match dashboard2's peers.php / repeaters.php exactly:
 *   - Bootstrap table classes: table table-striped table-hover
 *   - Row/column alignment class: table-center
 *   - Flag images via the a.tip / span tooltip pattern from repeaters.php
 *   - SafeOutput() / SafeOutputAttr() from dashboard2's functions.php
 */

// Defensive defaults in case a calling page omitted a variable
if (!isset($__peers))     { $__peers = []; }
if (!isset($__peerLabel)) { $__peerLabel = 'Gateway'; }
if (!isset($__pageSlug))  { $__pageSlug = 'ysfpeers'; }
if (!isset($ipModus))     { $ipModus = 'ShowFullIP'; }
if (!isset($maskChar))    { $maskChar = '*'; }

// Rows-per-page comes from the shared PeerPage config block
$__limitTo  = isset($PageOptions['PeerPage']['LimitTo']) ? (int)$PageOptions['PeerPage']['LimitTo'] : 99;
$__filterKey = 'PeerFilter_' . $__pageSlug;

// Column count used for colspan on the "no peers" row
$__colSpan = ($ipModus !== 'HideIP') ? 4 : 3;

// -------------------------------------------------------------------------
// Filter state — stored in session so it survives auto-refresh
// -------------------------------------------------------------------------
if (!isset($_SESSION[$__filterKey])) {
    $_SESSION[$__filterKey] = null;
}

// Apply filter value from POST (callsign search form submission)
if (isset($_POST['do']) && $_POST['do'] === 'SetPeerFilter'
    && isset($_POST['peer_page']) && $_POST['peer_page'] === $__pageSlug) {
    if (isset($_POST['txtPeerCallsignFilter'])) {
        $__val = trim($_POST['txtPeerCallsignFilter']);
        if ($__val === '') {
            $_SESSION[$__filterKey] = null;
        } else {
            // Strip characters that are not valid in a callsign or glob wildcard
            $__val = preg_replace('/[^A-Z0-9\*\-\/\s]/i', '', $__val);
            // Auto-wrap in wildcards so a bare callsign fragment matches
            if (strpos($__val, '*') === false) {
                $__val = '*' . $__val . '*';
            }
            $_SESSION[$__filterKey] = $__val;
        }
    }
} elseif (isset($_GET['do']) && $_GET['do'] === 'resetfilter'
          && isset($_GET['show']) && $_GET['show'] === $__pageSlug) {
    // Reset via GET link (safe: no state change beyond the session)
    $_SESSION[$__filterKey] = null;
}

// Apply the active filter to the peer list
if ($_SESSION[$__filterKey] !== null) {
    $__filtered = [];
    foreach ($__peers as $__p) {
        if (fnmatch($_SESSION[$__filterKey], $__p['callsign'], FNM_CASEFOLD)) {
            $__filtered[] = $__p;
        }
    }
    $__peers = $__filtered;
    unset($__filtered);
}

// -------------------------------------------------------------------------
// Pagination
// -------------------------------------------------------------------------
$__totalPeers  = count($__peers);
$__currentPage = isset($_GET['page']) ? max(1, intval($_GET['page'])) : 1;
$__totalPages  = ($__limitTo > 0) ? max(1, (int)ceil($__totalPeers / $__limitTo)) : 1;
if ($__currentPage > $__totalPages) { $__currentPage = $__totalPages; }
$__startIndex  = ($__currentPage - 1) * $__limitTo;
$__endIndex    = min($__startIndex + $__limitTo, $__totalPeers);

// Safe slug for use in HTML attributes/URLs
$__safeSlug = SafeOutputAttr($__pageSlug);

?>

<?php if (!empty($PageOptions['UserPage']['ShowFilter'])): ?>
<form name="frmPeerFilter_<?php echo $__safeSlug; ?>" method="post"
      action="./index.php?show=<?php echo $__safeSlug; ?>"
      class="form-inline" style="margin-bottom: 10px;">
    <input type="hidden" name="do" value="SetPeerFilter" />
    <input type="hidden" name="peer_page" value="<?php echo $__safeSlug; ?>" />
    <div class="form-group">
        <label for="txtPeerCallsignFilter_<?php echo $__safeSlug; ?>" class="sr-only">Callsign filter</label>
        <input type="text"
               class="form-control input-sm"
               id="txtPeerCallsignFilter_<?php echo $__safeSlug; ?>"
               name="txtPeerCallsignFilter"
               value="<?php echo SafeOutputAttr((string)$_SESSION[$__filterKey]); ?>"
               placeholder="Callsign"
               onfocus="SuspendPageRefresh();"
               onblur="setTimeout(ReloadPage, <?php echo (int)$PageOptions['PageRefreshDelay']; ?>);" />
    </div>
    <button type="submit" class="btn btn-default btn-sm">Apply</button>
    <?php if ($_SESSION[$__filterKey] !== null): ?>
    <a href="./index.php?show=<?php echo $__safeSlug; ?>&amp;do=resetfilter"
       class="btn btn-link btn-sm">Clear filter</a>
    <?php endif; ?>
</form>
<?php endif; ?>

<table class="table table-striped table-hover">
    <tr class="table-center">
        <th class="col-md-1">#</th>
        <th class="col-md-1">Flag</th>
        <th class="col-md-3"><?php echo SafeOutput($__peerLabel); ?></th>
        <?php if ($ipModus !== 'HideIP'): ?>
        <th class="col-md-2">IP</th>
        <?php endif; ?>
    </tr>
<?php

for ($__i = $__startIndex; $__i < $__endIndex; $__i++) {
    $peer     = $__peers[$__i];
    $rowNum   = $__i + 1;
    $callsign = $peer['callsign'];
    $ip       = $peer['ip'];

    echo '
    <tr class="table-center">
        <td>' . $rowNum . '</td>
        <td>';

    // Country flag via xReflector::GetFlag() — same pattern as repeaters.php
    list($flagFile, $flagName) = $Reflector->GetFlag($callsign);
    if ($flagFile !== '' && file_exists('./img/flags/' . $flagFile . '.png')) {
        echo '<a href="#" class="tip">'
            . '<img src="./img/flags/' . SafeOutputAttr($flagFile) . '.png" class="table-flag" alt="' . SafeOutputAttr($flagName) . '">'
            . '<span>' . SafeOutput($flagName) . '</span>'
            . '</a>';
    }

    echo '</td>
        <td>' . SafeOutput($callsign) . '</td>';

    if ($ipModus !== 'HideIP') {
        echo '
        <td>';
        $bytes = explode('.', $ip);
        if ($bytes !== false && count($bytes) === 4) {
            switch ($ipModus) {
                case 'ShowLast1ByteOfIP':
                    echo SafeOutput($maskChar . '.' . $maskChar . '.' . $maskChar . '.' . $bytes[3]);
                    break;
                case 'ShowLast2ByteOfIP':
                    echo SafeOutput($maskChar . '.' . $maskChar . '.' . $bytes[2] . '.' . $bytes[3]);
                    break;
                case 'ShowLast3ByteOfIP':
                    echo SafeOutput($maskChar . '.' . $bytes[1] . '.' . $bytes[2] . '.' . $bytes[3]);
                    break;
                default:
                    // ShowFullIP — link to the IP as dashboard2's peers.php does
                    echo '<a href="http://' . SafeOutputAttr($ip) . '" target="_blank"'
                        . ' style="text-decoration:none;color:#000000;">'
                        . SafeOutput($ip) . '</a>';
            }
        }
        echo '</td>';
    }

    echo '
    </tr>';
}

if ($__totalPeers === 0) {
    echo '
    <tr class="table-center">
        <td colspan="' . $__colSpan . '">No peers currently connected</td>
    </tr>';
}

?>
</table>

<?php if ($__totalPages > 1): ?>
<nav aria-label="Journal peer pagination">
    <ul class="pagination pagination-sm" style="display: flex; flex-wrap: wrap; gap: 2px;">
        <?php if ($__currentPage > 1): ?>
        <li><a href="./index.php?show=<?php echo $__safeSlug; ?>&amp;page=1">&laquo;</a></li>
        <li><a href="./index.php?show=<?php echo $__safeSlug; ?>&amp;page=<?php echo ($__currentPage - 1); ?>">&lsaquo;</a></li>
        <?php endif; ?>

        <?php
        $__startP = max(1, $__currentPage - 2);
        $__endP   = min($__totalPages, $__currentPage + 2);
        for ($__p = $__startP; $__p <= $__endP; $__p++):
        ?>
        <li<?php echo ($__p === $__currentPage) ? ' class="active"' : ''; ?>>
            <a href="./index.php?show=<?php echo $__safeSlug; ?>&amp;page=<?php echo $__p; ?>"><?php echo $__p; ?></a>
        </li>
        <?php endfor; ?>

        <?php if ($__currentPage < $__totalPages): ?>
        <li><a href="./index.php?show=<?php echo $__safeSlug; ?>&amp;page=<?php echo ($__currentPage + 1); ?>">&rsaquo;</a></li>
        <li><a href="./index.php?show=<?php echo $__safeSlug; ?>&amp;page=<?php echo $__totalPages; ?>">&raquo;</a></li>
        <?php endif; ?>
    </ul>
    <p class="text-muted text-sm" style="font-size: 0.85em;">
        Showing <?php echo ($__startIndex + 1); ?>&#8211;<?php echo $__endIndex; ?> of <?php echo $__totalPeers; ?>
    </p>
</nav>
<?php endif; ?>

<?php
// Clean up template variables to avoid polluting the calling scope
unset($__peers, $__peerLabel, $__pageSlug, $__safeSlug, $__limitTo, $__filterKey,
      $__colSpan, $__totalPeers, $__currentPage, $__totalPages, $__startIndex,
      $__endIndex, $__i, $__startP, $__endP, $__p, $__val,
      $peer, $rowNum, $callsign, $ip, $bytes, $flagFile, $flagName);
?>
