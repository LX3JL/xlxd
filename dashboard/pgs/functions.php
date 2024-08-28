<?php

function GetSystemUptime() {
   $out = exec("uptime");
   return substr($out, 0, strpos($out, ","));
}

function Debug($message) {
   echo '<br><hr><pre>';
   print_r($message);
   echo '</pre><hr><br>';
}

function ParseTime($Input) {
   
    if (strpos($Input, "<") !== false) {
       $Input = substr($Input, 0, strpos($Input, "<"));
    }
    
    // Tuesday Tue Nov 17 14:23:22 2015
    $tmp  = explode(" ", $Input);
    if (strlen(trim($tmp[3])) == 0) {
       unset($tmp[3]);
       $tmp = array_values($tmp);
    }

    $tmp1 = explode(":", $tmp[4]); 
    $month = "";
    switch (strtolower($tmp[2])) {
      case 'jan' : $month = 1; break;
      case 'feb' : $month = 2; break;
      case 'mar' : $month = 3; break;
      case 'apr' : $month = 4; break;
      case 'may' : $month = 5; break;
      case 'jun' : $month = 6; break;
      case 'jul' : $month = 7; break;
      case 'aug' : $month = 8; break;
      case 'sep' : $month = 9; break;
      case 'oct' : $month = 10; break;
      case 'nov' : $month = 11; break;
      case 'dec' : $month = 12; break;
      default    : $month = 1; 
    }
    return @mktime($tmp1[0], $tmp1[1], $tmp1[2], $month, $tmp[3], $tmp[5]);
    
}

function FormatSeconds($seconds) {
  $seconds = abs($seconds); 
  return sprintf("%d days %02d:%02d:%02d", $seconds/60/60/24,($seconds/60/60)%24,($seconds/60)%60,$seconds%60);
} 

function CreateCode ($laenge) {   
	$zeichen = "1234567890abcdefghijklmnopqrstuvwyxzABCDEFGHIJKLMNAOPQRSTUVWYXZ";   
	$out = "";
	for ($i=1;$i<=$laenge;$i++){ 
		$out .= $zeichen[mt_rand(0,(strlen($zeichen)-1))];       
	}         
	return $out;  
}

function VNStatLocalize($str) {
  global $L;
 	if (isset($L[$str])) {
      return $L[$str];
  }
  else {
      return $str;
  }
}

function VNStatGetData($iface, $vnstat_bin) {
   $vnstat_data = array();

   $fd = @popen("$vnstat_bin --dumpdb -i $iface", "r");
   if (is_resource($fd)) {
      $buffer = '';
      while (!feof($fd)) {
       	 $buffer .= fgets($fd);
      }
     	$vnstat_data = explode("\n", $buffer);
     	pclose($fd);
   }

   $day = array();
   $hour = array();
   $month = array();
   $top = array();

   if (isset($vnstat_data[0]) && strpos($vnstat_data[0], 'Error') !== false) {
      return;
   }

   foreach($vnstat_data as $line) {
      $d = explode(';', trim($line));
      if ($d[0] == 'd') {
         $day[$d[1]]['time']  = $d[2];
         $day[$d[1]]['rx']    = $d[3] * 1024 + $d[5];
         $day[$d[1]]['tx']    = $d[4] * 1024 + $d[6];
         $day[$d[1]]['act']   = $d[7];
         $day[$d[1]]['rx2']   = $d[5];
         $day[$d[1]]['tx2']   = $d[6];
      }
      else if ($d[0] == 'm') {
         $month[$d[1]]['time'] = $d[2];
         $month[$d[1]]['rx']   = $d[3] * 1024 + $d[5];
         $month[$d[1]]['tx']   = $d[4] * 1024 + $d[6];
         $month[$d[1]]['act']  = $d[7];
         $month[$d[1]]['rx2']  = $d[5];
         $month[$d[1]]['tx2']  = $d[6];
      }
      else if ($d[0] == 'h') {
         $hour[$d[1]]['time'] = $d[2];
         $hour[$d[1]]['rx']   = $d[3];
         $hour[$d[1]]['tx']   = $d[4];
         $hour[$d[1]]['act']  = 1;
      }
      else if ($d[0] == 't') {
         $top[$d[1]]['time'] = $d[2];
         $top[$d[1]]['rx']   = $d[3] * 1024 + $d[5];
         $top[$d[1]]['tx']   = $d[4] * 1024 + $d[6];
         $top[$d[1]]['act']  = $d[7];
      }
      else {
         $summary[$d[0]] = isset($d[1]) ? $d[1] : '';
      }
   }

   rsort($day);
   rsort($month);
   rsort($hour);
   
   return array($day, $month, $hour, $day, $month, $top, $summary);
}


function kbytes_to_string($kb) {
   $byte_notation  = null;
   $units          = array('TB','GB','MB','KB');
   $scale          = 1024*1024*1024;
   $ui             = 0;
   $custom_size = isset($byte_notation) && in_array($byte_notation, $units);

   while ((($kb < $scale) && ($scale > 1)) || $custom_size) {
      $ui++;
      $scale = $scale / 1024;
      if ($custom_size && $units[$ui] == $byte_notation) {
         break;
      }
   }
   return sprintf("%0.2f %s", ($kb/$scale),$units[$ui]);
}


?>
