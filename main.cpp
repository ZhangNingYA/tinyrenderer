#include "our_gl.h"
#include "model.h"

extern mat<4,4> ModelView, Perspective; // "OpenGL" state matrices and
extern std::vector<double> zbuffer;     // the depth buffer

struct PhongShader : IShader {
    const Model &model;
    // vec3 l;          // light direction in eye coordinates
    // vec3 tri[3];     // triangle in eye coordinates
    // vec3 varying_nrm[3]; // normal per vertex to be interpolated by the fragment shader
    vec4 l;              // light direction in eye coordinates
    vec2 varying_uv[3];  // triangle uv coordinates, written by the vertex shader, read by the fragment shader

    PhongShader(const vec3 light, const Model &m) : model(m) {
        // l = normalized((ModelView*vec4{light.x, light.y, light.z, 0.}).xyz()); // transform the light vector to view coordinates
        l = normalized((ModelView*vec4{light.x, light.y, light.z, 0.})); // transform the light vector to view coordinates
    }

    virtual vec4 vertex(const int face, const int vert) {
        // vec3 v = model.vert(face, vert);                          // current vertex in object coordinates
        // vec3 n = model.normal(face, vert);
        // varying_nrm[vert] = (ModelView.invert_transpose() * vec4{n.x, n.y, n.z, 0.}).xyz();
        // vec4 gl_Position = ModelView * vec4{v.x, v.y, v.z, 1.};
        // tri[vert] = gl_Position.xyz();                            // in eye coordinates
        varying_uv[vert] = model.uv(face, vert);
        vec4 gl_Position = ModelView * model.vert(face, vert);
        return Perspective * gl_Position;                         // in clip coordinates
    }

    virtual std::pair<bool, TGAColor> fragment(const vec3 bar) const {
        // 1. 插值计算当前像素对应的 UV 坐标
        vec2 uv = varying_uv[0] * bar[0] + varying_uv[1] * bar[1] + varying_uv[2] * bar[2];

        // 2. 【核心修改】：不再使用纯白色，而是从模型读取颜色贴图在这个 UV 点的颜色
        TGAColor color = model.diffuse(uv); 

        // 3. 计算法线（从法线贴图采样并变换到眼坐标系）
        vec4 n = normalized(ModelView.invert_transpose() * model.normal(uv));
        
        // 4. 计算反射光方向 (用于镜面高光)
        vec4 r = normalized(n * (n * l) * 2. - l); 

        // 5. 计算光照分量
        double ambient = 0.6;                                     // 环境光（给一点基础亮度，防止全黑）
        double diff = std::max(0., n * l);                        // 漫反射强度
        double spec = std::pow(std::max(r.z, 0.), 35);            // 镜面高光强度

        // 6. 组合最终颜色
        TGAColor gl_FragColor;
        for (int i : {0, 1, 2}) { // 分别处理 R, G, B 三个通道
            // 公式：最终颜色 = 贴图颜色 * (环境光 + 漫反射 + 高光)
            // 注意：加法项的权重可以根据视觉效果微调，比如 0.8*diff + 0.2*spec
            double intensity = ambient + diff + 0.6 * spec; 
            gl_FragColor[i] = (unsigned char)std::min(255., color[i] * intensity);
        }

        return {false, gl_FragColor};
    }
};
int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " obj/model.obj" << std::endl;
        return 1;
    }

    constexpr int width  = 800;      // output image size
    constexpr int height = 800;
    constexpr vec3  light{ 1, 1, 1}; // light source
    constexpr vec3    eye{-1, 0, 2}; // camera position
    constexpr vec3 center{ 0, 0, 0}; // camera direction
    constexpr vec3     up{ 0, 1, 0}; // camera up vector

    lookat(eye, center, up);                                   // build the ModelView   matrix
    init_perspective(norm(eye-center));                        // build the Perspective matrix
    init_viewport(width/16, height/16, width*7/8, height*7/8); // build the Viewport    matrix
    init_zbuffer(width, height);
    TGAImage framebuffer(width, height, TGAImage::RGB);

    for (int m=1; m<argc; m++) {                    // iterate through all input objects
        Model model(argv[m]);                       // load the data
        PhongShader shader(light, model);
        for (int f=0; f<model.nfaces(); f++) {      // iterate through all facets
            Triangle clip = { shader.vertex(f, 0),  // assemble the primitive
                              shader.vertex(f, 1),
                              shader.vertex(f, 2) };
            rasterize(clip, shader, framebuffer);   // rasterize the primitive
        }
    }

    framebuffer.write_tga_file("framebuffer.tga");
    return 0;
}
