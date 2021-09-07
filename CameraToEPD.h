#ifndef CAMERATOEPD_H
#define CAMERATOEPD_H

#include "epdpaint.h"

class CameraToEPD
{
    private:
        int get_one_dim_pixel_index(int col, int row, int row_width);
    public:
        void Convert(uint8_t *source, Paint dest, int width, int height);
};

#endif // CAMERATOEPD_H