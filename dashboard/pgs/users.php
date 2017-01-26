<table border="0">
   <tr>
      <td  valign="top">
         

<table class="listingtable">
 <tr>   
   <th>#</th>
   <th>Flag</th>
   <th>Callsign</th>
   <th>Suffix</th>
   <th>DPRS</th>
   <th>Via / Peer</th>
   <th>Last heard</th>
   <th align="center" valign="middle"><img src="./img/ear.png" alt="Listening on" /></th>
 </tr>
<?php

$Reflector->LoadFlags();
$odd = "";
for ($i=0;$i<$Reflector->StationCount();$i++) {
   if ($odd == "#FFFFFF") { $odd = "#F1FAFA"; } else { $odd = "#FFFFFF"; }
   echo '
  <tr height="30" bgcolor="'.$odd.'" onMouseOver="this.bgColor=\'#FFFFCA\';" onMouseOut="this.bgColor=\''.$odd.'\';">
   <td align="center" valign="middle" width="35">';
   if ($i==0 && $Reflector->Stations[$i]->GetLastHeardTime() > (time() - 60)) {
      echo '<img src="./img/tx.gif" style="margin-top:3px;" height="20"/>';
   }
   else {
      echo ($i+1);
   }
   
   
   echo '</td>
   <td align="center" width="60">';
   
   if (file_exists("./img/flags/".$Reflector->GetFlag($Reflector->Stations[$i]->GetCallSign()).".png")) {
      echo '<img src="./img/flags/'.$Reflector->GetFlag($Reflector->Stations[$i]->GetCallSign()).'.png" height="15" />';
   }
   echo '</td>
   <td width="75"><a href="https://www.qrz.com/db/'.$Reflector->Stations[$i]->GetCallsignOnly().'" class="pl" target="_blank">'.$Reflector->Stations[$i]->GetCallsignOnly().'</a></td>
   <td width="60">'.$Reflector->Stations[$i]->GetSuffix().'</td>
   <td width="50" align="center"><a href="http://www.aprs.fi/'.$Reflector->Stations[$i]->GetCallsignOnly().'" class="pl" target="_blank"><img src="./img/sat.png" /></a></td>
   <td width="150">'.$Reflector->Stations[$i]->GetVia();
   if ($Reflector->Stations[$i]->GetPeer() != $Reflector->GetReflectorName()) {
      echo ' / '.$Reflector->Stations[$i]->GetPeer();
   }
   echo '</td>
   <td width="150">'.@date("d.m.Y H:i", $Reflector->Stations[$i]->GetLastHeardTime()).'</td>
   <td align="center" width="30">'.$Reflector->Stations[$i]->GetModule().'</td>
 </tr>';
   if ($i == 39) { $i = $Reflector->StationCount()+1; }
}

?> 
 
</table>


</td>
<td style="padding-left:50px;" align="center" valign="top">
   
   


<table class="listingtable">
<?php 

$Modules = $Reflector->GetModules();
sort($Modules, SORT_STRING);
echo '
 <tr>';
for ($i=0;$i<count($Modules);$i++) {
   
   if (isset($PageOptions['ModuleNames'][$Modules[$i]])) {
      echo '
   
      <th>'.$PageOptions['ModuleNames'][$Modules[$i]];
      if (trim($PageOptions['ModuleNames'][$Modules[$i]]) != "") {
         echo '<br />';
      }
      echo $Modules[$i].'</th>
';
   }
   else {
   echo '
  
      <th>'.$Modules[$i].'</th>';
   }
}

echo '
</tr>
<tr bgcolor="#FFFFFF" style="padding:0px;">';

$GlobalPositions = array();

for ($i=0;$i<count($Modules);$i++) {

   $Users = $Reflector->GetNodesInModulesByID($Modules[$i]);
   echo '
   <td valign="top" style="border:0px;padding:0px;">
   
      <table width="100" border="0" style="padding:0px;margin:0px;">';
   $odd = "";
   
   $UserCheckedArray = array();
   
   for ($j=0;$j<count($Users);$j++) {
      
      if ($odd == "#FFFFFF") { $odd = "#F1FAFA"; } else { $odd = "#FFFFFF"; }
      $Displayname = $Reflector->GetCallsignAndSuffixByID($Users[$j]);
      echo '
            <tr height="25" bgcolor="'.$odd.'" onMouseOver="this.bgColor=\'#FFFFCA\';" onMouseOut="this.bgColor=\''.$odd.'\';">
               <td valign="top" style="border-bottom:1px #C1DAD7 solid;"><a href="http://www.aprs.fi/'.$Displayname.'" class="pl" target="_blank">'.$Displayname.'</a> </td>
            </tr>';
      $UserCheckedArray[] = $Users[$j];
   }
   echo '
      </table>
   
   </td>';
}

echo '
</tr>';

?>
</table>    


</td>
</tr>
</table>     
