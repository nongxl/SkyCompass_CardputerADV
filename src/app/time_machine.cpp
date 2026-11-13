#include "time_machine.h"

/**
 * @brief 构造函数
 * @param positionManager 位置和时间管理器指针
 */
static const uint32_t MIN_TIME_TS = 946684800UL;  // 2000-01-01 00:00:00
static const uint32_t MAX_TIME_TS = 4102444800UL; // 2100-01-01 00:00:00

TimeMachine::TimeMachine(PositionManager* positionManager) : 
    _positionManager(positionManager),
    _isActive(false),
    _selectedField(0),
    _displayTimestamp(0),
    _targetTimestamp(0),
    _animStartTimestamp(0),
    _animStartMillis(0),
    _animDurationMs(0),
    _isAnimating(false) {
    // 初始化目标时间为当前时间
    _targetTime = _positionManager->getTime();
}

/**
 * @brief 初始化时间机器
 * @return 初始化是否成功
 */
bool TimeMachine::begin() {
    // 时间机器不需要特殊初始化
    return true;
}

/**
 * @brief 激活时间机器
 */
void TimeMachine::activate() {
    _isActive = true;
    // 保存当前本地时间作为目标时间（与 PositionManager::getTimestamp 的手动时间本地时区约定一致）
    _targetTime = _positionManager->getLocalTimeData(_positionManager->getTimestamp());

    // 检查时间是否有效，如果无效则使用默认时间转换后的本地时间
    if (_targetTime.year == 0 || _targetTime.month == 0 || _targetTime.day == 0 || _targetTime.year > 100) {
        _targetTime = PositionManager::getDefaultTime();
        Serial.println("[DEBUG] TimeMachine: Using default time fallback");
    }
    
    // 初始化线性插值状态
    _targetTimestamp = PositionManager::calculateTimestamp(_targetTime);
    _displayTimestamp = _targetTimestamp;
    _animStartTimestamp = _targetTimestamp;
    _animStartMillis = millis();
    _animDurationMs = 0;
    _isAnimating = false;

    // 设置手动时间
    _positionManager->setManualTime(_targetTime);
    // 启用手动时间
    _positionManager->enableManualTime(true);
    Serial.println("[DEBUG] TimeMachine activated");
    Serial.printf("[DEBUG] Target time: %04d-%02d-%02d %02d:%02d:%02d\n", 
                  _targetTime.year + 2000, _targetTime.month, _targetTime.day,
                  _targetTime.hour, _targetTime.minute, _targetTime.second);
    Serial.flush();
}

/**
 * @brief 停用时间机器
 */
void TimeMachine::deactivate() {
    _isActive = false;
    _isAnimating = false;
    // 禁用手动时间
    _positionManager->enableManualTime(false);
}

/**
 * @brief 检查时间机器是否激活
 * @return 是否激活
 */
bool TimeMachine::isActive() {
    return _isActive;
}

/**
 * @brief 获取目标时间
 * @return 目标时间
 */
TimeData TimeMachine::getTargetTime() {
    return _targetTime;
}

/**
 * @brief 设置目标时间
 * @param time 目标时间
 */
void TimeMachine::setTargetTime(TimeData time) {
    if (isValidTime(time)) {
        _targetTime = time;
        // 应用目标时间
        _positionManager->setManualTime(_targetTime);
    }
}

/**
 * @brief 增加当前选中字段的值
 * @param step 增加的步长
 */
