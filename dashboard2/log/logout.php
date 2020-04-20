<?php
// Always start this first
session_start();

// Destroying the session clears the $_SESSION variable, thus "logging" the user
// out. This also happens automatically when the browser is closed
session_destroy();

?>
<html>
<head>
<script>

function closeMe()
{
var win=window.open("","_self");
win.close();
}
</script>

</head>
<body>
<form>
<input type="button" name="Close"
onclick="closeMe()" />
</form>
</body>
</html>
