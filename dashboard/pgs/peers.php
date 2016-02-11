<table class="listingtable">
 <tr>   
   <th width="25">#</th>
   <th width="100">XLX Peer</th>
   <th width="154">Last Heard</th>
   <th width="156">Linked for</th>
   <th width="90">Protocol</th>
   <th width="67">Module</th>
   <th width="130">IP</th>
 </tr>
<?php

$odd = "";
$Reflector->LoadFlags();

for ($i=0;$i<$Reflector->PeerCount();$i++) {
         
   if ($odd == "#FFFFFF") { $odd = "#F1FAFA"; } else { $odd = "#FFFFFF"; }
 
   echo '
  <tr height="30" bgcolor="'.$odd.'" onMouseOver="this.bgColor=\'#FFFFCA\';" onMouseOut="this.bgColor=\''.$odd.'\';">
   <td align="center">'.($i+1).'</td>
   <td>'.$Reflector->Peers[$i]->GetCallSign().'</td>
   <td>'.date("d.m.Y H:i", $Reflector->Peers[$i]->GetLastHeardTime()).'</td>
   <td>'.FormatSeconds(time()-$Reflector->Peers[$i]->GetConnectTime()).' s</td>
   <td align="center">'.$Reflector->Peers[$i]->GetProtocol().'</td>
   <td align="center">'.$Reflector->Peers[$i]->GetLinkedModule().'</td>
   <td>'.$Reflector->Peers[$i]->GetIP().'</td>
 </tr>';
   if ($i == 99) { $i = $Reflector->PeerCount()+1; }
}

?> 
 
</table>
