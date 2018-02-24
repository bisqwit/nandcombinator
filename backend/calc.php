<?php

require 'parser.php';
require 'predefs.php';

$function = @$_REQUEST['f'] or '0';
if(isset($argv))
  $function = $argv[1];
elseif(strlen($_SERVER['PATH_INFO']) > 1)
  $function = substr($_SERVER['PATH_INFO'], 1);

$n = new Parser;
$n->Parse($function);
$n->Evaluate();
$n->Analyze();
$n->Solve();

#print_r($n);

$result = Array(
  # The stuff that matters
  'i' => $n->num_inputs,
  'o' => $n->num_outputs,
  'g' => $n->num_gates,
  'l' => $n->logic_map,
  
  # Flavor
  'p' => $n->phrases,
  'q' => $n->interpreted_phrases,

  'm' => $n->truthtable,
  'v' => array_values($n->varnames),
  
  'u' => $n->unused_variables,
  'R' => $n->parse_errors,
  
  'k' => $n->key
);
print json_encode($result);
