<?php

if(!empty($_SERVER['REMOTE_ADDR'])) exit;

$path = '/mnt/nand/';
if(!file_exists($path)) $path = '.';

foreach(glob($path.'datalog_*.dat') as $filename)
{
  preg_match('/datalog_(\d+)_(\d+)/', $filename, $mat);
  $inputs  = $mat[1];
  $outputs = $mat[2];
  
  #if(filesize($filename) > 19115522) continue;

  # Length of the logic table: (2^inputs)*outputs bits
  #                            ceil(((2^inputs)*outputs+5+5)/6) characters (max. 174765 characters)
  # Length of connections:     ceil((num_gates*2*6+num_outputs*5)/6) characters (max. 46 characters)
  # Assuming max 16 inputs, 16 outputs, 16 gates

  $DBfn = sprintf("db_%02din%02dout.db", $inputs, $outputs);
  if(file_exists($DBfn) && filemtime($DBfn) >= filemtime($filename))
  {
    continue;
  }

  $fp = fopen($filename, 'rt');
  
  print "Creating {$DBfn}...\n";
  $db = new SQlite3($DBfn, SQLITE3_OPEN_READWRITE | SQLITE3_OPEN_CREATE);

  $db->exec("PRAGMA journal_mode = OFF");
  $db->exec("DROP TABLE conundrum");
  $db->exec("
CREATE TABLE conundrum(
  gates       INT NOT NULL,
  logic       TEXT NOT NULL,
  connections VARCHAR(46) NOT NULL
)");
  $db->exec("PRAGMA journal_mode = OFF");
  
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
    
    if(!strlen($SQL)) $SQL = "INSERT INTO conundrum(gates,logic,connections)VALUES($gates,'$logic','$connections')";
    else              $SQL .=                                                   ",($gates,'$logic','$connections')";

    if(strlen($SQL) >= 1048576*128)
    {
      #if(!$n) $db->exec('BEGIN TRANSACTION');

      $db->exec($SQL);
      $SQL = '';
      if(++$n >= 16)
      {
        #$db->exec("COMMIT");
        $n=0;
      }
    }
  }
  if(strlen($SQL) > 0) $db->exec($SQL);
  #if($n) $db->exec("COMMIT");
  fclose($fp);

  print "- Adding index\n";
  $db->exec("PRAGMA journal_mode = OFF");
  $db->exec("BEGIN TRANSACTION");
  $db->exec("PRAGMA journal_mode = OFF");
  $db->exec("ALTER TABLE conundrum RENAME TO temp");
  $db->exec("
CREATE TABLE conundrum(
  gates       INT NOT NULL,
  logic       TEXT NOT NULL,
  connections VARCHAR(46) NOT NULL,
  PRIMARY KEY(logic)
)");
  $db->exec("INSERT INTO conundrum SELECT * FROM temp");
  $db->exec("CREATE INDEX g ON conundrum(gates)");
  $db->exec("COMMIT");
  $db->exec("PRAGMA journal_mode = OFF");
  print "- Complete\n";
  $db->exec("DROP TABLE temp");

  unset($db);
}
