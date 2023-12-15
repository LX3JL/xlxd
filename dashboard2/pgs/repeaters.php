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
      $IPBinary = inet_pton($Reflector->Nodes[$i]->GetIP());
      $IPLength = strlen($IPBinary);
      $Bytes = str_split($IPBinary, 1);
      switch ($PageOptions['RepeatersPage']['IPModus']) {
         case 'ShowLast1ByteOfIP' : $MasqByte = 3; break;
         case 'ShowLast2ByteOfIP' : $MasqByte = 2; break;
         case 'ShowLast3ByteOfIP' : $MasqByte = 1; break;
         default                  : $MasqByte = 0; break;
      }
      switch ($IPLength) {
         case 4:
            for ($pos = 0; $pos < $IPLength; $pos++) {
               if ($pos) echo '.';
               if ($pos < $MasqByte) echo $PageOptions['RepeatersPage']['MasqueradeCharacter'];
               else echo ord($Bytes[$pos]);
            }
            break;
         case 16:
            for ($pos = 0; $pos < $IPLength; $pos += 2) {
               if ($pos) echo ':';
               if ($pos < ($MasqByte * 4)) echo $PageOptions['RepeatersPage']['MasqueradeCharacter'];
               else {
                  echo bin2hex($Bytes[$pos]);
                  echo bin2hex($Bytes[$pos + 1]);
               }
            }
            break;
         default:
            break;
      }
      echo '</td>';
   }
   echo '
 </tr>';
   if ($i == $PageOptions['RepeatersPage']['LimitTo']) { $i = $Reflector->NodeCount()+1; }
}

?> 
 
</table>
