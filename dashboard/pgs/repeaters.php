<?php
// ----------------------------------------------------------------------------
//  xlxd
//
//  Created by Luc Engelmann (LX1IQ) on 31/12.2015
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
<table class="listingtable">
 <tr>
   <th width="25">#</th>
   <th width="60">Flag</th>
   <th width="100">DV Station</th>
   <th width="75">Band</th>
   <th width="150">Last Heard</th>
   <th width="150">Linked for</th>
   <th width="90">Protocol</th>
   <th width="65">Module</th>
   <th width="125">IP</th>
 </tr>
<?php

$odd = "";
$Reflector->LoadFlags();

for ($i=0;$i<$Reflector->NodeCount();$i++) {

   if ($odd == "#FFFFFF") { $odd = "#F1FAFA"; } else { $odd = "#FFFFFF"; }

   echo '
  <tr height="30" bgcolor="'.$odd.'" onMouseOver="this.bgColor=\'#FFFFCA\';" onMouseOut="this.bgColor=\''.$odd.'\';">
   <td align="center">'.($i+1).'</td>
   <td align="center">';
   if (file_exists("./img/flags/".$Reflector->GetFlag($Reflector->Nodes[$i]->GetCallSign()).".png")) {
      echo '<img src="./img/flags/'.$Reflector->GetFlag($Reflector->Nodes[$i]->GetCallSign()).'.png" height="15" />';
   }
   echo '</td>
   <td><a href="http://www.aprs.fi/'.$Reflector->Nodes[$i]->GetCallSign();
   if ($Reflector->Nodes[$i]->GetSuffix() != "") echo '-'.$Reflector->Nodes[$i]->GetSuffix();
   echo '" class="pl" target="_blank">'.$Reflector->Nodes[$i]->GetCallSign();
   if ($Reflector->Nodes[$i]->GetSuffix() != "") { echo '-'.$Reflector->Nodes[$i]->GetSuffix(); }
   echo '</a></td>
   <td>';
   switch ($Reflector->Nodes[$i]->GetSuffix()) {
      case 'A' : echo '23cm'; break;
      case 'B' : echo '70cm'; break;
      case 'C' : echo '2m'; break;
      case 'G' : echo 'Internet-Gateway'; break;
      default  : echo '';
   }
   echo '</td>
   <td>'.date("d.m.Y H:i", $Reflector->Nodes[$i]->GetLastHeardTime()).'</td>
   <td>'.FormatSeconds(time()-$Reflector->Nodes[$i]->GetConnectTime()).' s</td>
   <td>'.$Reflector->Nodes[$i]->GetProtocol().'</td>
   <td align="center">'.$Reflector->Nodes[$i]->GetLinkedModule().'</td>
   <td>'.$Reflector->Nodes[$i]->GetIP().'</td>
 </tr>';
   if ($i == 41) { $i = $Reflector->NodeCount()+1; }
}

?>

</table>
