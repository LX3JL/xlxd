<?php

if (!isset($_SESSION['FilterCallSign'])) {
   $_SESSION['FilterCallSign'] = null;
}

if (!isset($_SESSION['FilterProtocol'])) {
   $_SESSION['FilterProtocol'] = null;
}

if (!isset($_SESSION['FilterModule'])) {
   $_SESSION['FilterModule'] = null;
}

if (isset($_POST['do'])) {
   if ($_POST['do'] == 'SetFilter') {
    
      if (isset($_POST['txtSetCallsignFilter'])) {
         $_POST['txtSetCallsignFilter'] = trim($_POST['txtSetCallsignFilter']);
         if ($_POST['txtSetCallsignFilter'] == "") {
            $_SESSION['FilterCallSign'] = null;
         }
         else {
            $_SESSION['FilterCallSign'] = $_POST['txtSetCallsignFilter'];
            if (strpos($_SESSION['FilterCallSign'], "*") === false) {
               $_SESSION['FilterCallSign'] = "*".$_SESSION['FilterCallSign']."*";
            }
         }
    
      }
    
      if (isset($_POST['txtSetProtocolFilter'])) {
         $_POST['txtSetProtocolFilter'] = trim($_POST['txtSetProtocolFilter']);
         if ($_POST['txtSetProtocolFilter'] == "") {
            $_SESSION['FilterProtocol'] = null;
         }
         else {
            $_SESSION['FilterProtocol'] = $_POST['txtSetProtocolFilter'];
         }
    
      }

      if (isset($_POST['txtSetModuleFilter'])) {
         $_POST['txtSetModuleFilter'] = trim($_POST['txtSetModuleFilter']);
         if ($_POST['txtSetModuleFilter'] == "") {
            $_SESSION['FilterModule'] = null;
         }
         else {
            $_SESSION['FilterModule'] = $_POST['txtSetModuleFilter'];
         }
    
      }
   }
}

if (isset($_GET['do'])) {
   if ($_GET['do'] == "resetfilter") {
      $_SESSION['FilterModule'] = null;
      $_SESSION['FilterProtocol'] = null;
      $_SESSION['FilterCallSign'] = null;
   }
}
   

?>

<table class="listingtable"><?php

if ($PageOptions['UserPage']['ShowFilter']) {
   echo '
 <tr>
   <th colspan="9">
      <table width="100%" border="0">
         <tr>
            <td align="left">
               <form name="frmFilterCallSign" method="post" action="./index.php?show=repeaters">
                  <input type="hidden" name="do" value="SetFilter" />
                  <input type="text" class="FilterField" value="'.$_SESSION['FilterCallSign'].'" name="txtSetCallsignFilter" placeholder="Callsign" onfocus="SuspendPageRefresh();" onblur="setTimeout(ReloadPage, '.$PageOptions['PageRefreshDelay'].');" />
                  <input type="submit" value="Apply" class="FilterSubmit" />
               </form>
            </td>';
   if (($_SESSION['FilterModule'] != null) || ($_SESSION['FilterCallSign'] != null) || ($_SESSION['FilterProtocol'] != null)) {
      echo '
         <td><a href="./index.php?show=repeaters&do=resetfilter" class="smalllink">Disable filters</a></td>';
   }
   echo '            
            <td align="right" style="padding-right:3px;">
               <form name="frmFilterProtocol" method="post" action="./index.php?show=repeaters">
                  <input type="hidden" name="do" value="SetFilter" />
                  <input type="text" class="FilterField" value="'.$_SESSION['FilterProtocol'].'" name="txtSetProtocolFilter" placeholder="Protocol" onfocus="SuspendPageRefresh();" onblur="setTimeout(ReloadPage, '.$PageOptions['PageRefreshDelay'].');" />
                  <input type="submit" value="Apply" class="FilterSubmit" />
               </form>
            </td>
            <td align="right" style="padding-right:3px;">
               <form name="frmFilterModule" method="post" action="./index.php?show=repeaters">
                  <input type="hidden" name="do" value="SetFilter" />
                  <input type="text" class="FilterField" value="'.$_SESSION['FilterModule'].'" name="txtSetModuleFilter" placeholder="Module" onfocus="SuspendPageRefresh();" onblur="setTimeout(ReloadPage, '.$PageOptions['PageRefreshDelay'].');" />
                  <input type="submit" value="Apply" class="FilterSubmit" />
               </form>
            </td>
      </table>
   </th>
</tr>';
}


?>
 <tr>
   <th width="25">#</th>
   <th width="60">Flag</th>
   <th width="100">DV Station</th>
   <th width="75">Band</th>
   <th width="150">Last Heard</th>
   <th width="150">Linked for</th>
   <th width="90">Protocol</th>
   <th width="65">Module</th><?php

if ($PageOptions['RepeatersPage']['IPModus'] != 'HideIP') {
   echo '
   <th width="125">IP</th>';
}

?>
 </tr>
<?php

$odd = "";
$Reflector->LoadFlags();

