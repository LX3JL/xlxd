<table class="listingtable">
 <tr>
   <th width="80" rowspan="2">Module</th>
   <th width="130" rowspan="2">Name</th>
   <th width="65" rowspan="2">Users</th>
   <th colspan="2">DPlus</th>
   <th colspan="2">DExtra</th>
   <th colspan="2">DCS</th>
   <th width="65" rowspan="2">DMR</th>
   <th width="65" rowspan="2">YSF<br />DG-ID</th>
 </tr>
 <tr>
   <th width="100">URCALL</th>
   <th width="100">DTMF</th>
   <th width="100">URCALL</th>
   <th width="100">DTMF</th>
   <th width="100">URCALL</th>
   <th width="100">DTMF</th>
 </tr>
<?php

$ReflectorNumber = substr($Reflector->GetReflectorName(), 3, 3);
$NumberOfModules = isset($PageOptions['NumberOfModules']) ? min(max($PageOptions['NumberOfModules'],0),26) : 26;

$odd = "";

for ($i = 1; $i <= $NumberOfModules; $i++) {

   $module = chr(ord('A')+($i-1));

   if ($odd == "#FFFFFF") { $odd = "#F1FAFA"; } else { $odd = "#FFFFFF"; }

   echo '
 <tr height="30" bgcolor="'.$odd.'" onMouseOver="this.bgColor=\'#FFFFCA\';" onMouseOut="this.bgColor=\''.$odd.'\';">
   <td align="center">'. sanitize_output($module) .'</td>
   <td align="center">'. sanitize_output(empty($PageOptions['ModuleNames'][$module]) ? '-' : $PageOptions['ModuleNames'][$module]) .'</td>
   <td align="center">'. count($Reflector->GetNodesInModulesByID($module)) .'</td>
   <td align="center">'. sanitize_output('REF' . $ReflectorNumber . $module . 'L') .'</td>
   <td align="center">'. sanitize_output(is_numeric($ReflectorNumber) ? '*' . sprintf('%01d',$ReflectorNumber) . (($i<=4)?$module:sprintf('%02d',$i)) : '-') .'</td>
   <td align="center">'. sanitize_output('XRF' . $ReflectorNumber . $module . 'L') .'</td>
   <td align="center">'. sanitize_output(is_numeric($ReflectorNumber) ? 'B' . sprintf('%01d',$ReflectorNumber) . (($i<=4)?$module:sprintf('%02d',$i)) : '-') .'</td>
   <td align="center">'. sanitize_output('DCS' . $ReflectorNumber . $module . 'L') .'</td>
   <td align="center">'. sanitize_output(is_numeric($ReflectorNumber) ? 'D' . sprintf('%01d',$ReflectorNumber) . (($i<=4)?$module:sprintf('%02d',$i)) : '-') .'</td>
   <td align="center">'. sanitize_output(4000+$i) .'</td>
   <td align="center">'. sanitize_output(9+$i) .'</td>
 </tr>';
}

?>

</table>
