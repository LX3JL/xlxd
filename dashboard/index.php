<?php
// ----------------------------------------------------------------------------
//  xlxd
//
//  Created by Luc Engelmann (LX1IQ) on 31/12/2015
//  Copyright © 2015 Luc Engelmann (LX1IQ). All rights reserved.
//
// ----------------------------------------------------------------------------
//    This file is part of xlxd.
//
//    xlxd is free software: you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation, either version 3 of the License, or
//    (at your option) any later version.
//
//    xlxd is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with Foobar.  If not, see <http://www.gnu.org/licenses/>.
// ----------------------------------------------------------------------------

$FILE = "/var/log/xlxd.xml";

require_once("./pgs/functions.php");
if (!class_exists('ParseXML'))   require_once("./pgs/class.parsexml.php");
if (!class_exists('Node'))       require_once("./pgs/class.node.php");
if (!class_exists('xReflector')) require_once("./pgs/class.reflector.php");
if (!class_exists('Station'))    require_once("./pgs/class.station.php");



//if (!isset($_GET['show'])) { $_GET['show'] == ''; }

if (!@file_exists($FILE) && (!@is_readable($FILE))) die("xlxd.xml does not exist or is not readable");

$handle = fopen($FILE, 'r');
$FILECONTENT = fread($handle, filesize($FILE));
fclose($handle);



$XML       = new ParseXML();
$Reflector = new xReflector();
$Reflector->SetFlagFile("./pgs/country.csv");

$ServiceName = substr($FILECONTENT, strpos($FILECONTENT, "<XLX")+4, 3);
if (intval($ServiceName) !== 0) {
   $LinkedNodesName = "XLX".$ServiceName."  linked nodes";
   $LinkedUsersName = "XLX".$ServiceName."  heard users";
}

$AllNodesString    = $XML->GetElement($FILECONTENT, $LinkedNodesName);
$tmpNodes          = $XML->GetAllElements($AllNodesString, "NODE");

for ($i=0;$i<count($tmpNodes);$i++) {
    $Node = new Node($XML->GetElement($tmpNodes[$i], 'Callsign'), $XML->GetElement($tmpNodes[$i], 'IP'), $XML->GetElement($tmpNodes[$i], 'LinkedModule'), $XML->GetElement($tmpNodes[$i], 'Protocol'), $XML->GetElement($tmpNodes[$i], 'ConnectTime'), $XML->GetElement($tmpNodes[$i], 'LastHeardTime'));
    $Reflector->AddNode($Node);
}

$AllStationsString = $XML->GetElement($FILECONTENT, $LinkedUsersName);
$tmpStations       = $XML->GetAllElements($AllStationsString, "STATION");

for ($i=0;$i<count($tmpStations);$i++) {
    $Station = new Station($XML->GetElement($tmpStations[$i], 'Callsign'), $XML->GetElement($tmpStations[$i], 'Via'), $XML->GetElement($tmpStations[$i], 'LastHeardTime'));
    $Reflector->AddStation($Station, false);
}


?><!DOCTYPE html PUBLIC"-//W3C//DTD XHTML 1.0 Strict//EN"
"http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
<head>
   <meta http-equiv="content-type" content="text/html; charset=utf-8" />
   <title>XLX Reflector Dashboard</title>
   <link rel="stylesheet" type="text/css" href="./css/layout.css">
   <link rel="icon" href="./favicon.ico" type="image/vnd.microsoft.icon">
   <script>

      function ReloadPage() {
         document.location.href = "./index.php<?php if (isset($_GET['show'])) { echo '?show='.$_GET['show']; } ?>";
      }
      <?php

         if (!isset($_GET['show']) || (($_GET['show'] != 'liveircddb') && ($_GET['show'] != 'liveccs'))) {
         echo '
      setTimeout(ReloadPage, 10000);';
         }
      ?>

   </script>
</head>
<body>
   <div id="top"><img src="./img/header.jpg" alt="XLX Multiprotocol Gateway Reflector" style="margin-top:15px;margin-left:3px;" /></div>
   <div id="menubar">
      <div id="menu">
         <table border="0">
            <tr>
               <td><a href="./index.php" class="menulink">Users / Modules</a></td><td><a href="./index.php?show=repeaters" class="menulink">Repeaters / Nodes</a></td><td><a href="./index.php?show=liveircddb" class="menulink">D-Star live</a></td><td><a href="./index.php?show=liveccs" class="menulink">CCS live</a></td>
            </tr>

          </table>
      </div>
      <div id="info"><?php echo str_replace ("up" , "&nbsp;&nbsp;Server uptime" , GetSystemUptime()); ?>&nbsp;<br />XLX<?php echo $ServiceName; ?>&nbsp;v<?php echo $XML->GetElement($FILECONTENT, "Version"); ?>&nbsp;-&nbsp;Dashboard v2.0.4&nbsp;&nbsp;</div>
   </div>
   <div id="content" align="center">

<?php
   if (!isset($_GET['show'])) $_GET['show'] = "";
   switch ($_GET['show']) {
      case 'users'      : require_once("./pgs/users.php"); break;
      case 'repeaters'  : require_once("./pgs/repeaters.php"); break;
      case 'liveircddb' : require_once("./pgs/liveircddb.php"); break;
      case 'liveccs'    : require_once("./pgs/liveccs.php"); break;
      default           : require_once("./pgs/users.php");
   }

?>

   <div style="width:100%;text-align:center;margin-top:50px;"><a href="mailto:dvc@rlx.lu" style="font-family:verdana;color:#000000;font-size:12pt;text-decoration:none;">dvc@rlx.lu</a></div>

   </div>


</body>
</html>
