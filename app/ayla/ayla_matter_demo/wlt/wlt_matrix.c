/**
 * @file wlt_matrix.c
 * @brief WS2812B 矩阵灯效驱动 (11x7)
 *
 * 矩阵布局:
 * - 11列 x 7行 = 77颗灯珠
 * - 最下面一行是索引 0-10，最上面一行是索引 66-76
 * - 每行从右到左排列（索引0在最右下方）
 *
 * 坐标映射:
 * - 给定索引 idx: x = 10 - (idx % 11), y = idx / 11
 * - 给定坐标 (x, y): idx = y * 11 + (10 - x)
 * - x: 0-10 (左到右)
 * - y: 0-6 (下到上)
 */

#include "wlt_matrix.h"
#include "iotalink_ws2812.h"
#include "iotalink_control.h"
#include <stdlib.h>

// 外部变量声明
extern light_ctrl_data_t sg_light_ctrl_data;

// ==================== 矩阵配置 ====================
#define MATRIX_WIDTH   11    // 列数
#define MATRIX_HEIGHT   7     // 行数
#define MATRIX_SIZE     (MATRIX_WIDTH * MATRIX_HEIGHT)  // 77

// ==================== 雪花效果参数 ====================
#define SNOWFLAKE_COUNT    15   // 雪花粒子数量（调少）
#define SNOW_SPEED_MIN     1    // 最小下落速度
#define SNOW_SPEED_MAX     2    // 最大下落速度（调慢）

// ==================== 内部函数声明 ====================
static void matrix_set_pixel_xy(uint8_t x, uint8_t y, uint8_t r, uint8_t g, uint8_t b);
static void matrix_set_pixel_idx(uint16_t idx, uint8_t r, uint8_t g, uint8_t b);
static void matrix_clear_all(void);
static void matrix_scroll_up(void);
static void matrix_fade_all(uint8_t factor);

// ==================== 矩阵坐标转换 ====================

/**
 * @brief 根据索引设置像素颜色
 * @param idx 灯珠索引 (0-76)
 */
static void matrix_set_pixel_idx(uint16_t idx, uint8_t r, uint8_t g, uint8_t b)
{
    if (idx < MATRIX_SIZE) {
        rgb_colorful_buffer_set(idx, r, g, b);
    }
}

/**
 * @brief 根据XY坐标设置像素颜色
 * @param x 列坐标 (0-10, 左到右)
 * @param y 行坐标 (0-6, 下到上)
 */
static void matrix_set_pixel_xy(uint8_t x, uint8_t y, uint8_t r, uint8_t g, uint8_t b)
{
    if (x < MATRIX_WIDTH && y < MATRIX_HEIGHT) {
        uint16_t idx = y * MATRIX_WIDTH + (MATRIX_WIDTH - 1 - x);
        matrix_set_pixel_idx(idx, r, g, b);
    }
}

/**
 * @brief 获取XY坐标对应的索引
 */
static uint16_t matrix_xy_to_idx(uint8_t x, uint8_t y)
{
    if (x < MATRIX_WIDTH && y < MATRIX_HEIGHT) {
        return y * MATRIX_WIDTH + (MATRIX_WIDTH - 1 - x);
    }
    return 0;
}

/**
 * @brief 清空所有像素
 */
static void matrix_clear_all(void)
{
    rgb_colorful_buffer_clean();
}

/**
 * @brief 所有像素向上滚动一行
 * @note 从顶行开始处理，避免覆盖还未移动的像素
 */
static void matrix_scroll_up(void)
{
    uint8_t y, x;

    // 从顶行开始向下处理，将颜色向上移动一行
    for (y = MATRIX_HEIGHT - 1; y > 0; y--) {
        for (x = 0; x < MATRIX_WIDTH; x++) {
            uint16_t idx_to = y * MATRIX_WIDTH + (MATRIX_WIDTH - 1 - x);
            uint16_t idx_from = (y - 1) * MATRIX_WIDTH + (MATRIX_WIDTH - 1 - x);

            rgb_colorful_values[idx_to][0] = rgb_colorful_values[idx_from][0];
            rgb_colorful_values[idx_to][1] = rgb_colorful_values[idx_from][1];
            rgb_colorful_values[idx_to][2] = rgb_colorful_values[idx_from][2];
        }
    }

    // 清空底行
    for (x = 0; x < MATRIX_WIDTH; x++) {
        uint16_t idx = MATRIX_WIDTH - 1 - x;
        rgb_colorful_buffer_set(idx, 0, 0, 0);
    }
}

/**
 * @brief 所有像素衰减（模拟拖尾效果）
 * @param factor 衰减因子，越大衰减越慢 (0-255)
 */
static void matrix_fade_all(uint8_t factor)
{
    uint8_t y, x;

    for (y = 0; y < MATRIX_HEIGHT; y++) {
        for (x = 0; x < MATRIX_WIDTH; x++) {
            uint16_t idx = y * MATRIX_WIDTH + (MATRIX_WIDTH - 1 - x);
            rgb_colorful_values[idx][0] = (rgb_colorful_values[idx][0] * factor) >> 8;
            rgb_colorful_values[idx][1] = (rgb_colorful_values[idx][1] * factor) >> 8;
            rgb_colorful_values[idx][2] = (rgb_colorful_values[idx][2] * factor) >> 8;
        }
    }
}

// ==================== 火焰效果 ====================

