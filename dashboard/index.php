<?php

// Secure session configuration
ini_set('session.cookie_httponly', 1);
ini_set('session.cookie_samesite', 'Strict');
if (isset($_SERVER['HTTPS']) && $_SERVER['HTTPS'] === 'on') {
    ini_set('session.cookie_secure', 1);
}

session_start();

// Security headers
header("X-Content-Type-Options: nosniff");
header("X-Frame-Options: SAMEORIGIN");
header("X-XSS-Protection: 1; mode=block");
header("Referrer-Policy: strict-origin-when-cross-origin");

if (file_exists("./pgs/functions.php"))   { require_once("./pgs/functions.php"); } else { die("functions.php does not exist."); }
if (file_exists("./pgs/config.inc.php"))  { require_once("./pgs/config.inc.php"); } else { die("config.inc.php does not exist."); }

if (!class_exists('ParseXML'))   require_once("./pgs/class.parsexml.php");
if (!class_exists('Node'))       require_once("./pgs/class.node.php");
if (!class_exists('xReflector')) require_once("./pgs/class.reflector.php");
if (!class_exists('Station'))    require_once("./pgs/class.station.php");
if (!class_exists('Peer'))       require_once("./pgs/class.peer.php");
if (!class_exists('Interlink'))  require_once("./pgs/class.interlink.php");

// Validate 'show' parameter
$allowed_pages = ['users', 'repeaters', 'liveircddb', 'peers', 'modules', 'reflectors', 'traffic'];
if (!isset($_GET['show'])) {
    $_GET['show'] = '';
} elseif (!in_array($_GET['show'], $allowed_pages, true)) {
    $_GET['show'] = '';
}

// Validate 'do' parameter for filter resets
if (isset($_GET['do']) && $_GET['do'] !== 'resetfilter') {
    unset($_GET['do']);
}

// Validate 'callhome' parameter
if (isset($_GET['callhome'])) {
    $_GET['callhome'] = filter_var($_GET['callhome'], FILTER_VALIDATE_BOOLEAN, FILTER_NULL_ON_FAILURE);
    if ($_GET['callhome'] === null) {
        unset($_GET['callhome']);
    }
}

$Reflector = new xReflector();
$Reflector->SetFlagFile("./pgs/country.csv");
$Reflector->SetPIDFile($Service['PIDFile']);
$Reflector->SetXMLFile($Service['XMLFile']);

$Reflector->LoadXML();

