<?php
/*
Set IPModus in Repeaters or Peer variable to show,
modify or hide the IP address.
Possible values for IPModus

HideIP
ShowFullIP
ShowLast1ByteOfIP
ShowLast2ByteOfIP
ShowLast3ByteOfIP

*/


$PageOptions = array();

$PageOptions['ContactEmail']                         = 'dvc@rlx.lu';  // Support E-Mail address

$PageOptions['DashboardVersion']                     = '2.1.6';       // Dashboard Version

$PageOptions['PageRefreshActive']                    = true;          // Activate automatic refresh
$PageOptions['PageRefreshDelay']                     = '10000';       // Wait time in miliseconds


$PageOptions['RepeatersPage'] = array();
$PageOptions['RepeatersPage']['LimitTo']             = 99;            // Number of Repeaters to show
$PageOptions['RepeatersPage']['IPModus']             = 'ShowFullIP';  // See possible options above
$PageOptions['RepeatersPage']['MasqueradeCharacter'] = '*';           // Character used for  masquerade


$PageOptions['PeerPage'] = array();
$PageOptions['PeerPage']['LimitTo']                  = 99;            // Number of peers to show
$PageOptions['PeerPage']['IPModus']                  = 'ShowFullIP';  // See possible options above
$PageOptions['PeerPage']['MasqueradeCharacter']      = '*';           // Character used for  masquerade


$PageOptions['ModuleNames'] = array();                                // Module nomination
$PageOptions['ModuleNames']['A']                     = 'International';
$PageOptions['ModuleNames']['B']                     = 'Regional';
$PageOptions['ModuleNames']['C']                     = 'National';
$PageOptions['ModuleNames']['D']                     = '';

?>
