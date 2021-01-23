<?php

class xReflector {
   
   public $Nodes                    = null;
   public $Stations                 = null;
   public $Peers                    = null;
   private $Flagarray               = null;
   private $Flagarray_DXCC          = null;
   private $Flagfile                = null;
   private $CallingHomeActive       = null;
   private $CallingHomeHash         = null;
   private $CallingHomeDashboardURL = null;
   private $CallingHomeServerURL    = null;
   private $ReflectorName           = null;
   private $ServiceUptime           = null;
   private $ProcessIDFile           = null;
   private $XMLContent              = null;
   private $XMLFile                 = null;
   private $ServiceName             = null;
   private $Version                 = null;
   private $CallingHomeCountry      = null;
   private $CallingHomeComment      = null;
   private $CallingHomeOverrideIP   = null;
   private $Transferinterlink       = null;
   private $Interlinkfile           = null;
   public $Interlinks               = null;
   private $InterlinkXML            = null;
   private $ReflectorXML            = null;
   
   public function __construct() {
      $this->Nodes             = array();
      $this->Stations          = array();
      $this->Peers             = array();
      $this->Interlinks        = array();
      $this->Transferinterlink = false;
   }
   
   public function LoadXML() {
      if ($this->XMLFile != null) {
         $handle = fopen($this->XMLFile, 'r');
         $this->XMLContent = fread($handle, filesize($this->XMLFile));
         fclose($handle);

 # XLX alphanumeric naming...
         $this->ServiceName = substr($this->XMLContent, strpos($this->XMLContent, "<XLX")+4, 3);
         if (preg_match('/[^a-zA-Z0-9]/', $this->ServiceName) == 1) {
            $this->ServiceName = null;
            return false;
         }

         $this->ReflectorName = "XLX".$this->ServiceName;
         
         $LinkedPeersName = "XLX".$this->ServiceName."  linked peers";
         $LinkedNodesName = "XLX".$this->ServiceName."  linked nodes";
         $LinkedUsersName = "XLX".$this->ServiceName."  heard users";
         
         $XML       = new ParseXML();
         
         $AllNodesString    = $XML->GetElement($this->XMLContent, $LinkedNodesName);
         $tmpNodes          = $XML->GetAllElements($AllNodesString, "NODE");
         
         for ($i=0;$i<count($tmpNodes);$i++) {
             $Node = new Node($XML->GetElement($tmpNodes[$i], 'Callsign'), $XML->GetElement($tmpNodes[$i], 'IP'), $XML->GetElement($tmpNodes[$i], 'LinkedModule'), $XML->GetElement($tmpNodes[$i], 'Protocol'), $XML->GetElement($tmpNodes[$i], 'ConnectTime'), $XML->GetElement($tmpNodes[$i], 'LastHeardTime'), CreateCode(16));
             $this->AddNode($Node);
         }
         
         $AllStationsString = $XML->GetElement($this->XMLContent, $LinkedUsersName);
         $tmpStations       = $XML->GetAllElements($AllStationsString, "STATION");
         for ($i=0;$i<count($tmpStations);$i++) {
             $Station = new Station($XML->GetElement($tmpStations[$i], 'Callsign'), $XML->GetElement($tmpStations[$i], 'Via node'), $XML->GetElement($tmpStations[$i], 'Via peer'), $XML->GetElement($tmpStations[$i], 'LastHeardTime'), $XML->GetElement($tmpStations[$i], 'On module'));
             $this->AddStation($Station, false);
         }
         
         $AllPeersString    = $XML->GetElement($this->XMLContent, $LinkedPeersName);
         $tmpPeers          = $XML->GetAllElements($AllPeersString, "PEER");
         for ($i=0;$i<count($tmpPeers);$i++) {
             $Peer = new Peer($XML->GetElement($tmpPeers[$i], 'Callsign'), $XML->GetElement($tmpPeers[$i], 'IP'), $XML->GetElement($tmpPeers[$i], 'LinkedModule'), $XML->GetElement($tmpPeers[$i], 'Protocol'), $XML->GetElement($tmpPeers[$i], 'ConnectTime'), $XML->GetElement($tmpPeers[$i], 'LastHeardTime'));
             $this->AddPeer($Peer, false);
         }
         
         $this->Version = $XML->GetElement($this->XMLContent, "Version");   
      }
   }
   
   public function GetVersion() {
      return $this->Version;
   }
   
   public function GetReflectorName() {
      return $this->ReflectorName;
   }
   
   public function SetXMLFile($XMLFile) {
      if (file_exists($XMLFile) && (is_readable($XMLFile))) {
         $this->XMLFile = $XMLFile;
      }
      else {
         die("File ".$XMLFile." does not exist or is not readable");
         $this->XMLContent = null;
      }
   }
   
   public function SetPIDFile($ProcessIDFile) {
      if (file_exists($ProcessIDFile)) {
         $this->ProcessIDFile = $ProcessIDFile;
         $this->ServiceUptime = time() - filectime($ProcessIDFile);
      }
      else {
         $this->ProcessIDFile = null;
         $this->ServiceUptime = null;
      }
   }
   
