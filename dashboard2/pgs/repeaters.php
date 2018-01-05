<table class="table table-striped table-hover">
   <tr class="table-center">
      <th class="col-md-1">#</th>
      <th class="col-md-1">Flag</th>
      <th class="col-md-2">DV Station</th>
      <th class="col-md-1">Band</th>
      <th class="col-md-2">Last Heard</th>
      <th class="col-md-2">Linked for</th>
      <th class="col-md-1">Protocol</th>
      <th class="col-md-1">Module</th><?php

if ($PageOptions['RepeatersPage']['IPModus'] != 'HideIP') {
   echo '
   <th class="col-md-1">IP</th>';
}

?>
         </tr>
<?php
$Reflector->LoadFlags();

for ($i=0;$i<$Reflector->NodeCount();$i++) {
         
   echo '
  <tr class="table-center">
   <td>'.($i+1).'</td>
   <td>';
   list ($Flag, $Name) = $Reflector->GetFlag($Reflector->Nodes[$i]->GetCallSign());
   if (file_exists("./img/flags/".$Flag.".png")) {
      echo '<a href="#" class="tip"><img src="./img/flags/'.$Flag.'.png" class="table-flag" alt="'.$Name.'"><span>'.$Name.'</span></a>';
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
   <td>'.$Reflector->Nodes[$i]->GetLinkedModule().'</td>';
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
