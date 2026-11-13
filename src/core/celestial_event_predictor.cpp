#include "celestial_event_predictor.h"
#include <math.h>

CelestialEventPredictor::CelestialEventPredictor(SunCalculator* sunCalculator, MoonCalculator* moonCalculator, PositionManager* positionManager)
    : _sunCalculator(sunCalculator), _moonCalculator(moonCalculator), _positionManager(positionManager), _lastSearchTimestamp(0) {
    _cachedEvent.type = EVENT_NONE;
    _cachedEvent.timestamp = 0;
    _cachedEvent.name[0] = '\0';
    _cachedEvent.dateStr[0] = '\0';
}

bool CelestialEventPredictor::evaluateMoonState(uint32_t ts, double lat, double lon, double& dist, double& phaseAngle, double& moonLat) {
    if (!_sunCalculator || !_moonCalculator) return false;

    SunPositionData sunPos = _sunCalculator->calculatePosition(ts, lat, lon);
    MoonPositionData moonPos = _moonCalculator->calculatePosition(ts, lat, lon);

    // 日月赤经赤纬夹角
    double raDiff = (sunPos.ra - moonPos.ra) * DEG_TO_RAD;
    double cosPhase = sin(sunPos.dec * DEG_TO_RAD) * sin(moonPos.dec * DEG_TO_RAD) +
                      cos(sunPos.dec * DEG_TO_RAD) * cos(moonPos.dec * DEG_TO_RAD) * cos(raDiff);
    
    if (cosPhase > 1.0) cosPhase = 1.0;
    if (cosPhase < -1.0) cosPhase = -1.0;

    phaseAngle = acos(cosPhase) * RAD_TO_DEG; // 相位角（近180度时为满月）
    dist = moonPos.distance;
    moonLat = moonPos.dec; // 简易纬度/赤纬估计

    return true;
}

