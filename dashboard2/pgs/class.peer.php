<?php

class Peer {
   
   private $Callsign;
   private $IP;
   private $LinkedModule;
   private $Protocol;
   private $ConnectTime;
   private $LastHeardTime;
   
   public function __construct($Callsign, $IP, $LinkedModule, $Protocol, $ConnectTime, $LastHeardTime) {
      
      $this->IP            = $IP;
      $this->Protocol      = $Protocol;
      $this->ConnectTime   = ParseTime($ConnectTime);
      $this->LastHeardTime = ParseTime($LastHeardTime);
      $this->Callsign      = trim($Callsign);      
      $this->LinkedModule  = trim($LinkedModule);
   }
   

   public function GetCallsign()             { return $this->Callsign;       }
   public function GetIP()                   { return $this->IP;             }
   public function GetLinkedModule()         { return $this->LinkedModule;   }
   public function GetProtocol()             { return $this->Protocol;       }
   public function GetConnectTime()          { return $this->ConnectTime;    }
   public function GetLastHeardTime()        { return $this->LastHeardTime;  }
   
}

?>