<?php

require 'backend/parser.php';
require 'backend/predefs.php';

$n = new Parser;

$n->Parse($predefs['HALFADDER']);
$n->Evaluate();
$n->Analyze();
$n->Solve();
print_r($n);
