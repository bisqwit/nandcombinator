<?php

require 'backend/parser.php';
require 'backend/predefs.php';

$n = new Parser;

#$n->Parse('x IMP (y IMP x)');
$n->Parse($predefs['FULLADDER2B']);
$n->Evaluate();
$n->Analyze();
$n->Solve();
print_r($n);
