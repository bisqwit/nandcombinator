<?php

require 'parser.php';
require 'predefs.php';

$n = new Parser;

$n->Parse($predefs['FULLADDER']);
$n->Evaluate();
$n->Analyze();
$n->Solve();
print_r($n);
