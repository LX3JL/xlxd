<?php
/**
 * ysfpeers.php — YSF Peers page (dashboard2)
 *
 * Displays connected YSF gateways parsed from the co-located YSF reflector's
 * systemd journal.  The journal parser is shared with dashboard1; the renderer
 * is dashboard2-specific (Bootstrap table classes, SafeOutput helpers).
 *
 * Enabled via $PageOptions['YSFPeerPage']['Show'] = true in config.inc.php.
 * IP display and masquerade settings are inherited from RepeatersPage config.
 */

if (empty($PageOptions['YSFPeerPage']['Show'])) {
    echo '<p class="text-muted">This page is not enabled.</p>';
    return;
}

require_once('./pgs/parse_journal_peers.php');

// Re-use pre-parsed data from index.php when available (avoids a second
// journalctl call on pages that also need the count in the nav menu)
if (!isset($YSFPeers) || !is_array($YSFPeers)) {
    $YSFPeers = ParseJournalPeers($PageOptions['YSFPeerPage']);
}

$__peers     = $YSFPeers;
$__peerLabel = 'YSF Gateway';
$__pageSlug  = 'ysfpeers';
$ipModus     = $PageOptions['RepeatersPage']['IPModus'];
$maskChar    = $PageOptions['RepeatersPage']['MasqueradeCharacter'];

$Reflector->LoadFlags();

require('./pgs/render_journal_peers.php');
?>
