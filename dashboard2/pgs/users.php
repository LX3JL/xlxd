<div class="row">
   <div class="col-md-9">
      <table class="table table-striped table-hover">
         <tr class="table-center">   
            <th class="col-md-1">#</th>
            <th class="col-md-1">Flag</th>
            <th class="col-md-2">Callsign</th>
            <th class="col-md-2">Suffix</th>
            <th class="col-md-1">DPRS</th>
            <th class="col-md-2">Via / Peer</th>
            <th class="col-md-2">Last heard</th>
            <th class="col-md-1"><img src="./img/ear.png" alt="Listening on" /></th>
         </tr>
<?php

$Reflector->LoadFlags();
for ($i=0;$i<$Reflector->StationCount();$i++) {
   echo '
  <tr class="table-center">
   <td>';
   echo ($i==0 ? '<img src="./img/radio-waves-hi.png" alt="">' : $i+1);
   
   
   echo '</td>
   <td>';
   
   if (file_exists("./img/flags/".$Reflector->GetFlag($Reflector->Stations[$i]->GetCallSign()).".png")) {
      echo '<img src="./img/flags/'.$Reflector->GetFlag($Reflector->Stations[$i]->GetCallSign()).'.png" class="table-flag" alt="">';
   }
   echo '</td>
   <td><a href="https://www.qrz.com/db/'.$Reflector->Stations[$i]->GetCallsignOnly().'" class="pl" target="_blank">'.$Reflector->Stations[$i]->GetCallsignOnly().'</a></td>
   <td>'.$Reflector->Stations[$i]->GetSuffix().'</td>
   <td><a href="http://www.aprs.fi/'.$Reflector->Stations[$i]->GetCallsignOnly().'" class="pl" target="_blank"><img src="./img/sat.png" alt=""></a></td>
   <td>'.$Reflector->Stations[$i]->GetVia();
   if ($Reflector->Stations[$i]->GetPeer() != $Reflector->GetReflectorName()) {
      echo ' / '.$Reflector->Stations[$i]->GetPeer();
   }
   echo '</td>
   <td>'.@date("d.m.Y H:i", $Reflector->Stations[$i]->GetLastHeardTime()).'</td>
   <td>';
   if ($Reflector->Stations[$i]->GetPeer() == $Reflector->GetReflectorName()) {
      echo trim($Reflector->GetModuleOfNode($Reflector->Stations[$i]->GetVia()));
   }
   echo '</td>
 </tr>';
   if ($i == 39) { $i = $Reflector->StationCount()+1; }
}

?> 
 
      </table>
   </div>
   <div class="col-md-3">
      <table class="table table-striped table-hover">
         <?php 

$Modules = $Reflector->GetModules();
echo '<tr>';
for ($i=0;$i<count($Modules);$i++) {
   
   if (isset($PageOptions['ModuleNames'][$Modules[$i]])) {
      echo '<th>'.$PageOptions['ModuleNames'][$Modules[$i]].' '.$Modules[$i].'</th>';
   }
   else {
   echo '
  
      <th>'.$Modules[$i].'</th>';
   }
}

echo '</tr><tr>';

for ($i=0;$i<count($Modules);$i++) {
    
   $Users = $Reflector->GetCallSignsInModules($Modules[$i]);
   echo '<td><table class="table table-hover">';
   $UserCheckedArray = array();
   
   for ($j=0;$j<count($Users);$j++) {
      if (in_array($Users[$j], $UserCheckedArray)) {
         $CurrentPositions = array_keys($UserCheckedArray,$Users[$j]);
         $Displayname = $Users[$j].'-'.$Reflector->GetSuffixOfRepeater($Users[$j], $Modules[$i], max($CurrentPositions)+1);
      }
      else {
         $Displayname = $Users[$j].'-'.$Reflector->GetSuffixOfRepeater($Users[$j], $Modules[$i]);
      }
      echo '
            <tr>
               <td><a href="http://www.aprs.fi/'.$Displayname.'" class="pl" target="_blank">'.$Displayname.'</a> </td>
            </tr>';
      $UserCheckedArray[] = $Users[$j];
   }
   echo '</table></td>';
}

echo '</tr>';

?>
      </table>
   </div>
</div>
