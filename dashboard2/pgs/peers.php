<?php

$Result = @fopen($CallingHome['ServerURL']."?do=GetReflectorList", "r");

$INPUT = "";

if ($Result) {

    while (!feof ($Result)) {
        $INPUT .= fgets ($Result, 1024);
    }

    $XML = new ParseXML();
    $Reflectorlist = $XML->GetElement($INPUT, "reflectorlist");
    $Reflectors    = $XML->GetAllElements($Reflectorlist, "reflector");
}

fclose($Result);
?>

<table class="table table-striped table-hover">
   <tr class="table-center">
      <th class="col-md-1">#</th>
      <th class="col-md-2">XLX Peer</th>
      <th class="col-md-2">Last Heard</th>
      <th class="col-md-2">Linked for</th>
      <th class="col-md-2">Protocol</th>
      <th class="col-md-1">Module</th><?php

if ($PageOptions['PeerPage']['IPModus'] != 'HideIP') {
   echo '
   <th class="col-md-2">IP</th>';
}

?>
 </tr>
<?php

$Reflector->LoadFlags();

for ($i=0;$i<$Reflector->PeerCount();$i++) {
         
   echo '
  <tr class="table-center">
   <td>'.($i+1).'</td>';

   $Name = $Reflector->Peers[$i]->GetCallSign();
   $URL = '';

    for ($j=1;$j<count($Reflectors);$j++) {
        if ($Name === $XML->GetElement($Reflectors[$j], "name")) {
            $URL  = $XML->GetElement($Reflectors[$j], "dashboardurl");
        }
    }
    if ($Result && (trim($URL) != "")) {
        echo '<td><a href="'.$URL.'" target="_blank" class="listinglink" title="Visit the Dashboard of&nbsp;'.$Name.'" style="text-decoration:none;color:#000000;">'.$Name.'</a></td>';
    } else {
        echo '<td>'.$Name.'</td>';
    }
    echo '
   <td>'.date("d.m.Y H:i", $Reflector->Peers[$i]->GetLastHeardTime()).'</td>
   <td>'.FormatSeconds(time()-$Reflector->Peers[$i]->GetConnectTime()).' s</td>
   <td>'.$Reflector->Peers[$i]->GetProtocol().'</td>
   <td>'.$Reflector->Peers[$i]->GetLinkedModule().'</td>';
   if ($PageOptions['PeerPage']['IPModus'] != 'HideIP') {
      echo '<td>';
      $IPBinary = inet_pton($Reflector->Peers[$i]->GetIP());
      $IPLength = strlen($IPBinary);
      $Bytes = str_split($IPBinary, 1);
      switch ($PageOptions['PeerPage']['IPModus']) {
         case 'ShowLast1ByteOfIP' : $MasqByte = 3; break;
         case 'ShowLast2ByteOfIP' : $MasqByte = 2; break;
         case 'ShowLast3ByteOfIP' : $MasqByte = 1; break;
         default                  : $MasqByte = 0; break;
      }
      if ($MasqByte == 0) {
         switch ($IPLength) {
            case 16: $IPAddrBracketL = '['; $IPAddrBracketR = ']'; break;
            default: $IPAddrBracketL = ''; $IPAddrBracketR = ''; break;
         }
         echo '<a href="http://'.$IPAddrBracketL.$Reflector->Peers[$i]->GetIP().$IPAddrBracketR.'" target="_blank" style="text-decoration:none;color:#000000;">'.$Reflector->Peers[$i]->GetIP().'</a>';
      } else {
         switch ($IPLength) {
            case 4:
               for ($pos = 0; $pos < $IPLength; $pos++) {
                  if ($pos) echo '.';
                  if ($pos < $MasqByte) echo $PageOptions['PeerPage']['MasqueradeCharacter'];
                  else echo ord($Bytes[$pos]);
               }
               break;
            case 16:
               for ($pos = 0; $pos < $IPLength; $pos += 2) {
                  if ($pos) echo ':';
                  if ($pos < ($MasqByte * 4)) echo $PageOptions['PeerPage']['MasqueradeCharacter'];
                  else {
                     echo bin2hex($Bytes[$pos]); 
                     echo bin2hex($Bytes[$pos + 1]);
                  }
               }
               break;
            default:
               break;
         }
      }
      echo '</td>';
   }
   echo '</tr>';
   if ($i == $PageOptions['PeerPage']['LimitTo']) { $i = $Reflector->PeerCount()+1; }
}

?> 
 
</table>