/**
 * @brief 根据热度值获取火焰RGB颜色
 */
static void heat_to_fire_color(uint8_t heat, uint8_t *r, uint8_t *g, uint8_t *b)
{
    if (heat > 220) {
        *r = 255; *g = 255; *b = 255;
    } else if (heat > 180) {
        *r = 255;
        *g = 200 + ((heat - 180) * 55) / 75;
        *b = 50;
    } else if (heat > 140) {
        *r = 255;
        *g = 100 + ((heat - 140) * 100) / 40;
        *b = 0;
    } else if (heat > 100) {
        *r = 255;
        *g = ((heat - 100) * 100) / 40;
        *b = 0;
    } else if (heat > 60) {
        *r = 100 + ((heat - 60) * 155) / 40;
        *g = 0;
        *b = 0;
    } else if (heat > 20) {
        *r = 50 + ((heat - 20) * 50) / 40;
        *g = 0;
        *b = 0;
    } else {
        *r = heat * 2;
        *g = 0;
        *b = 0;
    }
}

/**
 * @brief 矩阵火焰效果
 * @details 火焰从底部产生，向上升腾并逐渐冷却
 * - 行0(底部)：火焰源头，红橙色
 * - 行6(顶部)：火焰消失，黑色
 */
void wlt_matrix_fire_mode(void)
{
    static uint8_t heat[MATRIX_WIDTH][MATRIX_HEIGHT];
    uint8_t r, g, b;
    uint8_t x, y;

    // 1. 火焰物理模拟：热量向上扩散
    for (x = 0; x < MATRIX_WIDTH; x++) {
        for (y = MATRIX_HEIGHT - 1; y > 0; y--) {
            uint16_t above = heat[x][y - 1];
            uint16_t current = heat[x][y];

            if (above > current) {
                heat[x][y] = (above + current * 2) / 3;
            } else {
                heat[x][y] = above / 2 + current / 2;
            }

            // 随机波动
            if ((rand() % 100) > 70) {
                heat[x][y] = heat[x][y] * 9 / 10;
            }
        }

        // 2. 底部热量随机产生（火焰源）
        uint16_t rand_val = rand() % 256;
        if (rand_val > 100) {
            heat[x][0] = rand_val;
        } else if (heat[x][0] > 10) {
            heat[x][0] -= 10;
        } else {
            heat[x][0] = 0;
        }

        if (heat[x][0] < 150) {
            heat[x][0] += rand() % 50;
        }
        if (heat[x][0] > 255) heat[x][0] = 255;
    }

    // 3. 根据热量设置颜色
    for (y = MATRIX_HEIGHT - 1; y > 0; y--) {
        for (x = 0; x < MATRIX_WIDTH; x++) {
            uint8_t h = heat[x][y];
            heat_to_fire_color(h, &r, &g, &b);

            // 越往上越暗
            uint8_t fade = (y * 200) / MATRIX_HEIGHT + 55;
            r = (r * fade) / 256;
            g = (g * fade) / 256;
            b = (b * fade) / 256;

            matrix_set_pixel_xy(x, y, r, g, b);
        }
    }

    // 4. 底部火焰源
    for (x = 0; x < MATRIX_WIDTH; x++) {
        uint8_t h = heat[x][0];
        heat_to_fire_color(h, &r, &g, &b);
        matrix_set_pixel_xy(x, 0, r, g, b);
    }

    rgb_value_sync();
}

// ==================== 雪花效果 ====================

/**
 * @brief 雪花粒子结构
 */
typedef struct {
    uint8_t x;           // 列位置 (0-10)
    uint8_t y;           // 行位置 (0-255, 浮点用整数表示)
    uint8_t speed;       // 下落速度
    uint8_t size;        // 雪花大小 (亮度)
    uint8_t active;      // 是否激活
} snowflake_t;

/**
 * @brief 矩阵雪花效果
 * @details 白色雪花从顶部飘落，有大小和速度差异
 */
void wlt_matrix_snow_mode(void)
{
    static snowflake_t flakes[SNOWFLAKE_COUNT];
    static uint8_t initialized = 0;
    uint8_t i;

    // 初始化雪花
    if (!initialized) {
        for (i = 0; i < SNOWFLAKE_COUNT; i++) {
            flakes[i].active = 0;
            flakes[i].y = 0;
        }
        initialized = 1;
    }

    // 1. 衰减现有像素（产生拖尾效果）
    matrix_fade_all(200);

    // 2. 更新并绘制雪花
    for (i = 0; i < SNOWFLAKE_COUNT; i++) {
        if (flakes[i].active) {
            // 绘制雪花（带发光效果）
            uint8_t real_y = flakes[i].y >> 4;  // 转换回行坐标（每行16个单位）

            if (real_y < MATRIX_HEIGHT) {
                uint16_t idx = matrix_xy_to_idx(flakes[i].x, real_y);

                // 纯白色雪花
                rgb_colorful_values[idx][0] = flakes[i].size;
                rgb_colorful_values[idx][1] = flakes[i].size;
                rgb_colorful_values[idx][2] = flakes[i].size;
            }

            // 移动雪花：向底部移动 = y 减小（每行占16个单位）
            if (flakes[i].y >= flakes[i].speed * 4) {
                flakes[i].y -= flakes[i].speed * 4;
            } else {
                flakes[i].y = 0;
            }

            // 雪花超出底部范围，标记为不活跃
            if (flakes[i].y == 0) {
                flakes[i].active = 0;
            }
        } else {
            // 随机激活新雪花
            if ((rand() % 100) < 12) {  // 12% 概率产生新雪花（调稀疏）
                flakes[i].x = rand() % MATRIX_WIDTH;
                flakes[i].y = (MATRIX_HEIGHT - 1) << 4;  // 从顶部开始（6*16=96）
                flakes[i].speed = SNOW_SPEED_MIN + (rand() % (SNOW_SPEED_MAX - SNOW_SPEED_MIN + 1));
                flakes[i].size = 180 + (rand() % 76);   // 180-255 亮度（调亮）
                flakes[i].active = 1;
            }
        }
    }

    rgb_value_sync();
}

