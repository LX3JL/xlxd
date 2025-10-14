<?php

$Result = @fopen($CallingHome['ServerURL']."?do=GetReflectorList", "r");

if (!$Result) die("HEUTE GIBTS KEIN BROT");

$INPUT = "";
while (!feof ($Result)) {
    $INPUT .= fgets ($Result, 1024);
}
fclose($Result);

$XML = new ParseXML();
$Reflectorlist = $XML->GetElement($INPUT, "reflectorlist");
$Reflectors    = $XML->GetAllElements($Reflectorlist, "reflector");

?>

<table class="listingtable">
 <tr>
   <th width="40">#</th>
   <th width="75">Reflector</th>
   <th width="120">Country</th>
   <th width="90">Service</th>
   <th width="400">Comment</th>
 </tr>
<?php

$odd = "";

for ($i=0;$i<count($Reflectors);$i++) {

  $NAME          = sanitize_output($XML->GetElement($Reflectors[$i], "name"));
  $COUNTRY       = sanitize_output($XML->GetElement($Reflectors[$i], "country"));
  $LASTCONTACT   = intval($XML->GetElement($Reflectors[$i], "lastcontact"));
  $COMMENT       = sanitize_output($XML->GetElement($Reflectors[$i], "comment"));
  $DASHBOARDURL  = sanitize_attribute($XML->GetElement($Reflectors[$i], "dashboardurl"));

  if ($odd == "#FFFFFF") { $odd = "#F1FAFA"; } else { $odd = "#FFFFFF"; }

  echo '
 <tr height="30" bgcolor="'.$odd.'" onMouseOver="this.bgColor=\'#FFFFCA\';" onMouseOut="this.bgColor=\''.$odd.'\';">
   <td align="center">'.($i+1).'</td>
   <td><a href="'.$DASHBOARDURL.'" target="_blank" class="listinglink" title="Visit the Dashboard of&nbsp;'.$NAME.'">'.$NAME.'</a></td>
   <td>'.$COUNTRY.'</td>
   <td align="center" valign="middle"><img src="./img/'; if ($LASTCONTACT<(time()-1800)) { echo 'down'; } ELSE { echo 'up'; } echo '.png" height="25" /></td>
   <td>'.$COMMENT.'</td>
 </tr>';
}

?>
</table>
