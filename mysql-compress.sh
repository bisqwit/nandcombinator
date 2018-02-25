if [ -d "$1" ]; then
  if cd "$1"; then
    for s in conundrum_*.MYI;do myisampack $s;myisamchk -rq $s;done
  fi
else
  echo "Usage: mysql-compress <path-to-myisam-files>"
fi
