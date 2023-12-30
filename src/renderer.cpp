#include "renderer.h"
#include <math.h>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <string_view>
#include <sys/ioctl.h>
#include <unistd.h>
#include <cstring>
#include <signal.h>
#include <chrono>
#include <thread>
#include <termio.h>

struct termios old_settings;

void setup_tty() {
    struct termios raw;
    if(!isatty(STDIN_FILENO) || tcgetattr(STDIN_FILENO, &old_settings) < 0) {
        exit(1);
    }
    raw = old_settings;
    raw.c_lflag &= ~(ECHO | ICANON);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);

    std::cout << "\e 7\e[s" << std::endl;
}

void restore_tty()
{
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &old_settings);
    std::cout << "\e[?1049l\e 8\e[u\e[?25h" << std::endl;
}

void signal_cleanup(int) {
    restore_tty();
    exit(0);
}

// const char *CHAR_PIXEL_TABLE = R"##($@B%8&WM#*oahkbdpqwmZO0QLCJUYXzcvunxrjft/\|()1{}[]?-_+~<>i!lI;:,"^`'. )##";
const char *CHAR_PIXEL_TABLE = R"( `.-':_,^=;><+!rc*/z?sLTv)J7(|Fi{C}fI31tlu[neoZ5Yxjya]2ESwqkP6h9d4VpOGbUAKXHm8RD#$Bg0MNWQ%&@)";
const float CHAR_BRIGHTNESS_TABLE[] = {0.001, 0.0751, 0.0829, 0.0848, 0.1227, 0.1403, 0.1559, 0.185, 0.2183, 0.2417, 0.2571, 0.2852,
                                       0.2902, 0.2919, 0.3099, 0.3192, 0.3232, 0.3294, 0.3384, 0.3609, 0.3619, 0.3667, 0.3737, 0.3747, 0.3838, 0.3921, 0.396, 0.3984,
                                       0.3993, 0.4075, 0.4091, 0.4101, 0.42, 0.423, 0.4247, 0.4274, 0.4293, 0.4328, 0.4382, 0.4385, 0.442, 0.4473, 0.4477, 0.4503,
                                       0.4562, 0.458, 0.461, 0.4638, 0.4667, 0.4686, 0.4693, 0.4703, 0.4833, 0.4881, 0.4944, 0.4953, 0.4992, 0.5509, 0.5567, 0.5569,
                                       0.5591, 0.5602, 0.5602, 0.565, 0.5776, 0.5777, 0.5818, 0.587, 0.5972, 0.5999, 0.6043, 0.6049, 0.6093, 0.6099, 0.6465, 0.6561,
                                       0.6595, 0.6631, 0.6714, 0.6759, 0.6809, 0.6816, 0.6925, 0.7039, 0.7086, 0.7235, 0.7302, 0.7332, 0.7602, 0.7834, 0.8037, 0.9999};

static char brightness_to_ascii(float f)
{
    if (f < 0.0)
    {
        return CHAR_PIXEL_TABLE[0];
    }
    const float *it = std::upper_bound(CHAR_BRIGHTNESS_TABLE, CHAR_BRIGHTNESS_TABLE + sizeof(CHAR_BRIGHTNESS_TABLE) / sizeof(float) - 1, f);
    size_t i = it - CHAR_BRIGHTNESS_TABLE;
    return CHAR_PIXEL_TABLE[i];
}

static float brightness_by_distance(float dist)
{
    dist += 1.0;
    if (dist < 1.0)
    {
        return 1.0;
    }
    else
    {
        return 1.0 / dist;
    }
}

static float vec2_norm(vec2_t vec)
{
    return sqrtf(vec.first * vec.first + vec.second * vec.second);
}

static vec2_t vec2_normalize(vec2_t vec)
{
    float len = vec2_norm(vec);
    return vec2_t {vec.first / len, vec.second / len};
}