CelestialEvent CelestialEventPredictor::findNextEvent(uint32_t startTimestamp) {
    // 强力防抖缓存：如果已有缓存且 startTimestamp 未跨越当前事件窗口，零延迟直接返回缓存
    if (_lastSearchTimestamp != 0) {
        if (_cachedEvent.type != EVENT_NONE) {
            if ((int32_t)_cachedEvent.timestamp + (int32_t)EVENT_WINDOW_SECONDS >= (int32_t)startTimestamp &&
                (int32_t)startTimestamp >= (int32_t)_lastSearchTimestamp - 86400) {
                return _cachedEvent;
            }
        } else {
            // 如果上一次搜索结果为 NONE，且时间变化小于 7 天，直接返回 NONE 缓存，避免快进时反复全量扫描未来 1 年
            if (labs((int32_t)startTimestamp - (int32_t)_lastSearchTimestamp) < 7 * 86400) {
                return _cachedEvent;
            }
        }
    }

    double lat = 39.9042;
    double lon = 116.4074;
    if (_positionManager) {
        PositionData pos = _positionManager->getPosition();
        lat = pos.latitude;
        lon = pos.longitude;
    }

    const uint32_t STEP = 6 * 3600; // 6小时粗步长
    const uint32_t MAX_LOOKAHEAD = 365 * 24 * 3600; // 搜索未来1年

    // 关键修复：从当前时间提前 12 小时开始扫描，确保包含正在发生/刚刚经过峰值的天象
    uint32_t currentTs = (startTimestamp > EVENT_WINDOW_SECONDS) ? (startTimestamp - EVENT_WINDOW_SECONDS) : startTimestamp;
    uint32_t endTs = startTimestamp + MAX_LOOKAHEAD;

    while (currentTs < endTs) {
        SunPositionData sunPos = _sunCalculator->calculatePosition(currentTs, lat, lon);
        MoonPositionData moonPos = _moonCalculator->calculatePosition(currentTs, lat, lon);

        // 天文学满月精确判定：日月黄经差 Delta Lambda = 180度
        double longDiff = fmod(moonPos.longitude - sunPos.longitude + 360.0, 360.0);
        double distToFull = fabs(longDiff - 180.0);
        if (distToFull > 180.0) distToFull = 360.0 - distToFull;

        // 1. 判定满月区域 (月全食/血月、月偏食、超级月亮)
        if (distToFull < 6.0) {
            uint32_t bestTs = currentTs;
            double minDiff = distToFull;

            for (int offset = -6; offset <= 6; offset++) {
                uint32_t testTs = currentTs + offset * 3600;
                SunPositionData sPos = _sunCalculator->calculatePosition(testTs, lat, lon);
                MoonPositionData mPos = _moonCalculator->calculatePosition(testTs, lat, lon);
                double lDiff = fmod(mPos.longitude - sPos.longitude + 360.0, 360.0);
                double dFull = fabs(lDiff - 180.0);
                if (dFull > 180.0) dFull = 360.0 - dFull;
                if (dFull < minDiff) {
                    minDiff = dFull;
                    bestTs = testTs;
                }
            }

            if ((int32_t)bestTs + (int32_t)EVENT_WINDOW_SECONDS >= (int32_t)startTimestamp) {
                MoonPositionData peakMoon = _moonCalculator->calculatePosition(bestTs, lat, lon);
                SunPositionData peakSun = _sunCalculator->calculatePosition(bestTs, lat, lon);

                // 计算三维地球本影中心角距离
                double shadowRa = peakSun.ra + 180.0;
                if (shadowRa >= 360.0) shadowRa -= 360.0;
                double shadowDec = -peakSun.dec;

                double raDiff = (peakMoon.ra - shadowRa) * DEG_TO_RAD;
                double cosShadowAng = sin(peakMoon.dec * DEG_TO_RAD) * sin(shadowDec * DEG_TO_RAD) +
                                      cos(peakMoon.dec * DEG_TO_RAD) * cos(shadowDec * DEG_TO_RAD) * cos(raDiff);
                if (cosShadowAng > 1.0) cosShadowAng = 1.0;
                if (cosShadowAng < -1.0) cosShadowAng = -1.0;
                double shadowAngDeg = acos(cosShadowAng) * RAD_TO_DEG;

                // 判定：月全食(<=0.46°)、月偏食(0.46°~1.15°)、超级月亮(<=0.00252 AU)
                bool isBloodMoon = (shadowAngDeg <= 0.46);
                bool isPartialMoon = (!isBloodMoon && shadowAngDeg <= 1.15);
                bool isSupermoon = (!isBloodMoon && !isPartialMoon && peakMoon.distance <= 0.00252);

                if (isBloodMoon || isPartialMoon || isSupermoon) {
                    if (isBloodMoon) {
                        _cachedEvent.type = EVENT_BLOOD_MOON;
                        strcpy(_cachedEvent.name, "BLOOD MOON");
                    } else if (isPartialMoon) {
                        _cachedEvent.type = EVENT_LUNAR_PARTIAL;
                        strcpy(_cachedEvent.name, "LUNAR PARTIAL");
                    } else {
                        _cachedEvent.type = EVENT_SUPERMOON;
                        strcpy(_cachedEvent.name, "SUPERMOON");
                    }
                    _cachedEvent.timestamp = bestTs;

                    TimeData tData = _positionManager ? _positionManager->getLocalTimeData(bestTs) : TimeData{26, 8, 14, 0, 0, 0};
                    sprintf(_cachedEvent.dateStr, "%04d-%02d-%02d %02d:%02d", tData.year + 2000, tData.month, tData.day, tData.hour, tData.minute);
                    _lastSearchTimestamp = startTimestamp;
                    return _cachedEvent;
                }
            }
            currentTs = bestTs + 20 * 86400;
            continue;
        }

        // 2. 判定新月区域 (日食 Solar Eclipse)
        double distToNew = longDiff;
        if (distToNew > 180.0) distToNew = 360.0 - distToNew;

        if (distToNew < 6.0) {
            uint32_t bestTs = currentTs;
            double minSep = 999.0;
            for (int offset = -6; offset <= 6; offset++) {
                uint32_t testTs = currentTs + offset * 3600;
                SunPositionData sPos = _sunCalculator->calculatePosition(testTs, lat, lon);
                MoonPositionData mPos = _moonCalculator->calculatePosition(testTs, lat, lon);
                double raDiff = (mPos.ra - sPos.ra) * DEG_TO_RAD;
                double cosAng = sin(mPos.dec * DEG_TO_RAD) * sin(sPos.dec * DEG_TO_RAD) +
                                cos(mPos.dec * DEG_TO_RAD) * cos(sPos.dec * DEG_TO_RAD) * cos(raDiff);
                if (cosAng > 1.0) cosAng = 1.0;
                if (cosAng < -1.0) cosAng = -1.0;
                double sep = acos(cosAng) * RAD_TO_DEG;
                if (sep < minSep) {
                    minSep = sep;
                    bestTs = testTs;
                }
            }

            if ((int32_t)bestTs + (int32_t)EVENT_WINDOW_SECONDS >= (int32_t)startTimestamp) {
                // 日月中心距离 <= 0.85度发生日食 (含日偏食/环食)
                if (minSep <= 0.85) {
                    _cachedEvent.type = EVENT_SOLAR_ECLIPSE;
                    _cachedEvent.timestamp = bestTs;
                    strcpy(_cachedEvent.name, "SOLAR ECLIPSE");

                    TimeData tData = _positionManager ? _positionManager->getLocalTimeData(bestTs) : TimeData{26, 8, 14, 0, 0, 0};
                    sprintf(_cachedEvent.dateStr, "%04d-%02d-%02d %02d:%02d", tData.year + 2000, tData.month, tData.day, tData.hour, tData.minute);
                    _lastSearchTimestamp = startTimestamp;
                    return _cachedEvent;
                }
            }
            currentTs = bestTs + 20 * 86400;
            continue;
        }

        currentTs += STEP;
    }

    // 默认兜底事件（如果未来搜索未找到）
    _cachedEvent.type = EVENT_NONE;
    _cachedEvent.timestamp = 0;
    strcpy(_cachedEvent.name, "NONE");
    strcpy(_cachedEvent.dateStr, "N/A");
    _lastSearchTimestamp = startTimestamp;
    return _cachedEvent;
}