// ==================== 彩虹效果 ====================

/**
 * @brief 矩阵彩虹效果
 */
void wlt_matrix_rainbow_mode(void)
{
    static uint16_t hue = 0;
    uint8_t x, y;
    uint8_t r, g, b;

    for (y = 0; y < MATRIX_HEIGHT; y++) {
        for (x = 0; x < MATRIX_WIDTH; x++) {
            uint16_t current_hue = (hue + x * 15 + y * 20) % 360;
            hsv_to_rgb(&r, &g, &b, current_hue, 100, 80);
            matrix_set_pixel_xy(x, y, r, g, b);
        }
    }

    hue = (hue + 5) % 360;
    rgb_value_sync();
}

// ==================== 单色填充效果 ====================

/**
 * @brief 矩阵单色填充
 * @param hue HSV色调值 (0-360)
 * @param brightness 亮度 (0-100)
 */
void wlt_matrix_single_color_mode(uint16_t hue, uint8_t brightness)
{
    uint8_t x, y;
    uint8_t r, g, b;

    hsv_to_rgb(&r, &g, &b, hue, 100, brightness);

    for (y = 0; y < MATRIX_HEIGHT; y++) {
        for (x = 0; x < MATRIX_WIDTH; x++) {
            matrix_set_pixel_xy(x, y, r, g, b);
        }
    }

    rgb_value_sync();
}

// ==================== 测试效果 ====================

/**
 * @brief 矩阵测试效果 - 显示十字和角落标记
 */
void wlt_matrix_test_mode(void)
{
    uint8_t x, y;

    matrix_clear_all();

    // 中心十字
    for (x = 0; x < MATRIX_WIDTH; x++) {
        matrix_set_pixel_xy(x, MATRIX_HEIGHT / 2, 255, 0, 0);    // 红色横线
    }
    for (y = 0; y < MATRIX_HEIGHT; y++) {
        matrix_set_pixel_xy(MATRIX_WIDTH / 2, y, 0, 255, 0);    // 绿色竖线
    }

    // 四角标记
    matrix_set_pixel_xy(0, 0, 255, 255, 0);                     // 左下：黄色
    matrix_set_pixel_xy(MATRIX_WIDTH - 1, 0, 0, 255, 255);     // 右下：青色
    matrix_set_pixel_xy(0, MATRIX_HEIGHT - 1, 255, 0, 255);    // 左上：紫色
    matrix_set_pixel_xy(MATRIX_WIDTH - 1, MATRIX_HEIGHT - 1, 255, 255, 255);  // 右上：白色

    rgb_value_sync();
}

// ==================== 流星效果 ====================

/**
 * @brief 矩阵流星效果
 */
void wlt_matrix_meteor_mode(void)
{
    static uint8_t meteor_x = 0;
    static uint8_t meteor_y = 0;
    static uint8_t trail_pos = 0;
    uint8_t x;

    // 衰减拖尾
    matrix_fade_all(180);

    // 流星主体
    for (x = 0; x < 6; x++) {
        if (meteor_x >= x && meteor_x - x < MATRIX_WIDTH && meteor_y < MATRIX_HEIGHT) {
            uint8_t brightness = 255 * (6 - x) / 6;
            uint8_t r = brightness;
            uint8_t g = (brightness * 8) / 10;
            uint8_t b = (brightness * 2) / 10;
            matrix_set_pixel_xy(meteor_x - x, meteor_y, r, g, b);
        }
    }

    // 移动流星
    trail_pos++;
    if (trail_pos >= 3) {
        trail_pos = 0;
        meteor_x++;

        if (meteor_x >= MATRIX_WIDTH + 6) {
            meteor_x = 0;
            meteor_y++;
            if (meteor_y >= MATRIX_HEIGHT) {
                meteor_y = 0;
            }
        }
    }

    rgb_value_sync();
}

// ==================== 呼吸灯效果 ====================

/**
 * @brief 矩阵呼吸灯效果
 */
void wlt_matrix_breath_mode(void)
{
    static uint8_t breath_value = 0;
    static int8_t breath_direction = 1;
    uint8_t x, y;
    uint8_t r, g, b;

    breath_value += breath_direction * 8;

    if (breath_value >= 250) {
        breath_direction = -1;
    } else if (breath_value <= 5) {
        breath_direction = 1;
    }

    hsv_to_rgb(&r, &g, &b, 0, 100, breath_value / 2);

    for (y = 0; y < MATRIX_HEIGHT; y++) {
        for (x = 0; x < MATRIX_WIDTH; x++) {
            matrix_set_pixel_xy(x, y, r, g, b);
        }
    }

    rgb_value_sync();
}