static vec2_t vec2_rotate(vec2_t vec, float angle)
{
    float sin = sinf(angle);
    float cos = cosf(angle);
    return vec2_t {vec.first * cos + vec.second * sin, vec.second * cos - vec.first * sin};
}

#define CONCAT(a, b) a##b
#define CONCAT2(a, b) CONCAT(a, b)

#define PAIR_OP_ASSIGNMENT(op, name) \
vec2_t CONCAT(vec2_, name)(vec2_t a, vec2_t b) { \
    vec2_t out; \
    out.first = a.first op b.first; \
    out.second = a.second op b.second; \
    return out; \
} \
vec2_t CONCAT2(vec2_, CONCAT(name, _scalar))(vec2_t a, float b) { \
    vec2_t out; \
    out.first = a.first op b; \
    out.second = a.second op b; \
    return out; \
} \
uvec2_t CONCAT(uvec2_, name)(uvec2_t a, uvec2_t b) { \
    uvec2_t out; \
    out.first = a.first op b.first; \
    out.second = a.second op b.second; \
    return out; \
} \
uvec2_t CONCAT2(uvec2_, CONCAT(name, _scalar))(uvec2_t a, unsigned b) { \
    uvec2_t out; \
    out.first = a.first op b; \
    out.second = a.second op b; \
    return out; \
}

PAIR_OP_ASSIGNMENT(+, add)
PAIR_OP_ASSIGNMENT(-, sub)
PAIR_OP_ASSIGNMENT(*, mul)
PAIR_OP_ASSIGNMENT(/, div)

#undef PAIR_OP_ASSIGNMENT

constexpr unsigned WALL_TEXTURE_WIDTH = 6, WALL_TEXTURE_HEIGHT = 6;
Texture WALL_TEXTURE{{WALL_TEXTURE_WIDTH, WALL_TEXTURE_HEIGHT}};

void init_wall_texture() {
    for(unsigned i = 0; i < WALL_TEXTURE_WIDTH; i++) {
        for(unsigned j = 0; j < WALL_TEXTURE_HEIGHT; j++) {
            WALL_TEXTURE.set(uvec2_t {i, j}, (i & 1) ^ (j & 1) == 1 ? 1.0 : 0.5);
        }
    }
}

Texture::Texture(uvec2_t dims) : _dims{dims}, _buffer{std::make_unique<float[]>(dims.first * dims.second)}
{
}

float *Texture::data()
{
    return _buffer.get();
}

float Texture::sample(const vec2_t &uv) const
{
    unsigned x = _dims.first * uv.first;
    x = std::min(x, _dims.first - 1);
    unsigned y = _dims.second * uv.second;
    y = std::min(y, _dims.second - 1);
    return lookup({x, y});
}

const float *Texture::data() const
{
    return _buffer.get();
}

uvec2_t Texture::size() const
{
    return _dims;
}

float Texture::lookup(const uvec2_t &index) const
{
    if (index.first > _dims.first || index.second > _dims.second)
    {
        return 0.0;
    }
    /* Typically, the texture is traversed vertically*/
    return _buffer[index.first * _dims.second + index.second];
}

void Texture::set(const uvec2_t &index, float brightness)
{
    if (index.first > _dims.first || index.second > _dims.second)
    {
        return;
    }
    _buffer[index.first * _dims.second + index.second] = brightness;
}

void Texture::clear()
{
    std::memset(_buffer.get(), 0, _dims.first * _dims.second * sizeof(float));
}

Camera::Camera(float fovx, float fovy) : _fovx{fovx}, _fovy{fovy}
{
}

Camera Camera::from_fovy(uvec2_t dims, float fovy)
{
    double x = dims.first;
    double y = dims.second;
    // cos(y/2 * d) = fovy / 2
    // cos(x/2 * d) = fovx / 2
    float d = 2 * std::acos(fovy * 0.5) / y;
    float fovx = 2 * std::cos(x * 0.5 * d);
    return Camera(fovx, fovy);
}

