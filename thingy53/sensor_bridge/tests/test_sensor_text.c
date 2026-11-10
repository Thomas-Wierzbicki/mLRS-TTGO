#include <assert.h>
#include <string.h>
#include "../src/sensor_text.h"
int main(void) {
 char b[80];
 sensor_text(b,sizeof b,22400000,48000000,101325000);
 assert(!strcmp(b,"\nT:22.4C H:48% P:1013hPa\n"));
 sensor_text(b,sizeof b,-400000,99500000,100000000);
 assert(!strcmp(b,"\nT:-0.4C H:100% P:1000hPa\n"));
 sensor_text(b,sizeof b,-1250000,0,30000000);
 assert(!strcmp(b,"\nT:-1.3C H:0% P:300hPa\n"));
 sensor_text(b,sizeof b,-40000,0,100000000);
 assert(!strcmp(b,"\nT:0.0C H:0% P:1000hPa\n"));
 assert(sensor_text(b,4,22400000,48000000,101325000)>=4);
 return 0;
}
