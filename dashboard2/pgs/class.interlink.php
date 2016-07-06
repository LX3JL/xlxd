<?php

class Interlink {

   private $ReflectorName;
   private $ReflectorAddress;
   private $Modules;

   public function __construct() { 
      $this->ReflectorName    = null;
      $this->ReflectorAddress = null;
      $this->Modules          = null;
   }
   
   public function SetName($RefName)    {      $this->ReflectorName = trim($RefName);        }
   public function SetAddress($RefAdd)  {      $this->ReflectorAddress = trim($RefAdd);       }
   public function GetName()            {      return $this->ReflectorName;                   }
   public function GetAddress()         {      return $this->ReflectorAddress;                }
   public function GetModules()         {      return $this->Modules;                         }
   
   
   public function AddModule($Module) {
      $Module = trim($Module);
      if (strlen($Module) != 1) return false;
      if (strpos($this->Modules, $Module) === false) {
         $this->Modules .= $Module;
      }
      return true;
   }
   
   public function RemoveModule($Module) {
      $Module = trim($Module);
      if (strlen($Module) != 1) return false;
      if (strpos($this->Modules, $Module) !== false) {
         $this->Modules = substr($this->Modules, 0, strpos($this->Modules, $Module)-1).substr($this->Modules, strpos($this->Modules, $Module)+1, strlen($this->Modules));
      }
      return true;
   }
   
   public function HasModuleEnabled($Module) {
      if (strlen($Module) != 1) return false;
      return (strpos($this->Modules, $Module) !== false);
   }
   
}
      
   
   