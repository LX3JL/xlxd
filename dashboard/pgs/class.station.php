<?php

class Station {
   
   private $Callsign;
   private $Via;
   private $LastHeardTime;
   private $Suffix;
   private $CallsignOnly;
   private $Peer;
   private $OnModule;
      
   public function __construct($Callsign, $Via, $Peer, $LastHeardTime, $OnModule) {
    // Sanitize and validate callsign
    $Callsign = trim($Callsign);
    $this->Callsign = $Callsign;
    
    // Sanitize Via and Peer
    $this->Via = trim($Via);
    $this->Peer = trim($Peer);
    
    $this->LastHeardTime = ParseTime($LastHeardTime);
    
    if (strpos($Callsign, " / ") !== false) {
        $this->Suffix = trim(substr($Callsign, strpos($Callsign, " / ")+3));
    }
    else {
        $this->Suffix = "";
    }
    
    $tmp = explode(" ", $Callsign);
    $this->CallsignOnly = isset($tmp[0]) ? trim($tmp[0]) : '';
    
    // Validate callsign format
    if (!empty($this->CallsignOnly) && !preg_match('/^[A-Z0-9]{1,10}$/i', $this->CallsignOnly)) {
        $this->CallsignOnly = 'INVALID';
    }
    
    // Validate OnModule (single letter A-Z)
    $OnModule = trim(strtoupper($OnModule));
    $this->OnModule = preg_match('/^[A-Z]$/', $OnModule) ? $OnModule : '';
   }
 
   public function GetCallsign()             { return $this->Callsign;       }
   public function GetVIA()                  { return $this->Via;            }
   public function GetPeer()                 { return $this->Peer;           }
   public function GetLastHeardTime()        { return $this->LastHeardTime;  }
   public function GetSuffix()               { return $this->Suffix;         }
   public function GetCallsignOnly()         { return $this->CallsignOnly;   }
   public function GetModule()               { return $this->OnModule;       }
   
}

?>