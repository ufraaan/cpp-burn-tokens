#include "raster.hpp"
#include <cstring>
#include <limits>

static float det3(const float a[3][3]) {
    return a[0][0] * (a[1][1]*a[2][2] - a[2][1]*a[1][2])
         - a[0][1] * (a[1][0]*a[2][2] - a[2][0]*a[1][2])
         + a[0][2] * (a[1][0]*a[2][1] - a[2][0]*a[1][1]);
}

// =======================================================================
// Mat4
// =======================================================================

Mat4 Mat4::identity() {
    Mat4 r;
    for (int i = 0; i < 4; ++i) r(i,i) = 1;
    return r;
}

Mat4 Mat4::perspective(float fov_y, float aspect, float near, float far) {
    float f = 1.0f / std::tan(fov_y * 0.5f);
    Mat4 r;
    r(0,0) = f / aspect;
    r(1,1) = f;
    r(2,2) = (far + near) / (near - far);
    r(2,3) = -1;
    r(3,2) = 2 * far * near / (near - far);
    return r;
}

Mat4 Mat4::look_at(const Vec3f& eye, const Vec3f& target, const Vec3f& up) {
    Vec3f f = (target - eye).normalized();
    Vec3f s = cross(f, up.normalized()).normalized();
    Vec3f u = cross(s, f);
    Mat4 r = identity();
    r(0,0)=s[0]; r(0,1)=u[0]; r(0,2)=-f[0];
    r(1,0)=s[1]; r(1,1)=u[1]; r(1,2)=-f[1];
    r(2,0)=s[2]; r(2,1)=u[2]; r(2,2)=-f[2];
    r(3,0)=-s.dot(eye);
    r(3,1)=-u.dot(eye);
    r(3,2)= f.dot(eye);
    return r;
}

Mat4 Mat4::viewport(float x, float y, float w, float h) {
    Mat4 r;
    r(0,0)=w/2;  r(3,0)=x + w/2;
    r(1,1)=h/2;  r(3,1)=y + h/2;
    r(2,2)=0.5f; r(3,2)=0.5f;
    r(3,3)=1;
    return r;
}

Mat4 Mat4::rotate_x(float rad) {
    float c = std::cos(rad), s = std::sin(rad);
    Mat4 r = identity();
    r(1,1)=c; r(1,2)=s;
    r(2,1)=-s; r(2,2)=c;
    return r;
}

Mat4 Mat4::rotate_y(float rad) {
    float c = std::cos(rad), s = std::sin(rad);
    Mat4 r = identity();
    r(0,0)=c;  r(0,2)=-s;
    r(2,0)=s;  r(2,2)=c;
    return r;
}

Mat4 Mat4::rotate_z(float rad) {
    float c = std::cos(rad), s = std::sin(rad);
    Mat4 r = identity();
    r(0,0)=c;  r(0,1)=s;
    r(1,0)=-s; r(1,1)=c;
    return r;
}

Mat4 Mat4::translate(const Vec3f& t) {
    Mat4 r = identity();
    r(3,0)=t[0]; r(3,1)=t[1]; r(3,2)=t[2];
    return r;
}

Mat4 Mat4::scale(const Vec3f& s) {
    Mat4 r;
    r(0,0)=s[0]; r(1,1)=s[1]; r(2,2)=s[2]; r(3,3)=1;
    return r;
}

Mat4 Mat4::operator*(const Mat4& r) const {
    Mat4 o;
    for (int col = 0; col < 4; ++col)
        for (int row = 0; row < 4; ++row) {
            float sum = 0;
            for (int k = 0; k < 4; ++k)
                sum += m[k*4+row] * r(col,k);
            o(col,row) = sum;
        }
    return o;
}

Vec4f Mat4::operator*(const Vec4f& r) const {
    Vec4f o;
    for (int row = 0; row < 4; ++row) {
        float sum = 0;
        for (int col = 0; col < 4; ++col)
            sum += m[col*4+row] * r[col];
        o[row] = sum;
    }
    return o;
}

