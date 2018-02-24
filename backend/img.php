<?php

require_once 'base64.php';
require_once 'solver.php';
require_once 'smallcache.php';

define('NAME_REGEX', '[A-Za-z_][A-Za-z_0-9]*(?:\[[0-9]+\])?');
define('PORT_REGEX', '[A-Za-z_][A-Za-z_0-9]*(?:\[[0-9]+\])?(?:\\.'.NAME_REGEX.')?');

$key = '';
if(isset($argv))
  $key = $argv[1];
elseif(strlen($_SERVER['PATH_INFO']) > 1)
  $key = substr($_SERVER['PATH_INFO'], 1);

$key = preg_replace('/\\..*/',          '', $key);
$key = preg_replace('@[^A-Za-z0-9+/]@', '', $key);

$tokens      = Array();
$connections = Array();
preg_match_all('/('.PORT_REGEX.')=('.NAME_REGEX.')/', urldecode($_SERVER['QUERY_STRING']), $mat);
foreach($mat[1] as $k=>$v)
{
  $k = preg_replace('/\\..*/', '', $k);
  $v = preg_replace('/\\..*/', '', $v);

  $k = $mat[2][$k];
  $tokens[$k] = $k;
  $tokens[$v] = $v;
  $connections[$v][] = $k;
}

$pagetime = filemtime('img.php');
if(isset($_SERVER['HTTP_IF_MODIFIED_SINCE']))
  if(strtotime($_SERVER['HTTP_IF_MODIFIED_SINCE']) >= $pagetime)
  {
    header('HTTP/1.0 304 Not modified indeed');
    print 'You should use the version you have cached. ';
    print 'Why I am talking English with a web browser?';
    exit;
  }

$cache_key = serialize($key).serialize($connections).$pagetime;
$cache = new BinKeyCache($cache_key);
$cache_data = $cache->LoadOrLog();
if($cache_data !== false)
{
  header('Content-type: image/png');
  header('Content-length: '.strlen($cache_data));
  header('Last-Modified: '.gmdate('D, d M Y H:i:s', $pagetime).' GMT');
  header('Expires: Tue, Jan 19 2038 05:00:00 GMT');
  header('ETag: '.md5($cache_key));
  print $cache_data;
  return;
}

$s = new Solver($key);
$s->Solve();
$num_inputs  = $s->num_inputs;
$num_outputs = $s->num_outputs;
$num_gates   = $s->num_gates;

ob_start();

