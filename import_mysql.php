<?php

require 'backend/sqlsets.php';

if(!empty($_SERVER['REMOTE_ADDR'])) exit;

$path = '/mnt/nand/';
if(!file_exists($path)) $path = '.';

foreach(glob($path.'datalog_*.dat') as $filename)
{
  preg_match('/datalog_(\d+)_(\d+)/', $filename, $mat);
  $inputs  = $mat[1];
  $outputs = $mat[2];
  
  #if($inputs == 3 && $outputs < 7) continue;
  if($inputs != 3 || $outputs < 8) continue;
  #if($inputs != 9 || $outputs < 6) continue;
  #if(filesize($filename) > 19115522) continue;

  # Length of the logic table: (2^inputs)*outputs bits
  #                            ceil(((2^inputs)*outputs+5+5)/6) characters (max. 174765 characters)
  # ^ Primary key
  #   MySQL limit for primary key is 1000 bytes
  #   Can still do 9 inputs, 11 gates
  #   Or          10 inputs, 5 gates
  #   Or           8 inputs, 23 gates
  # Length of connections:     ceil((num_gates*2*6+num_outputs*5)/6) characters (max. 46 characters)
  # Assuming max 16 inputs, 16 outputs, 16 gates

  print "Reading {$filename}...\n";
  $fp = fopen($filename, 'rt');

  $db = new mysqli(DB_HOST, DB_USER, DB_PASS, DB_NAME);
  
  $tablename = sprintf("conundrum_%d_%d", $inputs, $outputs);
  
  $max_gates = 11;
  $key_size  = ceil(((1 << $inputs)*$outputs + 5+5) / 6);
  $conn_size = ceil(($max_gates*2*6+$outputs*5) / 6);

  $logictype = 'BLOB';
  if($key_size <= 255)
  {
    $logictype = "VARBINARY($key_size)";
  }
  
  $db->real_query("DROP TABLE {$tablename}");
  $db->real_query("CREATE TABLE {$tablename}(
  gates       TINYINT UNSIGNED NOT NULL,
  logic       $logictype NOT NULL,
  connections VARCHAR($conn_size) NOT NULL,
  PRIMARY KEY(logic($key_size)), KEY g(gates)
) ENGINE=MYISAM");
  print $db->error;
  
  // Disable keys.
  // Afterwards, run "myisampack table.MYI" and "myisamchk -rq table.MYI" on the server.
  //
  $db->real_query("ALTER TABLE {$tablename} DISABLE KEYS");
  print $db->error;
  
  $n   = 0;
  $SQL = '';

  $lines = 0;
  print "- Reading data...\n";
  while(($s = fgets($fp,65536)) !== false)
  {
    if(++$lines % 100 == 0) { print "\r$lines"; flush(); }

    $w = explode(' ', trim($s));
    $gates = $w[0];       // integer
    $logic = $w[1];       // base64; no chars needing escape
    $connections = $w[2]; // base64; no chars needing escape
    
    if(!strlen($SQL)) $SQL = "INSERT INTO {$tablename} VALUES($gates,'$logic','$connections')";
    else              $SQL .=                              ",($gates,'$logic','$connections')";

    if(strlen($SQL) >= 1048576*4)
    {
      #if(!$n) $db->real_query('BEGIN TRANSACTION');

      $db->real_query($SQL);
      print $db->error;

      $SQL = '';
      if(++$n >= 16)
      {
        #$db->real_query("COMMIT");
        $n=0;
      }
    }
  }
  if(strlen($SQL) > 0) $db->real_query($SQL);
  #if($n) $db->real_query("COMMIT");
  fclose($fp);

  print "- Adding index\n";
  #$db->real_query("ALTER TABLE {$tablename} ENABLE KEYS");
  print $db->error;

  $db->real_query("FLUSH TABLES");
  print $db->error;
  $db->ping();
  print "- Complete\n";
  unset($db);
}
