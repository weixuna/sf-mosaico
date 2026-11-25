/*
 * 单机磁吸边缘分类器
 *
 * 输入是两颗 BMM150 相对基线的增量，输出边缘掩码和
 * APPROACH / CONTACT / RELEASE 事件。
 *
 * 左右规则来自 docs/magnetic_interaction_porting.md；
 * 上下使用双传感器 X/Z 共模，top_axis 可选 0=X、1=Z。
 * 默认阈值只是占位值，必须用 mag_test 实测后重新标定。
 */

#ifndef __MAG_CLASSIFIER_H__
#define __MAG_CLASSIFIER_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    MAG_EDGE_NONE   = 0,
    MAG_EDGE_TOP    = 1 << 0,
    MAG_EDGE_RIGHT  = 1 << 1,
    MAG_EDGE_BOTTOM = 1 << 2,
    MAG_EDGE_LEFT   = 1 << 3,
} mag_edge_t;

typedef enum
{
    MAG_EVT_APPROACH = 0,
    MAG_EVT_CONTACT,
    MAG_EVT_RELEASE,
} mag_event_type_t;

typedef struct
{
    mag_event_type_t type;
    uint8_t edge;           /* mag_edge_t 掩码 */
    uint8_t confidence;     /* 0..100 */
    uint32_t tick_ms;
} mag_event_t;

typedef struct
{
    int16_t right_delta[3];
    int16_t left_delta[3];
    int16_t diff[3];        /* right_delta - left_delta */
    uint32_t right_mag2;    /* |B|^2 */
    uint32_t left_mag2;
    uint32_t diff_mag2;
} mag_features_t;

typedef struct
{
    int16_t right_y_contact;
    int16_t right_y_release;
    int16_t left_y_contact;
    int16_t left_y_release;
    int16_t diff_y_contact;
    int16_t diff_y_release;
    uint8_t top_axis;       /* 0 = X，1 = Z */
    int16_t top_contact;
    int16_t top_release;
    int16_t bottom_contact;
    int16_t bottom_release;
    uint8_t debounce_contact;
    uint8_t debounce_release;
} mag_profile_t;

/* 占位阈值，单位 uT；请按本机磁铁/结构实测后修改 */
#define MAG_PROFILE_DEFAULT() {             \
    .right_y_contact  = 80,                 \
    .right_y_release  = 40,                 \
    .left_y_contact   = 80,                 \
    .left_y_release   = 40,                 \
    .diff_y_contact   = 60,                 \
    .diff_y_release   = 30,                 \
    .top_axis         = 0,                  \
    .top_contact      = 100,                \
    .top_release      = 50,                 \
    .bottom_contact   = 100,                \
    .bottom_release   = 50,                 \
    .debounce_contact = 3,                  \
    .debounce_release = 5,                  \
}

typedef struct
{
    mag_profile_t profile;
    uint8_t candidate;          /* 正在确认的边缘掩码 */
    uint8_t contact_count;
    uint8_t release_count;
    uint8_t edge;               /* 已确认的边缘掩码 */
    uint8_t active;             /* 1 = CONTACT */
    uint8_t approach_sent;
} mag_classifier_t;

/* profile 为 NULL 时使用 MAG_PROFILE_DEFAULT() */
void mag_classifier_init(mag_classifier_t *classifier,
                         const mag_profile_t *profile);

/*
 * 输入一帧特征。返回 1 表示 evt 有效，0 表示无事件。
 * evt 由调用者提供，内部只写不读。
 */
int mag_classifier_update(mag_classifier_t *classifier,
                          const mag_features_t *features,
                          uint32_t now_ms,
                          mag_event_t *evt);

/* 当前已确认的边缘掩码 */
uint8_t mag_classifier_edges(const mag_classifier_t *classifier);

#ifdef __cplusplus
}
#endif

#endif /* __MAG_CLASSIFIER_H__ */
