<?php

$predefs = [
  "AND" => "x AND y",
  "OR"  => "x || y",
  "NOR" => "x NOR y",
  "XOR" => "x XOR y",
  "XNOR"=> "x XNOR y",
  "NOT" => "!x",
  "HALFADDER" => "out := x XOR y,\ncarry := x AND y",
  "FULLADDER" => "out := x XOR y XOR carryIn,\ncarryOut := (x AND y)OR(x AND carryIn)OR(y AND carryIn)",
  "MUX"   => "out := (sel & in2) | (!sel & in1)",
  "MUX4B" => "out[1] := (sel & in2[1]) | (!sel & in[1])".
          ";\nout[2] := (sel & in2[2]) | (!sel & in[2])".
          ";\nout[3] := (sel & in2[3]) | (!sel & in[3])".
          ";\nout[4] := (sel & in2[4]) | (!sel & in[4]);",

  "MUX4W" => "out := (sel[2] & ((sel[1] & in2) | (!sel[1] & in1)))\n".
             "    | (!sel[2] & ((sel[1] & in4) | (!sel[1] & in3)))",

  "DEMUX"   => "out1 := in & !sel,\nout2 := in & sel",

  "DEMUX4W" => "out1 := in & !sel[1] & !sel[2]".
            ";\nout2 := in &  sel[1] & !sel[2]".
            ";\nout3 := in & !sel[1] &  sel[2]".
            ";\nout4 := in &  sel[1] &  sel[2]"
];