print <<<EOF
digraph {
rankdir=LR;
node[fontsize=10, fontname="Helvetica", shape=rect];
edge[fontsize=10, fontname="Helvetica"];

EOF;

// Print nodes
foreach($tokens as $token)
{
  if(preg_match('/^g[0-9]+$/', $token))
    print "  \"$token\";\n";
  else
    print "  \"$token\" [shape=square];\n";
}

// Print connections
foreach($connections as $to => $inputs)
{
  $notgate = count($inputs) > 1 && ($inputs[0] == $inputs[1]);
  foreach($inputs as $source)
  {
    print "  \"{$source}\"->\"{$to}\";\n";
    if($notgate) break; // For NOT gates, only print one input
  }
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

ob_start();
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
$tag = Array();
$counter = 0;
foreach($tokens as $token)
{
  $tag[$token] = 'n'.$counter++;
  $thistag = $tag[$token];

  $label = $token;
  $label = preg_replace('/\[(.*)\]/', '_$1', $label);
  
  $c = @$coordinates[$token];
  $x = @$c[0]/$xscale;
  $y = @$c[1]/$yscale;

  if(preg_match('/^g[0-9]+$/', $token))
  {
    $notgate = $connections[$token][0] == $connections[$token][1];
    $glabel = 'g_'.substr($token,1);
    #$glabel = $token;
    if($notgate)
      print "  \\node[not  gate US, draw]                                        at ($x,$y) ($thistag) {\${$glabel}\$};\n";
    else
      print "  \\node[nand gate US, draw, logic gate inputs=nn, anchor=input 1]  at ($x,$y) ($thistag) {\${$glabel}\$};\n";
  }
  else
  {
    print "  \\node ($thistag) at ($x,$y) {\${$label}\$};\n";
  }
}

// Print connections
$uses = Array();
foreach($connections as $to => $inputs)
{
  #$shift = rand(25,130)/100;//0.25;
  $shift = 0.25;

  $target_is_gate    = count($inputs) > 1;
  $target_is_notgate = $target_is_gate && $inputs[0] == $inputs[1];

  foreach($inputs as $input_no => $input)
  {
    $source_is_gate    = preg_match('/^g[0-9]+$/', $input);
    $source_is_notgate = $source_is_gate && $connections[$input][0] == $connections[$input][1];

    $num = (int)substr($input,1);

    if($source_is_gate)
    {
      $in = $tag[$input].'.output';
    }
    else
    {
      $in = $tag[$input];
    }
    
    if($target_is_notgate)
    {
      $out = $tag[$to].'.input';
    }
    elseif($target_is_gate)
    {
      if($coordinates[$inputs[1]][1] > $coordinates[$inputs[0]][1])
      {
        $input_no ^= 1;
      }
      ++$input_no;

      $out = $tag[$to].".input {$input_no}";
    }
    else
    {
      $out = $tag[$to];
    }
    
    $negshift = 0.4;//rand(40,60)/100;//0.25;
    if(!$source_is_gate)
    {
      $negshift += 1;
      print "  \\draw ($in) -| ([xshift=-{$negshift}cm]$out) -- ($out);\n";
    }
    else
    {
      print "  \\draw ($in) -- ([xshift={$shift}cm]$in) |- ([xshift=-{$negshift}cm]$out) -- ($out);\n";
    }

    if($source_is_gate == 'g')
    {
      #$shift += 0.2;
      #if($topo[$num][0] == $prevx) $shift += 0.2; else $shift = 0.5;
      #$prevx = $topo[$num][0];
    }
    
    @++$uses[$input];

    if($target_is_notgate) break; // For NOT gates, only print one input
  }
}
/*
foreach($out_from as $n=>$input)
{
  $num = (int)substr($input,1);

  if($input[0] == '0')     print "  \\draw (zero -| $in) node[branch] {} -- ($in); node[above] {\$f_$n\$};\n";
  elseif($input[0] == '1') print "  \\draw (one -| $in) node[branch] {} -- ($in); node[above] {\$f_$n\$};\n";
  elseif($input[0] == 'x') print "  \\draw (x{$num} -| $in) node[branch] {} -- ($in); node[above] {\$f_$n\$};\n";
  else                     print "  \\draw (g{$num}.output) -- ([xshift=0.5cm]g{$num}.output) node[above] {\$f_$n\$};\n";
}
*/



print <<<EOF
\\end{tikzpicture}
\\end{document} 

EOF;

$outdir = '/tmp';
$prefix = 'kururu'.getmypid();

$pngfn = "{$outdir}/{$prefix}.png";
$texfn = "{$outdir}/{$prefix}.tex";
$dvifn = "{$outdir}/{$prefix}.dvi";
$psfn  = "{$outdir}/{$prefix}.ps";
$auxfn = "{$outdir}/{$prefix}.aux";
$logfn = "{$outdir}/{$prefix}.log";
file_put_contents($texfn, ob_get_clean());

#print '<pre>';passthru
exec("cd $outdir; latex $texfn -output-directory=$outdir &>/dev/null");
@unlink($texfn);
@unlink($logfn);

exec("cd $outdir; dvips {$dvifn} &>/dev/null");

file_put_contents($auxfn,
  "/setpagedevice {pop} bind 1 index where {dup wcheck {3 1 roll put} {pop def} ifelse} {def} ifelse\n".
  "<</UseCIEColor true>>setpagedevice\n".
  "-0 -0 translate\n");

exec("cd $outdir; gs -sstdout=%stderr -dQUIET -dSAFER -dBATCH -dNOPAUSE -dNOPROMPT ".
    "-dMaxBitmap=500000000 -dAlignToPixels=0 -dGridFitTT=2 -sDEVICE=png48 ".
    "-dTextAlphaBits=4 -dGraphicsAlphaBits=4 -r125 ".
    "-sOutputFile=$pngfn -f$psfn -f$auxfn &>/dev/null");

@unlink($dvifn);
@unlink($psfn);
@unlink($logfn);
@unlink($auxfn);

header('Content-type: image/png');
header('Content-length: '.filesize($pngfn));
header('Last-Modified: '.gmdate('D, d M Y H:i:s', $pagetime).' GMT');
header('Expires: Tue, Jan 19 2038 05:00:00 GMT');
header('ETag: '.md5($cache_key));

readfile($pngfn);

$cache->Save(ob_get_contents());