ColorRGB CelestialEventPredictor::getMoonColor(uint32_t currentTimestamp, const CelestialEvent& event) {
    ColorRGB whiteColor = {255, 255, 255};
    if (event.type == EVENT_NONE || event.timestamp == 0) {
        return whiteColor;
    }

    int32_t timeDiff = (int32_t)currentTimestamp - (int32_t)event.timestamp;
    uint32_t absDiff = (uint32_t)labs(timeDiff);

    // 超出 ±12 小时时间窗口，返回标准白色
    if (absDiff >= EVENT_WINDOW_SECONDS) {
        return whiteColor;
    }

    // 计算渐变因子 t in [0.0, 1.0] (1.0 代表峰值时刻)
    float factor = 1.0f - ((float)absDiff / (float)EVENT_WINDOW_SECONDS);
    // Smoothstep 平滑曲线插值: s = factor * factor * (3 - 2 * factor)
    float smoothFactor = factor * factor * (3.0f - 2.0f * factor);

    ColorRGB targetColor = whiteColor;
    if (event.type == EVENT_SUPERMOON) {
        // 超级月亮渐变目标色：暖金色 (255, 215, 50)
        targetColor = {255, 215, 50};
    } else if (event.type == EVENT_BLOOD_MOON) {
        // 血月渐变目标色：红铜色/血红色 (230, 40, 30)
        targetColor = {230, 40, 30};
    } else if (event.type == EVENT_SOLAR_ECLIPSE) {
        // 日食渐变目标色：暗金黑晕色 (120, 70, 30)
        targetColor = {120, 70, 30};
    } else if (event.type == EVENT_LUNAR_PARTIAL) {
        // 月偏食渐变目标色：暖棕红暗影色 (220, 130, 70)
        targetColor = {220, 130, 70};
    }

    // RGB 线性插值
    ColorRGB result;
    result.r = (uint8_t)(whiteColor.r + smoothFactor * (targetColor.r - whiteColor.r));
    result.g = (uint8_t)(whiteColor.g + smoothFactor * (targetColor.g - whiteColor.g));
    result.b = (uint8_t)(whiteColor.b + smoothFactor * (targetColor.b - whiteColor.b));

    return result;
}
