<?php
// ----------------------------------------------------------------------------
//  xlxd
//
//  Created by Luc Engelmann (LX1IQ) on 31/12.2015
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

class Station {

   private $Callsign;
   private $Via;
   private $LastHeardTime;
   private $Suffix;
   private $CallsignOnly;

   public function __construct($Callsign, $Via, $LastHeardTime) {
      $this->Callsign      = trim($Callsign);
      $this->Via           = trim($Via);
      $this->LastHeardTime = ParseTime($LastHeardTime);
      if (strpos($Callsign, " / ") !== false) {
         $this->Suffix = trim(substr($Callsign, strpos($Callsign, " / ")+3, strlen($Callsign)));
      }
      else {
         $this->Suffix = "";
      }

      $tmp = explode(" ", $Callsign);
      $this->CallsignOnly = $tmp[0];
   }

   public function GetCallsign()             { return $this->Callsign;       }
   public function GetVIA()                  { return $this->Via;            }
   public function GetLastHeardTime()        { return $this->LastHeardTime;  }
   public function GetSuffix()               { return $this->Suffix;         }
   public function GetCallsignOnly()         { return $this->CallsignOnly;   }

}

?>
