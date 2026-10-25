<?php
/**
 * nxdnpeers.php — NXDN Peers page
 *
 * Displays connected NXDN repeaters parsed from journalctl.
 * Uses shared parser from parse_journal_peers.php.
 */

if (empty($PageOptions['NXDNPeerPage']['Show'])) {
    if (!empty($PageOptions['NXDNPeerPage']['NoLocalPeer'])) {
        echo '<p style="font-family:verdana;font-size:10pt;">No local NXDN peer (127.0.0.1) in xlxd.interlink, this page only works with a NXDN reflector on this host.</p>';
    } else {
        echo '<p style="font-family:verdana;font-size:10pt;">This page is not enabled.</p>';
    }
    return;
}

require_once('./pgs/parse_journal_peers.php');

if (!isset($NXDNPeers) || empty($NXDNPeers)) {
    $NXDNPeers = ParseJournalPeers($PageOptions['NXDNPeerPage']);
}

$__peers     = $NXDNPeers;
$__peerLabel = 'NXDN Repeater';
$__pageSlug  = 'nxdnpeers';
$ipModus     = $PageOptions['RepeatersPage']['IPModus'];
$maskChar    = $PageOptions['RepeatersPage']['MasqueradeCharacter'];

$Reflector->LoadFlags();

require('./pgs/render_journal_peers.php');
?>
