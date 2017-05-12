<?php
if (file_exists("./pgs/functions.php"))  { require_once("./pgs/functions.php");  } else { die("functions.php does not exist.");  }
if (file_exists("./pgs/config.inc.php")) { require_once("./pgs/config.inc.php"); } else { die("config.inc.php does not exist."); }

if (!class_exists('ParseXML'))   require_once("./pgs/class.parsexml.php");

$reflectortype = strtoupper($_GET['type']);

if($reflectortype != "XRF" && $reflectortype != "DCS" && $reflectortype != "REF")
	$reflectortype = "XRF";

$Result = @fopen($CallingHome['ServerURL']."?do=GetReflectorList", "r");

if (!$Result) die("HEUTE GIBTS KEIN BROT");

$INPUT = "";
while (!feof ($Result)) {
    $INPUT .= fgets ($Result, 1024);
}
fclose($Result);

header('Content-type: text/plain');

switch($reflectortype)
{
	case "XRF" :
	header('Content-disposition: attachment;filename=DExtra_Hosts.txt');
	break;
	case "DCS" :
	header('Content-disposition: attachment;filename=DCS_Hosts.txt');
	break;
	case "REF" :
	header('Content-disposition: attachment;filename=DPlus_Hosts.txt');
	break;
}

$XML = new ParseXML();
$Reflectorlist = $XML->GetElement($INPUT, "reflectorlist");
$Reflectors    = $XML->GetAllElements($Reflectorlist, "reflector");


for ($i=0;$i<count($Reflectors);$i++) {
   
   $NAME          = $XML->GetElement($Reflectors[$i], "name");
   $IP		  = $XML->GetElement($Reflectors[$i], "lastip");

   $NAME = str_replace("XLX", $reflectortype, $NAME);

   echo $NAME . " " . $IP . "\n";
}

   
   
