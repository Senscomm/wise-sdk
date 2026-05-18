/**
 * @file wlt_matrix.h
 * @brief WS2812B 矩阵灯效驱动头文件 (11x7)
 */

#ifndef __WLT_MATRIX_H__
#define __WLT_MATRIX_H__

#include <stdint.h>

// ==================== 矩阵尺寸定义 ====================
#define MATRIX_WIDTH    11    // 列数
#define MATRIX_HEIGHT    7    // 行数
#define MATRIX_SIZE      (MATRIX_WIDTH * MATRIX_HEIGHT)  // 77

// ==================== 灯效函数声明 ====================

/**
 * @brief 矩阵火焰效果
 * @details 火焰从底部产生，向上升腾并逐渐冷却
 */
void wlt_matrix_fire_mode(void);

/**
 * @brief 矩阵雪花效果
 * @details 白色雪花从顶部飘落，有大小和速度差异
 */
void wlt_matrix_snow_mode(void);

/**
 * @brief 矩阵星空效果
 * @details 深蓝色背景上白色星星闪烁
 */
void wlt_matrix_star_mode(void);

/**
 * @brief 矩阵烟花效果
 * @details 烟花从底部发射，上升后爆炸，粒子散开并逐渐消失
 */
void wlt_matrix_firework_mode(void);

/**
 * @brief 矩阵彩虹效果
 */
void wlt_matrix_rainbow_mode(void);

/**
 * @brief 矩阵单色填充效果
 * @param hue HSV色调值 (0-360)
 * @param brightness 亮度 (0-100)
 */
void wlt_matrix_single_color_mode(uint16_t hue, uint8_t brightness);

/**
 * @brief 矩阵测试效果 - 显示十字和角落标记
 */
void wlt_matrix_test_mode(void);

/**
 * @brief 矩阵流星效果
 */
void wlt_matrix_meteor_mode(void);

/**
 * @brief 矩阵呼吸灯效果
 */
void wlt_matrix_breath_mode(void);

/**
 * @brief 清除矩阵所有像素
 */
void wlt_matrix_clear(void);

/**
 * @brief 日出效果 - 太阳从地平线升起，天空逐渐变亮
 */
void wlt_mode_sunup(void);

/**
 * @brief 日落效果 - 与日出相反
 * @details 与日出效果相反：
 * - 动态：从顶部向底部移动
 * - 颜色：橙色 -> 暗红色
 */
void wlt_mode_sundown(void);

/**
 * @brief 彩虹效果 - 七色彩虹流动
 * @details 彩虹七色（红橙黄绿青蓝紫）从左到右流动
 */
void wlt_matrix_rainbow(void);

/**
 * @brief 烛光效果 - 温暖的烛火闪烁
 * @details 模拟蜡烛火焰：
 * - 橙黄色温暖光芒
 * - 亮度随机闪烁变化
 * - 中心最亮，向边缘衰减
 */
void wlt_matrix_candle(void);

/**
 * @brief 结婚纪念日效果 - 双心跳动
 * @details 模拟两颗爱心跳动的喜庆效果：
 * - 红粉色调
 * - 双心图案
 * - 心跳闪烁动画
 */
void wlt_matrix_wedding(void);

#endif /* __WLT_MATRIX_H__ */
