#include "position_manager.h"

/**
 * @brief 构造函数
 * @param gnss GNSS模块指针
 */
PositionManager::PositionManager(HalGnss* gnss) : 
    _gnss(gnss),
    _useManualTime(false),
    _useManualPosition(false),
    _defaultTimeLogged(false),
    _lastSaveTime(0),
    _lastTickMillis(0) {
    // 初始化位置数据
    _position.latitude = 0.0;
    _position.longitude = 0.0;
    _position.altitude = 0.0;

    // 初始化时间数据
    _time.year = 0;
    _time.month = 0;
    _time.day = 0;
    _time.hour = 0;
    _time.minute = 0;
    _time.second = 0;

    // 初始化手动时间数据
    _manualTime.year = 0;
    _manualTime.month = 0;
    _manualTime.day = 0;
    _manualTime.hour = 0;
    _manualTime.minute = 0;
    _manualTime.second = 0;

    // 初始化手动位置数据
    _manualPosition.latitude = 0.0;
    _manualPosition.longitude = 0.0;
    _manualPosition.altitude = 0.0;
}

/**
 * @brief 初始化位置和时间管理器
 * @return 初始化是否成功
 */
bool PositionManager::begin() {
    // 初始化 NVS 缓存
    _preferences.begin("skycompass", false);
    
    // 从 NVS 加载位置数据
    double lat = _preferences.getDouble("lat", 0.0);
    double lon = _preferences.getDouble("lon", 0.0);
    if (lat != 0.0 || lon != 0.0) {
        _position.latitude = lat;
        _position.longitude = lon;
        _position.altitude = _preferences.getDouble("alt", 0.0);
        _manualPosition = _position;
        _useManualPosition = true; // 关键：开机即启用 NVS 缓存位置
        Serial.printf("[PositionManager] Loaded position from NVS: %.6f, %.6f\n", _position.latitude, _position.longitude);
    } else {
        _position = getDefaultPosition();
        _manualPosition = _position;
        _useManualPosition = true; // 关键：无 NVS 时默认启用默认位置（北京）
        _preferences.putDouble("lat", _position.latitude);
        _preferences.putDouble("lon", _position.longitude);
        _preferences.putDouble("alt", _position.altitude);
        Serial.printf("[PositionManager] Initialized default position to NVS: %.6f, %.6f\n", _position.latitude, _position.longitude);
    }

    // 从 NVS 加载时间数据；若 NVS 缓存时间早于当前固件编译时间，则自动刷新为编译期默认时间
    uint8_t timeVer = _preferences.getUChar("time_ver", 0);
    uint16_t year = _preferences.getUShort("year", 0);
    TimeData defaultTime = getDefaultTime();
    bool loadedFromNvs = false;

    if (year != 0 && timeVer >= 2) {
        TimeData nvsTime;
        nvsTime.year = year;
        nvsTime.month = _preferences.getUChar("month", 0);
        nvsTime.day = _preferences.getUChar("day", 0);
        nvsTime.hour = _preferences.getUChar("hour", 0);
        nvsTime.minute = _preferences.getUChar("minute", 0);
        nvsTime.second = _preferences.getUChar("second", 0);

        if (isValidTimeData(nvsTime) && calculateTimestamp(nvsTime) >= calculateTimestamp(defaultTime)) {
            _time = nvsTime;
            loadedFromNvs = true;
            Serial.printf("[PositionManager] Loaded time from NVS: %04d-%02d-%02d %02d:%02d:%02d\n",
                          _time.year + 2000, _time.month, _time.day, _time.hour, _time.minute, _time.second);
        }
    }

    if (!loadedFromNvs) {
        _time = defaultTime;
        saveTimeToNvs(_time);
        Serial.printf("[PositionManager] Updated NVS to default/build time: %04d-%02d-%02d %02d:%02d:%02d\n",
                      _time.year + 2000, _time.month, _time.day, _time.hour, _time.minute, _time.second);
    }

    _lastTickMillis = millis();

    // 初始化GNSS模块（如果存在）
    if (_gnss) {
        return _gnss->begin();
    }
    // 如果没有GNSS，仍然返回成功
    return true;
}