for ($i=0;$i<$Reflector->NodeCount();$i++) {
   $ShowThisStation = true;
   if ($PageOptions['UserPage']['ShowFilter']) {
      $CS = true;
      if ($_SESSION['FilterCallSign'] != null) {
         if (!fnmatch($_SESSION['FilterCallSign'], $Reflector->Nodes[$i]->GetCallSign(), FNM_CASEFOLD)) {
            $CS = false;
         }
      }
      $MO = true;
      if ($_SESSION['FilterModule'] != null) {
         if (trim(strtolower($_SESSION['FilterModule'])) != strtolower($Reflector->Nodes[$i]->GetLinkedModule())) {
            $MO = false;
         }
      }
      $PR = true;
      if ($_SESSION['FilterProtocol'] != null) {
         if (trim(strtolower($_SESSION['FilterProtocol'])) != strtolower($Reflector->Nodes[$i]->GetProtocol())) {
            $PR = false;
         }
      }

      $ShowThisStation = ($CS && $MO && $PR);
   }

   if ($ShowThisStation) {
      if ($odd == "#FFFFFF") { $odd = "#F1FAFA"; } else { $odd = "#FFFFFF"; }
   
      echo '
     <tr height="30" bgcolor="'.$odd.'" onMouseOver="this.bgColor=\'#FFFFCA\';" onMouseOut="this.bgColor=\''.$odd.'\';">
      <td align="center">'.($i+1).'</td>
      <td align="center">';
      list ($Flag, $Name) = $Reflector->GetFlag($Reflector->Nodes[$i]->GetCallSign());
      if (file_exists("./img/flags/".$Flag.".png")) {
         echo '<a href="#" class="tip"><img src="./img/flags/'.$Flag.'.png" height="15" alt="'.$Name.'" /><span>'.$Name.'</span></a>';
      }
      echo '</td>
      <td><a href="http://www.aprs.fi/'.$Reflector->Nodes[$i]->GetCallSign();
      if ($Reflector->Nodes[$i]->GetSuffix() != "") echo '-'.$Reflector->Nodes[$i]->GetSuffix();
      echo '" class="pl" target="_blank">'.$Reflector->Nodes[$i]->GetCallSign();
      if ($Reflector->Nodes[$i]->GetSuffix() != "") { echo '-'.$Reflector->Nodes[$i]->GetSuffix(); }
      echo '</a></td>
      <td>';
      if (($Reflector->Nodes[$i]->GetPrefix() == 'REF') || ($Reflector->Nodes[$i]->GetPrefix() == 'XRF')) {
         switch ($Reflector->Nodes[$i]->GetPrefix()) {
           case 'REF'  : echo 'REF-Link'; break;
           case 'XRF'  : echo 'XRF-Link'; break;
         }
      }
      else {
         switch ($Reflector->Nodes[$i]->GetSuffix()) {
            case 'A' : echo '23cm'; break;
            case 'B' : echo '70cm'; break;
            case 'C' : echo '2m'; break;
            case 'D' : echo 'Dongle'; break;
            case 'G' : echo 'Internet-Gateway'; break;
            default  : echo '';
         }
      }
      echo '</td>
      <td>'.date("d.m.Y H:i", $Reflector->Nodes[$i]->GetLastHeardTime()).'</td>
      <td>'.FormatSeconds(time()-$Reflector->Nodes[$i]->GetConnectTime()).' s</td>
      <td>'.$Reflector->Nodes[$i]->GetProtocol().'</td>
      <td align="center">'.$Reflector->Nodes[$i]->GetLinkedModule().'</td>';
      if ($PageOptions['RepeatersPage']['IPModus'] != 'HideIP') {
         echo '
      <td>';
         $Bytes = explode(".", $Reflector->Nodes[$i]->GetIP());
         if ($Bytes !== false && count($Bytes) == 4) {
            switch ($PageOptions['RepeatersPage']['IPModus']) {
               case 'ShowLast1ByteOfIP'      : echo $PageOptions['RepeatersPage']['MasqueradeCharacter'].'.'.$PageOptions['RepeatersPage']['MasqueradeCharacter'].'.'.$PageOptions['RepeatersPage']['MasqueradeCharacter'].'.'.$Bytes[3]; break;
               case 'ShowLast2ByteOfIP'      : echo $PageOptions['RepeatersPage']['MasqueradeCharacter'].'.'.$PageOptions['RepeatersPage']['MasqueradeCharacter'].'.'.$Bytes[2].'.'.$Bytes[3]; break;
               case 'ShowLast3ByteOfIP'      : echo $PageOptions['RepeatersPage']['MasqueradeCharacter'].'.'.$Bytes[1].'.'.$Bytes[2].'.'.$Bytes[3]; break;
               default                       : echo $Reflector->Nodes[$i]->GetIP();
            }
         }
         echo '</td>';
      }
      echo '
      </tr>';
   }
   if ($i == $PageOptions['RepeatersPage']['LimitTo']) { $i = $Reflector->NodeCount()+1; }
}

?>

</table>
