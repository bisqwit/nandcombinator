<?php
require_once 'base64.php';

if(!empty($_SERVER['REMOTE_ADDR'])) exit;

$num_inputs  = 2;
$num_outputs = 1;
#$input = '4 G ABBCACDEH';
#$input = '4 B BBAACDEEH';
$input = '5 J BBABADCEDFI';

#$num_inputs = 4;
#$num_outputs = 4;
#$input = '6 P8y/iMzMAoK ADEEBBFGDHCHLKIH';

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
  if($code < $num_inputs) { $gate_from[$n>>1][] = 'x'.($code); }
  else                    { $gate_from[$n>>1][] = 'g'.($code-$num_inputs); }
}
for($n=0; $n<$num_outputs; ++$n)
{
  $code = $connections->Get(5);
  $out_from[$n] = "g$code";
}

$used_sources = Array();
foreach($gate_from as $n=>$inputs) foreach($inputs as $i) $used_sources[$i] = $i;
foreach($out_from as $i) $used_sources[$i] = $i;

ob_start();

print <<<EOF
digraph {
rankdir=LR;
node[fontsize=10, fontname="Helvetica", shape=rect];
edge[fontsize=10, fontname="Helvetica"];

EOF;

// Print nodes
for($i=0; $i<$num_inputs; ++$i)
{
  if(isset($used_sources["x{$i}"])) print "  x$i [shape=square];\n";
}
if(isset($used_sources['0'])) print "  zero [shape=square];\n";
if(isset($used_sources['1'])) print "  one [shape=square];\n";

// Print gates
for($i=0; $i<$num_gates; ++$i)
{
  $x = $topo[$i][0];
  $y = $topo[$i][1];
  print "  g$i;\n";
}

// Print connections
foreach($gate_from as $n => $inputs)
{
  foreach($inputs as $input_no => $input)
  {
    $num = (int)substr($input,1);

    if($input[0] == '0')     print "  zero->g{$n};\n";
    elseif($input[0] == '1') print "  one->g{$n};\n";
    elseif($input[0] == 'x') print "  x{$num}->g{$n};\n";
    else                     print "  g{$num}->g{$n};\n";

    if(isset($not_gates[$n])) break; // For NOT gates, only print one input
  }
}
foreach($out_from as $n=>$input)
{
  $num = (int)substr($input,1);

  print "f{$n} [shape=point];\n";
  if($input[0] == '0')     print "  zero->f{$n};\n";
  elseif($input[0] == '1') print "  one->f{$n};\n";
  elseif($input[0] == 'x') print "  x{$num}->f{$n};\n";
  else                     print "  g{$num}->f{$n};\n";
}
print <<<EOF

}

EOF;

function ShellFix($s)
{
  return "'".str_replace("'", "'\''", $s)."'";
}
$dot_src = ob_get_clean();
exec("echo ".Shellfix($dot_src)."| /usr/bin/dot -Tsvg /dev/stdin", $lines);
foreach($lines as $line)
{
  if(preg_match('/-- (.*) --/', $line, $mat))
    $label = $mat[1];
  elseif(preg_match('/ellipse.*cx="(.*)" cy="(.*)" rx/',$line, $mat))
    $coordinates[$label] = Array($mat[1], $mat[2]);
  elseif(preg_match('@polygon.*black.*points="(.*)"/@',$line,$mat))
  {
    $x=0;$y=0;$n=0;
    foreach(explode(' ', $mat[1]) as $c)
    {
      #var_dump($c);
      $c = explode(',', $c);
      $x += $c[0]; $y += $c[1]; ++$n;
    }
    $coordinates[$label] = Array($x/$n,$y/$n);
  }
}
$coordinates['0'] = @$coordinates['zero'];
$coordinates['1'] = @$coordinates['one'];

// Transform all NANDs with two identical inputs into NOT gates
$not_gates = Array();
for($n=0; $n<$num_gates; ++$n)
  if($gate_from[$n][0] == $gate_from[$n][1])
    $not_gates[$n] = $n;


print <<<EOF
\\documentclass[border=3mm]{standalone}
\\usepackage{tikz}
\\usetikzlibrary{arrows,shapes.gates.logic.US,shapes.gates.logic.IEC,calc}
\\begin{document}
\\thispagestyle{empty}
\\tikzstyle{branch}=[fill,shape=circle,minimum size=3pt,inner sep=0pt]
\\begin{tikzpicture}[label distance=2mm]

EOF;

$xscale = 40;
$yscale = 33;