Mat4 Mat4::transpose() const {
    Mat4 r;
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            r(j,i) = m[i*4+j];
    return r;
}

Mat4 Mat4::inverse() const {
    float A[4][4];
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            A[j][i] = m[i*4+j];

    float C[4][4];
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            float minor[3][3];
            int mi = 0;
            for (int ri = 0; ri < 4; ++ri) {
                if (ri == i) continue;
                int mj = 0;
                for (int cj = 0; cj < 4; ++cj) {
                    if (cj == j) continue;
                    minor[mi][mj] = A[ri][cj];
                    ++mj;
                }
                ++mi;
            }
            float d = det3(minor);
            C[i][j] = ((i + j) & 1) ? -d : d;
        }
    }

    float det = A[0][0]*C[0][0] + A[0][1]*C[0][1] + A[0][2]*C[0][2] + A[0][3]*C[0][3];
    if (std::abs(det) < 1e-30f) return identity();

    float inv_det = 1.0f / det;
    Mat4 out;
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            out(j,i) = C[i][j] * inv_det;
    return out;
}

// =======================================================================
// Texture
// =======================================================================

Color Texture::sample(Vec2f uv, bool bilinear) const {
    auto wrap = [](float f) -> float {
        f -= std::floor(f);
        return f;
    };
    float fx = wrap(uv[0]) * float(width);
    float fy = wrap(uv[1]) * float(height);

    if (!bilinear) {
        int x = std::clamp(int(fx), 0, width-1);
        int y = std::clamp(int(fy), 0, height-1);
        return data[size_t(y)*width+x];
    }

    int x0 = std::clamp(int(fx), 0, width-1);
    int y0 = std::clamp(int(fy), 0, height-1);
    int x1 = std::clamp(x0+1, 0, width-1);
    int y1 = std::clamp(y0+1, 0, height-1);
    float tx = fx - std::floor(fx);
    float ty = fy - std::floor(fy);

    auto lerp = [](const Color& a, const Color& b, float t) {
        return a * (1-t) + b * t;
    };

    Color c00 = data[size_t(y0)*width+x0];
    Color c01 = data[size_t(y0)*width+x1];
    Color c10 = data[size_t(y1)*width+x0];
    Color c11 = data[size_t(y1)*width+x1];

    return lerp(lerp(c00, c01, tx), lerp(c10, c11, tx), ty);
}

Texture Texture::checkerboard(int w, int h, int tiles, Color a, Color b) {
    Texture t(w, h);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            int tx = int(float(x) / w * tiles);
            int ty = int(float(y) / h * tiles);
            t.data[size_t(y)*w+x] = ((tx+ty)&1) ? a : b;
        }
    return t;
}

// =======================================================================
// Framebuffer
// =======================================================================

Framebuffer::Framebuffer(int w, int h)
    : width(w), height(h)
    , color(size_t(w)*h)
    , depth(size_t(w)*h)
{}

void Framebuffer::clear(const Color& c, float d) {
    std::fill(color.begin(), color.end(), c);
    std::fill(depth.begin(), depth.end(), d);
}

void Framebuffer::write_ppm(const std::string& path, int gamma) const {
    std::ofstream out(path, std::ios::binary);
    float inv_gamma = 100.0f / float(gamma);
    out << "P6\n" << width << " " << height << "\n255\n";
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const Color& c = color[size_t(y)*width+x];
            auto to_byte = [&](float v) -> uint8_t {
                v = std::max(0.0f, std::min(1.0f, v));
                v = std::pow(v, inv_gamma);
                return uint8_t(v * 255.0f + 0.5f);
            };
            uint8_t rgb[3] = {to_byte(c[0]), to_byte(c[1]), to_byte(c[2])};
            out.write((const char*)rgb, 3);
        }
    }
}

// =======================================================================
// Rasterizer
// =======================================================================

Rasterizer::Rasterizer(Framebuffer& fb) : fb(fb) {}

