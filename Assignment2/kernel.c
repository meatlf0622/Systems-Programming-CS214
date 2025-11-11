#include <stdio.h>
#include <math.h>
#include <stdint.h>
void convert_to_YCrCb(unsigned char *rgb_pixels, unsigned char *ycc_pixels, int width, int height){
    if (width <= 0 || height <= 0 || rgb_pixels == NULL || ycc_pixels == NULL) {
       return; 
   }
   int totalPixels  = width * height * 3;


   for(int i = 0; i< totalPixels; i +=3) {
       unsigned char r = rgb_pixels[i];
       unsigned char g = rgb_pixels[i+1];
       unsigned char b = rgb_pixels[i+2];


      int y  = round(0.299 * r + 0.587 * g + 0.114 * b);
      int cb = round(128 - 0.168736 * r - 0.331264 * g + 0.5 * b);
    int cr = round(128 + 0.5 * r - 0.418688 * g - 0.081312 * b);

  
       if (y < 0) y = 0;
       if (y > 255) y = 255;
       if (cb < 0) cb = 0;
       if (cb > 255) cb = 255;
       if (cr < 0) cr = 0;
       if (cr > 255) cr = 255;


       ycc_pixels[i] = (unsigned char)y;
       ycc_pixels[i + 1] = (unsigned char)cb;
       ycc_pixels[i + 2] = (unsigned char)cr;


   }
}
