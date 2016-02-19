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
 </tr>
<?php

$Reflector->LoadFlags();
$odd = "";
for ($i=0;$i<$Reflector->StationCount();$i++) {
   if ($odd == "#FFFFFF") { $odd = "#F1FAFA"; } else { $odd = "#FFFFFF"; }
   echo '
  <tr height="30" bgcolor="'.$odd.'" onMouseOver="this.bgColor=\'#FFFFCA\';" onMouseOut="this.bgColor=\''.$odd.'\';">
   <td align="center" width="35">';
   echo ($i==0 ? '<img src="./img/radio-waves-hi.png" />' : $i+1);
   
   
   echo '</td>
   <td align="center" width="60">';
   
   if (file_exists("./img/flags/".$Reflector->GetFlag($Reflector->Stations[$i]->GetCallSign()).".png")) {
      echo '<img src="./img/flags/'.$Reflector->GetFlag($Reflector->Stations[$i]->GetCallSign()).'.png" height="15" />';
   }
   echo '</td>
   <td width="75"><a href="https://www.qrz.com/db/'.$Reflector->Stations[$i]->GetCallsignOnly().'" class="pl" target="_blank">'.$Reflector->Stations[$i]->GetCallsignOnly().'</a></td>
   <td width="60">'.$Reflector->Stations[$i]->GetSuffix().'</td>
   <td width="50" align="center"><a href="http://www.aprs.fi/'.$Reflector->Stations[$i]->GetCallsignOnly().'" class="pl" target="_blank"><img src="./img/sat.png" /></a></td>
   <td width="127">'.$Reflector->Stations[$i]->GetVia();
   if ($Reflector->Stations[$i]->GetPeer() != 'XLX'.$ServiceName) {
      echo ' / '.$Reflector->Stations[$i]->GetPeer();
   }
   echo '</td>
   <td width="150">'.date("d.m.Y H:i", $Reflector->Stations[$i]->GetLastHeardTime()).'</td>
 </tr>';
   if ($i == 39) { $i = $Reflector->StationCount()+1; }
}

?> 
 
</table>


</td>
<td style="padding-left:50px;" align="center" valign="top">
   
   


<table class="listingtable">
<?php 
echo '
<tr>';

$Modules = $Reflector->GetModules();
for ($i=0;$i<count($Modules);$i++) {
   echo '
   <th>'.$Modules[$i].'</th>';
}

echo '
</tr>
<tr bgcolor="#FFFFFF" style="padding:0px;">';

for ($i=0;$i<count($Modules);$i++) {
    
    $Users = $Reflector->GetCallSignsInModules($Modules[$i]);
    echo '
   <td valign="top" style="border:0px;padding:0px;">
   
   <table width="100" border="0" style="padding:0px;margin:0px;">';
   $odd = "";
   
   $UserCheckedArray = array();
   
   for ($j=0;$j<count($Users);$j++) {
      if ($odd == "#FFFFFF") { $odd = "#F1FAFA"; } else { $odd = "#FFFFFF"; }
      if (in_array($Users[$j], $UserCheckedArray)) {
         $CurrentPositions = array_keys($UserCheckedArray,$Users[$j]);
         $Displayname = $Users[$j].'-'.$Reflector->GetSuffixOfRepeater($Users[$j], $Modules[$i], max($CurrentPositions)+1);
      }
      else {
         $Displayname = $Users[$j].'-'.$Reflector->GetSuffixOfRepeater($Users[$j], $Modules[$i]);
      }
      echo '<tr height="25" bgcolor="'.$odd.'" onMouseOver="this.bgColor=\'#FFFFCA\';" onMouseOut="this.bgColor=\''.$odd.'\';">
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
