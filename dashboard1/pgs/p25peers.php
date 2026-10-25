<?php
/**
 * p25peers.php — P25 Peers page
 *
 * Displays connected P25 repeaters parsed from journalctl.
 * Uses shared parser from parse_journal_peers.php.
 */

if (empty($PageOptions['P25PeerPage']['Show'])) {
    if (!empty($PageOptions['P25PeerPage']['NoLocalPeer'])) {
        echo '<p style="font-family:verdana;font-size:10pt;">No local P25 peer (127.0.0.1) in xlxd.interlink, this page only works with a P25 reflector on this host.</p>';
    } else {
        echo '<p style="font-family:verdana;font-size:10pt;">This page is not enabled.</p>';
    }
    return;
}

require_once('./pgs/parse_journal_peers.php');

if (!isset($P25Peers) || empty($P25Peers)) {
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
