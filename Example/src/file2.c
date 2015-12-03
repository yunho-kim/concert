int f(int x);
int g(int x, int y);
int h(int x){
  return g(f(x), x);
}
