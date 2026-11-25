/*
 * 单机磁吸边缘分类器
 */

#include <stddef.h>
#include "mag_classifier.h"

/* 超过阈值 2 倍记 100%，用于 debug 输出 */
static uint8_t edge_confidence(int32_t value, int32_t threshold)
{
    int32_t percent;

    if ((threshold <= 0) || (value <= 0))
    {
        return 0;
    }

    percent = value * 100 / (threshold * 2);

    return (percent > 100) ? 100 : (uint8_t)percent;
}

static uint8_t contact_edges(const mag_profile_t *profile,
                             const mag_features_t *f,
                             uint8_t *confidence)
{
    uint8_t edges = 0;
    uint8_t conf = 0;
    uint8_t value;
    uint8_t axis = (profile->top_axis == 1) ? 1 : 0;
    int32_t common;

    if ((f->right_delta[1] >= profile->right_y_contact) &&
            (f->diff[1] >= profile->diff_y_contact))
    {
        edges |= MAG_EDGE_RIGHT;
        value = edge_confidence(f->right_delta[1], profile->right_y_contact);
        if (value > conf) conf = value;
        value = edge_confidence(f->diff[1], profile->diff_y_contact);
        if (value > conf) conf = value;
    }

    if ((f->left_delta[1] <= -profile->left_y_contact) &&
            (f->diff[1] <= -profile->diff_y_contact))
    {
        edges |= MAG_EDGE_LEFT;
        value = edge_confidence(-f->left_delta[1], profile->left_y_contact);
        if (value > conf) conf = value;
        value = edge_confidence(-f->diff[1], profile->diff_y_contact);
        if (value > conf) conf = value;
    }

    common = ((int32_t)f->right_delta[axis] + f->left_delta[axis]) / 2;

    if (common >= profile->top_contact)
    {
        edges |= MAG_EDGE_TOP;
        value = edge_confidence(common, profile->top_contact);
        if (value > conf) conf = value;
    }

    if (common <= -profile->bottom_contact)
    {
        edges |= MAG_EDGE_BOTTOM;
        value = edge_confidence(-common, profile->bottom_contact);
        if (value > conf) conf = value;
    }

    *confidence = conf;

    return edges;
}

static uint8_t release_edges(const mag_profile_t *profile,
                             const mag_features_t *f)
{
    uint8_t edges = 0;
    uint8_t axis = (profile->top_axis == 1) ? 1 : 0;
    int32_t common;

    if ((f->right_delta[1] >= profile->right_y_release) &&
            (f->diff[1] >= profile->diff_y_release))
    {
        edges |= MAG_EDGE_RIGHT;
    }

    if ((f->left_delta[1] <= -profile->left_y_release) &&
            (f->diff[1] <= -profile->diff_y_release))
    {
        edges |= MAG_EDGE_LEFT;
    }

    common = ((int32_t)f->right_delta[axis] + f->left_delta[axis]) / 2;

    if (common >= profile->top_release)
    {
        edges |= MAG_EDGE_TOP;
    }

    if (common <= -profile->bottom_release)
    {
        edges |= MAG_EDGE_BOTTOM;
    }

    return edges;
}

void mag_classifier_init(mag_classifier_t *classifier,
                         const mag_profile_t *profile)
{
    if (classifier == NULL)
    {
        return;
    }

    classifier->profile = (profile != NULL) ? *profile :
                          (mag_profile_t)MAG_PROFILE_DEFAULT();
    classifier->candidate = 0;
    classifier->contact_count = 0;
    classifier->release_count = 0;
    classifier->edge = 0;
    classifier->active = 0;
    classifier->approach_sent = 0;
}

int mag_classifier_update(mag_classifier_t *classifier,
                          const mag_features_t *features,
                          uint32_t now_ms,
                          mag_event_t *evt)
{
    uint8_t contact;
    uint8_t release;
    uint8_t confidence = 0;
    uint8_t edge;

    if ((classifier == NULL) || (features == NULL) || (evt == NULL))
    {
        return 0;
    }

    contact = contact_edges(&classifier->profile, features, &confidence);
    release = release_edges(&classifier->profile, features);

    if (contact != 0)
    {
        /* 磁吸仍在，释放计数重新开始 */
        if (classifier->active)
        {
            classifier->release_count = 0;
        }

        if (contact != classifier->candidate)
        {
            classifier->candidate = contact;
            classifier->contact_count = 1;
            classifier->approach_sent = 0;
        }
        else if (classifier->contact_count < 255)
        {
            classifier->contact_count++;
        }

        if (!classifier->active && !classifier->approach_sent)
        {
            classifier->approach_sent = 1;
            evt->type = MAG_EVT_APPROACH;
            evt->edge = contact;
            evt->confidence = confidence;
            evt->tick_ms = now_ms;
            return 1;
        }

        if (!classifier->active &&
                (classifier->contact_count >=
                 classifier->profile.debounce_contact))
        {
            classifier->active = 1;
            classifier->edge = contact;
            classifier->release_count = 0;
            evt->type = MAG_EVT_CONTACT;
            evt->edge = contact;
            evt->confidence = confidence;
            evt->tick_ms = now_ms;
            return 1;
        }
    }
    else
    {
        classifier->candidate = 0;
        classifier->contact_count = 0;
        classifier->approach_sent = 0;

        if (classifier->active)
        {
            if (release == 0)
            {
                if (classifier->release_count < 255)
                {
                    classifier->release_count++;
                }

                if (classifier->release_count >=
                        classifier->profile.debounce_release)
                {
                    edge = classifier->edge;
                    classifier->active = 0;
                    classifier->edge = 0;
                    classifier->release_count = 0;

                    evt->type = MAG_EVT_RELEASE;
                    evt->edge = edge;
                    evt->confidence = 0;
                    evt->tick_ms = now_ms;
                    return 1;
                }
            }
            else
            {
                classifier->release_count = 0;
            }
        }
    }

    return 0;
}

uint8_t mag_classifier_edges(const mag_classifier_t *classifier)
{
    return (classifier != NULL) ? classifier->edge : 0;
}