void TimeMachine::incrementField(uint8_t step) {
    switch (_selectedField) {
        case 0: // 年
            _targetTime.year += step;
            // 确保年份不会溢出
            if (_targetTime.year > 100) {
                _targetTime.year = 100; // 限制到 2100 年
            }
            break;
        case 1: // 月
            _targetTime.month += step;
            if (_targetTime.month > 12) {
                _targetTime.month = 1;
                _targetTime.year++;
                // 确保年份不会溢出
                if (_targetTime.year > 100) {
                    _targetTime.year = 100; // 限制到 2100 年
                }
            }
            break;
        case 2: // 日
            {
                uint16_t fullYear = 2000 + _targetTime.year;
                uint8_t daysInMonth = getDaysInMonth(fullYear, _targetTime.month);
                _targetTime.day += step;
                if (_targetTime.day > daysInMonth) {
                    _targetTime.day = 1;
                    _targetTime.month++;
                    if (_targetTime.month > 12) {
                        _targetTime.month = 1;
                        _targetTime.year++;
                        // 确保年份不会溢出
                        if (_targetTime.year > 100) {
                            _targetTime.year = 100; // 限制到 2100 年
                        }
                    }
                }
            }
            break;
        case 3: // 时
            _targetTime.hour += step;
            if (_targetTime.hour >= 24) {
                _targetTime.hour -= 24;
                _targetTime.day++;
                uint16_t fullYear = 2000 + _targetTime.year;
                uint8_t daysInMonth = getDaysInMonth(fullYear, _targetTime.month);
                if (_targetTime.day > daysInMonth) {
                    _targetTime.day = 1;
                    _targetTime.month++;
                    if (_targetTime.month > 12) {
                        _targetTime.month = 1;
                        _targetTime.year++;
                        // 确保年份不会溢出
                        if (_targetTime.year > 100) {
                            _targetTime.year = 100; // 限制到 2100 年
                        }
                    }
                }
            }
            break;
        case 4: // 分
            _targetTime.minute += step;
            if (_targetTime.minute >= 60) {
                _targetTime.minute -= 60;
                _targetTime.hour++;
                if (_targetTime.hour >= 24) {
                    _targetTime.hour -= 24;
                    _targetTime.day++;
                    uint16_t fullYear = 2000 + _targetTime.year;
                    uint8_t daysInMonth = getDaysInMonth(fullYear, _targetTime.month);
                    if (_targetTime.day > daysInMonth) {
                        _targetTime.day = 1;
                        _targetTime.month++;
                        if (_targetTime.month > 12) {
                            _targetTime.month = 1;
                            _targetTime.year++;
                        }
                    }
                }
            }
            break;
        case 5: // 秒
            _targetTime.second += step;
            if (_targetTime.second >= 60) {
                _targetTime.second -= 60;
                _targetTime.minute++;
                if (_targetTime.minute >= 60) {
                    _targetTime.minute -= 60;
                    _targetTime.hour++;
                    if (_targetTime.hour >= 24) {
                        _targetTime.hour -= 24;
                        _targetTime.day++;
                        uint16_t fullYear = 2000 + _targetTime.year;
                        uint8_t daysInMonth = getDaysInMonth(fullYear, _targetTime.month);
                        if (_targetTime.day > daysInMonth) {
                            _targetTime.day = 1;
                            _targetTime.month++;
                            if (_targetTime.month > 12) {
                                _targetTime.month = 1;
                                _targetTime.year++;
                            }
                        }
                    }
                }
            }
            break;
    }
    
    // 应用目标时间
    _positionManager->setManualTime(_targetTime);
}

/**
 * @brief 减少当前选中字段的值
 * @param step 减少的步长
 */
void TimeMachine::decrementField(uint8_t step) {
    switch (_selectedField) {
        case 0: // 年
            if (_targetTime.year >= step) {
                _targetTime.year -= step;
            }
            break;
        case 1: // 月
            _targetTime.month -= step;
            if (_targetTime.month < 1) {
                _targetTime.month = 12;
                if (_targetTime.year >= step) {
                    _targetTime.year -= step;
                }
            }
            break;
        case 2: // 日
            _targetTime.day -= step;
            if (_targetTime.day < 1) {
                _targetTime.month--;
                if (_targetTime.month < 1) {
                    _targetTime.month = 12;
                    if (_targetTime.year >= step) {
                        _targetTime.year -= step;
                    }
                }
                uint16_t fullYear = 2000 + _targetTime.year;
                _targetTime.day = getDaysInMonth(fullYear, _targetTime.month);
            }
            break;
        case 3: // 时
            if (_targetTime.hour >= step) {
                _targetTime.hour -= step;
            } else {
                _targetTime.hour += 24 - step;
                _targetTime.day--;
                if (_targetTime.day < 1) {
                    _targetTime.month--;
                    if (_targetTime.month < 1) {
                        _targetTime.month = 12;
                        if (_targetTime.year >= step) {
                            _targetTime.year -= step;
                        }
                    }
                    uint16_t fullYear = 2000 + _targetTime.year;
                    _targetTime.day = getDaysInMonth(fullYear, _targetTime.month);
                }
            }
            break;
        case 4: // 分
            if (_targetTime.minute >= step) {
                _targetTime.minute -= step;
            } else {
                _targetTime.minute += 60 - step;
                if (_targetTime.hour >= step) {
                    _targetTime.hour -= step;
                } else {
                    _targetTime.hour += 24 - step;
                    _targetTime.day--;
                    if (_targetTime.day < 1) {
                        _targetTime.month--;
                        if (_targetTime.month < 1) {
                            _targetTime.month = 12;
                            if (_targetTime.year >= step) {
                                _targetTime.year -= step;
                            }
                        }
                        uint16_t fullYear = 2000 + _targetTime.year;
                        _targetTime.day = getDaysInMonth(fullYear, _targetTime.month);
                    }
                }
            }
            break;
        case 5: // 秒
            if (_targetTime.second >= step) {
                _targetTime.second -= step;
            } else {
                _targetTime.second += 60 - step;
                if (_targetTime.minute >= step) {
                    _targetTime.minute -= step;
                } else {
                    _targetTime.minute += 60 - step;
                    if (_targetTime.hour >= step) {
                        _targetTime.hour -= step;
                    } else {
                        _targetTime.hour += 24 - step;
                        _targetTime.day--;
                        if (_targetTime.day < 1) {
                            _targetTime.month--;
                            if (_targetTime.month < 1) {
                                _targetTime.month = 12;
                                if (_targetTime.year >= step) {
                                    _targetTime.year -= step;
                                }
                            }
                            uint16_t fullYear = 2000 + _targetTime.year;
                            _targetTime.day = getDaysInMonth(fullYear, _targetTime.month);
                        }
                    }
                }
            }
            break;
    }
    
    // 应用目标时间
    _positionManager->setManualTime(_targetTime);
}