/**
 * @brief 更新位置和时间数据
 * @return 更新是否成功
 */
bool PositionManager::update() {
    // 无 GNSS 时基于 millis() 维持系统基准时间走时（即使处于时间机器预览模式，后台基准时间也继续走时）
    uint32_t now = millis();
    uint32_t elapsedSec = (now - _lastTickMillis) / 1000;
    if (elapsedSec > 0) {
        _lastTickMillis += elapsedSec * 1000;
        if (isValidTimeData(_time)) {
            uint32_t ts = calculateTimestamp(_time) + elapsedSec;
            _time = getTimeData(ts);
        }
    }

    // 如果使用手动时间和位置，不需要更新GNSS
    if (_useManualTime && _useManualPosition) {
        return true;
    }

    // 由 AppMain 按间隔节流调用 _gnss->update()，此处仅消费已就绪的新 GNSS 数据，避免每帧重复轮询串口造成卡顿
    if (_gnss && _gnss->hasNewData()) {
        GnssData gnssData = _gnss->getData();
        updateFromGnss(gnssData);
        return true;
    }

    return false;
}

/**
 * @brief 获取当前位置数据
 * @return 位置数据结构体
 */
PositionData PositionManager::getPosition() {
    if (_useManualPosition) {
        return _manualPosition;
    }
    
    // 检查位置是否有效，如果无效返回默认位置（北京）
    if (_position.latitude == 0.0 && _position.longitude == 0.0) {
        return getDefaultPosition();
    }
    
    return _position;
}

/**
 * @brief 获取当前时间数据
 * @return 时间数据结构体
 */
TimeData PositionManager::getTime() {
    if (_useManualTime) {
        // 检查手动时间是否有效，如果无效返回默认本地时间（2026年11月13日 22:00）
        if (!hasValidTime()) {
            return getDefaultTime();
        }
        return _manualTime;
    }
    
    // 检查时间是否有效，如果无效返回默认本地时间2026/11/13 22:00
    if (!hasValidTime()) {
        return getDefaultTime();
    }
    
    return _time;
}

/**
 * @brief 获取当前UTC时间的时间戳（秒）
 * @return UTC时间戳
 */
uint32_t PositionManager::getTimestamp() {
    if (_useManualTime) {
        if (hasValidTime()) {
            uint32_t localTs = calculateTimestamp(_manualTime);
            // 关键：时间机器设置的是本地时间，需减去时区偏移量以获得真正的 UTC 时间戳
            int8_t timezone = (int8_t)round(getPosition().longitude / 15.0);
            return localTs - (timezone * 3600);
        }
    } else {
        if (hasValidTime()) {
            // GNSS 原始时间通常就是 UTC，所以直接计算
            return calculateTimestamp(_time);
        }
    }
    
    // 默认时间处理
    TimeData defaultTime = getDefaultTime();
    return calculateTimestamp(defaultTime);
}

/**
 * @brief 将时间戳转换为时间数据结构体
 * @param timestamp UTC时间戳（秒）
 * @return 时间数据结构体
 */
