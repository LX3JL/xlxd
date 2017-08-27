<?php

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
