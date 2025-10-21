<?php

class Node {

   private $Callsign;
   private $IP;
   private $LinkedModule;
   private $Protocol;
   private $ConnectTime;
   private $LastHeardTime;
   private $Suffix;
   private $Prefix;
   private $RandomID;

   public function __construct($Callsign, $IP, $LinkedModule, $Protocol, $ConnectTime, $LastHeardTime, $RandomID) {

    // Validate and sanitize IP
    $IP = trim($IP);
    $this->IP = filter_var($IP, FILTER_VALIDATE_IP) ? $IP : '0.0.0.0';

    // Validate protocol
    $Protocol = trim($Protocol);
    $allowed_protocols = ['DPlus', 'DExtra', 'DCS', 'DMR', 'DMRplus', 'DMRMmdvm', 'YSF', 'XLX', 'Terminal/AP', 'IMRS', 'none'];
    $this->Protocol = in_array($Protocol, $allowed_protocols, true) ? $Protocol : 'Unknown';
    
    $this->ConnectTime   = ParseTime($ConnectTime);
    $this->LastHeardTime = ParseTime($LastHeardTime);

    // Sanitize callsign (remove excessive spaces, validate format)
    $Callsign = trim(preg_replace('/\s+/', ' ', $Callsign));
    
    $this->FullCallsign  = str_replace(" ", "-", $Callsign);
    
    if (strpos($Callsign, " ") !== false) {
        $this->Callsign = trim(substr($Callsign, 0, strpos($Callsign, " ")));
        $this->Suffix   = trim(substr($Callsign, strpos($Callsign, " ")));
        $this->Prefix   = strtoupper(trim(substr($Callsign, 0, 3)));
    }
    else {
        $this->Callsign = trim($Callsign);
        $this->Suffix   = "";
        $this->Prefix   = "";
    }

    // Validate callsign format (basic check)
    if (!preg_match('/^[A-Z0-9]{1,10}$/i', $this->Callsign)) {
       $this->Callsign = 'INVALID';
    }

    // Validate LinkedModule (single letter A-Z)
    $LinkedModule = trim(strtoupper($LinkedModule));
    $this->LinkedModule = preg_match('/^[A-Z]$/', $LinkedModule) ? $LinkedModule : '';
    
    $this->RandomID = $RandomID;
   }

   public function GetFullCallsign()         { return $this->FullCallsign;   }
   public function GetCallsign()             { return $this->Callsign;       }
   public function GetIP()                   { return $this->IP;             }
   public function GetLinkedModule()         { return $this->LinkedModule;   }
   public function GetProtocol()             { return $this->Protocol;       }
   public function GetConnectTime()          { return $this->ConnectTime;    }
   public function GetLastHeardTime()        { return $this->LastHeardTime;  }
   public function GetSuffix()               { return $this->Suffix;         }
   public function GetPrefix()               { return $this->Prefix;         }
   public function GetRandomID()             { return $this->RandomID;       }
   
}

?>
