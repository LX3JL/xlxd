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
session_start();

if (isset($_POST['password'])) {
   $_SESSION['password'] = $_POST['password'];
}

if (isset($_SESSION['password'])) {

   if ($_SESSION['password'] != "-D-Star+") {
   echo '
   <form name="frmpass" action="./xlxlog.php" method="post">

      <input type="password" name="password" />
      <input type="submit" />

   </form>';

   die();
   }
}
else {
   echo '
   <form name="frmpass" action="./xlxlog.php" method="post">

      <input type="password" name="password" />
      <input type="submit" />

   </form>';

   die();
}


?>
<!DOCTYPE html PUBLIC"-//W3C//DTD XHTML 1.0 Strict//EN"
"http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
<head>
   <meta http-equiv="content-type" content="text/html; charset=utf-8" />
   <title>XLX Live Log</title>
   <script>

      function ReloadPage() {
         document.location.href = "./xlxlog.php";
      }

      setTimeout(ReloadPage, 1000);




   </script>
</head>
<body>

   <pre>

<?php
   $a = array();
   $handle = fopen("/var/log/messages","r");
   if ($handle) {
      while(!feof($handle)) {
         $a[] = fgets($handle, 1024);
      }
      fclose($handle);
   }

   $a = array_reverse($a);

   $max = 65;
   if (count($a) < 65) $max = count($a);

   for ($i=0;$i<$max;$i++) {
       echo $a[$i];
   }

?>
   </pre>
</body>
</html>
