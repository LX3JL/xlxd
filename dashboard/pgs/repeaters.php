<table class="listingtable">
 <tr>   
   <th width="25">#</th>
   <th width="54">Flag</th>
   <th width="100">DV Station</th>
   <th width="60">Band</th>
   <th width="154">Last Heard</th>
   <th width="156">Linked for</th>
   <th width="90">Protocol</th>
   <th width="66">Module</th>
   <th width="130">IP</th>
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
   if ($i == 99) { $i = $Reflector->NodeCount()+1; }
}

?> 
 
</table>
