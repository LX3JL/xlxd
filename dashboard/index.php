<?php

if (file_exists("./pgs/functions.php"))  { require_once("./pgs/functions.php");  } else { die("functions.php does not exist.");  }
if (file_exists("./pgs/config.inc.php")) { require_once("./pgs/config.inc.php"); } else { die("config.inc.php does not exist."); }

if (!class_exists('ParseXML'))   require_once("./pgs/class.parsexml.php");
if (!class_exists('Node'))       require_once("./pgs/class.node.php");
if (!class_exists('xReflector')) require_once("./pgs/class.reflector.php");
if (!class_exists('Station'))    require_once("./pgs/class.station.php");
if (!class_exists('Peer'))       require_once("./pgs/class.peer.php");
if (!class_exists('Interlink'))  require_once("./pgs/class.interlink.php");

$Reflector = new xReflector();
$Reflector->SetFlagFile("./pgs/country.csv");
$Reflector->SetPIDFile($Service['PIDFile']);
$Reflector->SetXMLFile($Service['XMLFile']);

$Reflector->LoadXML();

if ($CallingHome['Active']) { 
   
   $CallHomeNow = false;
   if (!file_exists($CallingHome['HashFile'])) {
      $Hash = CreateCode(16);
      $LastSync = 0;
      $Ressource = @fopen($CallingHome['HashFile'],"w");
      if ($Ressource) {
         @fwrite($Ressource, "<?php\n");
         @fwrite($Ressource, "\n".'$LastSync = 0;');
         @fwrite($Ressource, "\n".'$Hash     = "'.$Hash.'";');
         @fwrite($Ressource, "\n\n".'?>');
         @fclose($Ressource);
         @exec("chmod 777 ".$CallingHome['HashFile']);
         $CallHomeNow = true;
      }
   }
   else {
      if (is_readable($CallingHome['HashFile'])) {
         include($CallingHome['HashFile']);
         if (isset($Hash)) {
            if (strlen($Hash) > 4) {
               if ($LastSync < (time() - $CallingHome['PushDelay'])) { 
                  $Ressource = @fopen($CallingHome['HashFile'],"w");
                  if ($Ressource) {
                     @fwrite($Ressource, "<?php\n");
                     @fwrite($Ressource, "\n".'$LastSync = '.time().';');
                     @fwrite($Ressource, "\n".'$Hash     = "'.$Hash.'";');
                     @fwrite($Ressource, "\n\n".'?>');
                     @fclose($Ressource);
                  }
                  $CallHomeNow = true;
               }
            }
         }
      }
   }
   
   if ($CallHomeNow || isset($_GET['callhome'])) {
      $Reflector->SetCallingHome($CallingHome, $Hash);
      $Reflector->ReadInterlinkFile();
      $Reflector->PrepareInterlinkXML();
      $Reflector->PrepareReflectorXML();
      $Reflector->CallHome();
   }
}
else {
   $Hash = "";
}




?><!DOCTYPE html PUBLIC"-//W3C//DTD XHTML 1.0 Strict//EN"
"http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
<head>
   <meta http-equiv="content-type" content="text/html; charset=utf-8" />
   <meta name="description" content="<?php echo $PageOptions['MetaDescription']; ?>" />
   <meta name="keywords"    content="<?php echo $PageOptions['MetaKeywords']; ?>" />
   <meta name="author"      content="<?php echo $PageOptions['MetaAuthor']; ?>" />
   <meta name="revisit"     content="<?php echo $PageOptions['MetaRevisit']; ?>" />
   <meta name="robots"      content="<?php echo $PageOptions['MetaAuthor']; ?>" />
   
   <meta http-equiv="content-type" content="text/html; charset=utf-8" />
   <title><?php echo $Reflector->GetReflectorName(); ?> Reflector Dashboard</title>
   <link rel="stylesheet" type="text/css" href="./css/layout.css">
   <link rel="icon" href="./favicon.ico" type="image/vnd.microsoft.icon"><?php

   if ($PageOptions['PageRefreshActive']) {
      echo '
   <script>

      function ReloadPage() {
         document.location.href = "./index.php';
     if (isset($_GET['show'])) {
        echo '?show='.$_GET['show'];
     }
     echo '";
      }';

     if (!isset($_GET['show']) || (($_GET['show'] != 'liveircddb') && ($_GET['show'] != 'reflectors') && ($_GET['show'] != 'interlinks'))) {
         echo '
      setTimeout(ReloadPage, '.$PageOptions['PageRefreshDelay'].');';
     }
     echo '

   </script>';
   }
?>
</head>
<body>
   <?php if (file_exists("./tracking.php")) { include_once("tracking.php"); }?>
   <div id="top"><img src="./img/header.jpg" alt="XLX Multiprotocol Gateway Reflector" style="margin-top:15px;" />
      <br />&nbsp;&nbsp;&nbsp;<?php echo $Reflector->GetReflectorName(); ?>&nbsp;v<?php echo $Reflector->GetVersion(); ?>&nbsp;-&nbsp;Dashboard v<?php echo $PageOptions['DashboardVersion']; ?>&nbsp;&nbsp;/&nbsp;&nbsp;Service uptime: <span id="suptime"><?php echo FormatSeconds($Reflector->GetServiceUptime()); ?></span></div>
   <div id="menubar">
      <div id="menu">
         <table border="0">
            <tr>
               <td><a href="./index.php" class="menulink">Users / Modules</a></td><td><a href="./index.php?show=repeaters" class="menulink">Repeaters / Nodes (<?php echo $Reflector->NodeCount(); ?>)</a></td><td><a href="./index.php?show=peers" class="menulink">Peers (<?php echo $Reflector->PeerCount(); ?>)</a></td><td><a href="./index.php?show=reflectors" class="menulink">Reflectorlist</a></td><td><a href="./index.php?show=liveircddb" class="menulink">D-Star live</a></td>
            </tr>
          </table>
      </div>
   </div>
   <div id="content" align="center">

<?php
   if ($CallingHome['Active']) {
      if (!is_readable($CallingHome['HashFile']) && (!is_writeable($CallingHome['HashFile']))) {
         echo '
         <div class="error">
            your private hash in '.$CallingHome['HashFile'].' could not be created, please check your config file and the permissions for the defined folder.
         </div>';
      }
   }

   if (!isset($_GET['show'])) $_GET['show'] = "";
   switch ($_GET['show']) {
      case 'users'      : require_once("./pgs/users.php"); break;
      case 'repeaters'  : require_once("./pgs/repeaters.php"); break;
      case 'liveircddb' : require_once("./pgs/liveircddb.php"); break;
      case 'peers'      : require_once("./pgs/peers.php"); break;
      case 'reflectors' : require_once("./pgs/reflectors.php"); break;
      default           : require_once("./pgs/users.php");
   }

?>

   <div style="width:100%;text-align:center;margin-top:50px;"><a href="mailto:<?php echo $PageOptions['ContactEmail']; ?>" style="font-family:verdana;color:#000000;font-size:12pt;text-decoration:none;"><?php echo $PageOptions['ContactEmail']; ?></a></div>

   </div>

</body>
</html>
