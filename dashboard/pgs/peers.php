<table class="listingtable">
 <tr>   
   <th width="25">#</th>
   <th width="100">XLX Peer</th>
   <th width="154">Last Heard</th>
   <th width="156">Linked for</th>
   <th width="90">Protocol</th>
   <th width="67">Module</th><?php

if ($PageOptions['PeerPage']['IPModus'] != 'HideIP') {
   echo '
   <th width="125">IP</th>';
}

?>
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
   <td align="center">'.$Reflector->Peers[$i]->GetLinkedModule().'</td>';
   if ($PageOptions['PeerPage']['IPModus'] != 'HideIP') {
      echo '
   <td>';
      $Bytes = explode(".", $Reflector->Peers[$i]->GetIP());
      if ($Bytes !== false && count($Bytes) == 4) {
         switch ($PageOptions['PeerPage']['IPModus']) {
            case 'ShowLast1ByteOfIP'      : echo $PageOptions['PeerPage']['MasqueradeCharacter'].'.'.$PageOptions['PeerPage']['MasqueradeCharacter'].'.'.$PageOptions['PeerPage']['MasqueradeCharacter'].'.'.$Bytes[3]; break;
            case 'ShowLast2ByteOfIP'      : echo $PageOptions['PeerPage']['MasqueradeCharacter'].'.'.$PageOptions['PeerPage']['MasqueradeCharacter'].'.'.$Bytes[2].'.'.$Bytes[3]; break;
            case 'ShowLast3ByteOfIP'      : echo $PageOptions['PeerPage']['MasqueradeCharacter'].'.'.$Bytes[1].'.'.$Bytes[2].'.'.$Bytes[3]; break;
            default                       : echo '<a href="http://'.$Reflector->Peers[$i]->GetIP().'" target="_blank" style="text-decoration:none;color:#000000;">'.$Reflector->Peers[$i]->GetIP().'</a>';
         }
      }
      echo '</td>';
   }
   echo '
   </tr>';
   if ($i == $PageOptions['PeerPage']['LimitTo']) { $i = $Reflector->PeerCount()+1; }
}

?> 
 
</table>
