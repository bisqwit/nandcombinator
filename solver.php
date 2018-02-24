<?php

require_once 'base64.php';

class Solver
{
  var $key;
  var $num_inputs;
  var $num_outputs;
  var $num_gates;
  var $gate_from;
  var $out_from;

  function __construct($key)
  {
    $this->key = $key;
  }
  
  function Solve()
  {
    $key = $this->key;
    $dec = new BASE64decoder($key);
    $num_inputs  = $dec->Get(5);
    $num_outputs = $dec->Get(5);

    if(!$num_inputs || !$num_outputs)
      return;
    
    $SQL = "SELECT * FROM conundrum WHERE logic='$key'";
    
    $fn  = sprintf("db_%02din%02dout.db", $num_inputs, $num_outputs);
    if(!file_exists($fn))
      return;
    
    #print "Opening $fn\n";
    $db  = new SQlite3($fn, SQLITE3_OPEN_READONLY);
    $result = $db->query($SQL);
    $row    = $result->fetchArray(SQLITE3_ASSOC);
    $this->num_inputs  = $num_inputs;
    $this->num_outputs = $num_outputs;
    $this->num_gates   = $row['gates'];
    $connections       = $row['connections'];

    $dec = new BASE64decoder($connections);
    for($n=0; $n<$this->num_gates*2; ++$n)
    {
      $code = $dec->Get(6);
      if($code < $num_inputs) { $this->gate_from[$n>>1][] = 'i'.($code); }
      else                    { $this->gate_from[$n>>1][] = 'g'.($code-$num_inputs); }
    }
    for($n=0; $n<$num_outputs; ++$n)
    {
      $code = $dec->Get(5);
      $this->out_from[$n] = "g$code";
    }
  }
};
