<?php

class Interlink {

   private $ReflectorName;
   private $ReflectorAddress;
   private $Modules;

   public function __construct() { 
      // Empty strings, not null, PHP 8.1+ warns on null in strpos() and htmlspecialchars()
      $this->ReflectorName    = '';
      $this->ReflectorAddress = '';
      $this->Modules          = '';
   }
   
   public function SetName($RefName) {
    $RefName = trim($RefName);
    // Validate reflector name format (XLX + 3 alphanumeric)
    if (preg_match('/^[A-Z0-9]{3,10}$/i', $RefName)) {
        $this->ReflectorName = strtoupper($RefName);
    }
   }
   public function SetAddress($RefAdd) {
    $RefAdd = trim($RefAdd);
    // Validate it's a valid hostname or IP, YSF/NXDN/P25 peers add a :port
    if ($this->IsValidHost($RefAdd) ||
        (preg_match('/^(.+):([0-9]{1,5})$/', $RefAdd, $Parts) && $Parts[2] >= 1 && $Parts[2] <= 65535 && $this->IsValidHost($Parts[1]))) {
        $this->ReflectorAddress = $RefAdd;
    }
   }

   private function IsValidHost($Host) {
    return filter_var($Host, FILTER_VALIDATE_IP) ||
           filter_var($Host, FILTER_VALIDATE_DOMAIN, FILTER_FLAG_HOSTNAME);
   }
   public function GetName()            {      return $this->ReflectorName;                   }
   public function GetAddress()         {      return $this->ReflectorAddress;                }
   public function GetModules()         {      return $this->Modules;                         }
   
   
   public function AddModule($Module) {
    $Module = trim(strtoupper($Module));
    if (strlen($Module) != 1) return false;
    if (!preg_match('/^[A-Z]$/', $Module)) return false; // Only A-Z
    if (strpos($this->Modules, $Module) === false) {
        $this->Modules .= $Module;
    }
    return true;
   }
   
   public function RemoveModule($Module) {
    $Module = trim(strtoupper($Module));
    if (strlen($Module) != 1) return false;
    if (!preg_match('/^[A-Z]$/', $Module)) return false; // Only A-Z
    if (strpos($this->Modules, $Module) !== false) {
        $this->Modules = str_replace($Module, '', $this->Modules);
    }
    return true;
   }
   
   public function HasModuleEnabled($Module) {
    $Module = trim(strtoupper($Module));
    if (strlen($Module) != 1 || !preg_match('/^[A-Z]$/', $Module)) {
        return false;
    }
    return (strpos($this->Modules, $Module) !== false);
   }
   
}
      
   
   