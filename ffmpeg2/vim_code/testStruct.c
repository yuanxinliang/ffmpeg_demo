#include<stdio.h>

struct st {
   int a;
   int b;
};

int main(int argc, char* argv[]) {
   struct st s1;
   s1.a = 10;
   s1.b = 20;
   printf("struct content is: %d, %d\n", s1.a, s1.b);
   return 0;
}