// ==================== 星空效果 ====================

/**
 * @brief 星星粒子结构
 */
typedef struct {
    uint8_t x;           // 列位置
    uint8_t y;           // 行位置
    uint8_t brightness;  // 当前亮度
    uint8_t target_brightness;  // 目标亮度
    uint8_t twinkle_speed;     // 闪烁速度
    uint8_t active;      // 是否激活
} star_t;

/**
 * @brief 矩阵星空效果
 * @details 深蓝色背景上的星星闪烁效果
 */
void wlt_matrix_star_mode(void)
{
    static star_t stars[30];
    static uint8_t initialized = 0;
    uint8_t i;
    uint8_t x, y;

    // 初始化星星
    if (!initialized) {
        for (i = 0; i < 30; i++) {
            stars[i].active = 0;
        }
        initialized = 1;
    }

    // 1. 全部清空（深蓝色背景）
    for (y = 0; y < MATRIX_HEIGHT; y++) {
        for (x = 0; x < MATRIX_WIDTH; x++) {
            // 深蓝色背景
            matrix_set_pixel_xy(x, y, 0, 0, 30);
        }
    }

    // 2. 更新并绘制星星
    for (i = 0; i < 30; i++) {
        if (!stars[i].active) {
            // 随机激活新星星
            if ((rand() % 100) < 3) {  // 3% 概率激活
                stars[i].x = rand() % MATRIX_WIDTH;
                stars[i].y = rand() % MATRIX_HEIGHT;
                stars[i].brightness = 0;
                stars[i].target_brightness = 100 + (rand() % 156);  // 100-255
                stars[i].twinkle_speed = 10 + (rand() % 20);  // 闪烁速度
                stars[i].active = 1;
            }
        } else {
            // 星星闪烁：向目标亮度靠近
            if (stars[i].brightness < stars[i].target_brightness) {
                stars[i].brightness += stars[i].twinkle_speed;
                if (stars[i].brightness > stars[i].target_brightness) {
                    stars[i].brightness = stars[i].target_brightness;
                }
            } else if (stars[i].brightness > stars[i].target_brightness) {
                stars[i].brightness -= stars[i].twinkle_speed;
                if (stars[i].brightness < stars[i].target_brightness) {
                    stars[i].brightness = stars[i].target_brightness;
                }
            }

            // 随机改变目标亮度（产生闪烁效果）
            if ((rand() % 100) < 5) {
                if (stars[i].brightness > 150) {
                    stars[i].target_brightness = 50 + (rand() % 50);
                } else {
                    stars[i].target_brightness = 150 + (rand() % 105);
                }
            }

            // 绘制星星（白色带蓝色光晕）
            uint16_t idx = matrix_xy_to_idx(stars[i].x, stars[i].y);
            rgb_colorful_values[idx][0] = stars[i].brightness;
            rgb_colorful_values[idx][1] = stars[i].brightness;
            rgb_colorful_values[idx][2] = stars[i].brightness + 30;  // 蓝色光晕

            // 限制最大值
            if (rgb_colorful_values[idx][2] > 255) {
                rgb_colorful_values[idx][2] = 255;
            }

            // 星星完全熄灭后标记为不活跃
            if (stars[i].brightness == 0 && stars[i].target_brightness < 20) {
                stars[i].active = 0;
            }
        }
    }

    rgb_value_sync();
}

// ==================== 烟花效果 ====================

#define FIREWORK_STATE_LAUNCH    0  // 发射状态
#define FIREWORK_STATE_EXPLODE   1  // 爆炸状态
#define FIREWORK_STATE_FADE     2  // 消散状态

/**
 * @brief 烟花粒子结构
 */
typedef struct {
    uint8_t x;              // X坐标（整数）
    uint8_t y;              // Y坐标（整数）
    int8_t vx;              // X方向速度
    int8_t vy;              // Y方向速度
    uint8_t brightness;      // 当前亮度
    uint8_t hue;            // 色调
    uint8_t state;          // 状态
    uint8_t life;           // 剩余寿命
} firework_particle_t;

/**
 * @brief 烟花发射器结构
 */
typedef struct {
    uint8_t x;              // 发射X坐标
    uint8_t y;              // 当前Y坐标
    int8_t vy;              // 上升速度
    uint8_t target_y;       // 目标爆炸高度
    uint8_t state;          // 0=发射中, 1=爆炸
    uint8_t hue;            // 烟花颜色
    uint8_t active;         // 是否激活
} firework_launcher_t;

#define MAX_FIREWORK_PARTICLES  50
#define MAX_FIREWORK_LAUNCHERS  3

/**
 * @brief 矩阵烟花效果
 * @details 烟花从底部发射，上升后爆炸，粒子散开并逐渐消失
 */
