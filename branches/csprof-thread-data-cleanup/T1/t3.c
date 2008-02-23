#define LIMIT_OUTER 10000
#define LIMIT 100000

int main(int argc, char *argv[]){
  double x,y;
  int i,j;

  x = 1.0;
  for (i=0; i < LIMIT_OUTER; i++){
    for (j=0; j < LIMIT; j++){
      x += 1.0;
    }
  }
  return 0;
}


