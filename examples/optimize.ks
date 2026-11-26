# Run with --emit-ir to see LLVM's passes simplify this function:
# (1+2+x)*(x+(1+2)) becomes  t = x + 3.0;  t * t
def test(x) (1+2+x)*(x+(1+2));
test(4);   # 49
