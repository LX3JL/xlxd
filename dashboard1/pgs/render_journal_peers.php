<?php
/**
 * render_journal_peers.php — Shared table rendering for journal peer pages
 *
 * Expects these variables to be set by the calling page:
 *   $__peers     — array of peer entries (each: callsign, ip, port)
 *   $__peerLabel — column header label (e.g. "YSF Gateway")
 *   $__pageSlug  — page slug for paging/filter links (e.g. "ysfpeers")
 *   $ipModus     — IP display mode
 *   $maskChar    — masquerade character
 *   $Reflector   — xReflector instance (for GetFlag)
 *   $PageOptions — global config (reads PeerPage LimitTo, PageRefreshDelay)
 */

if (!isset($__peers))     { $__peers = []; }
if (!isset($__peerLabel)) { $__peerLabel = 'Gateway'; }
if (!isset($__pageSlug))  { $__pageSlug = 'ysfpeers'; }

$__limitTo = isset($PageOptions['PeerPage']['LimitTo']) ? (int)$PageOptions['PeerPage']['LimitTo'] : 99;
$__filterKey = 'PeerFilter_' . $__pageSlug;
$__colSpan = ($ipModus !== 'HideIP') ? 4 : 3;

// Handle filter POST/reset
if (!isset($_SESSION[$__filterKey])) {
    $_SESSION[$__filterKey] = null;
}
if (isset($_POST['do']) && $_POST['do'] == 'SetPeerFilter' && isset($_POST['peer_page']) && $_POST['peer_page'] === $__pageSlug) {
    if (isset($_POST['txtPeerCallsignFilter'])) {
        $__val = trim($_POST['txtPeerCallsignFilter']);
        if ($__val === '') {
            $_SESSION[$__filterKey] = null;
        } else {
            $__val = preg_replace('/[^A-Z0-9\*\-\/\s]/i', '', $__val);
            if (strpos($__val, '*') === false) {
                $__val = '*' . $__val . '*';
            }
            $_SESSION[$__filterKey] = $__val;
        }
    }
} elseif (isset($_GET['do']) && $_GET['do'] === 'resetfilter' && isset($_GET['show']) && $_GET['show'] === $__pageSlug) {
    $_SESSION[$__filterKey] = null;
}

// Apply callsign filter
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

// Pagination
$__totalPeers = count($__peers);
$__currentPage = isset($_GET['page']) ? max(1, intval($_GET['page'])) : 1;
$__totalPages = ($__limitTo > 0) ? max(1, ceil($__totalPeers / $__limitTo)) : 1;
if ($__currentPage > $__totalPages) { $__currentPage = $__totalPages; }
$__startIndex = ($__currentPage - 1) * $__limitTo;
$__endIndex = min($__startIndex + $__limitTo, $__totalPeers);

?>
<table class="listingtable">
<?php if ($PageOptions['UserPage']['ShowFilter']): ?>
 <tr>
   <th colspan="<?php echo $__colSpan; ?>">
      <table width="100%" border="0">
         <tr>
            <td align="left">
               <form name="frmPeerFilter_<?php echo sanitize_attribute($__pageSlug); ?>" method="post" action="./index.php?show=<?php echo sanitize_attribute($__pageSlug); ?>">
                  <input type="hidden" name="do" value="SetPeerFilter" />
                  <input type="hidden" name="peer_page" value="<?php echo sanitize_attribute($__pageSlug); ?>" />
                  <input type="text" class="FilterField" value="<?php echo sanitize_attribute($_SESSION[$__filterKey]); ?>" name="txtPeerCallsignFilter" placeholder="Callsign" onfocus="SuspendPageRefresh();" onblur="setTimeout(ReloadPage, <?php echo $PageOptions['PageRefreshDelay']; ?>);" />
                  <input type="submit" value="Apply" class="FilterSubmit" />
               </form>
            </td>
<?php if ($_SESSION[$__filterKey] !== null): ?>
            <td><a href="./index.php?show=<?php echo sanitize_attribute($__pageSlug); ?>&amp;do=resetfilter" class="smalllink">Disable filter</a></td>
<?php endif; ?>
         </tr>
      </table>
   </th>
 </tr>
<?php endif; ?>
 <tr>
   <th width="25">#</th>
   <th width="60">Flag</th>
   <th width="150"><?php echo sanitize_output($__peerLabel); ?></th><?php

if ($ipModus !== 'HideIP') {
    echo '
   <th width="150">IP</th>';
}

?>
 </tr>
