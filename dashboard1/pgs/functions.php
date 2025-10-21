<?php
function sanitize_output($string) {
    if ($string === null) return '';
    return htmlspecialchars($string, ENT_QUOTES | ENT_HTML5, 'UTF-8');
}

function sanitize_attribute($string) {
    if ($string === null) return '';
    return htmlspecialchars($string, ENT_QUOTES | ENT_HTML5, 'UTF-8');
}

function validate_callsign($callsign) {
    $callsign = trim($callsign);
    if (preg_match('/^[A-Z0-9\-\/\s]{3,20}$/i', $callsign)) {
        return strtoupper($callsign);
    }
    return '';
}

function validate_module($module) {
    $module = trim(strtoupper($module));
    if (preg_match('/^[A-Z]$/', $module)) {
        return $module;
    }
    return '';
}

function validate_protocol($protocol) {
    $allowed = ['DPlus', 'DExtra', 'DCS', 'DMR', 'DMRplus', 'DMRMmdvm', 'YSF', 'XLX', 'Terminal/AP', 'IMRS', 'none'];
    return in_array(trim($protocol), $allowed, true) ? trim($protocol) : '';
}

function GetSystemUptime() {
    if (file_exists('/proc/uptime')) {
        $uptime = @file_get_contents('/proc/uptime');
        if ($uptime !== false) {
            $parts = explode(' ', $uptime);
            return isset($parts[0]) ? (int)$parts[0] : 0;
        }
    }
    return 0;
}

function Debug($message) {
    if (defined('DEBUG_MODE') && DEBUG_MODE === true) {
        echo '<br><hr><pre>';
        print_r($message); // Don't sanitize here as it's debug only
        echo '</pre><hr><br>';
    }
}

function ParseTime($Input) {
    if (empty($Input) || !is_string($Input)) {
        return false;
    }
    
    $Input = strip_tags($Input); // Remove any HTML tags
   
    if (strpos($Input, "<") !== false) {
       $Input = substr($Input, 0, strpos($Input, "<"));
    }
    
    // Tuesday Tue Nov 17 14:23:22 2015
    $tmp = explode(" ", $Input);
    
    // Add bounds checking
    if (count($tmp) < 6) {
        return false;
    }
    
    if (strlen(trim($tmp[3])) == 0) {
       unset($tmp[3]);
       $tmp = array_values($tmp);
    }
    
    // Check array indices exist after potential unset
    if (!isset($tmp[4]) || !isset($tmp[2]) || !isset($tmp[3]) || !isset($tmp[5])) {
        return false;
    }

    $tmp1 = explode(":", $tmp[4]);
    
    // Check time parts exist
    if (count($tmp1) < 3) {
        return false;
    }
    
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
   // Validate interface name (only allow alphanumeric, dash, underscore)
   if (!preg_match('/^[a-zA-Z0-9_-]+$/', $iface)) {
      return null;
   }
    
   // Validate vnstat binary path
   if (!file_exists($vnstat_bin) || !is_executable($vnstat_bin)) {
      return null;
   }
    
   // Escape shell arguments
   $iface_escaped = escapeshellarg($iface);
   $vnstat_bin_escaped = escapeshellarg($vnstat_bin);

   $vnstat_data = array();

   $fd = @popen("$vnstat_bin_escaped --dumpdb -i $iface_escaped", "r");
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