Camera Camera::from_fovx(uvec2_t dims, float fovx)
{
    Camera c = Camera::from_fovy({dims.second, dims.first}, fovx);
    float new_fovx = c._fovy;
    float new_fovy = c._fovx;
    return Camera(new_fovx, new_fovy);
}

float Camera::fovy() const
{
    return _fovy;
}

float Camera::fovx() const
{
    return _fovx;
}

float Camera::column_height(float dist)
{
    float view_height = std::atan(_fovy * 0.5f) * dist * 2.0f;
    return REAL_COLUMN_HEIGHT / view_height;
}

Renderer::Renderer(Map &&map, uvec2_t framebuffer_size) : _map{std::move(map)}, _fb{framebuffer_size}, _camera{Camera::from_fovy(framebuffer_size, M_PI / 2)}
{
}

ssize_t compute_starting_row(size_t screen_height, size_t wall_height) {
    size_t wall_center = screen_height / 2;
    return wall_center - static_cast<ssize_t>(wall_height) / 2;
}

void Renderer::draw_from(vec2_t pos, vec2_t look_dir)
{
    _fb.clear();
    look_dir = vec2_normalize(look_dir);
    float angle = -_camera.fovy() * 0.5f;
    float dangle = _camera.fovy() / (_fb.size().first - 1);
    const auto &fb_size = _fb.size();
    for (size_t i = 0; i < fb_size.first; i++)
    {
        vec2_t ray_dir = vec2_rotate(look_dir, angle);
        HitResult hit = ray(pos, ray_dir);
        float depth = vec2_norm(vec2_sub(hit.pos, pos));
        float height_ratio = _camera.column_height(depth);

        size_t screen_height = fb_size.second;
        size_t wall_height = screen_height * height_ratio;
        size_t column_height = std::min(screen_height, wall_height);
        
        float v_step = 1.0 / (wall_height - 1);
        ssize_t starting_row = std::max(static_cast<ssize_t>(0), compute_starting_row(screen_height, column_height));
        ssize_t unlimited_starting_row = compute_starting_row(screen_height, wall_height); 
        vec2_t uv = {hit.u, v_step * (starting_row - unlimited_starting_row)};

        for (size_t j = static_cast<size_t>(starting_row), k = 0; k < column_height; k++, j++)
        {
            _fb.set(uvec2_t {(unsigned)i, (unsigned)j}, WALL_TEXTURE.sample(uv) * brightness_by_distance(depth));
            uv.second += v_step;
        }
        angle += dangle;
    }
    std::cout << std::endl;
    render_fb();
}

const Map &Renderer::map() const
{
    return _map;
}

HitResult Renderer::ray(vec2_t pos, vec2_t dir) const
{
    float x = pos.first;
    float y = pos.second;
    int cell_x = pos.first;
    int cell_y = pos.second;
    int xDelta = dir.first >= 0.0f ? 1 : -1;
    int yDelta = dir.second >= 0.0f ? 1 : -1;
    float xRemaining = (float)(cell_x + (dir.first >= 0.0f ? 1 : 0)) - pos.first;
    float yRemaining = (float)(cell_y + (dir.second >= 0.0f ? 1 : 0)) - pos.second;
    bool x_hit;
    while (!_map.check(uvec2_t {(unsigned)cell_x, (unsigned)cell_y}))
    {
        float xStep = xRemaining / dir.first;
        float yStep = yRemaining / dir.second;
        if (xStep > yStep)
        {
            // y is intersected next
            y += yRemaining;
            yRemaining = yDelta;
            float xDiff = dir.first * yStep;
            x += xDiff;
            xRemaining -= xDiff;
            cell_y += yDelta;
            x_hit = false;
        }
        else
        {
            // x is intersected next
            x += xRemaining;
            xRemaining = xDelta;
            float yDiff = dir.second * xStep;
            y += yDiff;
            yRemaining -= yDiff;
            cell_x += xDelta;
            x_hit = true;
        }
    }
    float u;
    if(x_hit) {
        u = yDelta >= 0 ? yRemaining : 1.0 + yRemaining;
    } else {
        u = xDelta >= 0 ? xRemaining : 1.0 + xRemaining;
    }
    return HitResult{true, {x, y}, u};
}

