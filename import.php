<?php
foreach(glob('/mnt/nand/datalog_*.dat') as $filename)
{
  preg_match('/datalog_(\d+)_(\d+)/', $filename, $mat);
  $inputs  = $mat[1];
  $outputs = $mat[2];
  $fp = fopen($filename, 'rt');
  
  # Length of the logic table: (2^inputs)*outputs bits
  #                            ceil(((2^inputs)*outputs+5+5)/6) characters (max. 174765 characters)
  # Length of connections:     ceil((num_gates*2*6+num_outputs*5)/6) characters (max. 46 characters)
  # Assuming max 16 inputs, 16 outputs, 16 gates
  $SQLfn = sprintf("db_%02din%02dout.sql", $inputs, $outputs);
  $fo = fopen($SQLfn, "w");

  fprintf($fo, "
CREATE TABLE conundrum(
  gates       INT NOT NULL,
  logic       TEXT NOT NULL,
  connections VARCHAR(46) NOT NULL,
  PRIMARY KEY(logic)
);
DELETE FROM conundrum;
");

  $n = 0;
  while(($s = fgets($fp,4096)) !== false)
  {
    $w = explode(' ', trim($s));
    $gates = $w[0];
    $logic = $w[1];
    $connections = $w[2];

    if($n==0) fprintf($fo, "begin transaction;\n");

    $SQL = "insert into conundrum(gates,logic,connections)values($gates,'$logic','$connections')";
    fprintf($fo, "%s;\n", $SQL);
    
    if(++$n == 16384)
    {
      fprintf($fo, "commit;\n");
      $n=0;
    }
  }
  if($n) fprintf($fo, "commit;\n");
  fclose($fp);
  fclose($fo);

  $DBfn = sprintf("db_%02din%02dout.db", $inputs, $outputs);
  unlink($DBfn);
  print "sqlite $DBfn < $SQLfn; rm $SQLfn\n";
}
