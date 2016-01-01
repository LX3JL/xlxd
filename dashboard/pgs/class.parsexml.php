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

class ParseXML {

   public function __construct() {
      return true;
   }

   public function GetElement($InputString, $ElementName) {
      if (strpos($InputString, "<".$ElementName.">") === false) return false;
      if (strpos($InputString, "</".$ElementName.">") === false) return false;

      $Element = substr($InputString, strpos($InputString, "<".$ElementName.">")+strlen($ElementName)+2, strpos($InputString, "</".$ElementName.">")-strpos($InputString, "<".$ElementName.">")-strlen($ElementName)-2);
      return $Element;

   }

   public function GetAllElements($InputString, $ElementName) {
      $Elements = array();
      while (strpos($InputString, $ElementName) !== false) {
          $Elements[] = $this->GetElement($InputString, $ElementName);
          $InputString = substr($InputString, strpos($InputString, "</".$ElementName.">")+strlen($ElementName)+3, strlen($InputString));
      }
      return $Elements;
   }


}



?>
