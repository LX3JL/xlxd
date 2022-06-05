<table class="listingtable">
 <tr>
   <th width="75" rowspan="2">Module</th>
   <th width="130" rowspan="2">Name</th>
   <th width="60" rowspan="2">Users</th>
   <th width="60" rowspan="2">Trans<br />coder</th>
   <th colspan="2">DPlus</th>
   <th colspan="2">DExtra</th>
   <th colspan="2">DCS</th>
   <th width="65" rowspan="2">DMR</th>
   <th width="65" rowspan="2">YSF<br />DG-ID</th>
 </tr>
 <tr>
   <th width="100">URCALL</th>
   <th width="85">DTMF</th>
   <th width="100">URCALL</th>
   <th width="85">DTMF</th>
   <th width="100">URCALL</th>
   <th width="85">DTMF</th>
 </tr>
<?php

$ReflectorNumber = substr($Reflector->GetReflectorName(), 3, 3);
$NumberOfModules = isset($PageOptions['NumberOfModules']) ? min(max($PageOptions['NumberOfModules'],0),26) : 26;

$TranscoderModulesOn = '';
$TranscoderModulesAuto = '';
if (isset($PageOptions['TranscoderFile']) && file_exists($PageOptions['TranscoderFile']) && is_readable($PageOptions['TranscoderFile'])) {
   $TranscoderFileContent = file($PageOptions['TranscoderFile']);
   for ($i=0; $i < count($TranscoderFileContent); $i++) {
      if (substr(trim($TranscoderFileContent[$i]), 0, 1) != '#') {
         $TranscoderOption = explode(" ", trim($TranscoderFileContent[$i]));
         if (isset($TranscoderOption[0]) && isset($TranscoderOption[1])) {
            if ($TranscoderOption[0] === 'ModulesOn') {
               $TranscoderModulesOn = trim($TranscoderOption[1]);
            } else if ($TranscoderOption[0] === 'ModulesAuto') {
               $TranscoderModulesAuto = trim($TranscoderOption[1]);
            }
         }
      }
   }
}

$odd = "";

for ($i = 1; $i <= $NumberOfModules; $i++) {

   $module = chr(ord('A')+($i-1));

   if ($odd == "#FFFFFF") { $odd = "#F1FAFA"; } else { $odd = "#FFFFFF"; }

   $transcoderstate = 'Off';
   if ((strstr($TranscoderModulesOn,'*') !== false) || (strstr($TranscoderModulesOn,$module) !== false)) {
      $transcoderstate = 'On';
   } else if ((strstr($TranscoderModulesAuto,'*') !== false) || (strstr($TranscoderModulesAuto,$module) !== false)) {
      $transcoderstate = 'Auto';
   }

   echo '
 <tr height="30" bgcolor="'.$odd.'" onMouseOver="this.bgColor=\'#FFFFCA\';" onMouseOut="this.bgColor=\''.$odd.'\';">
   <td align="center">'. $module .'</td>
   <td align="center">'. (empty($PageOptions['ModuleNames'][$module]) ? '-' : $PageOptions['ModuleNames'][$module]) .'</td>
   <td align="center">'. count($Reflector->GetNodesInModulesByID($module)) .'</td>
   <td align="center">'. $transcoderstate .'</td>
   <td align="center">'. 'REF' . $ReflectorNumber . $module . 'L' .'</td>
   <td align="center">'. (is_numeric($ReflectorNumber) ? '*' . sprintf('%01d',$ReflectorNumber) . (($i<=4)?$module:sprintf('%02d',$i)) : '-') .'</td>
   <td align="center">'. 'XRF' . $ReflectorNumber . $module . 'L' .'</td>
   <td align="center">'. (is_numeric($ReflectorNumber) ? 'B' . sprintf('%01d',$ReflectorNumber) . (($i<=4)?$module:sprintf('%02d',$i)) : '-') .'</td>
   <td align="center">'. 'DCS' . $ReflectorNumber . $module . 'L' .'</td>
   <td align="center">'. (is_numeric($ReflectorNumber) ? 'D' . sprintf('%01d',$ReflectorNumber) . (($i<=4)?$module:sprintf('%02d',$i)) : '-') .'</td>
   <td align="center">'. (4000+$i) .'</td>
   <td align="center">'. (9+$i) .'</td>
 </tr>';
}

?>

</table>
