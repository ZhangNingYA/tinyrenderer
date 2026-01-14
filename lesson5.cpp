#include <cmath>
#include <tuple>
#include "geometry.h"
#include "model.h"
#include "tgaimage.h"

constexpr int width  = 800;
constexpr int height = 800;

vec4 persp(vec4 v) {
    constexpr double c = 3.;
    return v / (1-v.z/c);
}
double signed_triangle_area(int ax, int ay, int bx, int by, int cx, int cy) {
    return .5*((by-ay)*(bx+ax) + (cy-by)*(cx+bx) + (ay-cy)*(ax+cx));
}

void triangle(int ax, int ay, int az, int bx, int by, int bz, int cx, int cy, int cz, TGAImage &zbuffer, TGAImage &framebuffer, TGAColor color) {
    int bbminx = std::min(std::min(ax, bx), cx); // bounding box for the triangle
    int bbminy = std::min(std::min(ay, by), cy); // defined by its top left and bottom right corners
    int bbmaxx = std::max(std::max(ax, bx), cx);
    int bbmaxy = std::max(std::max(ay, by), cy);
    double total_area = signed_triangle_area(ax, ay, bx, by, cx, cy);
    if (total_area<1) return; // backface culling + discarding triangles that cover less than a pixel

#pragma omp parallel for
    for (int x=bbminx; x<=bbmaxx; x++) {
        for (int y=bbminy; y<=bbmaxy; y++) {
            double alpha = signed_triangle_area(x, y, bx, by, cx, cy) / total_area;
            double beta  = signed_triangle_area(x, y, cx, cy, ax, ay) / total_area;
            double gamma = signed_triangle_area(x, y, ax, ay, bx, by) / total_area;
            if (alpha<0 || beta<0 || gamma<0) continue; // negative barycentric coordinate => the pixel is outside the triangle
            unsigned char z = static_cast<unsigned char>(alpha * az + beta * bz + gamma * cz);
            TGAColor c{};
            c.bgra[0] = z; // B
            c.bgra[1] = z; // G
            c.bgra[2] = z; // R
            c.bgra[3] = 255;
            if (z <= zbuffer.get(x, y)[0]) continue;
            zbuffer.set(x, y, c);
            framebuffer.set(x, y, color);
        }
    }
}
vec4 rot(vec4 v) {
    constexpr double a = M_PI/6;
    mat<4,4> Ry = {{
    { std::cos(a), 0.0, std::sin(a) },
    { 0.0,         1.0, 0.0         },
    { -std::sin(a),0.0, std::cos(a) },
    {0.0,         0.0, 0.0}
}};
    // constexpr mat<3,3> Ry = {{{std::cos(a), 0, std::sin(a)}, {0,1,0}, {-std::sin(a), 0, std::cos(a)}}};
    return Ry*v;
}


std::tuple<int,int,int> project(vec4 v) { // First of all, (x,y) is an orthogonal projection of the vector (x,y,z).
    int x = (v.x + 1.) * width/2;
    int y = (v.y + 1.) * height/2;
    int z = (v.z + 1.) * 255./2;

    z = std::max(0, std::min(255, z)); // 关键：避免 >255 或 <0

    return { x, y, z };
}

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " obj/model.obj" << std::endl;
        return 1;
    }

    Model model(argv[1]);
    TGAImage framebuffer(width, height, TGAImage::RGB);
    TGAImage     zbuffer(width, height, TGAImage::RGB);

    for (int i=0; i<model.nfaces(); i++) { // iterate through all triangles
        auto [ax, ay, az] = project(rot(model.vert(i, 0)));
        auto [bx, by, bz] = project(rot(model.vert(i, 1)));
        auto [cx, cy, cz] = project(rot(model.vert(i, 2)));
        TGAColor rnd;
        for (int c=0; c<3; c++) rnd[c] = std::rand()%255;
        triangle(ax, ay, az, bx, by, bz, cx, cy, cz, zbuffer, framebuffer, rnd);
    }

    framebuffer.write_tga_file("framebuffer.tga");
    zbuffer.write_tga_file("zbuffer.tga");
    return 0;
}