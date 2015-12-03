int f(int x){
  return x + 1;
}

int g(int x, int y){
  int ret;
  if (x <= y) ret = x + y;
  else ret = x - y;
  return ret;
}
