#ifndef CELESTIAL_EVENT_PREDICTOR_H
#define CELESTIAL_EVENT_PREDICTOR_H

#include <Arduino.h>
#include "sun_calculator.h"
#include "moon_calculator.h"
#include "position_manager.h"

enum CelestialEventType {
    EVENT_NONE = 0,
    EVENT_SUPERMOON,
    EVENT_BLOOD_MOON,
    EVENT_SOLAR_ECLIPSE,
    EVENT_LUNAR_PARTIAL
};

struct CelestialEvent {
    CelestialEventType type;
    uint32_t timestamp;  // UTC 秒时间戳
    char name[32];       // 事件显示名称
    char dateStr[32];    // 格式化日期时间
};

struct ColorRGB {
    uint8_t r;
    uint8_t g;
    uint8_t b;
};

class CelestialEventPredictor {
private:
    SunCalculator* _sunCalculator;
    MoonCalculator* _moonCalculator;
    PositionManager* _positionManager;

    CelestialEvent _cachedEvent;
    uint32_t _lastSearchTimestamp;
    static const uint32_t EVENT_WINDOW_SECONDS = 12 * 3600; // ±12小时渐变窗口

public:
    CelestialEventPredictor(SunCalculator* sunCalculator, MoonCalculator* moonCalculator, PositionManager* positionManager);

    /**
     * @brief 寻找从 startTimestamp 开始未来最近的一次重大天象（超级月亮或血月）
     * @param startTimestamp 当前 UTC 时间戳
     * @return 预测的天象事件
     */
    CelestialEvent findNextEvent(uint32_t startTimestamp);

    /**
     * @brief 清空天象预测缓存（在位置改变时强制重新搜索）
     */
    void resetCache() {
        _cachedEvent.type = EVENT_NONE;
        _lastSearchTimestamp = 0;
    }

    /**
     * @brief 根据当前时间与天象发生时间的距离，计算月面亮面动态渐变颜色
     * @param currentTimestamp 当前 UTC 时间戳
     * @param event 当前关心的天象事件
     * @return RGB 颜色结构体
     */
    ColorRGB getMoonColor(uint32_t currentTimestamp, const CelestialEvent& event);

private:
    /**
     * @brief 判断给定时刻的月相相角与满月特征
     */
    bool evaluateMoonState(uint32_t ts, double lat, double lon, double& dist, double& phaseAngle, double& moonLat);
};

#endif // CELESTIAL_EVENT_PREDICTOR_H
