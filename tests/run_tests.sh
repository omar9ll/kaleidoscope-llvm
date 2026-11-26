#!/usr/bin/env bash
# Runs the examples through the JIT and checks the output.
set -u
cd "$(dirname "$0")/.."
BIN=./build/kaleido
pass=0; fail=0

check() {  # name, expected, command...
    local name="$1" expected="$2"; shift 2
    local got; got="$("$@" 2>&1)"
    if [[ "$got" == "$expected" ]]; then pass=$((pass+1)); echo "PASS  $name"
    else fail=$((fail+1)); echo "FAIL  $name"; echo "  expected: $expected"; echo "  got:      $got"; fi
}

check "fibonacci"        $'=> 55\n=> 6765'                         $BIN examples/fib.ks
check "loops + externs"  $'*****\n=> 0\n1\n2\n3\n=> 0'             $BIN examples/loops.ks
check "optimized result" '=> 49'                                   $BIN examples/optimize.ks
check "unoptimized result" '=> 49'                                 $BIN --no-opt examples/optimize.ks

# The optimizer should merge the two (1+2+x) / (x+(1+2)) additions into one.
adds_opt=$($BIN --emit-ir examples/optimize.ks | grep -c 'fadd')
adds_raw=$($BIN --no-opt --emit-ir examples/optimize.ks | grep -c 'fadd')
check "GVN removes duplicate add" "1 2" echo "$adds_opt $adds_raw"

tmp=$(mktemp -d)
echo 'def f(x) y;' > "$tmp/bad.ks"
check "unknown variable" "error: unknown variable name 'y'" $BIN "$tmp/bad.ks"
echo 'nope(1);' > "$tmp/bad2.ks"
check "unknown function" "error: unknown function 'nope'" $BIN "$tmp/bad2.ks"
rm -rf "$tmp"

echo; echo "$pass passed, $fail failed"
[[ $fail -eq 0 ]]