void Renderer::render_fb()
{
    const auto &fb_size = _fb.size();
    size_t ascii_width = fb_size.first, ascii_height = fb_size.second / 2;
    size_t ascii_size = (ascii_width + 1) * ascii_height;

    std::unique_ptr<char[]> ascii_image = std::make_unique<char[]>(ascii_size);
#if 0
    float char_ratio = 0.5f; // width/height
    const auto &fb_size = _fb.size();
    if(char_ratio < 1.0f) {
        const auto &fb_size = _fb.size();
        ascii_width = fb_size.first;
        ascii_height = std::ceil(fb_size.second / char_ratio);
    } else {
        const auto &fb_size = _fb.size();
        ascii_width = std::ceil(fb_size.first * char_ratio);
        ascii_height = fb_size.second;
    }
#endif

    for (size_t ascii_y = 0; ascii_y < ascii_height; ascii_y++)
    {
        for (size_t ascii_x = 0; ascii_x < ascii_width; ascii_x++)
        {
            // TODO: don't hardcode character ratio and sampling rate
            float sample1 = _fb.lookup(uvec2_t {(unsigned)ascii_x, 2 * (unsigned)ascii_y});
            float sample2 = _fb.lookup(uvec2_t {(unsigned)ascii_x, 2 * (unsigned)ascii_y + 1});
            float brightness = (sample1 + sample2) * 0.5;
            ascii_image[ascii_y * (ascii_width + 1) + ascii_x] = brightness_to_ascii(brightness);
        }
        ascii_image[ascii_y * (ascii_width + 1) + ascii_width] = '\n';
    }

    std::cout << "\e[H\e[?25l\e[?1049h";
    std::cout << std::string_view(ascii_image.get(), ascii_size);
}

Map::Map(uvec2_t dims) : _map{std::make_unique<uint8_t[]>((dims.first * dims.second + 7) / 8)}, _dims{dims}
{
}

void Map::set(uvec2_t pos, bool val)
{
    if (pos.first > _dims.first || pos.second > _dims.second)
        return;
    size_t linear_index = pos.first * _dims.second + pos.second;

    if (val)
    {
        _map[linear_index / 8] |= (1 << linear_index % 8);
    }
    else
    {
        _map[linear_index / 8] &= ~(1 << linear_index % 8);
    }
}

bool Map::check(uvec2_t pos) const
{
    if (pos.first > _dims.first || pos.second > _dims.second)
        return true;
    size_t linear_index = pos.first * _dims.second + pos.second;

    return _map[linear_index / 8] & (1 << linear_index % 8);
}

Map Map::from_string(std::string_view s)
{
    size_t height = std::count(s.begin(), s.end(), '\n') + 1;
    size_t width = std::find(s.begin(), s.end(), '\n') - s.begin();
    Map map(uvec2_t { (unsigned)width, (unsigned)height });
    size_t x = 0;
    size_t y = height - 1;
    for (char c : s)
    {
        switch (c)
        {
        case '\n':
            --y;
            x = 0;
            break;
        case '#':
            // bounds check is handled by Map::set
            map.set(uvec2_t { (unsigned)x, (unsigned)y }, true);
            ++x;
            break;
        default:
            map.set(uvec2_t { (unsigned)x, (unsigned)y }, false);
            ++x;
            break;
        }
    }
    return map;
}

const std::string map_example =
R"(##############################
#  ###          #    #   #   #
#       ##### # # ## # ###   #
######  #     # # #    #     #
#    #  # ##### # # #####    #
# ## #  #         #   # #    #
# #     # ############# #    #
# ## #### # #### #      #    #
#                # ###       #
##############################)";

struct inputs_t
{
    bool forward;
    bool backward;
    bool left;
    bool right;
    bool turn_left;
    bool turn_right;
};

