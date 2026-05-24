#include "raster.hpp"
#include <cstdio>
#include <vector>

constexpr float PI = 3.14159265358979323846f;

// -----------------------------------------------------------------------
// Mesh generators
// -----------------------------------------------------------------------

void generate_sphere(std::vector<Vertex>& verts, std::vector<Vec3i>& indices,
                     float radius, int stacks, int slices, Color color) {
    for (int i = 0; i <= stacks; ++i) {
        float phi = PI * float(i) / stacks;
        for (int j = 0; j <= slices; ++j) {
            float theta = 2 * PI * float(j) / slices;
            Vec3f p{std::sin(phi)*std::cos(theta), std::cos(phi), std::sin(phi)*std::sin(theta)};
            Vertex v;
            v.position  = {p[0]*radius, p[1]*radius, p[2]*radius, 1.0f};
            v.normal    = p;
            v.texcoord  = {float(j)/slices, float(i)/stacks};
            v.color     = color;
            verts.push_back(v);
        }
    }
    for (int i = 0; i < stacks; ++i)
        for (int j = 0; j < slices; ++j) {
            int a = i*(slices+1)+j, b = a+1;
            int c = (i+1)*(slices+1)+j, d = c+1;
            indices.push_back({a,b,c});
            indices.push_back({b,d,c});
        }
}

void generate_cube(std::vector<Vertex>& verts, std::vector<Vec3i>& indices,
                   float size, Color color) {
    float h = size/2;
    Vec3f pos[8] = {
        {-h,-h, h}, { h,-h, h}, { h, h, h}, {-h, h, h},
        {-h,-h,-h}, { h,-h,-h}, { h, h,-h}, {-h, h,-h},
    };
    Vec3f norm[6] = {
        { 0, 0, 1}, { 0, 0,-1}, { 1, 0, 0},
        {-1, 0, 0}, { 0, 1, 0}, { 0,-1, 0},
    };
    int faces[6][4] = {
        {0,1,2,3}, {5,4,7,6}, {1,5,6,2},
        {4,0,3,7}, {3,2,6,7}, {1,0,4,5},
    };
    for (int f = 0; f < 6; ++f) {
        for (int i = 0; i < 4; ++i) {
            int vi = faces[f][i];
            Vertex v;
            v.position = {pos[vi][0], pos[vi][1], pos[vi][2], 1.0f};
            v.normal   = norm[f];
            v.texcoord = {float(i==1||i==2), float(i==2||i==3)};
            v.color    = color;
            verts.push_back(v);
        }
        int base = f*4;
        indices.push_back({base, base+1, base+2});
        indices.push_back({base, base+2, base+3});
    }
}

// =======================================================================
// Scene builder
// =======================================================================

struct Scene {
    std::vector<Vertex> sphere_v, cube_v;
    std::vector<Vec3i> sphere_i, cube_i;
    Texture checker;
    float S, H_;

    Scene() {
        S = 2.5f; H_ = 2.0f;
        checker = Texture::checkerboard(256, 256, 31,
                                        Color{0.7f, 0.7f, 0.75f},
                                        Color{0.25f, 0.25f, 0.3f});
        generate_sphere(sphere_v, sphere_i, 1.0f, 48, 48, Color{1.0f, 0.85f, 0.3f});
        generate_cube(cube_v, cube_i, 0.5f, Color{0.3f, 0.5f, 1.0f});
    }

