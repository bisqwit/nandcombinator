<?php
require_once 'base64.php';

if(!empty($_SERVER['REMOTE_ADDR'])) exit;

$num_inputs  = 2;
$num_outputs = 1;
#$input = '4 G ABBCACDEH';
$input = '4 B BBAACDEEH';

$tokens = explode(' ', $input);
$num_gates   = (int)$tokens[0];
$truthtable  = new BASE64decoder($tokens[1]);
$connections = new BASE64decoder($tokens[2]);

// Parse connections
$gate_from    = Array();
$out_from     = Array();

for($n=0; $n<$num_gates*2; ++$n)
{
  $code = $connections->Get(6);
  if($code < $num_inputs) { $gate_from[$n>>1][] = 'i'.($code); }
  else                    { $gate_from[$n>>1][] = 'g'.($code-$num_inputs); }
}
for($n=0; $n<$num_outputs; ++$n)
{
  $code = $connections->Get(5);
  $out_from[$n] = "g$code";
}

// Transform all NANDs with two identical inputs into NOT gates
$not_gates = Array();
for($n=0; $n<$num_gates; ++$n)
  if($gate_from[$n][0] == $gate_from[$n][1])
    $not_gates[$n] = $n;

// Create topology
$available = Array();
$topo      = Array();
$round     = $num_inputs + 3;
while(count($topo) < $num_gates)
{
  $newtopo = $topo;
  $line = 1;
  for($n=0; $n<$num_gates; ++$n)
    if(!isset($topo[$n]))
    {
      $ok = 0;
      $first = null;
      foreach($gate_from[$n] as $input)
        if($input[0] == '0' || $input[0] == '1' || $input[0] == 'i')
        {
          ++$ok;
        }
        elseif($input[0] == 'g')
        {
          $s = (int)substr($input,1);
          if(isset($topo[$s]))
          {
            ++$ok;
            if($first === null || $topo[$s][1] < $first) $first = $topo[$s][1];
          }
        }
      if($ok == 2)
      {
        $newtopo[$n] = Array($round, -($n+1));//-max($line, (int)$first));
        #$newtopo[$n] = Array($num_inputs+3+$n, -($n+1));//-max($line, (int)$first));
        ++$line;
      }
    }
  $round += 1.5; //(count($newtopo) - count($topo)) * 2;
  $topo = $newtopo;
}

print <<<EOF
\\documentclass[border=3mm]{standalone}
\\usepackage{tikz}
\\usetikzlibrary{arrows,shapes.gates.logic.US,shapes.gates.logic.IEC,calc}
\\begin{document}
\\thispagestyle{empty}
\\tikzstyle{branch}=[fill,shape=circle,minimum size=3pt,inner sep=0pt]
\\begin{tikzpicture}[label distance=2mm]

EOF;

// Print nodes
for($i=0; $i<$num_inputs; ++$i)
{
  print "  \\node (x$i) at ($i,0) {\$x_{$i}\$};\n";
}
print "  \\node (zero) at ($i,0) {0};\n"; ++$i;
print "  \\node (one) at ($i,0) {1};\n"; ++$i;

// Print gates
for($i=0; $i<$num_gates; ++$i)
{
  $x = $topo[$i][0];
  $y = $topo[$i][1];
  if(isset($not_gates[$i]))
    print "  \\node[not  gate US, draw]                                        at ($x,$y) (g$i) {};\n";
  else
    print "  \\node[nand gate US, draw, logic gate inputs=nn, anchor=input 1]  at ($x,$y) (g$i) {};\n";
}

// Print connections
$uses = Array();
uksort($gate_from, function($a,$b)
                   {
                     global $topo;
                     return $topo[$a][1] - $topo[$b][1];
                   });
foreach($gate_from as $n => $inputs)
{
  $shift = 0.25;
  $prevx = 0;
  foreach($inputs as $input_no => $input)
  {
    $num = (int)substr($input,1);
    ++$input_no;
    if(isset($not_gates[$n]))
      $in = "g{$n}.input";
    else
      $in = "g{$n}.input {$input_no}";

    if(isset($uses[$input]))
    {
      if($input[0] == '0')     print "  \\draw (zero |- $in) node[branch] {} -- ($in);\n";
      elseif($input[0] == '1') print "  \\draw (one |- $in) node[branch] {} -- ($in);\n";
      elseif($input[0] == 'i') print "  \\draw (x{$num} |- $in) node[branch] {} -- ($in);\n";
      else                     print "  \\draw (g{$num}.output) -- ([xshift={$shift}cm]g{$num}.output) |- ($in);\n";
    }
    else
    {
      if($input[0] == '0')     print "  \\draw (zero) |- ($in);\n";
      elseif($input[0] == '1') print "  \\draw (one) |- ($in);\n";
      elseif($input[0] == 'i') print "  \\draw (x{$num}) |- ($in);\n";
      else                     print "  \\draw (g{$num}.output) -- ([xshift={$shift}cm]g{$num}.output) |- ($in);\n";
    }

    if($input[0] == 'g')
    {
      $shift += 0.2;
      #if($topo[$num][0] == $prevx) $shift += 0.2; else $shift = 0.5;
      $prevx = $topo[$num][0];
    }
    
    @++$uses[$input];

    if(isset($not_gates[$n])) break; // For NOT gates, only print one input
  }
}
foreach($out_from as $n=>$input)
{
  $num = (int)substr($input,1);

  if($input[0] == '0')     print "  \\draw (zero |- $in) node[branch] {} -- ($in); node[above] {\$f_$n\$};\n";
  elseif($input[0] == '1') print "  \\draw (one |- $in) node[branch] {} -- ($in); node[above] {\$f_$n\$};\n";
  elseif($input[0] == 'i') print "  \\draw (x{$num} |- $in) node[branch] {} -- ($in) node[above] {\$f_$n\$};\n";
  else                     print "  \\draw (g{$num}.output) -- ([xshift=0.5cm]g{$num}.output) node[above] {\$f_$n\$};\n";
}



print <<<EOF
\\end{tikzpicture}
\\end{document} 

EOF;
