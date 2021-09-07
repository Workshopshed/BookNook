#include "CameraToEPD.h"

#ifndef BUFFER
#define BUFFER(A, B)     source[get_one_dim_pixel_index(A,B,width)]
#endif


int CameraToEPD::get_one_dim_pixel_index(int col, int row, int row_width)
{
    int pixelIndexA = row * row_width + col;
    return pixelIndexA;
}

void CameraToEPD::Convert(uint8_t *source, Paint dest, int width, int height)
{
      #define offset 30

      dest.SetRotate(ROTATE_180);

      for (int i = 1; i < height-1; i++) {
        for (int j = 1; j < width-1; j++) {
            if (BUFFER(i,j) > 128) {
              dest.DrawPixel(i,j+offset,1);
            }
        }
      }
}