    void render(Framebuffer& fb, const Vec3f& eye, const Vec3f& target) {
        Rasterizer rs(fb);
        float aspect = float(fb.width) / fb.height;
        Mat4 proj = Mat4::perspective(PI/3, aspect, 0.1f, 100.0f);
        Mat4 view = Mat4::look_at(eye, target, {0,1,0});
        Mat4 vp   = Mat4::viewport(0, 0, float(fb.width), float(fb.height));

        rs.projection = proj;
        rs.viewport   = vp;
        rs.eye_pos    = eye;
        rs.ambient    = {0.18f, 0.18f, 0.22f};
        rs.world_light = {0.4f, -1.0f, 0.6f};
        rs.light_color = {1.0f, 0.95f, 0.9f};
        rs.clear_color[0] = 0.08f;
        rs.clear_color[1] = 0.08f;
        rs.clear_color[2] = 0.12f;
        rs.clear();

        rs.modelview = view;

        auto add_quad = [&](const Vec3f& p0, const Vec3f& p1,
                            const Vec3f& p2, const Vec3f& p3,
                            const Vec3f& normal, Color color) {
            std::vector<Vertex> v; std::vector<Vec3i> idx;
            // manual quad with corner UVs
            Vec3f p[4] = {p0, p1, p2, p3};
            for (int i = 0; i < 4; ++i) {
                Vertex vtx;
                vtx.position = {p[i][0], p[i][1], p[i][2], 1.0f};
                vtx.normal   = normal;
                vtx.texcoord = {float(i==1||i==2), float(i==2||i==3)};
                vtx.color    = color;
                v.push_back(vtx);
            }
            idx.push_back({0,1,2}); idx.push_back({0,2,3});
            rs.draw_mesh(v, idx);
        };

        // Cornell box walls
        add_quad({-S, -H_, -S}, { S, -H_, -S}, { S,  H_, -S}, {-S,  H_, -S},
                 {0,0,1}, Color{0.7f,0.7f,0.8f});                           // back
        add_quad({-S,  H_, -S}, { S,  H_, -S}, { S,  H_,  S}, {-S,  H_,  S},
                 {0,-1,0}, Color{0.8f,0.8f,0.8f});                           // ceiling
        add_quad({-S, -H_,  S}, {-S,  H_,  S}, {-S,  H_, -S}, {-S, -H_, -S},
                 {1,0,0}, Color{0.8f,0.3f,0.3f});                             // left
        add_quad({ S, -H_, -S}, { S,  H_, -S}, { S,  H_,  S}, { S, -H_,  S},
                 {-1,0,0}, Color{0.3f,0.8f,0.3f});                            // right

        // Floor with checkerboard texture
        {
            std::vector<Vertex> fv;
            std::vector<Vec3i> fi;
            Vec3f fp[4] = {{-S, -H_, -S}, {S, -H_, -S}, {S, -H_, S}, {-S, -H_, S}};
            Vec2f fuv[4] = {{0,0}, {4,0}, {4,4}, {0,4}};
            for (int i = 0; i < 4; ++i) {
                Vertex vtx;
                vtx.position = {fp[i][0], fp[i][1], fp[i][2], 1.0f};
                vtx.normal   = {0,1,0};
                vtx.texcoord = fuv[i];
                vtx.color    = {1,1,1};
                fv.push_back(vtx);
            }
            fi.push_back({0,1,2}); fi.push_back({0,2,3});
            rs.cull_backface = false;
            rs.texture = &checker;
            rs.draw_mesh(fv, fi);
            rs.texture = nullptr;
            rs.cull_backface = true;
        }

        // Sphere (golden)
        rs.draw_mesh(sphere_v, sphere_i);

        // Cube (blue, rotated)
        Mat4 cube_tf = Mat4::translate({1.2f, -1.65f, 0.8f}) * Mat4::rotate_y(0.7f);
        rs.modelview = view * cube_tf;
        rs.draw_mesh(cube_v, cube_i);
    }
};

// =======================================================================

int main() {
    Scene scene;

    {
        Framebuffer fb(1920, 1080);
        scene.render(fb, {2.5f, 1.8f, 3.0f}, {0, 0, 0});
        fb.write_ppm("render_main.ppm");
        printf("Written render_main.ppm (1920x1080)\n");
    }
    {
        Framebuffer fb(960, 540);
        scene.render(fb, {0.0f, 3.5f, 0.5f}, {0, 0, -1.0f});
        fb.write_ppm("render_top.ppm");
        printf("Written render_top.ppm (960x540)\n");
    }
    {
        Framebuffer fb(960, 540);
        scene.render(fb, {0.5f, 0.8f, 2.5f}, {0, 0.2f, 0});
        fb.write_ppm("render_close.ppm");
        printf("Written render_close.ppm (960x540)\n");
    }

    return 0;
}
