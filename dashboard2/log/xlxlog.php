<?php
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
