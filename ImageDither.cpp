#include <stdio.h>
#include <math.h>
#include "ImageDither.h"

#ifndef BUFFER
#define BUFFER(A, B)     source[get_one_dim_pixel_index(A,B,source_width)]
#endif


int ImageDither::get_one_dim_pixel_index(int col, int row, int row_width)
{
    int pixelIndexA = row * row_width + col;
    return pixelIndexA;
}

uint8_t ImageDither::find_closest_palette_color(uint8_t pixel)
{
    return pixel > 128 ? 255 : 0;
}

void ImageDither::dither_image(uint8_t *source, int source_width, int source_height)
{
    //Floyd-Steinberg dithering
    //Ref https://gist.github.com/JEFworks/637308c2a1dd8a6faff7b6264104847a
      for (int i = 1; i < source_height-1; i++) {
        for (int j = 1; j < source_width-1; j++) {
            uint8_t P = find_closest_palette_color(BUFFER(i,j));
            int e = BUFFER(i,j) - P;
            BUFFER(i,j) = P;
            BUFFER(i,j+1) = BUFFER(i,j+1) + (e * 7/16);
            BUFFER(i+1,j-1) = BUFFER(i+1,j-1) + (e * 3/16);
            BUFFER(i+1,j) = BUFFER(i+1,j) + (e * 5/16);
            BUFFER(i+1,j+1) = BUFFER(i+1,j+1) + (e * 1/16);
        }
    }

}
