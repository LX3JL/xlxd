<?php

class Peer {
   
   private $Callsign;
   private $IP;
   private $LinkedModule;
   private $Protocol;
   private $ConnectTime;
   private $LastHeardTime;
   
   public function __construct($Callsign, $IP, $LinkedModule, $Protocol, $ConnectTime, $LastHeardTime) {
    
    // Validate and sanitize IP
    $IP = trim($IP);
    $this->IP = filter_var($IP, FILTER_VALIDATE_IP) ? $IP : '0.0.0.0';
    
    // Validate protocol
    $Protocol = trim($Protocol);
    $allowed_protocols = ['DPlus', 'DExtra', 'DCS', 'DMR', 'YSF', 'DEXTRA', 'DPLUS', 'XLX'];
    $this->Protocol = in_array($Protocol, $allowed_protocols, true) ? $Protocol : 'Unknown';
    
    $this->ConnectTime   = ParseTime($ConnectTime);
    $this->LastHeardTime = ParseTime($LastHeardTime);
    
    // Sanitize and validate callsign
    $Callsign = trim($Callsign);
    if (preg_match('/^[A-Z0-9]{3,10}$/i', $Callsign)) {
        $this->Callsign = strtoupper($Callsign);
    } else {
        $this->Callsign = 'INVALID';
    }
    
    // Validate LinkedModule (single letter A-Z)
    $LinkedModule = trim(strtoupper($LinkedModule));
    $this->LinkedModule = preg_match('/^[A-Z]+$/', $LinkedModule) ? $LinkedModule : '';
   }
   

   public function GetCallsign()             { return $this->Callsign;       }
   public function GetIP()                   { return $this->IP;             }
   public function GetLinkedModule()         { return $this->LinkedModule;   }
   public function GetProtocol()             { return $this->Protocol;       }
   public function GetConnectTime()          { return $this->ConnectTime;    }
   public function GetLastHeardTime()        { return $this->LastHeardTime;  }
   
}

?>