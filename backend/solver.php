<?php

require_once 'base64.php';
require_once 'sqlsets.php';

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
    $num_gates   = null;
    $connections = '';

    if(!$num_inputs || !$num_outputs)
      return;
    
    $db = new mysqli(DB_HOST, DB_USER, DB_PASS, DB_NAME);
    $tablename = sprintf("conundrum_%d_%d", $num_inputs, $num_outputs);

    $stmt = $db->prepare("SELECT gates,connections FROM $tablename WHERE logic=?");
    if(!$stmt) return;
    $stmt->bind_param('s', $key);
    $stmt->execute();
    $stmt->bind_result($this->num_gates, $connections);
    $stmt->fetch();
    unset($db);

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
