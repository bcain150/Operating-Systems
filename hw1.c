//open iomem
//open ioimports
//taking in a single command line arg hexidecimal address
//display all hexidecimall adresses and their peripherals


//#define iomemAdd '/proc/iomem'
//#define ioportsAdd '/proc/ioports'

#define HEX argv[1]
#define _POSIX_SOURCE

#include <features.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>



bool isHex(char num);
bool isValid(const char * hex, int len);

int main(int argc, char * argv[]) {
  //ensures that there is only one address given
  if(argc == 2) {
    FILE *mem;
    FILE *ports;

    int m = open("/proc/iomem", O_RDONLY);
    int p = open("/proc/ioports", O_RDONLY);

    mem = fdopen(m, "r");
    ports = fdopen(m, "r");

    
  }
  
  else if(argc==1)
    printf("Not enough arguments given\n");
  else
    printf("Too many arguments given.\n");
  return 0;
}


//converts a hex char to a decimal int
bool isHex(char num) {
  if(num == '0') return true;
  if(num == '1') return true;
  if(num == '2') return true;
  if(num == '3') return true;
  if(num == '4') return true;
  if(num == '5') return true;
  if(num == '6') return true;
  if(num == '7') return true;
  if(num == '8') return true;
  if(num == '9') return true;
  if(num == 'a' || num == 'A') return true;
  if(num == 'b' || num == 'B') return true;
  if(num == 'c' || num == 'C') return true;
  if(num == 'd' || num == 'D') return true;
  if(num == 'e' || num == 'E') return true;
  if(num == 'f' || num == 'F') return true;
  return false;
}

bool isValid(const char * hex, int len) {

  for(int i=0; i<len && hex[i] != '\0'; i++) {
    if(!isHex(hex[i])) {
      if(i!=1) return false;
      else if(hex[i] != 'x') return false;
    }
  }
  return true;
}
