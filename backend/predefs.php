<?php

$predefs = [
  "AND" => "x AND y",
  "OR"  => "x || y",
  "NOR" => "x NOR y",
  "XOR" => "x XOR y",
  "XNOR"=> "x XNOR y",
  "NOT" => "!x",

  "HALFADDER"   => "out := x XOR y,\ncarry := x AND y",
  "HALFADDER2B" => "out[1]   :=  x[1] XOR y[1];\n".
                   "carry[1] :=  x[1] AND y[1];\n".
                   "out[2]   :=  x[2] XOR x[2] XOR carry[1];\n".
                   "carry[2] := (x[2] AND y[2])OR(x[2] AND carry[1])OR(y[2] AND carry[1])",
  "HALFADDER4B" => "out[1]   :=  x[1] XOR y[1];\n".
                   "carry[1] :=  x[1] AND y[1];\n".
                   "out[2]   :=  x[2] XOR x[2] XOR carry[1];\n".
                   "carry[2] := (x[2] AND y[2])OR(x[2] AND carry[1])OR(y[2] AND carry[1]);\n".
                   "out[3]   :=  x[3] XOR x[3] XOR carry[2];\n".
                   "carry[3] := (x[3] AND y[3])OR(x[3] AND carry[2])OR(y[3] AND carry[2]);\n".
                   "out[4]   :=  x[4] XOR x[4] XOR carry[3];\n".
                   "carry[4] := (x[4] AND y[4])OR(x[4] AND carry[3])OR(y[4] AND carry[3])",

  "FULLADDER"   => "out := x XOR y XOR carryIn,\n".
                   "carryOut := (x AND y)OR(x AND carryIn)OR(y AND carryIn)",
  "FULLADDER2B" => "out[1]   :=  x[1] XOR y[1] XOR carryIn;\n".
                   "carry[1] := (x[1] AND y[1])OR(x[1] AND carryIn)OR(y[1] AND carryIn);\n".
                   "out[2]   :=  x[2] XOR x[2] XOR carry[1];\n".
                   "carry[2] := (x[2] AND y[2])OR(x[2] AND carry[1])OR(y[2] AND carry[1])",
  "FULLADDER4B" => "out[1]   :=  x[1] XOR y[1] XOR carryIn;\n".
                   "carry[1] := (x[1] AND y[1])OR(x[1] AND carryIn)OR(y[1] AND carryIn);\n".
                   "out[2]   :=  x[2] XOR x[2] XOR carry[1];\n".
                   "carry[2] := (x[2] AND y[2])OR(x[2] AND carry[1])OR(y[2] AND carry[1]);\n".
                   "out[3]   :=  x[3] XOR x[3] XOR carry[2];\n".
                   "carry[3] := (x[3] AND y[3])OR(x[3] AND carry[2])OR(y[3] AND carry[2]);\n".
                   "out[4]   :=  x[4] XOR x[4] XOR carry[3];\n".
                   "carry[4] := (x[4] AND y[4])OR(x[4] AND carry[3])OR(y[4] AND carry[3])",

  "MUX"   => "out := (sel & in2) | (!sel & in1)",

  "MUX2B" => "out[1] := (sel & in2[1]) | (!sel & in1[1])".
          ";\nout[2] := (sel & in2[2]) | (!sel & in1[2])",

  "MUX4B" => "out[1] := (sel & in2[1]) | (!sel & in1[1])".
          ";\nout[2] := (sel & in2[2]) | (!sel & in1[2])".
          ";\nout[3] := (sel & in2[3]) | (!sel & in1[3])".
          ";\nout[4] := (sel & in2[4]) | (!sel & in1[4]);",

  "MUX4W" => "out := (sel[2] & ((sel[1] & in2) | (!sel[1] & in1)))\n".
             "    | (!sel[2] & ((sel[1] & in4) | (!sel[1] & in3)))",

  "DEMUX"   => "out1 := in & !sel,\nout2 := in & sel",

  "DEMUX2B" => "out1[1] := in[1] & !sel,\nout2[1] := in[1] & sel\n".
               "out1[2] := in[2] & !sel,\nout2[2] := in[2] & sel\n",

  "DEMUX4W" => "out1 := in & !sel[1] & !sel[2]".
            ";\nout2 := in &  sel[1] & !sel[2]".
            ";\nout3 := in & !sel[1] &  sel[2]".
            ";\nout4 := in &  sel[1] &  sel[2]"
];