void Rasterizer::clear() {
    fb.clear({clear_color[0],clear_color[1],clear_color[2],clear_color[3]}, clear_depth);
}

float Rasterizer::edge(const Vec2f& a, const Vec2f& b, const Vec2f& p) {
    return (b[0]-a[0])*(p[1]-a[1]) - (b[1]-a[1])*(p[0]-a[0]);
}

bool Rasterizer::is_top_left(const Vec2f& a, const Vec2f& b) {
    float dx = b[0] - a[0];
    float dy = b[1] - a[1];
    return dy < 0 || (dy == 0 && dx < 0);
}

Vec4f Rasterizer::phong(const Vec3f& normal, const Vec4f& base,
                         const Vec3f& view_dir) const {
    Vec3f n = normal.normalized();
    Vec3f l = view_light.normalized();

    float diff = std::max(0.0f, n.dot(l));

    Vec3f half = (l + view_dir).normalized();
    float spec = std::pow(std::max(0.0f, n.dot(half)), 64.0f);

    Vec3f base_rgb{base[0], base[1], base[2]};
    Vec3f result = {base_rgb[0]*ambient[0], base_rgb[1]*ambient[1], base_rgb[2]*ambient[2]};
    result = {result[0] + base_rgb[0]*light_color[0]*diff,
              result[1] + base_rgb[1]*light_color[1]*diff,
              result[2] + base_rgb[2]*light_color[2]*diff};
    float spec_strength = 0.25f * std::max(light_color[0], std::max(light_color[1], light_color[2]));
    result = {result[0] + spec * spec_strength,
              result[1] + spec * spec_strength,
              result[2] + spec * spec_strength};

    return {result[0], result[1], result[2], 1.0f};
}

void Rasterizer::rasterize(const Vertex& v0, const Vertex& v1, const Vertex& v2,
                           const Vec4f& p0, const Vec4f& p1, const Vec4f& p2) {
    Vec2f s0{p0[0], p0[1]}, s1{p1[0], p1[1]}, s2{p2[0], p2[1]};

    float area = edge(s0, s1, s2);
    if (cull_backface && area <= 0) return;
    bool reverse = area < 0;

    // compute bounding box with sub-pixel precision
    auto bbox_min = [](float a, float b, float c) {
        return std::min({a, b, c});
    };
    auto bbox_max = [](float a, float b, float c) {
        return std::max({a, b, c});
    };

    int min_x = int(std::max(0.0f, std::floor(bbox_min(s0[0], s1[0], s2[0]))));
    int max_x = int(std::min(float(fb.width-1), std::ceil(bbox_max(s0[0], s1[0], s2[0]))));
    int min_y = int(std::max(0.0f, std::floor(bbox_min(s0[1], s1[1], s2[1]))));
    int max_y = int(std::min(float(fb.height-1), std::ceil(bbox_max(s0[1], s1[1], s2[1]))));

    float inv_area = (reverse ? -1.0f : 1.0f) / area;

    float w0_inv = 1.0f / p0[3];
    float w1_inv = 1.0f / p1[3];
    float w2_inv = 1.0f / p2[3];

    Vec3f vp0{v0.view_pos[0], v0.view_pos[1], v0.view_pos[2]};
    Vec3f vp1{v1.view_pos[0], v1.view_pos[1], v1.view_pos[2]};
    Vec3f vp2{v2.view_pos[0], v2.view_pos[1], v2.view_pos[2]};

    for (int y = min_y; y <= max_y; ++y) {
        for (int x = min_x; x <= max_x; ++x) {
            Vec2f p{float(x) + 0.5f, float(y) + 0.5f};

            float e0 = edge(s1, s2, p);
            float e1 = edge(s2, s0, p);
            float e2 = edge(s0, s1, p);

            if (reverse) {
                if (e0 > 0 || (e0 == 0 && is_top_left(s1, s2))) continue;
                if (e1 > 0 || (e1 == 0 && is_top_left(s2, s0))) continue;
                if (e2 > 0 || (e2 == 0 && is_top_left(s0, s1))) continue;
            } else {
                if (e0 < 0 || (e0 == 0 && !is_top_left(s1, s2))) continue;
                if (e1 < 0 || (e1 == 0 && !is_top_left(s2, s0))) continue;
                if (e2 < 0 || (e2 == 0 && !is_top_left(s0, s1))) continue;
            }

            float a, b, c;
            if (reverse) {
                a = -e0 * inv_area; b = -e1 * inv_area; c = -e2 * inv_area;
            } else {
                a = e0 * inv_area; b = e1 * inv_area; c = e2 * inv_area;
            }

            float za = a * w0_inv;
            float zb = b * w1_inv;
            float zc = c * w2_inv;
            float zsum = za + zb + zc;
            float depth_val = (za * p0[2] + zb * p1[2] + zc * p2[2]) / zsum;

            size_t idx = size_t(y) * fb.width + x;
            if (depth_val >= fb.depth[idx]) continue;
            fb.depth[idx] = depth_val;

            float inv_zsum = 1.0f / zsum;
            float wa = za * inv_zsum;
            float wb = zb * inv_zsum;
            float wc = zc * inv_zsum;

            Vec3f normal = (v0.normal * wa + v1.normal * wb + v2.normal * wc).normalized();
            Vec2f uv     = v0.texcoord * wa + v1.texcoord * wb + v2.texcoord * wc;
            Color base   = v0.color * wa + v1.color * wb + v2.color * wc;

            Vec4f tex_color{1,1,1,1};
            if (texture) tex_color = texture->sample(uv);
            Vec4f material{base[0]*tex_color[0], base[1]*tex_color[1],
                           base[2]*tex_color[2], 1.0f};

            Vec3f view_pos = vp0 * wa + vp1 * wb + vp2 * wc;
            Vec3f view_dir = view_pos.normalized();

            fb.color[idx] = phong(normal, material, -view_dir);
        }
    }
}

