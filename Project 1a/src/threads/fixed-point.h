#define F (1 << 14)
#define INT_MAX ((1 << 31) - 1)
#define INT_MIN (-(1 << 31))

/* pintos dokumani reference olarak alınmıstır.*/

int addi(int x, int y);
int add_mixed(int x, int n);
int subi(int x, int y);
int sub_mixed(int x, int n);
int integer_to_float(int n);
int float_to_int(int x);
int float_to_int_r(int x);
int multiply_float(int x, int y);
int multiply_mixed(int x, int y);
int division_float(int x, int y);
int division_mixed(int x, int n);

int integer_to_float(int n){
  return n * F;
}

int float_to_int_r(int x){
  if (x < 0)
    return (x - F / 2) / F;
  return (x + F / 2) / F;
}

int float_to_int(int x){
  return x / F;
}

int addi(int x, int y){
  return x + y;
}

int subi(int x, int y){
  return x - y;
}

// add two integer around by n 
int add_mixed(int x, int n){
  return x + integer_to_float(n);
}
// sub x around by convert to float n
int sub_mixed(int x, int n){
  return x - integer_to_float(n);
}
// multiply  x and y around F 
int multiply_float(int x, int y){
  return ((int64_t) x) * y / F;
}

// multiply x and n
int multiply_mixed(int x, int n){
  return x * n;
}

// division x and y around by F
int division_float(int x, int y){
  return ((int64_t) x) * F / y;
}

// division x and n 
int division_mixed(int x, int n){
  return x / n;
}