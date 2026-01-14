#include <cmath>
#include <algorithm>
#include "tgaimage.h"

double signed_triangle_area(int ax, int ay, int bx, int by, int cx, int cy) {
    return .5*((by-ay)*(bx+ax) + (cy-by)*(cx+bx) + (ay-cy)*(ax+cx));
}

void triangle(int ax, int ay, int az,
              int bx, int by, int bz,
              int cx, int cy, int cz,
              TGAImage &framebuffer,
              TGAColor *color) {

    int bbminx = std::min(std::min(ax, bx), cx);
    int bbminy = std::min(std::min(ay, by), cy);
    int bbmaxx = std::max(std::max(ax, bx), cx);
    int bbmaxy = std::max(std::max(ay, by), cy);

    double total_area = signed_triangle_area(ax, ay, bx, by, cx, cy);
    if (total_area < 1) return; // 你的原逻辑：背面剔除 + 太小丢弃

    #pragma omp parallel for
    for (int x = bbminx; x <= bbmaxx; x++) {
        for (int y = bbminy; y <= bbmaxy; y++) {
            double alpha = signed_triangle_area(x, y, bx, by, cx, cy) / total_area;
            double beta  = signed_triangle_area(x, y, cx, cy, ax, ay) / total_area;
            double gamma = signed_triangle_area(x, y, ax, ay, bx, by) / total_area;
            if (alpha < 0 || beta < 0 || gamma < 0) continue;
            // if (alpha > 0.1 && beta > 0.1 && gamma > 0.1) continue; // 防止浮点数误差导致的越界
            for(int c=0; c<3; c++) {
                TGAColor  col = {};
                col[0] = static_cast<unsigned char>(alpha * color[0][0] + beta * color[1][0] + gamma * color[2][0]); // B
                col[1] = static_cast<unsigned char>(alpha * color[0][1] + beta * color[1][1] + gamma * color[2][1]); // G
                col[2] = static_cast<unsigned char>(alpha * color[0][2] + beta * color[1][2] + gamma * color[2][2]); // R
                col[3] = 255; 
                framebuffer.set(x, y, col);
            }
        }
    }
}

int main(int argc, char** argv) {
    constexpr int width  = 640;
    constexpr int height = 640;

    // 方案1：改为 RGB
    TGAImage framebuffer(width, height, TGAImage::RGB);

    // 建议：清屏为黑色，避免背景未初始化导致“看起来不对”
    TGAColor black;
    black[0] = 0; black[1] = 0; black[2] = 0; black[3] = 255;
    for (int x = 0; x < width; x++) {
        for (int y = 0; y < height; y++) {
            framebuffer.set(x, y, black);
        }
    }
    TGAColor color[3] = {
        {255, 0, 0, 255},   // R
        {0, 255, 0, 255},   // G
        {0, 0, 255, 255}    // B
    };
    int ax = 17, ay =  4, az =  13;
    int bx = 620, by = 20, bz = 128;
    int cx = 90, cy = 559, cz = 255;

    triangle(ax, ay, az, bx, by, bz, cx, cy, cz, framebuffer, color);

    framebuffer.write_tga_file("framebuffer.tga");
    return 0;
}
