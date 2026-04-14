#include <cstdio>
#include <cmath>
#include <vector>
#include <algorithm>
#include <cstdint>

#pragma pack(push, 1)
struct BMPHeader {
    uint16_t type;
    uint32_t size;
    uint16_t reserved1;
    uint16_t reserved2;
    uint32_t offset;
};

struct DIBHeader {
    uint32_t size;
    int32_t  width;
    int32_t  height;
    uint16_t planes;
    uint16_t bits_per_pixel;
    uint32_t compression;
    uint32_t image_size;
    int32_t  x_ppm;
    int32_t  y_ppm;
    uint32_t colors_used;
    uint32_t important_colors;
};
#pragma pack(pop)

struct RGB {
    uint8_t b, g, r;

    RGB() : b(0), g(0), r(0) {}
    RGB(uint8_t blue, uint8_t green, uint8_t red) : b(blue), g(green), r(red) {}
};

class Image {
public:
    int width, height;
    std::vector<RGB> data;

    Image() : width(0), height(0) {}

    bool loadBMP(const char* filename) {
        FILE* f = fopen(filename, "rb");
        if (!f) return false;

        BMPHeader bmp;
        DIBHeader dib;

        fread(&bmp, sizeof(BMPHeader), 1, f);
        fread(&dib, sizeof(DIBHeader), 1, f);

        if (bmp.type != 0x4D42 || dib.bits_per_pixel != 24) {
            fclose(f);
            return false;
        }

        width = dib.width;
        height = (dib.height > 0) ? dib.height : -dib.height;
        data.resize(width * height);

        int row_size = ((width * 3 + 3) / 4) * 4;
        std::vector<uint8_t> row(row_size);

        for (int y = 0; y < height; y++) {
            fread(row.data(), 1, row_size, f);
            int dest_y = (dib.height > 0) ? (height - 1 - y) : y;
            for (int x = 0; x < width; x++) {
                RGB& pixel = data[dest_y * width + x];
                pixel.b = row[x * 3];
                pixel.g = row[x * 3 + 1];
                pixel.r = row[x * 3 + 2];
            }
        }

        fclose(f);
        return true;
    }

    bool saveBMP(const char* filename) const {
        FILE* f = fopen(filename, "wb");
        if (!f) return false;

        BMPHeader bmp;
        DIBHeader dib;

        int row_size = ((width * 3 + 3) / 4) * 4;
        int image_size = row_size * height;

        bmp.type = 0x4D42;
        bmp.size = 54 + image_size;
        bmp.reserved1 = 0;
        bmp.reserved2 = 0;
        bmp.offset = 54;

        dib.size = 40;
        dib.width = width;
        dib.height = height;
        dib.planes = 1;
        dib.bits_per_pixel = 24;
        dib.compression = 0;
        dib.image_size = image_size;
        dib.x_ppm = 0;
        dib.y_ppm = 0;
        dib.colors_used = 0;
        dib.important_colors = 0;

        fwrite(&bmp, sizeof(BMPHeader), 1, f);
        fwrite(&dib, sizeof(DIBHeader), 1, f);

        std::vector<uint8_t> row(row_size, 0);
        for (int y = 0; y < height; y++) {
            int src_y = y;
            for (int x = 0; x < width; x++) {
                const RGB& pixel = data[src_y * width + x];
                row[x * 3] = pixel.b;
                row[x * 3 + 1] = pixel.g;
                row[x * 3 + 2] = pixel.r;
            }
            fwrite(row.data(), 1, row_size, f);
        }

        fclose(f);
        return true;
    }

    RGB getPixel(double x, double y) const {
        int ix = (int)std::floor(x);
        int iy = (int)std::floor(y);

        if (ix < 0 || ix >= width || iy < 0 || iy >= height) {
            return RGB(0, 0, 0);
        }

        double fx = x - ix;
        double fy = y - iy;

        RGB p00 = data[iy * width + ix];

        RGB p10 = (ix + 1 < width) ? data[iy * width + (ix + 1)] : p00;
        RGB p01 = (iy + 1 < height) ? data[(iy + 1) * width + ix] : p00;
        RGB p11 = (ix + 1 < width && iy + 1 < height) ? data[(iy + 1) * width + (ix + 1)] : p00;

        auto lerp = [](uint8_t a, uint8_t b, double t) -> uint8_t {
            return (uint8_t)(a * (1 - t) + b * t);
        };

        RGB result;
        result.r = lerp(lerp(p00.r, p10.r, fx), lerp(p01.r, p11.r, fx), fy);
        result.g = lerp(lerp(p00.g, p10.g, fx), lerp(p01.g, p11.g, fx), fy);
        result.b = lerp(lerp(p00.b, p10.b, fx), lerp(p01.b, p11.b, fx), fy);

        return result;
    }
};

