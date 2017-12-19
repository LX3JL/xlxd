<?php

if (!isset($_GET['iface'])) {
	 if (isset($VNStat['Interfaces'][0]['Address'])) {
	 		$_GET['iface'] = $VNStat['Interfaces'][0]['Address'];
	 }
	 else {
	 	  $_GET['iface'] = "";
	 }
}
else {
	 $f = false;
	 $i = 0;
	 while ($i < count($VNStat['Interfaces']) && (!$f)) {
	 	  if ($_GET['iface'] == $VNStat['Interfaces'][$i]['Address']) {
	 	  	 $f = true;
	 	  }
	 	  $i++;
	 }
	 if (!$f) {
	 	  $_GET['iface'] = "";
	 }
}

?>

<table class="listingtable">
 <tr>
   <th>Network interfaces</th>
   <th>Statistics</th>
 </tr>
 <tr>
 	 <td bgcolor="#F1FAFA" align="left" valign="top" style="padding-left:5px;"><?php
 	 
   for ($i=0;$i<count($VNStat['Interfaces']);$i++) {
   	   echo '<a href="./index.php?show=traffic&iface='.$VNStat['Interfaces'][$i]['Address'].'" class="listinglink">'.$VNStat['Interfaces'][$i]['Name'].'</a>';
   	   if ($i < count($VNStat['Interfaces'])) {
   	   	  echo '<br />';
   	   }
   } 	
      
 	 ?></td>
 	 <td bgcolor="#FFFFFF"><?php

$Data = VNStatGetData($_GET['iface'], $VNStat['Binary']);

echo '
<table style="margin:10px;">
	<tr>
		<td>Day</td>
		<td>RX</td>
		<td>TX</td>
		<td>Avg Rx</td>
		<td>Avg TX</td>
  </tr>';

for ($i=0;$i<count($Data[0]);$i++) {  
	if ($Data[0][$i]['time'] > 0) {
		  echo '
		<tr>
			<td width="100">'.date("d.m.Y", $Data[0][$i]['time']).'</td>
			<td width="100">'.kbytes_to_string($Data[0][$i]['rx']).'</td>
			<td width="100">'.kbytes_to_string($Data[0][$i]['tx']).'</td>
			<td width="100">'.kbytes_to_string($Data[0][$i]['rx2']).'</td>
			<td width="100">'.kbytes_to_string($Data[0][$i]['tx2']).'</td>
	  </tr>';
	 }
}

echo '</table>';




echo '
<table style="margin:10px;">
	<tr>
		<td>Month</td>
		<td>RX</td>
		<td>TX</td>
		<td>Avg Rx</td>
		<td>Avg TX</td>
  </tr>';

for ($i=0;$i<count($Data[1]);$i++) {  
	if ($Data[1][$i]['time'] > 0) {
		  echo '
		<tr>
			<td width="100">'.date("F", $Data[1][$i]['time']).'</td>
			<td width="100">'.kbytes_to_string($Data[1][$i]['rx']).'</td>
			<td width="100">'.kbytes_to_string($Data[1][$i]['tx']).'</td>
			<td width="100">'.kbytes_to_string($Data[1][$i]['rx2']).'</td>
			<td width="100">'.kbytes_to_string($Data[1][$i]['tx2']).'</td>
	  </tr>';
	 }
}

echo '</table>';
?>
</td>
 </tr>
</table>