if ($CallingHome['Active']) { 
   
   $CallHomeNow = false;
   $LastSync = 0;
   $Hash = "";
   
   if (!file_exists($CallingHome['HashFile'])) {
      $Ressource = fopen($CallingHome['HashFile'], "w+"); 
      if ($Ressource) { 
         $Hash = CreateCode(16);
		   @fwrite($Ressource, "<?php\n"); 
		   @fwrite($Ressource, "\n".'$Hash = "'.$Hash.'";'); 
		   @fwrite($Ressource, "\n\n".'?>'); 
		   @fflush($Ressource); 
		   @fclose($Ressource); 
		   @chmod($HashFile, 0777); 
		}
   }
   else {
      require_once($CallingHome['HashFile']);
   }
   
   if (@file_exists($CallingHome['LastCallHomefile'])) {
      if (@is_readable($CallingHome['LastCallHomefile'])) {
         $tmp      = @file($CallingHome['LastCallHomefile']);
         if (isset($tmp[0])) {
            $LastSync = $tmp[0];
         }
         unset($tmp);
      }
   }
         
   if ($LastSync < (time() - $CallingHome['PushDelay'])) { 
      $CallHomeNow = true;
      $Ressource = @fopen($CallingHome['LastCallHomefile'], "w+"); 
	   if ($Ressource) { 
	      @fwrite($Ressource, time()); 
		   @fflush($Ressource); 
		   @fclose($Ressource); 
		   @chmod($HashFile, 0777); 
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


?><!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Strict//EN"
"http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
<head>
   <meta http-equiv="content-type" content="text/html; charset=utf-8" />
   <meta name="description" content="<?php echo sanitize_attribute($PageOptions['MetaDescription']); ?>" />
   <meta name="keywords"    content="<?php echo sanitize_attribute($PageOptions['MetaKeywords']); ?>" />
   <meta name="author"      content="<?php echo sanitize_attribute($PageOptions['MetaAuthor']); ?>" />
   <meta name="revisit"     content="<?php echo sanitize_attribute($PageOptions['MetaRevisit']); ?>" />
   <meta name="robots"      content="<?php echo sanitize_attribute($PageOptions['MetaRobots']); ?>" />
   
   <meta http-equiv="content-type" content="text/html; charset=utf-8" />
   <title><?php echo sanitize_output($Reflector->GetReflectorName()); ?> Reflector Dashboard</title>
   <link rel="stylesheet" type="text/css" href="./css/layout.css">
   <link rel="icon" href="./favicon.ico" type="image/vnd.microsoft.icon"><?php

   if ($PageOptions['PageRefreshActive']) {
      echo '
   <script src="./js/jquery-1.12.4.min.js"></script>
   <script>
      var PageRefresh;
      
      function ReloadPage() {
         $.get("./index.php'.((!empty($_GET['show'])) ? '?show='.urlencode($_GET['show']) : '').'", function(data) {
            var BodyStart = data.indexOf("<bo"+"dy");
            var BodyEnd = data.indexOf("</bo"+"dy>");
            if ((BodyStart >= 0) && (BodyEnd > BodyStart)) {
               BodyStart = data.indexOf(">", BodyStart)+1;
               $("body").html(data.substring(BodyStart, BodyEnd));
            }
         })
            .always(function() {
               PageRefresh = setTimeout(ReloadPage, '.$PageOptions['PageRefreshDelay'].');
            });
      }';

     if (!isset($_GET['show']) || (($_GET['show'] != 'liveircddb') && ($_GET['show'] != 'reflectors') && ($_GET['show'] != 'interlinks'))) {
         echo '
      PageRefresh = setTimeout(ReloadPage, '.$PageOptions['PageRefreshDelay'].');';
     }
     echo '

      function SuspendPageRefresh() {
         clearTimeout(PageRefresh);
      }
   </script>';
   }
   
   if (!isset($_GET['show'])) $_GET['show'] = "";
?>
</head>
<body>
   <?php if (file_exists("./tracking.php")) { include_once("tracking.php"); }?>
   <div id="top"><img src="./img/header.jpg" alt="XLX Multiprotocol Gateway Reflector" style="margin-top:15px;" />
      <br />&nbsp;&nbsp;&nbsp;<?php echo sanitize_output($Reflector->GetReflectorName()); ?>&nbsp;v<?php echo sanitize_output($Reflector->GetVersion()); ?>&nbsp;-&nbsp;Dashboard v<?php echo sanitize_output($PageOptions['DashboardVersion']); ?>&nbsp;<?php echo sanitize_output($PageOptions['CustomTXT']); ?>&nbsp;&nbsp;/&nbsp;&nbsp;Service uptime: <span id="suptime"><?php echo FormatSeconds($Reflector->GetServiceUptime()); ?></span></div>
   <div id="menubar">
      <div id="menu">
         <table border="0">
            <tr>
               <td><a href="./index.php" class="menulink<?php if ($_GET['show'] == '') { echo 'active'; } ?>">Users / Modules</a></td>
<td><a href="./index.php?show=repeaters" class="menulink<?php if ($_GET['show'] == 'repeaters') { echo 'active'; } ?>">Repeaters / Nodes (<?php echo intval($Reflector->NodeCount()); ?>)</a></td>
<td><a href="./index.php?show=peers" class="menulink<?php if ($_GET['show'] == 'peers') { echo 'active'; } ?>">Peers (<?php echo intval($Reflector->PeerCount()); ?>)</a></td>
               <td><a href="./index.php?show=modules" class="menulink<?php if ($_GET['show'] == 'modules') { echo 'active'; } ?>">Modules list</a></td>
               <td><a href="./index.php?show=reflectors" class="menulink<?php if ($_GET['show'] == 'reflectors') { echo 'active'; } ?>">Reflectors list</a></td>
               <?php
               
               if ($PageOptions['Traffic']['Show']) {
                   echo '
               <td><a href="./index.php?show=traffic" class="menulink';
                   if ($_GET['show'] == 'traffic') { echo 'active'; }
                   echo '">Traffic statistics</a></td>';
               }
		     if ($PageOptions['IRCDDB']['Show']) {
                   echo '
               <td><a href="./index.php?show=liveircddb" class="menulink';
                   if ($_GET['show'] == 'liveircddb') { echo 'active'; }
                   echo '">D-Star live</a></td>';
              }
               
               ?>
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
            your private hash in '.sanitize_output($CallingHome['HashFile']).' could not be created, please check your config file and the permissions for the defined folder.
         </div>';
      }
   }

   switch ($_GET['show']) {
      case 'users'      : require_once("./pgs/users.php"); break;
      case 'repeaters'  : require_once("./pgs/repeaters.php"); break;
      case 'liveircddb' : require_once("./pgs/liveircddb.php"); break;
      case 'peers'      : require_once("./pgs/peers.php"); break;
      case 'modules'    : require_once("./pgs/modules.php"); break;
      case 'reflectors' : require_once("./pgs/reflectors.php"); break;
      case 'traffic'		: require_once("./pgs/traffic.php"); break;
      default           : require_once("./pgs/users.php");
   }

?>

   <div style="width:100%;text-align:center;margin-top:50px;"><a href="mailto:<?php echo sanitize_attribute($PageOptions['ContactEmail']); ?>" style="font-family:verdana;color:#000000;font-size:12pt;text-decoration:none;"><?php echo sanitize_output($PageOptions['ContactEmail']); ?></a></div>

   </div>

</body>
</html>