<?php

$odd = '';

for ($__i = $__startIndex; $__i < $__endIndex; $__i++) {
    $peer = $__peers[$__i];
    $rowNum = $__i + 1;

    $odd = ($odd === '#FFFFFF') ? '#F1FAFA' : '#FFFFFF';

    $callsign = $peer['callsign'];
    $ip       = $peer['ip'];

    echo '
   <tr height="30" bgcolor="' . $odd . '" onMouseOver="this.bgColor=\'#FFFFCA\';" onMouseOut="this.bgColor=\'' . $odd . '\';">
   <td align="center">' . $rowNum . '</td>
   <td align="center">';

    list($flagFile, $flagName) = $Reflector->GetFlag($callsign);
    if ($flagFile !== '' && file_exists('./img/flags/' . $flagFile . '.png')) {
        echo '<a href="#" class="tip"><img src="./img/flags/' . sanitize_attribute($flagFile) . '.png" height="15" alt="' . sanitize_attribute($flagName) . '" /><span>' . sanitize_output($flagName) . '</span></a>';
    }

    echo '</td>
   <td>' . sanitize_output($callsign) . '</td>';

    if ($ipModus !== 'HideIP') {
        echo '
   <td>';
        $bytes = explode('.', $ip);
        if ($bytes !== false && count($bytes) === 4) {
            switch ($ipModus) {
                case 'ShowLast1ByteOfIP':
                    echo sanitize_output($maskChar . '.' . $maskChar . '.' . $maskChar . '.' . $bytes[3]);
                    break;
                case 'ShowLast2ByteOfIP':
                    echo sanitize_output($maskChar . '.' . $maskChar . '.' . $bytes[2] . '.' . $bytes[3]);
                    break;
                case 'ShowLast3ByteOfIP':
                    echo sanitize_output($maskChar . '.' . $bytes[1] . '.' . $bytes[2] . '.' . $bytes[3]);
                    break;
                default:
                    echo sanitize_output($ip);
            }
        }
        echo '</td>';
    }

    echo '
   </tr>';
}

if ($__totalPeers === 0) {
    echo '
   <tr height="30" bgcolor="#FFFFFF">
      <td colspan="' . $__colSpan . '" align="center" style="font-family:verdana;font-size:10pt;color:#666666;">No peers currently connected</td>
   </tr>';
}

?>
</table>

<?php if ($__totalPages > 1): ?>
<div style="text-align: center; margin: 10px 0; padding: 10px;">
   <span style="margin-right: 15px;">Showing <?php echo ($__startIndex + 1); ?>-<?php echo $__endIndex; ?> of <?php echo $__totalPeers; ?></span>
   <?php if ($__currentPage > 1): ?>
      <a href="./index.php?show=<?php echo sanitize_attribute($__pageSlug); ?>&amp;page=1" class="smalllink">&laquo; First</a>
      <a href="./index.php?show=<?php echo sanitize_attribute($__pageSlug); ?>&amp;page=<?php echo ($__currentPage - 1); ?>" class="smalllink">&lt; Prev</a>
   <?php endif; ?>

   <?php
   $__startP = max(1, $__currentPage - 2);
   $__endP = min($__totalPages, $__currentPage + 2);
   for ($__p = $__startP; $__p <= $__endP; $__p++):
      if ($__p == $__currentPage):
   ?>
      <strong style="margin: 0 5px;"><?php echo $__p; ?></strong>
   <?php else: ?>
      <a href="./index.php?show=<?php echo sanitize_attribute($__pageSlug); ?>&amp;page=<?php echo $__p; ?>" class="smalllink" style="margin: 0 5px;"><?php echo $__p; ?></a>
   <?php
      endif;
   endfor;
   ?>

   <?php if ($__currentPage < $__totalPages): ?>
      <a href="./index.php?show=<?php echo sanitize_attribute($__pageSlug); ?>&amp;page=<?php echo ($__currentPage + 1); ?>" class="smalllink">Next &gt;</a>
      <a href="./index.php?show=<?php echo sanitize_attribute($__pageSlug); ?>&amp;page=<?php echo $__totalPages; ?>" class="smalllink">Last &raquo;</a>
   <?php endif; ?>
</div>
<?php endif; ?>
<?php unset($__peers, $__peerLabel, $__pageSlug, $__totalPeers, $__currentPage, $__totalPages, $__startIndex, $__endIndex, $__limitTo, $__filterKey, $__colSpan, $__i, $__startP, $__endP, $__p, $__val); ?>