// Print nodes
for($i=0; $i<$num_inputs; ++$i)
{
  $c = @$coordinates["x$i"]; $x = @$c[0]/$xscale; $y = @$c[1]/$yscale;
  if(isset($used_sources["x{$i}"])) print "  \\node (x$i) at ($x,$y) {\$x_{$i}\$};\n";
}
if(isset($used_sources['0']))
{
  $c = @$coordinates["zero"]; $x = @$c[0]/$xscale; $y = @$c[1]/$yscale;
  print "  \\node (zero) at ($x,$y) {0};\n";
}
if(isset($used_sources['1']))
{
  $c = @$coordinates["one"]; $x = @$c[0]/$xscale; $y = @$c[1]/$yscale;
  print "  \\node (one) at ($x,$y) {1};\n";
}

// Print gates
for($i=0; $i<$num_gates; ++$i)
{
  $c = @$coordinates["g$i"]; $x = @$c[0]/$xscale; $y = @$c[1]/$yscale;
  if(isset($not_gates[$i]))
    print "  \\node[not  gate US, draw]                                        at ($x,$y) (g$i) {};\n";
  else
    print "  \\node[nand gate US, draw, logic gate inputs=nn, anchor=input 1]  at ($x,$y) (g$i) {};\n";
}

// Print connections
$uses = Array();
/*uksort($gate_from, function($a,$b)
                   {
                     global $topo;
                     return $topo[$a][1] - $topo[$b][1];
                   });*/
foreach($gate_from as $n => $inputs)
{
  $shift = 0.25;
  #$prevx = 0;
  foreach($inputs as $input_no => $input)
  {
    $num = (int)substr($input,1);

    if(isset($not_gates[$n]))
      $in = "g{$n}.input";
    else
    {
      if($coordinates[$inputs[1]][1] > $coordinates[$inputs[0]][1])
      {
        $input_no ^= 1;
      }
      ++$input_no;

      $in = "g{$n}.input {$input_no}";
    }

    if(1)
    {
      if(0&&isset($uses[$input]))
      {
        if($input[0] == '0')     print "  \\draw (zero -| $in) node[branch] {} |- ([xshift=-0.5cm]$in) -- ($in);\n";
        elseif($input[0] == '1') print "  \\draw (one -| $in) node[branch] {} |- ([xshift=-0.5cm]$in) -- ($in);\n";
        elseif($input[0] == 'x') print "  \\draw (x{$num} -| $in) node[branch] {} |- ([xshift=-0.5cm]$in) |- ($in);\n";
        else                     print "  \\draw (g{$num}.output) -- ([xshift={$shift}cm]g{$num}.output) |- ([xshift=-0.5cm]$in) -- ($in);\n";
      }
      else
      {
        if($input[0] == '0')     print "  \\draw (zero) -| ([xshift=-0.5cm]$in) -- ($in);\n";
        elseif($input[0] == '1') print "  \\draw (one) -| ([xshift=-0.5cm]$in) -- ($in);\n";
        elseif($input[0] == 'x') print "  \\draw (x{$num}) -| ([xshift=-0.5cm]$in) -- ($in);\n";
        else                     print "  \\draw (g{$num}.output) -- ([xshift={$shift}cm]g{$num}.output) |- ([xshift=-0.5cm]$in) -- ($in);\n";
      }
    }
    else
    {
      if($input[0] == '0')     print "  \\draw (zero) -- ($in);\n";
      elseif($input[0] == '1') print "  \\draw (one) -- ($in);\n";
      elseif($input[0] == 'x') print "  \\draw (x{$num}) -- ($in);\n";
      else                     print "  \\draw (g{$num}.output) -- ([xshift={$shift}cm]g{$num}.output) -- ($in);\n";
    }

    /*if($input[0] == 'g')
    {
      $shift += 0.2;
      #if($topo[$num][0] == $prevx) $shift += 0.2; else $shift = 0.5;
      $prevx = $topo[$num][0];
    }*/
    
    @++$uses[$input];

    if(isset($not_gates[$n])) break; // For NOT gates, only print one input
  }
}
foreach($out_from as $n=>$input)
{
  $num = (int)substr($input,1);

  if($input[0] == '0')     print "  \\draw (zero -| $in) node[branch] {} -- ($in); node[above] {\$f_$n\$};\n";
  elseif($input[0] == '1') print "  \\draw (one -| $in) node[branch] {} -- ($in); node[above] {\$f_$n\$};\n";
  elseif($input[0] == 'x') print "  \\draw (x{$num} -| $in) node[branch] {} -- ($in); node[above] {\$f_$n\$};\n";
  else                     print "  \\draw (g{$num}.output) -- ([xshift=0.5cm]g{$num}.output) node[above] {\$f_$n\$};\n";
}



print <<<EOF
\\end{tikzpicture}
\\end{document} 

EOF;