void wlt_matrix_firework_mode(void)
{
    static firework_launcher_t launchers[MAX_FIREWORK_LAUNCHERS];
    static firework_particle_t particles[MAX_FIREWORK_PARTICLES];
    static uint8_t initialized = 0;
    uint8_t i, j;
    uint8_t x, y;

    // 初始化
    if (!initialized) {
        for (i = 0; i < MAX_FIREWORK_LAUNCHERS; i++) {
            launchers[i].active = 0;
        }
        for (i = 0; i < MAX_FIREWORK_PARTICLES; i++) {
            particles[i].state = FIREWORK_STATE_FADE;
            particles[i].life = 0;
        }
        initialized = 1;
    }

    // 1. 清空背景（黑色）
    for (y = 0; y < MATRIX_HEIGHT; y++) {
        for (x = 0; x < MATRIX_WIDTH; x++) {
            matrix_set_pixel_xy(x, y, 0, 0, 0);
        }
    }

    // 2. 更新发射器
    for (i = 0; i < MAX_FIREWORK_LAUNCHERS; i++) {
        if (!launchers[i].active) {
            // 随机激活新发射器
            if ((rand() % 100) < 5) {  // 5% 概率发射
                launchers[i].x = 1 + (rand() % (MATRIX_WIDTH - 2));  // 留边距
                launchers[i].y = 0;
                launchers[i].vy = 1 + (rand() % 2);  // 上升速度1-2
                launchers[i].target_y = 2 + (rand() % (MATRIX_HEIGHT - 3));  // 爆炸高度
                launchers[i].hue = rand() % 360;  // 随机色调
                launchers[i].state = FIREWORK_STATE_LAUNCH;
                launchers[i].active = 1;
            }
        } else if (launchers[i].state == FIREWORK_STATE_LAUNCH) {
            // 发射阶段：向上移动
            launchers[i].y += launchers[i].vy;

            if (launchers[i].y >= launchers[i].target_y) {
                // 爆炸：创建多个粒子
                launchers[i].state = FIREWORK_STATE_EXPLODE;

                // 创建爆炸粒子
                uint8_t particle_count = 8 + (rand() % 8);  // 8-15个粒子
                for (j = 0; j < particle_count; j++) {
                    // 找一个空闲粒子
                    for (uint8_t k = 0; k < MAX_FIREWORK_PARTICLES; k++) {
                        if (particles[k].life == 0) {
                            particles[k].x = launchers[i].x;
                            particles[k].y = launchers[i].y;

                            // 随机方向散开
                            particles[k].vx = (rand() % 3) - 1;  // -1, 0, 1
                            particles[k].vy = (rand() % 3) - 1;  // -1, 0, 1

                            // 颜色基于发射器色调（使用临时变量避免溢出）
                            uint16_t temp_hue = (uint16_t)launchers[i].hue + (rand() % 60) - 30;
                            if (temp_hue >= 360) temp_hue -= 360;
                            particles[k].hue = (uint8_t)temp_hue;

                            particles[k].brightness = 255;
                            particles[k].state = FIREWORK_STATE_EXPLODE;
                            particles[k].life = 30 + (rand() % 30);  // 生命周期
                            break;
                        }
                    }
                }

                // 发光效果：爆炸中心亮一下
                matrix_set_pixel_xy(launchers[i].x, launchers[i].y, 255, 255, 255);
            } else {
                // 绘制发射轨迹
                uint8_t trail_y = launchers[i].y;
                if (trail_y < MATRIX_HEIGHT) {
                    uint8_t r, g, b;
                    hsv_to_rgb(&r, &g, &b, launchers[i].hue, 100, 80);
                    matrix_set_pixel_xy(launchers[i].x, trail_y, r, g, b);
                }
            }
        }

        // 发射完成后标记为不活跃
        if (launchers[i].state == FIREWORK_STATE_EXPLODE) {
            launchers[i].active = 0;
        }
    }

    // 3. 更新粒子
    for (i = 0; i < MAX_FIREWORK_PARTICLES; i++) {
        if (particles[i].life > 0) {
            // 绘制粒子
            uint8_t px = particles[i].x;
            uint8_t py = particles[i].y;

            if (px < MATRIX_WIDTH && py < MATRIX_HEIGHT) {
                uint8_t r, g, b;
                hsv_to_rgb(&r, &g, &b, particles[i].hue, 100,
                          (particles[i].brightness * particles[i].life) / 100);
                matrix_set_pixel_xy(px, py, r, g, b);
            }

            // 移动粒子（带重力效果）
            particles[i].x += particles[i].vx;
            particles[i].y += particles[i].vy;

            // 边界检查
            if (particles[i].x == 0) particles[i].vx = 1;
            if (particles[i].x >= MATRIX_WIDTH - 1) particles[i].vx = -1;
            if (particles[i].y >= MATRIX_HEIGHT - 1) {
                particles[i].vy = -particles[i].vy / 2;  // 碰底反弹并减速
                particles[i].y = MATRIX_HEIGHT - 1;
            }

            // 重力和摩擦
            particles[i].vy += 0;  // 简化版：无重力，水平散开
            particles[i].vx = particles[i].vx * 9 / 10;  // 摩擦力

            // 生命周期减少
            particles[i].life--;

            // 亮度随生命周期降低
            if (particles[i].brightness > 5) {
                particles[i].brightness -= 5;
            }
        }
    }

    rgb_value_sync();
}

// ==================== 清除效果 ====================

/**
 * @brief 清除矩阵所有像素
 */
void wlt_matrix_clear(void)
{
    matrix_clear_all();
    rgb_value_sync();
}

// ==================== 日出效果 ====================

// 日出状态变量
static uint8_t g_sunup_step = 0;       // 日出进度 (0-100)
static uint8_t g_sunup_phase = 0;      // 0=准备, 1=进行中, 2=完成
static uint8_t g_last_magicunit = 0;    // 上次场景号

/**
 * @brief 日出效果 - 太阳从地平线升起
 * @details 参考火焰效果的红橙色调
 */
