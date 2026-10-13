<?php
/**
 * ysfpeers.php — YSF Peers page
 *
 * Displays connected YSF gateways parsed from journalctl.
 * Uses shared parser from parse_journal_peers.php.
 */

if (empty($PageOptions['YSFPeerPage']['Show'])) {
    echo '<p style="font-family:verdana;font-size:10pt;">This page is not enabled.</p>';
    return;
}

require_once('./pgs/parse_journal_peers.php');

if (!isset($YSFPeers) || empty($YSFPeers)) {
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
