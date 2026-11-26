# for loops + calling C functions from the JIT-compiled code
extern putchard(char);
extern printd(x);

# Prints `n` stars followed by a newline
def stars(n)
  (for i = 0, i < n in putchard(42)) + putchard(10);

stars(5);

# Prints 1..n: the loop variable lives in a PHI node
def count(n)
  for i = 1, i < n + 1 in printd(i);

count(3);