void wlt_mode_sunup(void)
{
    uint8_t x, y;
    uint8_t r, g, b;

    // 检测重新触发：从头开始
    if (sg_light_ctrl_data.magicunit != g_last_magicunit) {
        g_sunup_step = 0;
        g_last_magicunit = sg_light_ctrl_data.magicunit;
    }

    // 更新进度：每帧+2，完成后从头循环
    g_sunup_step += 2;
    if (g_sunup_step >= 100) {
        g_sunup_step = 0;  // 循环从头开始
    }

    uint16_t p = g_sunup_step;

    // 动态渐变位置：从底部向顶部移动 (0 -> MATRIX_HEIGHT)
    uint8_t horizon_y = (p * (MATRIX_HEIGHT + 1)) / 100;  // 0 -> 8

    // ========== 1. 绘制天空 ==========
    for (y = 0; y < MATRIX_HEIGHT; y++) {
        for (x = 0; x < MATRIX_WIDTH; x++) {
            if (y >= horizon_y) {
                // 渐变线以上：顶部深红色
                r = 50; g = 0; b = 0;
            } else if (y >= horizon_y - 2) {
                // 渐变区域：橙红色
                r = 255; g = 40; b = 0;
            } else {
                // 底部地平线：最亮的橙红色
                r = 255; g = 80; b = 0;
            }
            matrix_set_pixel_xy(x, y, r, g, b);
        }
    }

    // ========== 2. 绘制太阳 ==========
    uint8_t sun_y;
    if (horizon_y >= 2) {
        sun_y = horizon_y - 1;
    } else {
        sun_y = 1;
    }

    if (sun_y < MATRIX_HEIGHT) {
        uint8_t sun_x = MATRIX_WIDTH / 2;

        // 太阳颜色
        uint8_t sun_r = 255;
        uint8_t sun_g = 100;
        uint8_t sun_b = 0;

        matrix_set_pixel_xy(sun_x, sun_y, sun_r, sun_g, sun_b);

        // 太阳光晕
        if (sun_y > 0) {
            matrix_set_pixel_xy(sun_x, sun_y - 1, 200, 50, 0);
        }
        if (sun_y > 1) {
            matrix_set_pixel_xy(sun_x, sun_y - 2, 150, 30, 0);
        }
    }

    rgb_value_sync();
}

// ==================== 日落效果 ====================

// 日落状态变量
static uint8_t g_sundown_step = 0;       // 日落进度 (0-100)
static uint8_t g_last_magicunit_sundown = 0;  // 上次场景号

/**
 * @brief 日落效果 - 与日出相反
 * @details 与日出效果相反：
 * - 动态：从顶部向底部移动
 * - 颜色：橙色 -> 暗红色
 */
void wlt_mode_sundown(void)
{
    uint8_t x, y;
    uint8_t r, g, b;

    // 检测重新触发：从头开始
    if (sg_light_ctrl_data.magicunit != g_last_magicunit_sundown) {
        g_sundown_step = 0;
        g_last_magicunit_sundown = sg_light_ctrl_data.magicunit;
    }

    // 更新进度：每帧+2，完成后从头循环
    g_sundown_step += 2;
    if (g_sundown_step >= 100) {
        g_sundown_step = 0;
    }

    uint16_t p = g_sundown_step;

    // 动态渐变位置：从顶部向底部移动
    uint8_t horizon_y = MATRIX_HEIGHT - (p * (MATRIX_HEIGHT + 1)) / 100;  // 7 -> 0

    // ========== 1. 绘制天空 ==========
    for (y = 0; y < MATRIX_HEIGHT; y++) {
        for (x = 0; x < MATRIX_WIDTH; x++) {
            // 与日出相反：顶部亮橙色，底部暗红色
            if (y <= horizon_y) {
                // 渐变线以上：亮橙色
                r = 255; g = 80; b = 0;
            } else if (y <= horizon_y + 2) {
                // 渐变区域：暗红色
                r = 50; g = 0; b = 0;
            } else {
                // 底部：深暗色
                r = 10; g = 0; b = 0;
            }
            matrix_set_pixel_xy(x, y, r, g, b);
        }
    }

    // ========== 2. 绘制落日 ==========
    uint8_t sun_y;
    if (horizon_y + 1 < MATRIX_HEIGHT) {
        sun_y = horizon_y + 1;
    } else {
        sun_y = MATRIX_HEIGHT - 2;
    }

    if (sun_y < MATRIX_HEIGHT && sun_y > 0) {
        uint8_t sun_x = MATRIX_WIDTH / 2;

        // 落日颜色：暗橙红色
        uint8_t sun_r = 200;
        uint8_t sun_g = 50;
        uint8_t sun_b = 0;

        matrix_set_pixel_xy(sun_x, sun_y, sun_r, sun_g, sun_b);

        // 落日光晕
        if (sun_y < MATRIX_HEIGHT - 1) {
            matrix_set_pixel_xy(sun_x, sun_y + 1, 150, 30, 0);
        }
        if (sun_y < MATRIX_HEIGHT - 2) {
            matrix_set_pixel_xy(sun_x, sun_y + 2, 80, 10, 0);
        }
    }

    rgb_value_sync();
}

// ==================== 彩虹效果 ====================

// 彩虹状态变量
static uint16_t g_rainbow_step = 0;  // 彩虹流动进度 (0-360)

