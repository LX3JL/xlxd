<table class="listingtable">
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
         default  : 
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
   if ($i == $PageOptions['RepeatersPage']['LimitTo']) { $i = $Reflector->NodeCount()+1; }
}

?> 
 
</table>
