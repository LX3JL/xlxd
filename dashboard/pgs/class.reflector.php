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

class xReflector {

   public $Nodes         = null;
   public $Stations      = null;
   private $Flagarray    = null;
   private $Flagfile     = null;

   public function __construct() {
      $this->Nodes    = array();
      $this->Stations = array();
   }

   public function SetFlagFile($Flagfile) {
      if (file_exists($Flagfile) && (is_readable($Flagfile))) {
         $this->Flagfile = $Flagfile;
         return true;
      }
      return false;
   }

   public function LoadFlags() {
      if ($this->Flagfile != null) {
         $this->Flagarray = array();
         $handle = fopen($this->Flagfile,"r");
         if ($handle) {
            $i = 0;
            while(!feof($handle)) {
               $row = fgets($handle,1024);
               $tmp = explode(";", $row);

               if (isset($tmp[0])) { $this->Flagarray[$i]['Country'] = $tmp[0]; } else { $this->Flagarray[$i]['Country'] = 'Undefined'; }
               if (isset($tmp[1])) { $this->Flagarray[$i]['ISO']     = $tmp[1]; } else { $this->Flagarray[$i]['ISO'] = "Undefined"; }
               $this->Flagarray[$i]['DXCC']    = array();
               if (isset($tmp[2])) {
                  $tmp2 = explode("-", $tmp[2]);
                  for ($j=0;$j<count($tmp2);$j++) {
                     $this->Flagarray[$i]['DXCC'][] = $tmp2[$j];
                  }
               }
               $i++;
            }
            fclose($handle);
         }
         return true;
      }
      return false;
   }

   public function AddNode($NodeObject) {
      if (is_object($NodeObject)) {
         $this->Nodes[] = $NodeObject;
      }
   }

   public function NodeCount() {
      return count($this->Nodes);
   }

   public function GetNode($ArrayIndex) {
      if (isset($this->Nodes[$ArrayIndex])) {
         return $this->Nodes[$ArrayIndex];
      }
      return false;
   }

   public function AddStation($StationObject, $AllowDouble = false) {
      if (is_object($StationObject)) {

         if ($AllowDouble) {
            $this->Stations[] = $StationObject;
         }
         else {
            $FoundStationInList = false;
            $i                  = 0;

            $tmp = explode(" ", $StationObject->GetCallsign());
            $RealCallsign       = trim($tmp[0]);

            while (!$FoundStationInList && $i<$this->StationCount()) {
               if ($this->Stations[$i]->GetCallsignOnly() == $RealCallsign) {
                  $FoundStationInList = true;
               }
               $i++;
            }

            if (!$FoundStationInList) {
               $this->Stations[] = $StationObject;
            }

         }
      }
   }

   public function GetSuffixOfRepeater($Repeater) {
      $suffix = "";
      $found  = false;
      $i      = 0;
      while (!$found && $i < $this->NodeCount()) {

         if (strpos($this->Nodes[$i]->GetCallSign(), $Repeater) !== false) {


            $suffix = $this->Nodes[$i]->GetSuffix();
            $found = true;
         }
         $i++;
      }
      return $suffix;
   }

   public function StationCount() {
      return count($this->Stations);
   }

   public function GetStation($ArrayIndex) {
      if (isset($this->Stations[$ArrayIndex])) {
         return $this->Stations[$ArrayIndex];
      }
      return false;
   }

   public function GetFlag($Callsign) {
      $Image     = "";
      $FoundFlag = false;
      $Letters = 2;
      while (($Letters < 5) && (!$FoundFlag)) {
         $j = 0;
         $Prefix = substr($Callsign, 0, $Letters);
         while (($j < count($this->Flagarray)) && (!$FoundFlag)) {

            $z = 0;
            while (($z < count($this->Flagarray[$j]['DXCC'])) && (!$FoundFlag)) {
               if (trim($Prefix) == trim($this->Flagarray[$j]['DXCC'][$z])) {
                  $Image = $this->Flagarray[$j]['ISO'];
                  $FoundFlag = true;
               }
               $z++;
            }
            $j++;
         }
         $Letters++;
      }

      if (!$FoundFlag) {
         $Prefix = substr($Callsign, 0, 1);
         if ($Prefix == 'N') { $Image = "us"; }
         if ($Prefix == 'K') { $Image = "us"; }
         if ($Prefix == 'W') { $Image = "us"; }
         if ($Prefix == 'G') { $Image = "gb"; }
         if ($Prefix == 'R') { $Image = "ru"; }
         if ($Prefix == 'F') { $Image = "fr"; }
      }
      return strtolower($Image);
   }

   public function GetModules() {
      $out = array();
      for ($i=0;$i<$this->NodeCount();$i++) {
          $Found = false;
          $b = 0;
          while ($b < count($out) && !$Found) {
             if ($out[$b] == $this->Nodes[$i]->GetLinkedModule()) {
                $Found = true;
             }
             $b++;
          }
          if (!$Found && (trim($this->Nodes[$i]->GetLinkedModule()) != "")) {
             $out[] = $this->Nodes[$i]->GetLinkedModule();
          }
      }
      return $out;
   }

   public function GetCallSignsInModules($Module) {
      $out = array();
      for ($i=0;$i<$this->NodeCount();$i++) {
          if ($this->Nodes[$i]->GetLinkedModule() == $Module) {
             $out[] = $this->Nodes[$i]->GetCallsign();
          }
      }
      return $out;
   }

}

?>
