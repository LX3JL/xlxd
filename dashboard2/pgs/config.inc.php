<?php
/*
Possible values for IPModus

HideIP
ShowFullIP
ShowLast1ByteOfIP
ShowLast2ByteOfIP
ShowLast3ByteOfIP

*/

$Service     = array();
$CallingHome = array();
$PageOptions = array();

$PageOptions['ContactEmail']                         = 'your_email';		    // Support E-Mail address

$PageOptions['DashboardVersion']                     = '2.3.10';       			// Dashboard Version

$PageOptions['PageRefreshActive']                    = true;          			// Activate automatic refresh
$PageOptions['PageRefreshDelay']                     = '10000';       			// Page refresh time in miliseconds


$PageOptions['RepeatersPage'] = array();
$PageOptions['RepeatersPage']['LimitTo']             = 99;            			// Number of Repeaters to show
$PageOptions['RepeatersPage']['IPModus']             = 'ShowFullIP'; 		 	// See possible options above
$PageOptions['RepeatersPage']['MasqueradeCharacter'] = '*';	        			// Character used for  masquerade


$PageOptions['PeerPage'] = array();
$PageOptions['PeerPage']['LimitTo']                  = 99;            			// Number of peers to show
$PageOptions['PeerPage']['IPModus']                  = 'ShowFullIP';  			// See possible options above
$PageOptions['PeerPage']['MasqueradeCharacter']      = '*';           			// Character used for  masquerade

$PageOptions['LastHeardPage']['LimitTo']             = 39;                      // Number of stations to show

$PageOptions['ModuleNames'] = array();                                			// Module nomination
$PageOptions['ModuleNames']['A']                     = 'Int.';
$PageOptions['ModuleNames']['B']                     = 'Regional';
$PageOptions['ModuleNames']['C']                     = 'National';
$PageOptions['ModuleNames']['D']                     = '';


$PageOptions['MetaDescription']                      = 'XLX is a D-Star Reflector System for Ham Radio Operators.';  // Meta Tag Values, usefull for Search Engine
$PageOptions['MetaKeywords']                         = 'Ham Radio, D-Star, XReflector, XLX, XRF, DCS, REF, ';        // Meta Tag Values, usefull forSearch Engine
$PageOptions['MetaAuthor']                           = 'LX1IQ';                                                      // Meta Tag Values, usefull for Search Engine
$PageOptions['MetaRevisit']                          = 'After 30 Days';                                              // Meta Tag Values, usefull for Search Engine
$PageOptions['MetaRobots']                           = 'index,follow';                                               // Meta Tag Values, usefull for Search Engine

$PageOptions['UserPage']['ShowFilter']               = true;                                                         // Show Filter on Users page

$PageOptions['IRCDDB']['Show']                       = true;        // Show liveircddb menu option
// $PageOptions['IRCDDB']['URL']                     = 'http://live.ircddb.net:8080';  // Optional: Override ircddb server URL
// $PageOptions['IRCDDB']['Page']                    = 'ircddblive5.html';             // Optional: Override ircddb page

$Service['PIDFile']                                  = '/var/log/xlxd.pid';
$Service['XMLFile']                                  = '/var/log/xlxd.xml';

$CallingHome['Active']                               = false;					               // xlx phone home, true or false
$CallingHome['MyDashBoardURL']                       = 'http://your_dashboard';			       // dashboard url
$CallingHome['ServerURL']                            = 'http://xlxapi.rlx.lu/api.php';         // database server, do not change !!!!
$CallingHome['PushDelay']                            = 600;  	                               // push delay in seconds
$CallingHome['Country']                              = "your_country";                         // Country
$CallingHome['Comment']                              = "your_comment"; 				           // Comment. Max 100 character
$CallingHome['HashFile']                             = "/tmp/callinghome.php";                 // Make sure the apache user has read and write permissions in this folder.
$CallingHome['OverrideIPAddress']                    = "";                                     // Insert your IP address here. Leave blank for autodetection. No need to enter a fake address.
$CallingHome['InterlinkFile']                        = "/xlxd/xlxd.interlink";                 // Path to interlink file


// -------------------------------------------------------------------------
// Co-located peer reflector pages (YSF, NXDN, P25)
//
// Each block controls one co-located reflector whose connected gateways are
// shown by reading that reflector's systemd journal via journalctl.
//
// Show         — set true to enable the nav item and page (default: false)
// ServiceUnit  — exact systemd unit name passed to journalctl -u
// PageTitle    — label shown in the sidebar nav and as the page heading
//
// IP display and masquerade character are inherited from RepeatersPage above;
// there are no separate per-page IP settings for these pages.
// -------------------------------------------------------------------------

$PageOptions['YSFPeerPage'] = array();
$PageOptions['YSFPeerPage']['Show']        = false;                    // Enable/disable the YSF Peers page
$PageOptions['YSFPeerPage']['ServiceUnit'] = 'YSFReflector.service';  // systemd unit name to query via journalctl
$PageOptions['YSFPeerPage']['PageTitle']   = 'YSF Peers';

$PageOptions['NXDNPeerPage'] = array();
$PageOptions['NXDNPeerPage']['Show']        = false;                    // Enable/disable the NXDN Peers page
$PageOptions['NXDNPeerPage']['ServiceUnit'] = 'NXDNReflector.service'; // systemd unit name to query via journalctl
$PageOptions['NXDNPeerPage']['PageTitle']   = 'NXDN Peers';

$PageOptions['P25PeerPage'] = array();
$PageOptions['P25PeerPage']['Show']        = false;                    // Enable/disable the P25 Peers page
$PageOptions['P25PeerPage']['ServiceUnit'] = 'P25Reflector.service';  // systemd unit name to query via journalctl
$PageOptions['P25PeerPage']['PageTitle']   = 'P25 Peers';


/*
  include an extra config file for people who dont like to mess with shipped config.ing.php
  this makes updating dashboard from git a little bit easier
*/

if (file_exists("../config.inc.php")) {
  include ("../config.inc.php");
}

?>
