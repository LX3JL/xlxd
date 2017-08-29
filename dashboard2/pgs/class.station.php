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
      $this->Callsign      = trim($Callsign);
      $this->Via           = trim($Via);
      $this->Peer          = trim($Peer);
      $this->LastHeardTime = ParseTime($LastHeardTime);
      if (strpos($Callsign, " / ") !== false) {
         $this->Suffix = trim(substr($Callsign, strpos($Callsign, " / ")+3, strlen($Callsign)));
      }
      else {
         $this->Suffix = "";
      }
      
      $tmp = explode(" ", $Callsign);
      $this->CallsignOnly = $tmp[0];
      $this->OnModule     = $OnModule;
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