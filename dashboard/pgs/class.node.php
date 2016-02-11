<?php

class Node {
   
   private $Callsign;
   private $IP;
   private $LinkedModule;
   private $Protocol;
   private $ConnectTime;
   private $LastHeardTime;
   private $Suffix;
   
   public function __construct($Callsign, $IP, $LinkedModule, $Protocol, $ConnectTime, $LastHeardTime) {
      
      $this->IP            = $IP;
      
      $this->Protocol      = $Protocol;
      $this->ConnectTime   = ParseTime($ConnectTime);
      $this->LastHeardTime = ParseTime($LastHeardTime);
      
      if (strpos($Callsign, " ") !== false) {
         $this->Callsign      = trim(substr($Callsign, 0, strpos($Callsign, " ")));
         $this->Suffix        = trim(substr($Callsign, strpos($Callsign, " "), strlen($Callsign)));
      }
      else {
         $this->Callsign      = trim($Callsign);
         $this->Suffix        = "";
      }
      
      
      $this->LinkedModule  = trim($LinkedModule);
   }
   

   public function GetCallsign()             { return $this->Callsign;       }
   public function GetIP()                   { return $this->IP;             }
   public function GetLinkedModule()         { return $this->LinkedModule;   }
   public function GetProtocol()             { return $this->Protocol;       }
   public function GetConnectTime()          { return $this->ConnectTime;    }
   public function GetLastHeardTime()        { return $this->LastHeardTime;  }
   public function GetSuffix()               { return $this->Suffix;         }
   
}

?>