/**
 * @brief 选择下一个时间字段
 */
void TimeMachine::selectNextField() {
    _selectedField = (_selectedField + 1) % 6;
}

/**
 * @brief 选择上一个时间字段
 */
void TimeMachine::selectPreviousField() {
    _selectedField = (_selectedField - 1 + 6) % 6;
}

/**
 * @brief 获取当前选中的字段索引
 * @return 字段索引
 */
uint8_t TimeMachine::getSelectedField() {
    return _selectedField;
}

/**
 * @brief 应用目标时间（更新到位置和时间管理器）
 */
void TimeMachine::applyTime() {
    _targetTimestamp = PositionManager::calculateTimestamp(_targetTime);
    _displayTimestamp = _targetTimestamp;
    _isAnimating = false;
    _positionManager->setManualTime(_targetTime);
}

/**
 * @brief 重置为当前时间
 */
void TimeMachine::resetToCurrentTime() {
    // 禁用手动时间以获取真实的当前本地时间
    _positionManager->enableManualTime(false);
    _targetTime = _positionManager->getLocalTimeData(_positionManager->getTimestamp());
    _targetTimestamp = PositionManager::calculateTimestamp(_targetTime);
    _displayTimestamp = _targetTimestamp;
    _isAnimating = false;
    // 重新启用手动时间
    _positionManager->enableManualTime(true);
    // 应用目标时间
    _positionManager->setManualTime(_targetTime);
}

/**
 * @brief 调整时间
 * @param seconds 调整的秒数（正数增加，负数减少）
 */
void TimeMachine::adjustTime(int seconds) {
    // 确保时间机器已激活
    if (!_isActive) {
        activate();
    }

    // 计算目标时间戳并限制在 [2000, 2100] 年范围内
    int64_t nextTs = (int64_t)_targetTimestamp + (int64_t)seconds;
    if (nextTs < (int64_t)MIN_TIME_TS) nextTs = MIN_TIME_TS;
    if (nextTs > (int64_t)MAX_TIME_TS) nextTs = MAX_TIME_TS;
    _targetTimestamp = (uint32_t)nextTs;
    _targetTime = PositionManager::getTimeData(_targetTimestamp);

    // 从当前已显示的时间戳出发，启动线性插值动画，使天体轨迹平稳转动而非瞬间跳变
    int64_t diff = (int64_t)_targetTimestamp - (int64_t)_displayTimestamp;
    uint32_t absDiff = (diff >= 0) ? (uint32_t)diff : (uint32_t)(-diff);

    if (absDiff == 0) {
        _isAnimating = false;
        return;
    }

    _animStartTimestamp = _displayTimestamp;
    _animStartMillis = millis();
    if (absDiff <= 300) {
        _animDurationMs = 120;
    } else if (absDiff <= 3600) {
        _animDurationMs = 280;
    } else {
        _animDurationMs = 450;
    }
    _isAnimating = true;
}

/**
 * @brief 推进时间机器的线性插值动画
 * @return 如果本帧更新了插值时间并需要重绘，返回 true
 */
bool TimeMachine::update() {
    if (!_isActive || !_isAnimating) {
        return false;
    }

    uint32_t now = millis();
    uint32_t elapsed = now - _animStartMillis;
    if (_animDurationMs == 0 || elapsed >= _animDurationMs) {
        _displayTimestamp = _targetTimestamp;
        _isAnimating = false;
    } else {
        double t = (double)elapsed / (double)_animDurationMs;
        int64_t delta = (int64_t)_targetTimestamp - (int64_t)_animStartTimestamp;
        _displayTimestamp = (uint32_t)((int64_t)_animStartTimestamp + (int64_t)(delta * t));
    }

    _positionManager->setManualTime(PositionManager::getTimeData(_displayTimestamp));
    return true;
}

/**
 * @brief 是否正在进行线性插值动画
 */
bool TimeMachine::isAnimating() const {
    return _isAnimating;
}

/**
 * @brief 验证时间是否有效
 * @param time 时间数据
 * @return 是否有效
 */
bool TimeMachine::isValidTime(TimeData time) {
    // 验证月份
    if (time.month < 1 || time.month > 12) {
        return false;
    }
    
    // 验证日期
    uint16_t fullYear = 2000 + time.year;
    uint8_t daysInMonth = getDaysInMonth(fullYear, time.month);
    if (time.day < 1 || time.day > daysInMonth) {
        return false;
    }
    
    // 验证小时
    if (time.hour >= 24) {
        return false;
    }
    
    // 验证分钟
    if (time.minute >= 60) {
        return false;
    }
    
    // 验证秒
    if (time.second >= 60) {
        return false;
    }
    
    return true;
}

/**
 * @brief 获取指定月份的天数
 * @param year 年份
 * @param month 月份（1-12）
 * @return 天数
 */
uint8_t TimeMachine::getDaysInMonth(uint16_t year, uint8_t month) {
    uint8_t daysInMonth[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    
    // 检查是否是闰年
    if (month == 2 && ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0))) {
        return 29;
    }
    
    return daysInMonth[month];
}
