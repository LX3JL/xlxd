<?php
// ----------------------------------------------------------------------------
//  xlxd
//
//  Created by Luc Engelmann (LX1IQ) on 31/12/2015
//  Copyright © 2015 Luc Engelmann (LX1IQ). All rights reserved.
//
// ----------------------------------------------------------------------------
//    This file is part of xlxd.
//
//    xlxd is free software: you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation, either version 3 of the License, or
//    (at your option) any later version.
//
//    xlxd is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with Foobar.  If not, see <http://www.gnu.org/licenses/>.
// ----------------------------------------------------------------------------

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
