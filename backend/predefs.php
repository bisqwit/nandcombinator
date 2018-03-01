<?php

$predefs = [
  "AND" => "x AND y",
  "OR"  => "x || y",
  "NOR" => "x NOR y",
  "XOR" => "x XOR y",
  "XNOR"=> "x XNOR y",
  "NOT" => "!x",
  
  "ALL_OF_ABOVE" => "andresult := x ∧ y,  nandresult := x ¬∧ y,\n".
                    "orresult  := x ∨ y,  norresult  := x ¬∨ y,\n".
                    "xorresult := x ⊕ y,  xnorresult := x ¬⊕ y,\n".
                    "impresult := x->y,  notx := !x, noty := ~y",

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

  "DEMUX2B" => "out1[1] := in[1] & !sel,\nout2[1] := in[1] & sel,\n".
               "out1[2] := in[2] & !sel,\nout2[2] := in[2] & sel\n",

  "DEMUX4W" => "out1 := in & !sel[1] & !sel[2]".
            ";\nout2 := in &  sel[1] & !sel[2]".
            ";\nout3 := in & !sel[1] &  sel[2]".
            ";\nout4 := in &  sel[1] &  sel[2]",

  "FULLADDER"   => "out := x XOR y XOR carryIn,\n".              "carryOut := (x AND y) OR (carryIn AND NOT out)",
  "FULLADDER2B" => "out[1]   :=  x[1] XOR y[1] XOR carryIn;\n".  "carry[1]  = (x[1] AND y[1]) OR (carryIn  AND NOT out[1]);\n".
                   "out[2]   :=  x[2] XOR y[2] XOR carry[1];\n". "carry[2] := (x[2] AND y[2]) OR (carry[1] AND NOT out[2])",
  "FULLADDER4B" => "out[1]   :=  x[1] XOR y[1] XOR carryIn;\n".  "carry[1]  = (x[1] AND y[1]) OR (carryIn  AND NOT out[1]);\n".
                   "out[2]   :=  x[2] XOR y[2] XOR carry[1];\n". "carry[2]  = (x[2] AND y[2]) OR (carry[1] AND NOT out[2]);\n".
                   "out[3]   :=  x[3] XOR y[3] XOR carry[2];\n". "carry[3]  = (x[3] AND y[3]) OR (carry[2] AND NOT out[3]);\n".
                   "out[4]   :=  x[4] XOR y[4] XOR carry[3];\n". "carry[4] := (x[4] AND y[4]) OR (carry[3] AND NOT out[4])",

  "HALFADDER"   => "carryIn  = 0;\n".
                   "out      := x XOR y XOR carryIn,\n".         "carryOut := (x AND y) OR (carryIn AND NOT out)",
  "HALFADDER2B" => "carryIn  = 0;\n".
                   "out[1]   :=  x[1] XOR y[1] XOR carryIn;\n".  "carry[1]  = (x[1] AND y[1]) OR (carryIn  AND NOT out[1]);\n".
                   "out[2]   :=  x[2] XOR y[2] XOR carry[1];\n". "carryOut := (x[2] AND y[2]) OR (carry[1] AND NOT out[2])",
  "HALFADDER4B" => "carryIn  = 0;\n".
                   "out[1]   :=  x[1] XOR y[1] XOR carryIn;\n".  "carry[1]  = (x[1] AND y[1]) OR (carryIn  AND NOT out[1]);\n".
                   "out[2]   :=  x[2] XOR y[2] XOR carry[1];\n". "carry[2]  = (x[2] AND y[2]) OR (carry[1] AND NOT out[2]);\n".
                   "out[3]   :=  x[3] XOR y[3] XOR carry[2];\n". "carry[3]  = (x[3] AND y[3]) OR (carry[2] AND NOT out[3]);\n".
                   "out[4]   :=  x[4] XOR y[4] XOR carry[3];\n". "carryOut := (x[4] AND y[4]) OR (carry[3] AND NOT out[4])",

  "CARRYLESSADDER2B" => "out[1]   :=  x[1] XOR y[1];\n".              "carry[1]  =  x[1] AND y[1];\n".
                        "out[2]   :=  x[2] XOR y[2] XOR carry[1];\n",
  "CARRYLESSADDER4B" => "out[1]   :=  x[1] XOR y[1];\n".              "carry[1]  =  x[1] AND y[1];\n".
                        "out[2]   :=  x[2] XOR y[2] XOR carry[1];\n". "carry[2]  = (x[2] AND y[2]) OR (carry[1] AND NOT out[2]);\n".
                        "out[3]   :=  x[3] XOR y[3] XOR carry[2];\n". "carry[3]  = (x[3] AND y[3]) OR (carry[2] AND NOT out[3]);\n".
                        "out[4]   :=  x[4] XOR y[4] XOR carry[3];\n"
];
