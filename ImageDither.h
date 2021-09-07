
#ifndef IMAGE_DITHER_H
#define IMAGE_DITHER_H

class ImageDither
{
    private:
        int get_one_dim_pixel_index(int col, int row, int row_width);
        uint8_t find_closest_palette_color(uint8_t pixel);

    public:
        void dither_image(uint8_t *source, int source_width, int source_height);
};

#endif // IMAGE_DITHER_H