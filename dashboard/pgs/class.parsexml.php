<?php

class ParseXML {

   public function __construct() {
      return true;
   }

   public function GetElement($InputString, $ElementName) {
    // Sanitize element name to prevent XML injection
    $ElementName = preg_replace('/[^a-zA-Z0-9_\-\s]/', '', $ElementName);
    
    if (strpos($InputString, "<".$ElementName.">") === false) return false;
    if (strpos($InputString, "</".$ElementName.">") === false) return false;

    $Element = substr($InputString, strpos($InputString, "<".$ElementName.">")+strlen($ElementName)+2, strpos($InputString, "</".$ElementName.">")-strpos($InputString, "<".$ElementName.">")-strlen($ElementName)-2);
    
    // Return raw content - sanitization happens at output time
    return $Element;
   }

   public function GetAllElements($InputString, $ElementName) {
    // Sanitize element name to prevent XML injection
    $ElementName = preg_replace('/[^a-zA-Z0-9_\-\s]/', '', $ElementName);
    
    $Elements = array();
    while (strpos($InputString, $ElementName) !== false) {
        $element = $this->GetElement($InputString, $ElementName);
        if ($element !== false) {
            $Elements[] = $element;
        }
        $InputString = substr($InputString, strpos($InputString, "</".$ElementName.">")+strlen($ElementName)+3, strlen($InputString));
    }
    return $Elements;
   }

}

?>
