<?php
/**
 * p25peers.php — P25 Peers page (dashboard2)
 *
 * Displays connected P25 repeaters parsed from the co-located P25 reflector's
 * systemd journal.  The journal parser is shared with dashboard1; the renderer
 * is dashboard2-specific (Bootstrap table classes, SafeOutput helpers).
 *
 * Enabled via $PageOptions['P25PeerPage']['Show'] = true in config.inc.php.
 * IP display and masquerade settings are inherited from RepeatersPage config.
 */

if (empty($PageOptions['P25PeerPage']['Show'])) {
    echo '<p class="text-muted">This page is not enabled.</p>';
    return;
}

require_once('./pgs/parse_journal_peers.php');

// Re-use pre-parsed data from index.php when available (avoids a second
// journalctl call on pages that also need the count in the nav menu)
if (!isset($P25Peers) || !is_array($P25Peers)) {
    $P25Peers = ParseJournalPeers($PageOptions['P25PeerPage']);
}

$__peers     = $P25Peers;
$__peerLabel = 'P25 Repeater';
$__pageSlug  = 'p25peers';
$ipModus     = $PageOptions['RepeatersPage']['IPModus'];
$maskChar    = $PageOptions['RepeatersPage']['MasqueradeCharacter'];

$Reflector->LoadFlags();

require('./pgs/render_journal_peers.php');
?>
