/*
 * 双 BMM150 磁力计服务
 *
 * 10 Hz 采样，维护滤波后的双路快照、相对基线的特征，并把
 * APPROACH / CONTACT / RELEASE 事件回调给订阅者。
 *
 * 不依赖 LVGL；UI 线程可以直接读取快照或订阅事件。
 */

#ifndef __MAG_SERVICE_H__
#define __MAG_SERVICE_H__

#include <stdint.h>
#include "bmm150.h"
#include "mag_classifier.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    bmm150_sample_t right;  /* 0x11，滤波后 */
    bmm150_sample_t left;   /* 0x12，滤波后 */
    uint32_t tick_ms;
    uint8_t valid;
} mag_pair_t;

/* 回调在采样线程上下文执行，不能做阻塞操作 */
typedef void (*mag_event_cb_t)(const mag_event_t *evt, void *ctx);

/* 创建采样线程；重复调用无副作用 */
int mag_service_init(void);

/* 读取最新双路快照；valid = 0 表示还没有有效样本 */
int mag_service_get(mag_pair_t *out);

/* 读取相对基线的特征；基线未就绪时返回 -1 */
int mag_service_get_features(mag_features_t *out);

/* 当前已确认的边缘掩码；无磁吸时为 0 */
uint8_t mag_service_get_edges(void);

/* 重新采集基线（要求当前没有磁吸） */
int mag_service_calibrate(void);

/* 订阅事件；cb 为 NULL 时取消订阅 */
int mag_service_subscribe(mag_event_cb_t cb, void *ctx);

#ifdef __cplusplus
}
#endif

#endif /* __MAG_SERVICE_H__ */