   public function GetServiceUptime() {
      return $this->ServiceUptime;
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
         $this->Flagarray_DXCC = array();
         $handle = fopen($this->Flagfile,"r");
         if ($handle) {
            $i = 0;
            while(!feof($handle)) {
               $row = fgets($handle,1024);
               $tmp = explode(";", $row);
         
               if (isset($tmp[0])) { $this->Flagarray[$i]['Country'] = $tmp[0]; } else { $this->Flagarray[$i]['Country'] = 'Undefined'; }
               if (isset($tmp[1])) { $this->Flagarray[$i]['ISO']     = $tmp[1]; } else { $this->Flagarray[$i]['ISO'] = "Undefined"; }
               //$this->Flagarray[$i]['DXCC']    = array();
               if (isset($tmp[2])) { 
                  $tmp2 = explode("-", $tmp[2]);
                  for ($j=0;$j<count($tmp2);$j++) {
                     //$this->Flagarray[$i]['DXCC'][] = $tmp2[$j];
                     $this->Flagarray_DXCC[ trim($tmp2[$j]) ] = $i;
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

   public function AddPeer($PeerObject) {
      if (is_object($PeerObject)) {
         $this->Peers[] = $PeerObject;
      }
   }
   
   public function PeerCount() {
      return count($this->Peers);
   }
   
   public function GetPeer($ArrayIndex) {
      if (isset($this->Peer[$ArrayIndex])) {
         return $this->Peer[$ArrayIndex];
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
               if (strlen(trim($RealCallsign)) > 3) {
                  $this->Stations[] = $StationObject;
               }
            }
            
         }
      }
   }
      
   public function GetSuffixOfRepeater($Repeater, $LinkedModul, $StartWithIndex = 0) {
      $suffix = "";
      $found  = false;
      $i      = $StartWithIndex;
      while (!$found && $i < $this->NodeCount()) {
         if ($this->Nodes[$i]->GetLinkedModule() == $LinkedModul) {
            if (strpos($this->Nodes[$i]->GetCallSign(), $Repeater) !== false) {
               $suffix = $this->Nodes[$i]->GetSuffix();
               $found = true;
            }
         }
         $i++;
      }
      return $suffix;
   }
   
   public function GetCallsignAndSuffixByID($RandomId) {
      $suffix   = "";
      $callsign = "";
      $i        = 0;
      while ($i < $this->NodeCount()) {
         if ($this->Nodes[$i]->GetRandomID() == $RandomId) {
            if (trim($this->Nodes[$i]->GetSuffix()) == "") {
               return $this->Nodes[$i]->GetCallSign();
            }
            else {
               return $this->Nodes[$i]->GetCallSign().'-'.$this->Nodes[$i]->GetSuffix();
            }
         }
         $i++;
      }
      return 'N/A';
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
      $Letters = 4;
      $Name = "";
      while ($Letters >= 2) {
         $Prefix = substr(trim($Callsign), 0, $Letters);
               if (isset($this->Flagarray_DXCC[$Prefix])) {
                  $Image = $this->Flagarray[ $this->Flagarray_DXCC[$Prefix] ]['ISO'];
                  $Name  = $this->Flagarray[ $this->Flagarray_DXCC[$Prefix] ]['Country'];
                  break;
               }
         $Letters--;
      }
      return array(strtolower($Image), $Name);
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
      
   public function GetModuleOfNode($Node) {
      die("FUNCTION DEPRECATED...");
      $Node = trim(str_replace("   ", "-", $Node));
      $Node = trim(str_replace("  ", "-", $Node));
      $Node = trim(str_replace(" ", "-", $Node));
      $found  = false;
      $i      = 0;
      $Module = "";
      while (!$found && $i<$this->NodeCount()) {
         if (strpos($Node, $this->Nodes[$i]->GetFullCallsign()) !== false) { 
            $Module = $this->Nodes[$i]->GetLinkedModule(); 
            $found = true;
         }
         
         $i++;
      }
      return $Module;
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
   
   public function GetNodesInModulesById($Module) {
      $out = array();
      for ($i=0;$i<$this->NodeCount();$i++) {
          if ($this->Nodes[$i]->GetLinkedModule() == $Module) {
             $out[] = $this->Nodes[$i]->GetRandomID();
          }  
      }
      return $out;
   }
   
   public function SetCallingHome($CallingHomeVariables, $Hash) {
      
      if (!isset($CallingHomeVariables['Active']))                {    $CallingHomeVariables['Active']            = false; }
      if (!isset($CallingHomeVariables['MyDashBoardURL']))        {    $CallingHomeVariables['MyDashBoardURL']    = '';    }
      if (!isset($CallingHomeVariables['ServerURL']))             {    $CallingHomeVariables['ServerURL']         = '';    }
      if (!isset($CallingHomeVariables['Country']))               {    $CallingHomeVariables['Country']           = '';    }
      if (!isset($CallingHomeVariables['Comment']))               {    $CallingHomeVariables['Comment']           = '';    }
      if (!isset($CallingHomeVariables['OverrideIPAddress']))     {    $CallingHomeVariables['OverrideIPAddress'] = false; }
      if (!isset($CallingHomeVariables['InterlinkFile']))         {    $CallingHomeVariables['InterlinkFile']     = '';    }
      
      if (!file_exists($CallingHomeVariables['InterlinkFile']))   {    
         $this->Interlinkfile      = '';    
         $this->Transferinterlink  = false;
      }
      else {
         $this->Transferinterlink  = true;
         $this->Interlinkfile      = $CallingHomeVariables['InterlinkFile'];
      }
      
      $this->CallingHomeActive          = ($CallingHomeVariables['Active'] === true);
      $this->CallingHomeHash            = $Hash;
      $this->CallingHomeDashboardURL    = $CallingHomeVariables['MyDashBoardURL'];
      $this->CallingHomeServerURL       = $CallingHomeVariables['ServerURL'];
      $this->CallingHomeCountry         = $CallingHomeVariables['Country'];
      $this->CallingHomeComment         = $CallingHomeVariables['Comment'];
      $this->CallingHomeOverrideIP      = $CallingHomeVariables['OverrideIPAddress'];
      
   }
      
   public function PushCallingHome() {
      $CallingHome = @fopen($this->CallingHomeServerURL."?ReflectorName=".$this->ReflectorName."&ReflectorUptime=".$this->ServiceUptime."&ReflectorHash=".$this->CallingHomeHash."&DashboardURL=".$this->CallingHomeDashboardURL."&Country=".urlencode($this->CallingHomeCountry)."&Comment=".urlencode($this->CallingHomeComment)."&OverrideIP=".$this->CallingHomeOverrideIP, "r");
   }   
   
   public function ReadInterlinkFile() {
      if (file_exists($this->Interlinkfile) && (is_readable($this->Interlinkfile))) {
         $this->Interlinks   = array();
         $this->InterlinkXML = "";
         $Interlinkfilecontent = file($this->Interlinkfile);
         for ($i=0;$i<count($Interlinkfilecontent);$i++) {
             if (substr($Interlinkfilecontent[$i], 0, 1) != '#') {
                $Interlink = explode(" ", $Interlinkfilecontent[$i]);
                $this->Interlinks[] = new Interlink();
                if (isset($Interlink[0])) { $this->Interlinks[count($this->Interlinks)-1]->SetName(trim($Interlink[0]));    }
                if (isset($Interlink[1])) { $this->Interlinks[count($this->Interlinks)-1]->SetAddress(trim($Interlink[1])); }
                if (isset($Interlink[2])) { 
                   $Modules = str_split(trim($Interlink[2]), 1);
                   for ($j=0;$j<count($Modules);$j++) {
                       $this->Interlinks[count($this->Interlinks)-1]->AddModule($Modules[$j]);
                   }
                }
             }
         }
         return true;
      }
      return false;
   }
   
   public function PrepareInterlinkXML() {
      $xml = '
<interlinks>';
      for ($i=0;$i<count($this->Interlinks);$i++) {
          $xml .= '
   <interlink>
      <name>'.$this->Interlinks[$i]->GetName().'</name>
      <address>'.$this->Interlinks[$i]->GetAddress().'</address>
      <modules>'.$this->Interlinks[$i]->GetModules().'</modules>
   </interlink>';
      }
      $xml .= '
</interlinks>';
      $this->InterlinkXML = $xml;
   }
   
   public function PrepareReflectorXML() {
      $this->ReflectorXML = '
<reflector>
   <name>'.$this->ReflectorName.'</name>
   <uptime>'.$this->ServiceUptime.'</uptime>
   <hash>'.$this->CallingHomeHash.'</hash>
   <url>'.$this->CallingHomeDashboardURL.'</url>
   <country>'.$this->CallingHomeCountry.'</country>
   <comment>'.$this->CallingHomeComment.'</comment>
   <ip>'.$this->CallingHomeOverrideIP.'</ip>
   <reflectorversion>'.$this->Version.'</reflectorversion>
</reflector>';
   }
      
   public function CallHome() {
      $xml = '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<query>CallingHome</query>'.$this->ReflectorXML.$this->InterlinkXML;
      $p = @stream_context_create(array('http' => array('header'  => "Content-type: application/x-www-form-urlencoded\r\n",
                                                       'method'  => 'POST', 
                                                       'content' => http_build_query(array('xml' => $xml)) )));
      $result = @file_get_contents($this->CallingHomeServerURL, false, $p);
      if ($result === false) {
         die("CONNECTION FAILED!");
      }
   }
   
   public function InterlinkCount() {
      return count($this->Interlinks);
   }
   
   public function GetInterlink($Index) {
      if (isset($this->Interlinks[$Index])) return $this->Interlinks[$Index];
      return array();
   }
   
   public function IsInterlinked($Reflectorname) {
      $i = -1;
      $f = false;
      while (!$f && $i<$this->InterlinkCount()) {
         $i++;
         if (isset($this->Interlinks[$i])) {
            if ($this->Interlinks[$i]->GetName() == $Reflectorname) {
               $f = true;
               return $i;
            }
         }
      }
      return -1;
   }
         
}

?>
