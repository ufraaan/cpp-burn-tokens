#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

// -----------------------------------------------------------------------
// Vec (aggregate — no constructors, brace-init works naturally)
// -----------------------------------------------------------------------

template <typename T, int N>
struct Vec {
    T v[N] = {};

    T& operator[](int i) { return v[i]; }
    T  operator[](int i) const { return v[i]; }

    Vec operator+(const Vec& r) const { Vec o; for (int i=0;i<N;++i) o[i]=v[i]+r[i]; return o; }
    Vec operator-(const Vec& r) const { Vec o; for (int i=0;i<N;++i) o[i]=v[i]-r[i]; return o; }
    Vec operator*(T s)          const { Vec o; for (int i=0;i<N;++i) o[i]=v[i]*s;   return o; }
    Vec operator/(T s)          const { return *this * (T(1)/s); }
    Vec operator-()             const { return *this * T(-1); }

    Vec& operator+=(const Vec& r) { for (int i=0;i<N;++i) v[i]+=r[i]; return *this; }
    Vec& operator-=(const Vec& r) { for (int i=0;i<N;++i) v[i]-=r[i]; return *this; }
    Vec& operator*=(T s)          { for (int i=0;i<N;++i) v[i]*=s;    return *this; }

    T dot(const Vec& r) const { T d{}; for (int i=0;i<N;++i) d+=v[i]*r[i]; return d; }
    T length2() const { return dot(*this); }
    T length()  const { return std::sqrt(length2()); }
    Vec normalized() const { return *this / length(); }
};

template <typename T, int N> Vec<T,N> operator*(T s, const Vec<T,N>& v) { return v * s; }

using Vec2f = Vec<float,2>;
using Vec3f = Vec<float,3>;
using Vec4f = Vec<float,4>;
using Vec3i = Vec<int,3>;

inline Vec3f cross(const Vec3f& a, const Vec3f& b) {
    return { a[1]*b[2] - a[2]*b[1],
             a[2]*b[0] - a[0]*b[2],
             a[0]*b[1] - a[1]*b[0] };
}

// -----------------------------------------------------------------------
// Color (linear RGBA)
// -----------------------------------------------------------------------

struct Color : Vec4f {
    Color() : Vec4f{0,0,0,1} {}
    Color(float r, float g, float b, float a=1) : Vec4f{r,g,b,a} {}
    Color(const Vec4f& v) : Vec4f{v} {}
};

// -----------------------------------------------------------------------
// Mat4 (column-major)
// -----------------------------------------------------------------------

struct Mat4 {
    float m[16] = {};

    Mat4() = default;
    float& operator()(int col, int row) { return m[col*4+row]; }
    float  operator()(int col, int row) const { return m[col*4+row]; }

    static Mat4 identity();
    static Mat4 perspective(float fov_y, float aspect, float near, float far);
    static Mat4 look_at(const Vec3f& eye, const Vec3f& target, const Vec3f& up);
    static Mat4 viewport(float x, float y, float w, float h);
    static Mat4 rotate_x(float rad);
    static Mat4 rotate_y(float rad);
    static Mat4 rotate_z(float rad);
    static Mat4 translate(const Vec3f& t);
    static Mat4 scale(const Vec3f& s);

    Mat4 operator*(const Mat4& r) const;
    Vec4f operator*(const Vec4f& r) const;
    Mat4 transpose() const;
    Mat4 inverse() const;
};

// -----------------------------------------------------------------------
// Texture
// -----------------------------------------------------------------------

struct Texture {
    int width  = 0;
    int height = 0;
    std::vector<Color> data;

    Texture() = default;
    Texture(int w, int h) : width(w), height(h), data(size_t(w)*h) {}

    Color sample(Vec2f uv, bool bilinear = true) const;

    static Texture checkerboard(int w, int h, int tiles, Color a, Color b);
};

// -----------------------------------------------------------------------
// Vertex
// -----------------------------------------------------------------------

struct Vertex {
    Vec4f  position;      // object space position
    Vec3f  normal;        // view-space normal (set during draw_mesh)
    Vec2f  texcoord;
    Color  color;         // base material colour
    Vec3f  view_pos;      // view-space position (set during draw_mesh)
};

// -----------------------------------------------------------------------
// Framebuffer
// -----------------------------------------------------------------------

struct Framebuffer {
    int width  = 0;
    int height = 0;
    std::vector<Color>  color;
    std::vector<float>  depth;

    Framebuffer() = default;
    Framebuffer(int w, int h);

    void clear(const Color& c, float d);
    void write_ppm(const std::string& path, int gamma = 220) const;
};

// -----------------------------------------------------------------------
// Rasterizer
// -----------------------------------------------------------------------

struct Rasterizer {
    Framebuffer& fb;
    const Texture* texture = nullptr;

    // transforms
    Mat4  modelview;
    Mat4  projection;
    Mat4  viewport;

    // lighting (set light_dir in world space; draw_mesh transforms it)
    Vec3f ambient      = {0.2f, 0.2f, 0.2f};
    Vec3f world_light  = {0.5f, -1.0f, 0.3f};
    Vec3f light_color  = {1.0f, 1.0f, 1.0f};
    Vec3f eye_pos      = {0, 0, 4};
    Vec3f view_light   = world_light;

    // state
    bool cull_backface = true;
    float clear_color[4] = {0,0,0,1};
    float clear_depth = 1e30f;

    Rasterizer(Framebuffer& fb);

    void clear();
    void draw_triangle(const Vertex& v0, const Vertex& v1, const Vertex& v2);

    void draw_mesh(const std::vector<Vertex>& verts,
                   const std::vector<Vec3i>& indices);

private:
    static float edge(const Vec2f& a, const Vec2f& b, const Vec2f& p);
    static bool is_top_left(const Vec2f& a, const Vec2f& b);

    void rasterize(const Vertex& v0, const Vertex& v1, const Vertex& v2,
                   const Vec4f& p0, const Vec4f& p1, const Vec4f& p2);

    Vec4f phong(const Vec3f& normal, const Vec4f& base,
                const Vec3f& view_dir) const;
};
