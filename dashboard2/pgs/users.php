<?php

if (!isset($_SESSION['FilterCallSign'])) {
    $_SESSION['FilterCallSign'] = null;
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
        $_SESSION['FilterCallSign'] = null;
    }
}

?>

<div class="row">
   <div class="col-md-9">
      <table class="table table-striped table-hover">
<?php
if ($PageOptions['UserPage']['ShowFilter']) {
  echo '
 <tr>
   <th colspan="8">
      <table width="100%" border="0">
         <tr>
            <td align="left">
               <form name="frmFilterCallSign" method="post" action="./index.php">
                  <input type="hidden" name="do" value="SetFilter" />
                  <input type="text" class="FilterField" value="'.$_SESSION['FilterCallSign'].'" name="txtSetCallsignFilter" placeholder="Callsign" onfocus="SuspendPageRefresh();" onblur="setTimeout(ReloadPage, '.$PageOptions['PageRefreshDelay'].');" />
                  <input type="submit" value="Apply" class="FilterSubmit" />
               </form>
            </td>';
              if (($_SESSION['FilterModule'] != null) || ($_SESSION['FilterCallSign'] != null)) {
                  echo '
         <td><a href="./index.php?do=resetfilter" class="smalllink">Disable filters</a></td>';
              }
              echo '            
            <td align="right" style="padding-right:3px;">
               <form name="frmFilterModule" method="post" action="./index.php">
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
         <tr class="table-center">   
            <th class="col-md-1">#</th>
            <th class="col-md-1">Flag</th>
            <th class="col-md-2">Callsign</th>
            <th class="col-md-2">Suffix</th>
            <th class="col-md-1">DPRS</th>
            <th class="col-md-2">Via / Peer</th>
            <th class="col-md-2">Last heard</th>
            <th class="col-md-1"><img src="./img/ear.png" alt="Listening on" /></th>
         </tr>
<?php

$Reflector->LoadFlags();
for ($i=0;$i<$Reflector->StationCount();$i++) {
    $ShowThisStation = true;
    if ($PageOptions['UserPage']['ShowFilter']) {
        $CS = true;
        if ($_SESSION['FilterCallSign'] != null) {
            if (!fnmatch($_SESSION['FilterCallSign'], $Reflector->Stations[$i]->GetCallSign(), FNM_CASEFOLD)) {
                $CS = false;
            }
        }
        $MO = true;
        if ($_SESSION['FilterModule'] != null) {
            if (trim(strtolower($_SESSION['FilterModule'])) != strtolower($Reflector->Stations[$i]->GetModule())) {
                $MO = false;
            }
        }

        $ShowThisStation = ($CS && $MO);
    }

    if ($ShowThisStation) {

        echo '
      <tr class="table-center">
       <td>';
        if ($i == 0 && $Reflector->Stations[$i]->GetLastHeardTime() > (time() - 60)) {
            echo '<img src="./img/tx.gif" style="margin-top:3px;" height="20"/>';
        } else {
            echo($i + 1);
        }


        echo '</td>
        <td>';

        list ($Flag, $Name) = $Reflector->GetFlag($Reflector->Stations[$i]->GetCallSign());
        if (file_exists("./img/flags/" . $Flag . ".png")) {
            echo '<a href="#" class="tip"><img src="./img/flags/' . $Flag . '.png" class="table-flag" alt="' . $Name . '"><span>' . $Name . '</span></a>';
        }
        echo '</td>
   <td><a href="https://www.qrz.com/db/' . $Reflector->Stations[$i]->GetCallsignOnly() . '" class="pl" target="_blank">' . $Reflector->Stations[$i]->GetCallsignOnly() . '</a></td>
   <td>' . $Reflector->Stations[$i]->GetSuffix() . '</td>
   <td><a href="http://www.aprs.fi/' . $Reflector->Stations[$i]->GetCallsignOnly() . '" class="pl" target="_blank"><img src="./img/sat.png" alt=""></a></td>
   <td>' . $Reflector->Stations[$i]->GetVia();
        if ($Reflector->Stations[$i]->GetPeer() != $Reflector->GetReflectorName()) {
            echo ' / ' . $Reflector->Stations[$i]->GetPeer();
        }
        echo '</td>
   <td>' . @date("d.m.Y H:i", $Reflector->Stations[$i]->GetLastHeardTime()) . '</td>
   <td>' . $Reflector->Stations[$i]->GetModule() . '</td>
 </tr>';
    }
    if ($i == $PageOptions['LastHeardPage']['LimitTo']) {
        $i = $Reflector->StationCount() + 1;
    }
}

?> 
 
      </table>
   </div>
   <div class="col-md-3">
      <table class="table table-striped table-hover">
         <?php 

$Modules = $Reflector->GetModules();
sort($Modules, SORT_STRING);
echo '<tr>';
for ($i=0;$i<count($Modules);$i++) {
   
   if (isset($PageOptions['ModuleNames'][$Modules[$i]])) {
      echo '<th>'.$PageOptions['ModuleNames'][$Modules[$i]];
       if (trim($PageOptions['ModuleNames'][$Modules[$i]]) != "") {
           echo '<br />';
       }
      echo $Modules[$i].'</th>';
   }
   else {
   echo '
  
      <th>'.$Modules[$i].'</th>';
   }
}

echo '</tr><tr>';

$GlobalPositions = array();

for ($i=0;$i<count($Modules);$i++) {
    
   $Users = $Reflector->GetNodesInModulesByID($Modules[$i]);
   echo '<td><table class="table table-hover">';

   $UserCheckedArray = array();
   
   for ($j=0;$j<count($Users);$j++) {
       $Displayname = $Reflector->GetCallsignAndSuffixByID($Users[$j]);
      echo '
            <tr>
               <td><a href="http://www.aprs.fi/'.$Displayname.'" class="pl" target="_blank">'.$Displayname.'</a> </td>
            </tr>';
      $UserCheckedArray[] = $Users[$j];
   }
   echo '</table></td>';
}

echo '</tr>';

?>
      </table>
   </div>
</div>