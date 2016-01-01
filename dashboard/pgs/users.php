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
?>
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
   <th>Via</th>
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
   <td width="90">'.$Reflector->Stations[$i]->GetVia().'</td>
   <td width="150">'.date("d.m.Y H:i", $Reflector->Stations[$i]->GetLastHeardTime()).'</td>
 </tr>';
   if ($i == 41) { $i = $Reflector->StationCount()+1; }
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
   for ($j=0;$j<count($Users);$j++) {
      if ($odd == "#FFFFFF") { $odd = "#F1FAFA"; } else { $odd = "#FFFFFF"; }
      echo '<tr height="25" bgcolor="'.$odd.'" onMouseOver="this.bgColor=\'#FFFFCA\';" onMouseOut="this.bgColor=\''.$odd.'\';">
               <td valign="top" style="border-bottom:1px #C1DAD7 solid;"><a href="http://www.aprs.fi/'.$Users[$j].'-'.$Reflector->GetSuffixOfRepeater($Users[$j]).'" class="pl" target="_blank">'.$Users[$j].'-'.$Reflector->GetSuffixOfRepeater($Users[$j]).'</a> </td>
            </tr>';
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
