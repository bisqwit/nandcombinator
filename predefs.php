<?php

$predefs = [
  'AND' => 'x AND y',
  'OR'  => 'x || y',
  'NOR' => 'x NOR y',
  'XOR' => 'x XOR y',
  'XNOR'=> 'x XNOR y',
  'NOT' => '!x',
  'HALFADDER' => 'out := x XOR y, carry := (x AND y) OR(z AND NOT z)',
  'FULLADDER' => 'out := x XOR y XOR carryIn, carryOut := (x AND y)OR(x AND carryIn)OR(y AND carryIn)',
  'MUX'   => 'out := (sel & in2) | (!sel & in1)',
  'MUX4B' => 'out[1] := (sel & in2[1]) | (!sel & in[1])'.
           '; out[2] := (sel & in2[2]) | (!sel & in[2])'.
           '; out[3] := (sel & in2[3]) | (!sel & in[3])'.
           '; out[4] := (sel & in2[4]) | (!sel & in[4])',

  'MUX4W' => 'out := (sel[2] & ((sel[1] & in2) | (!sel[1] & in1)))'.
                ' | (!sel[2] & ((sel[1] & in4) | (!sel[1] & in3)))',


  'DEMUX'   => 'out1 := in & !sel, out2 := in & sel',

  'DEMUX4W' => 'out1 := in & !sel[1] & !sel[2]'.
             '; out2 := in &  sel[1] & !sel[2]'.
             '; out3 := in & !sel[1] &  sel[2]'.
             '; out4 := in &  sel[1] &  sel[2]'
];