void Rasterizer::draw_triangle(const Vertex& v0, const Vertex& v1, const Vertex& v2) {
    Mat4 mvp = projection * modelview;
    Vec4f p0 = viewport * (mvp * v0.position);
    Vec4f p1 = viewport * (mvp * v1.position);
    Vec4f p2 = viewport * (mvp * v2.position);

    // reject if any vertex is behind the camera (w <= 0 in clip space)
    if (p0[3] <= 0 || p1[3] <= 0 || p2[3] <= 0) return;
    for (int i = 0; i < 3; ++i) p0[i] /= p0[3];
    for (int i = 0; i < 3; ++i) p1[i] /= p1[3];
    for (int i = 0; i < 3; ++i) p2[i] /= p2[3];

    if ((p0[2] > 1 && p1[2] > 1 && p2[2] > 1) ||
        (p0[2] < 0 && p1[2] < 0 && p2[2] < 0))
        return;

    rasterize(v0, v1, v2, p0, p1, p2);
}

void Rasterizer::draw_mesh(const std::vector<Vertex>& verts,
                           const std::vector<Vec3i>& indices) {
    Mat4 normal_mat = modelview.inverse().transpose();
    Vec4f vl = normal_mat * Vec4f{world_light[0], world_light[1], world_light[2], 0};
    view_light = {vl[0], vl[1], vl[2]};

    for (const auto& idx : indices) {
        Vertex v[3];
        for (int i = 0; i < 3; ++i) {
            v[i] = verts[idx[i]];
            Vec4f vp = modelview * v[i].position;
            v[i].view_pos = {vp[0]/vp[3], vp[1]/vp[3], vp[2]/vp[3]};
            Vec4f tn = normal_mat * Vec4f{v[i].normal[0], v[i].normal[1], v[i].normal[2], 0};
            v[i].normal = Vec3f{tn[0], tn[1], tn[2]}.normalized();
        }
        draw_triangle(v[0], v[1], v[2]);
    }
}