int main(int argc, char* argv[]) {
    if (argc != 3) {
        printf("Usage: %s input.bmp output.bmp\n", argv[0]);
        return 1;
    }

    Image img;
    if (!img.loadBMP(argv[1])) {
        printf("Failed to load image (must be 24-bit BMP)\n");
        return 1;
    }

    printf("Image loaded: %dx%d\n", img.width, img.height);

    double cx = img.width / 2.0;
    double cy = img.height / 2.0;
    double rs = std::min(img.width, img.height) * 0.06;
    double min_radius = rs * 1.5;

    double accretion_disk_inner = rs * 1;
    double accretion_disk_outer = rs * 100;
    double disk_brightness = 0;

    double doppler_strength = 0.0;
    bool enable_photon_ring = false;
    double photon_ring_brightness = 1.0;

    Image output;
    output.width = img.width;
    output.height = img.height;
    output.data.resize(output.width * output.height);

    printf("Rendering with rs = %.1f pixels...\n", rs);

    for (int y = 0; y < img.height; y++) {
        for (int x = 0; x < img.width; x++) {
            double dx = x - cx;
            double dy = y - cy;
            double b = sqrt(dx * dx + dy * dy);
            double angle = atan2(dy, dx);

            RGB color = {0, 0, 0};

            if (b < min_radius) {
                if (enable_photon_ring && b > min_radius * 0.95) {
                    double ring_intensity = photon_ring_brightness *
                        (1.0 - (min_radius - b) / (min_radius * 0.05));
                    ring_intensity = std::min(1.0, ring_intensity);
                    color = RGB(200 * ring_intensity,
                                180 * ring_intensity,
                                100 * ring_intensity);
                }
                output.data[y * img.width + x] = color;
                continue;
            }

            double factor = 2.0 * rs * rs / (b * b);
            double sample_x = x - factor * dx;
            double sample_y = y - factor * dy;
            RGB bg_color = img.getPixel(sample_x, sample_y);

            double disk_factor = 0.0;
            if (b >= accretion_disk_inner && b <= accretion_disk_outer) {
                double t = (b - accretion_disk_inner) / (accretion_disk_outer - accretion_disk_inner);
                double intensity = disk_brightness * (1.0 - t * 0.7);

                double doppler = 1.0;
                if (doppler_strength > 0) {
                    doppler = 1.0 + doppler_strength * cos(angle) * 0.5;
                    doppler = std::max(0.3, std::min(2.0, doppler));
                }

                intensity *= doppler;

                double inner_hot = std::max(0.0, 1.0 - t * 1.5);
                disk_factor = intensity;

                color.r = (bg_color.r * (1 - disk_factor) + 220 * disk_factor) * (0.7 + 0.3 * inner_hot);
                color.g = (bg_color.g * (1 - disk_factor) + 180 * disk_factor) * (0.8 + 0.2 * inner_hot);
                color.b = (bg_color.b * (1 - disk_factor) + 100 * disk_factor) * (0.9 + 0.5 * inner_hot);
            } else {
                color = bg_color;
            }

            double redshift = 1.0 - std::max(0.0, (min_radius / b) * 0.3);
            color.r = std::min(255, (int)(color.r * redshift));
            color.g = std::min(255, (int)(color.g * redshift * 0.9));
            color.b = std::min(255, (int)(color.b * redshift * 0.8));

            output.data[y * img.width + x] = bg_color;
        }
        if (y % 100 == 0) {
            printf("Progress: %.1f%%\r", 100.0 * y / img.height);
            fflush(stdout);
        }
    }

    printf("\nSaving to %s...\n", argv[2]);
    if (!output.saveBMP(argv[2])) {
        printf("Failed to save image\n");
        return 1;
    }

    printf("Done!\n");
    return 0;
}