/**
 * @brief 彩虹效果 - 平滑彩虹流动
 * @details 
 * - 连续色调渐变（0-360度），颜色平滑过渡
 * - 环形连接：第11列与第1列相邻
 */
void wlt_matrix_rainbow(void)
{
    uint8_t x, y;
    uint8_t r, g, b;

    // 更新进度：每帧+5，360度循环
    g_rainbow_step += 5;
    if (g_rainbow_step >= 360) {
        g_rainbow_step = 0;
    }

    // 绘制彩虹：连续色调渐变
    for (y = 0; y < MATRIX_HEIGHT; y++) {
        for (x = 0; x < MATRIX_WIDTH; x++) {
            // 计算色调：连续渐变 (0-360度)
            // 环形：x=0 和 x=10 是相邻的
            uint16_t hue = (g_rainbow_step + (x * 360) / MATRIX_WIDTH) % 360;

            // 饱和度和亮度
            uint8_t saturation = 100;
            uint8_t brightness = 80;

            // 每行稍有偏移，产生立体感
            if (y > 0) {
                hue = (hue + y * 20) % 360;  // 每行颜色偏移
                brightness = 80 - y * 5;  // 越往上越暗
            }

            hsv_to_rgb(&r, &g, &b, hue, saturation, brightness);
            matrix_set_pixel_xy(x, y, r, g, b);
        }
    }

    rgb_value_sync();
}

// ==================== 烛光效果 ====================

// 烛光状态变量
static uint8_t g_candle_brightness = 70;  // 当前亮度
static int8_t g_candle_delta = 1;        // 亮度变化增量

/**
 * @brief 烛光效果 - 温暖的烛火闪烁
 * @details 模拟蜡烛火焰：
 * - 橙黄色温暖光芒
 * - 亮度缓慢柔和变化
 * - 中心最亮，向边缘渐暗
 */
void wlt_matrix_candle(void)
{
    uint8_t x, y;
    uint8_t r, g, b;

    // 更新亮度：缓慢变化
    g_candle_brightness += g_candle_delta;

    // 限制亮度范围 (55-85)
    if (g_candle_brightness >= 85) {
        g_candle_brightness = 85;
        g_candle_delta = -1;
    } else if (g_candle_brightness <= 55) {
        g_candle_brightness = 55;
        g_candle_delta = 1;
    }

    // 偶尔随机变化方向，产生自然感
    if ((rand() % 30) == 0) {
        g_candle_delta = (rand() % 2) ? 1 : -1;
    }

    // 烛光中心位置（稍微偏下，模拟火焰）
    uint8_t center_x = MATRIX_WIDTH / 2;
    uint8_t center_y = 2;

    for (y = 0; y < MATRIX_HEIGHT; y++) {
        for (x = 0; x < MATRIX_WIDTH; x++) {
            // 计算到中心的距离
            int8_t dx = (int8_t)x - (int8_t)center_x;
            int8_t dy = (int8_t)y - (int8_t)center_y;
            uint8_t dist = (dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy);

            // 距离越远，亮度越低
            uint8_t dist_factor;
            if (dist <= 1) {
                dist_factor = 100;
            } else if (dist == 2) {
                dist_factor = 70;
            } else if (dist == 3) {
                dist_factor = 40;
            } else {
                dist_factor = 15;
            }

            // 最终亮度
            uint8_t final_brightness = (g_candle_brightness * dist_factor) / 100;

            // 烛光颜色：RGB 橙黄色
            uint8_t candle_r = 255;
            uint8_t candle_g = 130;
            uint8_t candle_b = 0;

            // 根据亮度缩放
            r = (candle_r * final_brightness) / 100;
            g = (candle_g * final_brightness) / 100;
            b = 0;

            matrix_set_pixel_xy(x, y, r, g, b);
        }
    }

    rgb_value_sync();
}

// ==================== 结婚纪念日效果 ====================

// 结婚纪念日状态变量
static uint8_t g_wedding_step = 0;      // 动画步进
static uint8_t g_wedding_brightness = 0; // 平滑亮度
static uint8_t g_wedding_dir = 1;       // 亮度方向（0=下降，1=上升）
static uint8_t g_wedding_color_idx = 0;  // 当前颜色索引

// 结婚纪念日颜色数组：红、橙、黄、紫、粉
static const uint8_t g_wedding_colors[5][3] = {
    {255, 0, 50},    // 红色
    {255, 80, 0},    // 橙色
    {255, 150, 0},   // 黄色
    {180, 0, 255},   // 紫色
    {255, 50, 150}   // 粉色
};

/**
 * @brief 检查是否为心形区域
 * @param x X坐标
 * @param y Y坐标
 * @param heart_x 心形中心X
 * @param heart_y 心形中心Y
 * @return 1如果在心形内，0如果不在
 */
static uint8_t is_heart(uint8_t x, uint8_t y, uint8_t heart_x, uint8_t heart_y)
{
    // 心形近似算法
    int8_t dx = (int8_t)x - (int8_t)heart_x;
    int8_t dy = (int8_t)y - (int8_t)heart_y;

    // 心形上半部分（两个圆）
    if (dy <= 0) {
        // 左圆
        int8_t dx1 = dx + 1;
        int8_t dy1 = dy + 2;
        if (dx1 * dx1 + dy1 * dy1 <= 4) return 1;

        // 右圆
        int8_t dx2 = dx - 1;
        int8_t dy2 = dy + 2;
        if (dx2 * dx2 + dy2 * dy2 <= 4) return 1;
    }

    // 心形下半部分（倒三角）
    if (dy >= -3 && dy <= 0) {
        if (dx >= dy - 1 && dx <= -dy + 1) return 1;
    }

    return 0;
}