TimeData PositionManager::getTimeData(uint32_t timestamp) {
    TimeData result;
    
    // Unix时间戳是从1970年1月1日开始的
    // 将时间戳转换为年月日时分秒
    result.second = timestamp % 60;
    result.minute = (timestamp / 60) % 60;
    result.hour = (timestamp / 3600) % 24;
    uint32_t days = timestamp / 86400;
    
    // 从1970年1月1日开始计算
    uint16_t year = 1970;
    uint8_t month = 1;
    uint8_t day = 1;
    
    // 计算年月日
    while (days > 0) {
        // 检查是否是闰年
        bool isLeapYear = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
        uint16_t daysInYear = isLeapYear ? 366 : 365;
        
        if (days >= daysInYear) {
            days -= daysInYear;
            year++;
        } else {
            // 计算月份
            int monthDays[] = {31, isLeapYear ? 29 : 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
            for (uint8_t m = 0; m < 12; m++) {
                if (days >= monthDays[m]) {
                    days -= monthDays[m];
                    month++;
                } else {
                    day += days;
                    days = 0;
                    break;
                }
            }
        }
    }
    
    result.year = (year >= 2000) ? (year - 2000) : 0;
    result.month = month;
    result.day = day;
    
    return result;
}

/**
 * @brief 将时间戳转换为本地时间数据结构体（根据经度计算时区）
 * @param timestamp UTC时间戳（秒）
 * @return 本地时间数据结构体
 */
TimeData PositionManager::getLocalTimeData(uint32_t timestamp) {
    // 获取 UTC 时间
    TimeData utcTime = getTimeData(timestamp);
    
    // 计算时区偏移（简单的整数时区计算）
    PositionData pos = getPosition();
    // 经度每 15 度为一个时区，四舍五入到最接近的整数
    int8_t timezone = (int8_t)round(pos.longitude / 15.0);
    
    // 计算本地时间戳
    int32_t offset = timezone * 3600;
    uint32_t localTimestamp = (uint32_t)((int64_t)timestamp + offset);
    
    // 再将本地时间戳转换为时间数据
    return getTimeData(localTimestamp);
}

/**
 * @brief 设置手动时间（用于时间机器功能）
 * @param time 手动设置的时间数据
 */
void PositionManager::setManualTime(TimeData time) {
    _manualTime = time;
}

/**
 * @brief 启用/禁用手动时间
 * @param enable 是否启用手动时间
 */
void PositionManager::enableManualTime(bool enable) {
    _useManualTime = enable;
}

/**
 * @brief 将当前时间（含手动调节后的时间）作为系统基准时间持久化到 NVS，并恢复走时
 */
void PositionManager::saveCurrentTimeToNvs() {
    uint32_t utcTs = getTimestamp();
    _time = getTimeData(utcTs);
    _lastTickMillis = millis();
    _useManualTime = false;
    saveTimeToNvs(_time);
    _lastSaveTime = _lastTickMillis;
    Serial.printf("[PositionManager] Saved synced UTC time to NVS: %04d-%02d-%02d %02d:%02d:%02d\n",
                  _time.year + 2000, _time.month, _time.day, _time.hour, _time.minute, _time.second);
}

/**
 * @brief 将 UTC 时间写入 NVS
 */
void PositionManager::saveTimeToNvs(const TimeData& utcTime) {
    _preferences.putUChar("time_ver", 2);
    _preferences.putUShort("year", utcTime.year);
    _preferences.putUChar("month", utcTime.month);
    _preferences.putUChar("day", utcTime.day);
    _preferences.putUChar("hour", utcTime.hour);
    _preferences.putUChar("minute", utcTime.minute);
    _preferences.putUChar("second", utcTime.second);
}

/**
 * @brief 检查时间结构体字段是否在有效范围内
 */
bool PositionManager::isValidTimeData(const TimeData& time) {
    return (time.year != 0 && time.year <= 100 &&
            time.month >= 1 && time.month <= 12 &&
            time.day >= 1 && time.day <= 31);
}

/**
 * @brief 设置手动位置
 * @param position 手动设置的位置数据
 */
void PositionManager::setManualPosition(PositionData position) {
    _manualPosition = position;
    _position = position;
    _preferences.putDouble("lat", _position.latitude);
    _preferences.putDouble("lon", _position.longitude);
    _preferences.putDouble("alt", _position.altitude);
    Serial.printf("[PositionManager] Manual position saved to NVS: lat=%.6f, lon=%.6f, alt=%.1f\n", _position.latitude, _position.longitude, _position.altitude);
}

/**
 * @brief 启用/禁用手动位置
 * @param enable 是否启用手动位置
 */
void PositionManager::enableManualPosition(bool enable) {
    _useManualPosition = enable;
}

/**
 * @brief 检查是否有有效的位置数据
 * @return 是否有有效的位置数据
 */
bool PositionManager::hasValidPosition() {
    if (_useManualPosition) {
        return (_manualPosition.latitude != 0.0 || _manualPosition.longitude != 0.0);
    }
    return (_position.latitude != 0.0 || _position.longitude != 0.0);
}

/**
 * @brief 检查是否有有效的时间数据
 * @return 是否有有效的时间数据
 */
bool PositionManager::hasValidTime() {
    if (_useManualTime) {
        return isValidTimeData(_manualTime);
    }
    return isValidTimeData(_time);
}

/**
 * @brief 从GNSS数据更新位置和时间
 * @param gnssData GNSS数据结构体
 */
void PositionManager::updateFromGnss(GnssData gnssData) {
    // 检查GNSS是否已经获取到有效定位
    if (!gnssData.isValid || gnssData.status != GNSS_STATUS_LOCKED) {
        Serial.printf("[PositionManager] GNSS not locked yet (status=%d), ignoring position update\n", gnssData.status);
        return;
    }
    
    // 检查位置数据是否有效（经纬度不能为0）
    if (gnssData.latitude == 0.0 && gnssData.longitude == 0.0) {
        Serial.printf("[PositionManager] Invalid position from GNSS (lat=0, lon=0), ignoring\n");
        return;
    }
    
    // 更新位置数据
    _position.latitude = gnssData.latitude;
    _position.longitude = gnssData.longitude;
    _position.altitude = gnssData.altitude;
    Serial.printf("[PositionManager] Updated position from GNSS: lat=%.6f, lon=%.6f, alt=%.1f\n", _position.latitude, _position.longitude, _position.altitude);

    // 更新时间数据
    // 检查GNSS数据是否有效，以及日期是否有效
    Serial.printf("[PositionManager] GNSS date data: year=%d, month=%d, day=%d, dateValid=%d, isValid=%d\n", gnssData.year, gnssData.month, gnssData.day, gnssData.dateValid, gnssData.isValid);
    
    // 更严格的日期验证
    bool monthValid = gnssData.month >= 1 && gnssData.month <= 12;
    bool dayValid = gnssData.day >= 1 && gnssData.day <= 31;
    
    if (!gnssData.isValid || !gnssData.dateValid || !monthValid || !dayValid) {
        Serial.printf("[PositionManager] Date not valid from GNSS (monthValid=%d, dayValid=%d), ignoring time update\n", monthValid, dayValid);
        return;
    }
    
    // GNSS返回的是完整年份（如2026），需要转换为相对于2000年的偏移量（如26）
    // 检查年份是否在合理范围内（2000-2100）
    if (gnssData.year >= 2000 && gnssData.year <= 2100) {
        _time.year = gnssData.year - 2000;
        _time.month = gnssData.month;
        _time.day = gnssData.day;
        _time.hour = gnssData.hour;
        _time.minute = gnssData.minute;
        _time.second = gnssData.second;
        Serial.printf("[PositionManager] Updated time from GNSS: %04d-%02d-%02d %02d:%02d:%02d\n", _time.year + 2000, _time.month, _time.day, _time.hour, _time.minute, _time.second);
    } else if (gnssData.year >= 100 && gnssData.year < 2000) {
        // 处理1900-1999年的情况
        _time.year = gnssData.year - 1900;
        _time.month = gnssData.month;
        _time.day = gnssData.day;
        _time.hour = gnssData.hour;
        _time.minute = gnssData.minute;
        _time.second = gnssData.second;
        Serial.printf("[PositionManager] Updated time from GNSS: %04d-%02d-%02d %02d:%02d:%02d\n", _time.year + 1900, _time.month, _time.day, _time.hour, _time.minute, _time.second);
    } else if (gnssData.year > 0 && gnssData.year < 100) {
        // 已经是偏移量
        _time.year = gnssData.year;
        _time.month = gnssData.month;
        _time.day = gnssData.day;
        _time.hour = gnssData.hour;
        _time.minute = gnssData.minute;
        _time.second = gnssData.second;
        Serial.printf("[PositionManager] Updated time from GNSS: %04d-%02d-%02d %02d:%02d:%02d\n", _time.year + 2000, _time.month, _time.day, _time.hour, _time.minute, _time.second);
    } else {
        // 年份无效，不更新时间数据
        Serial.printf("[PositionManager] Invalid year from GNSS: %d, ignoring time update\n", gnssData.year);
    }

    // 重置本地走时基准
    _lastTickMillis = millis();

    // 缓存到 NVS
    uint32_t currentMillis = millis();
    if (currentMillis - _lastSaveTime > 60000 || _lastSaveTime == 0) {
        _preferences.putDouble("lat", _position.latitude);
        _preferences.putDouble("lon", _position.longitude);
        _preferences.putDouble("alt", _position.altitude);
        saveTimeToNvs(_time);
        _lastSaveTime = currentMillis;
        Serial.println("[PositionManager] Saved time and position to NVS");
    }
}

/**
 * @brief 计算时间戳
 * @param time 时间数据结构体
 * @return UTC时间戳（秒）
 */
uint32_t PositionManager::calculateTimestamp(TimeData time) {
    // 计算 Unix 时间戳（从 1970 年 1 月 1 日开始的秒数）
    uint16_t fullYear = 2000 + time.year;
    
    // 确保年份在合理范围内
    if (fullYear < 1970) {
        fullYear = 1970;
    } else if (fullYear > 2100) {
        fullYear = 2100;
    }
    
    // 计算从1970年到当前年份的天数
    uint32_t days = 0;
    for (uint16_t y = 1970; y < fullYear; y++) {
        // 闰年判断
        if ((y % 4 == 0 && y % 100 != 0) || (y % 400 == 0)) {
            days += 366;
        } else {
            days += 365;
        }
    }

    // 计算从当年1月1日到当前月份的天数
    uint8_t monthDays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    // 检查闰年
    if ((fullYear % 4 == 0 && fullYear % 100 != 0) || (fullYear % 400 == 0)) {
        monthDays[1] = 29;
    }
    for (uint8_t m = 0; m < time.month - 1; m++) {
        days += monthDays[m];
    }

    // 加上当前日期
    days += time.day - 1;

    // 计算总秒数（时间数据已经是UTC时间，不需要时区转换）
    uint32_t timestamp = days * 86400UL + time.hour * 3600UL + time.minute * 60UL + time.second;

    return timestamp;
}

/**
 * @brief 获取默认时间数据
 * @return 默认时间数据结构体
 */
TimeData PositionManager::getDefaultTime() {
    TimeData defaultTime = {26, 11, 13, 16, 0, 0};

    // 使用编译期 __DATE__ ("Mmm dd yyyy") 和 __TIME__ ("hh:mm:ss") 作为出厂默认时间
    const char* dateStr = __DATE__;
    const char* timeStr = __TIME__;
    const char* months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                            "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

    uint8_t month = 0;
    for (uint8_t i = 0; i < 12; i++) {
        if (strncmp(dateStr, months[i], 3) == 0) {
            month = i + 1;
            break;
        }
    }

    int day = atoi(dateStr + 4);
    int fullYear = atoi(dateStr + 7);
    int hour = atoi(timeStr);
    int minute = atoi(timeStr + 3);
    int second = atoi(timeStr + 6);

    if (month >= 1 && month <= 12 && day >= 1 && day <= 31 && fullYear >= 2000 && fullYear <= 2100) {
        // 将编译机本地时间（按默认位置东八区 +8h）转换为 UTC 存储
        hour -= 8;
        if (hour < 0) {
            hour += 24;
            day -= 1;
            if (day < 1) {
                month -= 1;
                if (month < 1) {
                    month = 12;
                    fullYear -= 1;
                }
                const uint8_t daysInMonth[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
                day = daysInMonth[month];
                if (month == 2 && ((fullYear % 4 == 0 && fullYear % 100 != 0) || (fullYear % 400 == 0))) {
                    day = 29;
                }
            }
        }
        defaultTime.year = (uint16_t)(fullYear - 2000);
        defaultTime.month = (uint8_t)month;
        defaultTime.day = (uint8_t)day;
        defaultTime.hour = (uint8_t)hour;
        defaultTime.minute = (uint8_t)minute;
        defaultTime.second = (uint8_t)second;
    }

    return defaultTime;
}

/**
 * @brief 获取默认位置数据
 * @return 默认位置数据结构体
 */
PositionData PositionManager::getDefaultPosition() {
    PositionData defaultPosition;
    defaultPosition.latitude = 39.9042; // 北京纬度
    defaultPosition.longitude = 116.4074; // 北京经度
    defaultPosition.altitude = 0.0;
    return defaultPosition;
}