inputs_t handle_input()
{
    inputs_t inputs = {};
    fd_set fds;
    struct timeval tv = {};
    char chars[256];
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    while (1)
    {
        int ready = select(1, &fds, NULL, NULL, &tv);
        if (ready == 1)
        {
            ssize_t bytes_read = read(STDIN_FILENO, chars, sizeof(chars));
            for (ssize_t i = 0; i < bytes_read; i++)
            {
                switch (chars[i])
                {
                case 'w':
                case 'W':
                    inputs.forward = true;
                    break;
                case 's':
                case 'S':
                    inputs.backward = true;
                    break;
                case 'a':
                case 'A':
                    inputs.left = true;
                    break;
                case 'd':
                case 'D':
                    inputs.right = true;
                    break;
                case 'q':
                case 'Q':
                    inputs.turn_left = true;
                    break;
                case 'e':
                case 'E':
                    inputs.turn_right = true;
                    break;
                default:
                    // ignore
                    break;
                }
            }
        }
        else
        {
            break;
        }
    }
    return inputs;
}




int main()
{
    std::ofstream logfile("labyrinth.log");
    if(!isatty(STDOUT_FILENO)) {
        logfile << "STDOUT not a tty!\n";
        exit(1);
    }
    struct winsize tty_size;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &tty_size);
    size_t ascii_width = tty_size.ws_col, ascii_height = tty_size.ws_row - 2;

    setup_tty();
    atexit(restore_tty);
    signal(SIGINT, signal_cleanup);

    init_wall_texture();

    Map map = Map::from_string(map_example);
    Renderer r{std::move(map), uvec2_t {(unsigned)ascii_width, (unsigned)ascii_height * 2}};
    auto target_frame_duration = std::chrono::milliseconds(1000) / 30;

    vec2_t player_pos = {1.5, 1.5};
    vec2_t player_dir = {0.0, 1.0};
    float movement_speed = 5.0f;
    float movement_factor = target_frame_duration.count() * movement_speed / 1000.0f;
    float rotation_speed = M_PI_2f32;
    float rotation_factor = target_frame_duration.count() * rotation_speed / 1000.0f;

    while (true)
    {
        auto frame_start = std::chrono::high_resolution_clock::now();
        r.draw_from(player_pos, player_dir);
        auto frame_end = std::chrono::high_resolution_clock::now();
        auto real_frame_duration = (frame_end - frame_start);
        if (target_frame_duration > real_frame_duration)
        {
            std::this_thread::sleep_for(target_frame_duration - real_frame_duration);
        }
        else
        {
            logfile << "Too slow! Target duration: " << target_frame_duration.count()
                    << " Real duration: " << std::chrono::duration_cast<std::chrono::milliseconds>(real_frame_duration).count()
                    << "\n";
        }
        inputs_t inputs = handle_input();
        vec2_t old_pos = player_pos;
        if(inputs.forward) {
            player_pos = vec2_add(player_pos, vec2_mul_scalar(player_dir, movement_factor));
        }
        if(inputs.backward) {
            player_pos = vec2_sub(player_pos, vec2_mul_scalar(player_dir, movement_factor));
        }
        if(inputs.left) {
            player_pos = vec2_sub(player_pos, vec2_mul_scalar(vec2_rotate(player_dir, M_PI_2f32), rotation_factor));
        }
        if(inputs.right) {
            player_pos = vec2_add(player_pos, vec2_mul_scalar(vec2_rotate(player_dir, M_PI_2f32), rotation_factor));
        }
        if(inputs.turn_left) {
            player_dir = vec2_rotate(player_dir, -rotation_factor);
        }
        if(inputs.turn_right) {
            player_dir = vec2_rotate(player_dir, rotation_factor);
        }
        if(r.map().check(uvec2_t {(unsigned)player_pos.first, (unsigned)player_pos.second})) {
            player_pos = old_pos;
        }

    }
}