/**
 * @brief 结婚纪念日效果 - 双心跳动多色彩
 * @details 模拟两颗爱心跳动的喜庆效果：
 * - 多色彩渐变：红、橙、黄、紫、粉
 * - 双心图案
 * - 平滑过渡动画
 */
void wlt_matrix_wedding(void)
{
    uint8_t x, y;
    uint8_t r, g, b;

    // 更新动画步进
    g_wedding_step++;

    // 平滑亮度变化：使用增量方式，每帧变化2
    if (g_wedding_dir == 1) {
        // 上升阶段
        if (g_wedding_brightness < 100) {
            g_wedding_brightness += 2;
        } else {
            g_wedding_dir = 0;  // 开始下降
        }
    } else {
        // 下降阶段（更缓慢）
        if (g_wedding_brightness > 40) {
            g_wedding_brightness -= 1;
        } else {
            g_wedding_dir = 1;  // 开始上升
        }
    }

    // 每50帧切换一次颜色
    if (g_wedding_step % 50 == 0) {
        g_wedding_color_idx = (g_wedding_color_idx + 1) % 5;
    }

    // 获取当前颜色和下一个颜色（用于渐变）
    uint8_t cur_idx = g_wedding_color_idx;
    uint8_t next_idx = (g_wedding_color_idx + 1) % 5;

    // 颜色渐变因子（每帧变化，实现平滑过渡）
    uint8_t color_blend = (g_wedding_step % 50) * 5 / 1;  // 0-250 渐变

    // 清空背景
    matrix_clear_all();

    // 双心位置
    uint8_t heart1_x = 3;   // 左心
    uint8_t heart1_y = 3;
    uint8_t heart2_x = 8;   // 右心
    uint8_t heart2_y = 3;

    // 绘制双心
    for (y = 0; y < MATRIX_HEIGHT; y++) {
        for (x = 0; x < MATRIX_WIDTH; x++) {
            // 检查是否在心形内
            uint8_t in_heart1 = is_heart(x, y, heart1_x, heart1_y);
            uint8_t in_heart2 = is_heart(x, y, heart2_x, heart2_y);

            if (in_heart1 || in_heart2) {
                // 平滑颜色插值
                uint16_t base_r = g_wedding_colors[cur_idx][0];
                uint16_t base_g = g_wedding_colors[cur_idx][1];
                uint16_t base_b = g_wedding_colors[cur_idx][2];

                uint16_t next_r = g_wedding_colors[next_idx][0];
                uint16_t next_g = g_wedding_colors[next_idx][1];
                uint16_t next_b = g_wedding_colors[next_idx][2];

                // 线性插值
                r = (base_r * (250 - color_blend) + next_r * color_blend) / 250;
                g = (base_g * (250 - color_blend) + next_g * color_blend) / 250;
                b = (base_b * (250 - color_blend) + next_b * color_blend) / 250;

                // 添加心形内部亮度变化（中心更亮）
                int8_t dx1 = (int8_t)x - (int8_t)heart1_x;
                int8_t dy1 = (int8_t)y - (int8_t)heart1_y;
                int8_t dx2 = (int8_t)x - (int8_t)heart2_x;
                int8_t dy2 = (int8_t)y - (int8_t)heart2_y;
                int8_t dist = 10;  // 默认很大距离

                if (in_heart1) {
                    dist = (dx1 * dx1 + dy1 * dy1);
                }
                if (in_heart2) {
                    int8_t d = (dx2 * dx2 + dy2 * dy2);
                    if (d < dist) dist = d;
                }

                // 中心更亮的因子
                uint8_t center_bonus = (dist < 2) ? 30 : ((dist < 5) ? 15 : 0);

                // 应用心跳亮度
                uint16_t brightness = g_wedding_brightness + center_bonus;
                if (brightness > 100) brightness = 100;

                r = (r * brightness) / 100;
                g = (g * brightness) / 100;
                b = (b * brightness) / 100;

                matrix_set_pixel_xy(x, y, r, g, b);
            }
        }
    }

    // 添加平滑闪烁的装饰点（模拟喜庆光芒）
    for (y = 0; y < MATRIX_HEIGHT; y++) {
        for (x = 0; x < MATRIX_WIDTH; x++) {
            // 跳过爱心区域
            if (is_heart(x, y, heart1_x, heart1_y) || is_heart(x, y, heart2_x, heart2_y)) {
                continue;
            }

            // 使用伪随机但稳定的闪烁（基于位置和时间）
            uint8_t spark_seed = (x * 7 + y * 13 + g_wedding_step) % 30;
            if (spark_seed < 5) {
                // 使用当前颜色数组，但添加一些变化
                uint8_t color_var = (x + y + g_wedding_step) % 5;
                uint8_t spark_r = g_wedding_colors[color_var][0] * 60 / 100;
                uint8_t spark_g = g_wedding_colors[color_var][1] * 60 / 100;
                uint8_t spark_b = g_wedding_colors[color_var][2] * 60 / 100;

                matrix_set_pixel_xy(x, y, spark_r, spark_g, spark_b);
            }
        }
    }

    rgb_value_sync();
}

