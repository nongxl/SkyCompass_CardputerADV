#include <M5Cardputer.h>
#include <M5_DLight.h>
#include <math.h>

// --- 常量定义 ---
const char UP_KEY_STR = '.'; // 上箭头键对应的字符
const char DOWN_KEY_STR = ';'; // 下箭头键对应的字符

// 调试信息全局变量
String debug_message = "";

// --- 核心UI常量 ---
const int PARAMETER_LIST_SIZE = 5;  // 参数选择列表中显示的最大项目数（奇数）
const int LIST_ITEM_HEIGHT = 20;  // 列表项的高度（像素）
//const float UNIFIED_FONT_SIZE = 1.0; // 字体大小，与Python版本匹配

// --- 字体设置常量 ---
const float LEFT_PANEL_FONT_SIZE = 0.9;
const float PARAM_LIST_FONT_SIZE = 1.2;
const float TITLE_BAR_FONT_SIZE = 1.0;

// --- 字体类型设置 ---
const lgfx::GFXfont* LEFT_PANEL_FONT = &lgfx::fonts::DejaVu24;
const lgfx::GFXfont* PARAM_LIST_FONT = &lgfx::fonts::DejaVu18;
const lgfx::GFXfont* TITLE_BAR_FONT = &lgfx::fonts::DejaVu18;


// --- 颜色定义 ---（使用精确的RGB565值）
const uint16_t COLOR_DEFAULT = 0xFFFF;  // 白色
const uint16_t COLOR_FOCUSED = 0x7fc0 & 0xFFFF;  // 亮绿色（选中状态）
const uint16_t COLOR_INVALID = 0xF800;  // 红色（无效选项-高亮显示）
const uint16_t COLOR_BACKGROUND = 0x0000;  // 黑色
const uint16_t COLOR_TITLE_BG = 0x001F;  // 标题背景色 - 蓝色
const uint16_t COLOR_GRADIENT_1 = 0x3666;  // 渐变色1（浅绿色）
const uint16_t COLOR_GRADIENT_2 = 0x0400;  // 渐变色2（深绿色）
const uint16_t COLOR_INVALID_GRADIENT_1 = 0xb104;  // 无效渐变色1（浅红色）
const uint16_t COLOR_INVALID_GRADIENT_2 = 0x8800;  // 无效渐变色2（深红色）

// --- UI布局坐标 ---
// 标题栏尺寸和位置
const int TITLE_BAR_HEIGHT = 22; // 标题栏高度（像素）
const int TITLE_BAR_WIDTH = 240; // 标题栏宽度（像素）
const int TITLE_TEXT_X = 5;      // 标题文本X坐标（距离左边缘像素）
const int TITLE_TEXT_Y = 5;      // 标题文本Y坐标（距离上边缘像素）
//const int BATTERY_LABEL_X = 188; // 电池标签X坐标
//const int BATTERY_LABEL_Y = 4;   // 电池标签Y坐标
const int BATTERY_PERCENT_X = 180; // 电池百分比X坐标 - 向左移动以避免百分号回行
const int BATTERY_PERCENT_Y = 5;   // 电池百分比Y坐标
const int BATTERY_AREA_WIDTH = 56; // 电池显示区域宽度

// 左侧面板尺寸和位置
const int LEFT_PANEL_X = 8;     // 左侧面板X坐标（距离左边缘像素）
const int LEFT_PANEL_Y = 25;     // 左侧面板Y坐标（距离上边缘像素）
const int LEFT_PANEL_WIDTH = 145; // 左侧面板宽度（像素）
const int LEFT_PANEL_HEIGHT = 110; // 左侧面板高度（像素）

// 参数列表尺寸和位置
const int PARAM_LIST_X = 158;    // 参数列表X坐标（距离左边缘像素）
const int PARAM_LIST_Y = 35;     // 参数列表Y坐标（距离上边缘像素）
const int PARAM_LIST_WIDTH = 80; // 参数列表宽度（像素）
const int PARAM_LIST_HEIGHT = PARAMETER_LIST_SIZE * LIST_ITEM_HEIGHT; // 参数列表高度（像素）

// 左侧面板文本标签位置
const int LEFT_PANEL_LABEL_X = 1; // 左侧面板标签X坐标（相对面板的位置）
const int LUX_LABEL_Y = 18;       // 光照度标签Y坐标（相对面板的位置）
const int ISO_LABEL_Y = 45;       // ISO标签Y坐标（相对面板的位置）
const int APERTURE_LABEL_Y = 70;  // 光圈标签Y坐标（相对面板的位置）
const int SHUTTER_LABEL_Y = 95;   // 快门标签Y坐标（相对面板的位置）


// --- 键盘按键定义 ---
// 使用库定义的KEY_TAB
// 使用预定义的上下键常量
// （上 = '.', 下 = ';'）- 与Python版本匹配

// --- 辅助函数 ---
// 格式化光圈值，去除小数点后为0的情况
String format_aperture(float aperture) {
  if (aperture == floor(aperture)) {
    // 如果是整数，不显示小数点
    return String((int)aperture);
  } else {
    // 否则显示一位小数
    return String(aperture, 1);
  }
}

// --- 全局变量 ---
// 状态变量
char current_mode = 'i';
char priority_mode = 'A';
float last_lux_value = 0;
bool is_initialized = false;

// 动画状态变量
bool is_animating = false;
int anim_duration = 120;  // 动画时间（毫秒）
unsigned long anim_start_time = 0;
float anim_current_y = 0.0;
float anim_y_start_pos = 0;

// 预定义值
const int iso_values[] = {50, 100, 200, 400, 800, 1000, 1200, 1600, 3200, 6400};
const int iso_count = sizeof(iso_values) / sizeof(iso_values[0]);
                                                                                                                                                                                                                         
const float aperture_values[] = {0.95, 1.0, 1.2, 1.4, 1.8, 2.0, 2.2, 2.8, 4, 5.6, 8, 11, 16, 22};
const int aperture_count = sizeof(aperture_values) / sizeof(aperture_values[0]);

const char* shutter_values[] = {"30s", "15s", "8s", "4s", "2s", "1s", "1/2", "1/4", "1/8", "1/15", "1/30", "1/60", "1/125", "1/250", "1/500", "1/1000", "1/2000", "1/4000"};
const int shutter_count = sizeof(shutter_values) / sizeof(shutter_values[0]);

// 当前预定义值的索引
int preview_indices[3] = {1, 4, 11};  // ISO, 光圈, 快门

// 硬件相关对象
M5Canvas *param_list_canvas;
M5Canvas *left_panel_canvas;

// 传感器对象
M5_DLight *dlight_0 = nullptr;

// --- 辅助函数 ---
float shutter_to_float(const char* shutter_str) {
  String str = shutter_str;
  str.trim();
  if (str.endsWith("s")) {
    str = str.substring(0, str.length() - 1);
  }
  
  int slash_pos = str.indexOf('/');
  if (slash_pos != -1) {
    float numerator = str.substring(0, slash_pos).toFloat();
    float denominator = str.substring(slash_pos + 1).toFloat();
    return numerator / denominator;
  } else {
    return str.toFloat();
  }
}

float compute_aperture(float ev, int iso, float shutter_speed) {
  float ev_corrected = ev + log2(iso / 100.0f);
  float f_number_sq = (powf(2, ev_corrected)) * shutter_speed;
  return f_number_sq > 0 ? sqrtf(f_number_sq) : 0;
}

float compute_shutter_speed(float ev, int iso, float aperture) {
  float ev_corrected = ev + log2(iso / 100.0f);
  float denominator = powf(2, ev_corrected);
  return denominator != 0 ? (aperture * aperture) / denominator : 0;
}

// 函数前向声明
void draw_left_panel();
void update_and_recalculate();
void update_parameter_colors();

// 更新并重新计算所有参数值
void update_and_recalculate() {
  // 获取当前参数值
  int current_iso = iso_values[preview_indices[0]];
  float current_aperture = aperture_values[preview_indices[1]];
  float current_shutter = shutter_to_float(shutter_values[preview_indices[2]]);
  
  // 根据当前光照读数计算EV值
  float ev_value = last_lux_value > 0 ? log2(last_lux_value / 2.5) : 0;
  
  // 根据当前模式确定要计算的值
  if (current_mode == 'i') {
    // ISO优先模式：重新计算光圈
    float calculated_aperture = compute_aperture(ev_value, current_iso, current_shutter);
    
    // 查找最接近的光圈值
    int closest_aperture_idx = 0;
    float min_diff = abs(aperture_values[0] - calculated_aperture);
    for (int i = 1; i < aperture_count; i++) {
      float diff = abs(aperture_values[i] - calculated_aperture);
      if (diff < min_diff) {
        min_diff = diff;
        closest_aperture_idx = i;
      }
    }
    
    // 根据需要更新光圈
    if (abs(aperture_values[preview_indices[1]] - aperture_values[closest_aperture_idx]) > 0.01) {
      preview_indices[1] = closest_aperture_idx;
    }
    
  } else if (current_mode == 'a') {
    // 光圈优先模式：重新计算快门速度
    float calculated_shutter = compute_shutter_speed(ev_value, current_iso, current_aperture);
    
    // 查找最接近的快门速度值
    int closest_shutter_idx = 0;
    float min_diff = abs(shutter_to_float(shutter_values[0]) - calculated_shutter);
    for (int i = 1; i < shutter_count; i++) {
      float diff = abs(shutter_to_float(shutter_values[i]) - calculated_shutter);
      if (diff < min_diff) {
        min_diff = diff;
        closest_shutter_idx = i;
      }
    }
    
    // 根据需要更新快门
    if (abs(shutter_to_float(shutter_values[preview_indices[2]]) - shutter_to_float(shutter_values[closest_shutter_idx])) > 0.001) {
      preview_indices[2] = closest_shutter_idx;
    }
    
  } else if (current_mode == 's') {
    // 快门优先模式：重新计算光圈
    float calculated_aperture = compute_aperture(ev_value, current_iso, current_shutter);
    
    // 查找最接近的光圈值
    int closest_aperture_idx = 0;
    float min_diff = abs(aperture_values[0] - calculated_aperture);
    for (int i = 1; i < aperture_count; i++) {
      float diff = abs(aperture_values[i] - calculated_aperture);
      if (diff < min_diff) {
        min_diff = diff;
        closest_aperture_idx = i;
      }
    }
    
    // 根据需要更新光圈
    if (abs(aperture_values[preview_indices[1]] - aperture_values[closest_aperture_idx]) > 0.01) {
      preview_indices[1] = closest_aperture_idx;
    }
  }
  
  // 更新UI显示
  update_parameter_colors();
}

bool is_choice_valid(const char* param_key, int choice_idx) {
  // ISO值始终被视为有效
  if (strcmp(param_key, "ISO") == 0) {
    return true;
  }
  
  if (!dlight_0 || last_lux_value <= 0) {
    return true;
  }
  
  try {
    float ev = log2(last_lux_value / 2.5);
    int current_iso = iso_values[preview_indices[0]];
    
    if (strcmp(param_key, "APERTURE") == 0) {
      // 测试光圈值是否有效
      float aperture_to_test = aperture_values[choice_idx];
      float shutter_float_result = compute_shutter_speed(ev, current_iso, aperture_to_test);
      float min_shutter_val = shutter_to_float(shutter_values[shutter_count - 1]);
      float max_shutter_val = shutter_to_float(shutter_values[0]);
      return min_shutter_val <= shutter_float_result && shutter_float_result <= max_shutter_val;
    } else if (strcmp(param_key, "SHUTTER") == 0) {
      // 测试快门值是否有效
      float shutter_float_to_test = shutter_to_float(shutter_values[choice_idx]);
      float aperture_float_result = compute_aperture(ev, current_iso, shutter_float_to_test);
      return aperture_values[0] <= aperture_float_result && aperture_float_result <= aperture_values[aperture_count - 1];
    }
  } catch (...) {
    return false;
  }
  
  return false;
}

void draw_parameter_list() {
  param_list_canvas->fillScreen(COLOR_BACKGROUND);
  param_list_canvas->setTextSize(PARAM_LIST_FONT_SIZE);
  
  const char* mode_map[] = {"ISO", "APERTURE", "SHUTTER"};
  int param_index = current_mode == 'i' ? 0 : (current_mode == 'a' ? 1 : 2);
  const char* param_key = mode_map[param_index];
  
  // 设置参数值
  const char** param_values = nullptr;
  int param_count = 0;
  
  if (param_index == 0) {
    // ISO值
    static char* iso_strings[10];
    static char iso_buffers[10][8];
    for (int i = 0; i < iso_count; i++) {
      sprintf(iso_buffers[i], "%d", iso_values[i]);
      iso_strings[i] = iso_buffers[i];
    }
    param_values = const_cast<const char**>(iso_strings);
    param_count = iso_count;
  } else if (param_index == 1) {
    // 光圈值
    static char* aperture_strings[14];
    static char aperture_buffers[14][10];
    for (int i = 0; i < aperture_count; i++) {
      if (aperture_values[i] == floor(aperture_values[i])) {
        // 如果是整数，不显示小数点
        sprintf(aperture_buffers[i], "f/%d", (int)aperture_values[i]);
      } else {
        // 否则显示一位小数
        sprintf(aperture_buffers[i], "f/%.1f", aperture_values[i]);
      }
      aperture_strings[i] = aperture_buffers[i];
    }
    param_values = const_cast<const char**>(aperture_strings);
    param_count = aperture_count;
  } else if (param_index == 2) {
    // 快门值
    param_values = shutter_values;
    param_count = shutter_count;
  }
  
  int current_index = preview_indices[param_index];
  
  // 计算画布上的中心位置
  int center_y_on_canvas = (PARAMETER_LIST_SIZE / 2) * LIST_ITEM_HEIGHT;
  
  // 绘制列表中的每个项目
  for (int data_idx = 0; data_idx < param_count; data_idx++) {
    // 计算带有动画的绘制位置
    int draw_y = int((data_idx * LIST_ITEM_HEIGHT) + anim_current_y + center_y_on_canvas);
    
    // 只绘制在可见范围内的项目
    if (-LIST_ITEM_HEIGHT < draw_y && draw_y < PARAM_LIST_HEIGHT) {
      // 获取要绘制的文本，带有适当的前缀
      const char* value_text = param_values[data_idx];
      
      // 计算文本位置 - 居中以更好地显示长值
      int text_x = PARAM_LIST_WIDTH / 2;  // 中心位置
      
      // 根据位置和有效性确定文本颜色
      int distance_from_center = abs(data_idx - current_index);
      bool is_valid = is_choice_valid(param_key, data_idx);
      uint16_t text_color = COLOR_DEFAULT;
      
      if (is_valid) {
        if (distance_from_center == 0) {
          text_color = COLOR_FOCUSED;
        } else if (distance_from_center == 1) {
          text_color = COLOR_GRADIENT_1;
        } else {
          text_color = COLOR_GRADIENT_2;
        }
      } else {
        // 确保无效值被正确高亮显示
        if (distance_from_center == 0) {
          text_color = COLOR_INVALID;
        } else if (distance_from_center == 1) {
          text_color = COLOR_INVALID_GRADIENT_1;
        } else {
          text_color = COLOR_INVALID_GRADIENT_2;
        }
      }
      
      // 绘制文本 - 居中对齐
      param_list_canvas->setTextColor(text_color, COLOR_BACKGROUND);
      param_list_canvas->setTextDatum(middle_center);
      param_list_canvas->drawString(value_text, text_x, draw_y + LIST_ITEM_HEIGHT / 2);
      
      // 在中心项目周围绘制选择框
      if (distance_from_center == 0) {
        int box_padding_x = 4;
        int box_padding_y = 2;
        int box_x = 4;
        int box_y = draw_y - box_padding_y;
        int box_w = PARAM_LIST_WIDTH - 8;
        int box_h = LIST_ITEM_HEIGHT;
        
        // 绘制框边框
        //param_list_canvas->drawRect(box_x, box_y, box_w, box_h, text_color);
        // 只绘制上边框和下边框（移除左右边框）
        param_list_canvas->drawFastHLine(box_x, box_y, box_w, text_color);          // 上边框
        param_list_canvas->drawFastHLine(box_x, box_y + box_h - 1, box_w, text_color);  // 下边框
      }
    }
  }
  
  // 将画布推送到显示屏
  param_list_canvas->pushSprite(PARAM_LIST_X, PARAM_LIST_Y);
}

void scroll_list(int direction) {
  int param_index = current_mode == 'i' ? 0 : (current_mode == 'a' ? 1 : 2);
  int param_count = 0;
  
  if (param_index == 0) {
    param_count = iso_count;
  } else if (param_index == 1) {
    param_count = aperture_count;
  } else if (param_index == 2) {
    param_count = shutter_count;
  }
  
  // 保存旧位置用于动画
  anim_y_start_pos = anim_current_y;
  anim_start_time = millis();
  is_animating = true;
  
  // 更新索引 - 反转方向以匹配Python版本行为
  preview_indices[param_index] -= direction;
  preview_indices[param_index] = constrain(preview_indices[param_index], 0, param_count - 1);
  
  // 根据需要重新计算其他参数
  update_and_recalculate();
}

void draw_left_panel() {
  left_panel_canvas->fillScreen(COLOR_BACKGROUND);
  
  // 获取当前参数值
  int current_iso = iso_values[preview_indices[0]];
  float current_aperture = aperture_values[preview_indices[1]];
  float current_shutter = shutter_to_float(shutter_values[preview_indices[2]]);
  
  // 根据当前光照读数计算EV值
  float ev_value = last_lux_value > 0 ? log2(last_lux_value / 2.5) : 0;
  
  // 根据当前模式确定要计算的值
  if (current_mode == 'i') {
    // ISO优先模式：重新计算光圈
    float calculated_aperture = compute_aperture(ev_value, current_iso, current_shutter);
    
    // 查找最接近的光圈值
    int closest_aperture_idx = 0;
    float min_diff = abs(aperture_values[0] - calculated_aperture);
    for (int i = 1; i < aperture_count; i++) {
      float diff = abs(aperture_values[i] - calculated_aperture);
      if (diff < min_diff) {
        min_diff = diff;
        closest_aperture_idx = i;
      }
    }
    
    // 根据需要更新光圈
    if (abs(aperture_values[preview_indices[1]] - aperture_values[closest_aperture_idx]) > 0.01) {
      preview_indices[1] = closest_aperture_idx;
    }
    
  } else if (current_mode == 'a') {
    // 光圈优先模式：重新计算快门速度
    float calculated_shutter = compute_shutter_speed(ev_value, current_iso, current_aperture);
    
    // 查找最接近的快门速度值
    int closest_shutter_idx = 0;
    float min_diff = abs(shutter_to_float(shutter_values[0]) - calculated_shutter);
    for (int i = 1; i < shutter_count; i++) {
      float diff = abs(shutter_to_float(shutter_values[i]) - calculated_shutter);
      if (diff < min_diff) {
        min_diff = diff;
        closest_shutter_idx = i;
      }
    }
    
    // 根据需要更新快门
    if (abs(shutter_to_float(shutter_values[preview_indices[2]]) - shutter_to_float(shutter_values[closest_shutter_idx])) > 0.001) {
      preview_indices[2] = closest_shutter_idx;
    }
    
  } else if (current_mode == 's') {
    // 快门优先模式：重新计算光圈
    float calculated_aperture = compute_aperture(ev_value, current_iso, current_shutter);
    
    // 查找最接近的光圈值
    int closest_aperture_idx = 0;
    float min_diff = abs(aperture_values[0] - calculated_aperture);
    for (int i = 1; i < aperture_count; i++) {
      float diff = abs(aperture_values[i] - calculated_aperture);
      if (diff < min_diff) {
        min_diff = diff;
        closest_aperture_idx = i;
      }
    }
    
    // 根据需要更新光圈
    if (abs(aperture_values[preview_indices[1]] - aperture_values[closest_aperture_idx]) > 0.01) {
      preview_indices[1] = closest_aperture_idx;
    }
  }
  
  // 更新当前值
  current_iso = iso_values[preview_indices[0]];
  current_aperture = aperture_values[preview_indices[1]];
  current_shutter = shutter_to_float(shutter_values[preview_indices[2]]);
  
  // 使用统一的字体大小绘制参数标签和值
  left_panel_canvas->setTextSize(LEFT_PANEL_FONT_SIZE);
  left_panel_canvas->setTextDatum(middle_left);  // 为所有元素设置文本基准点为middle_left
  
  // 以Python风格格式绘制光照度："LUX:150"
  left_panel_canvas->setTextColor(COLOR_DEFAULT);
  left_panel_canvas->drawString("LUX:" + String(last_lux_value, 0), LEFT_PANEL_LABEL_X, LUX_LABEL_Y);
  
  // 绘制ISO - 左对齐
  left_panel_canvas->setTextColor(current_mode == 'i' ? COLOR_FOCUSED : COLOR_DEFAULT);
  left_panel_canvas->drawString("ISO:" + String(current_iso), LEFT_PANEL_LABEL_X, ISO_LABEL_Y);
  
  // 绘制光圈 - 左对齐，简化标签，格式化光圈值
  left_panel_canvas->setTextColor(current_mode == 'a' ? COLOR_FOCUSED : COLOR_DEFAULT);
  left_panel_canvas->drawString("APER:f/" + format_aperture(current_aperture), LEFT_PANEL_LABEL_X, APERTURE_LABEL_Y);
  
  // 绘制快门 - 左对齐，简化标签
  left_panel_canvas->setTextColor(current_mode == 's' ? COLOR_FOCUSED : COLOR_DEFAULT);
  String shutter_str = "SSPD:";
  shutter_str += shutter_values[preview_indices[2]];
  left_panel_canvas->drawString(shutter_str, LEFT_PANEL_LABEL_X, SHUTTER_LABEL_Y);
  
  // 绘制边框
  //left_panel_canvas->drawRect(0, 0, LEFT_PANEL_WIDTH, LEFT_PANEL_HEIGHT, COLOR_DEFAULT);
  
  // 将画布推送到显示屏
  left_panel_canvas->pushSprite(LEFT_PANEL_X, LEFT_PANEL_Y);
}

void update_parameter_colors() {
  // 根据当前模式更新UI颜色
  draw_left_panel();
  draw_parameter_list();
}

void init_hardware() {
  // 初始化DLight传感器
  dlight_0 = new M5_DLight(0x23);
  
  try {
    dlight_0->begin(&Wire, 2, 1, 100000); // sda=引脚(2), scl=引脚(1), 频率=100kHz
    dlight_0->setMode(CONTINUOUSLY_H_RESOLUTION_MODE);
    debug_message = "LUX: 0.00";
  } catch (...) {
    debug_message = "DLight sensor not found";
    delete dlight_0;
    dlight_0 = nullptr;
  }
  
  last_lux_value = 0; // 默认值
}

// 更新调试标题
void update_debug_title() {
  // 清空标题栏区域
  M5Cardputer.Display.fillRect(0, 0, TITLE_BAR_WIDTH, TITLE_BAR_HEIGHT, COLOR_TITLE_BG);
  
  // 设置字体
  M5Cardputer.Display.setTextColor(COLOR_DEFAULT, COLOR_TITLE_BG);
  M5Cardputer.Display.setTextSize(TITLE_BAR_FONT_SIZE);
  M5Cardputer.Display.setFont(TITLE_BAR_FONT);
  
  // 绘制标题
  M5Cardputer.Display.setCursor(TITLE_TEXT_X, TITLE_TEXT_Y);
  M5Cardputer.Display.print("LightMeter");
  
  // 绘制电池标签（已注释掉）
  //M5Cardputer.Display.setCursor(BATTERY_LABEL_X, BATTERY_LABEL_Y);
  //M5Cardputer.Display.print("Battery:");
  
  // 绘制电池电量
  M5Cardputer.Display.setCursor(BATTERY_PERCENT_X, BATTERY_PERCENT_Y);
  M5Cardputer.Display.print(String(M5Cardputer.Power.getBatteryLevel()) + "%");
}

void init_ui() {
  // 使用调试标题初始化UI
  update_debug_title();
  
  // 创建画布
  param_list_canvas = new M5Canvas(&M5Cardputer.Display);
  param_list_canvas->createSprite(PARAM_LIST_WIDTH, PARAM_LIST_HEIGHT);
  // 设置参数列表字体
  param_list_canvas->setFont(PARAM_LIST_FONT);
  param_list_canvas->setTextSize(PARAM_LIST_FONT_SIZE);
  
  left_panel_canvas = new M5Canvas(&M5Cardputer.Display);
  left_panel_canvas->createSprite(LEFT_PANEL_WIDTH, LEFT_PANEL_HEIGHT);
  // 设置左侧面板字体
  left_panel_canvas->setFont(LEFT_PANEL_FONT);
  left_panel_canvas->setTextSize(LEFT_PANEL_FONT_SIZE);
}

// 初始化设置
void setup() {
  M5Cardputer.begin();
  // 设置屏幕旋转为1（正确方向）
  M5Cardputer.Display.setRotation(1);
  M5Cardputer.Display.setBrightness(160);
  
  // 使用我们定义的字体常量而不是硬编码值
  //M5Cardputer.Display.setTextSize(TITLE_BAR_FONT_SIZE);
  //M5Cardputer.Display.setTextColor(COLOR_DEFAULT, COLOR_BACKGROUND);
  //M5Cardputer.Display.setFont(TITLE_BAR_FONT);
  
  // 初始化键盘
  M5Cardputer.Keyboard.begin();
  
  init_hardware();
  init_ui();
  update_parameter_colors();
  
  // 初始化动画位置
  const char* mode_map[] = {"ISO", "APERTURE", "SHUTTER"};
  int param_index = current_mode == 'i' ? 0 : (current_mode == 'a' ? 1 : 2);
  int initial_idx = preview_indices[param_index];
  anim_current_y = -initial_idx * LIST_ITEM_HEIGHT;
  
  update_and_recalculate();
}

void loop() {
  M5Cardputer.update();
  
  if (is_animating) {
    // --- 动画逻辑 ---
    int param_index = current_mode == 'i' ? 0 : (current_mode == 'a' ? 1 : 2);
    int target_idx = preview_indices[param_index];
    float anim_y_end_pos = -target_idx * LIST_ITEM_HEIGHT;
    
    unsigned long elapsed_time = millis() - anim_start_time;
    if (elapsed_time >= anim_duration) {
      anim_current_y = anim_y_end_pos;
      is_animating = false;
    } else {
      float progress = static_cast<float>(elapsed_time) / anim_duration;
      anim_current_y = anim_y_start_pos + (anim_y_end_pos - anim_y_start_pos) * progress;
    }
    draw_parameter_list();
  
  } else {
      // --- 键盘事件处理 ---
      // 更新键盘状态
      M5Cardputer.Keyboard.updateKeysState();
      
      // 检查键盘变化
      if (M5Cardputer.Keyboard.isChange()) {
        // 检查模式切换键
        if (M5Cardputer.Keyboard.isKeyPressed('i')) {
          current_mode = 'i';
          update_parameter_colors();
          update_and_recalculate();
        } else if (M5Cardputer.Keyboard.isKeyPressed('a')) {
          current_mode = 'a';
          priority_mode = 'A';
          update_parameter_colors();
          update_and_recalculate();
        } else if (M5Cardputer.Keyboard.isKeyPressed('s')) {
          current_mode = 's';
          priority_mode = 'S';
          update_parameter_colors();
          update_and_recalculate();
        } else if (M5Cardputer.Keyboard.isKeyPressed(KEY_TAB)) {
          // Tab键循环切换i, a, s模式
          if (current_mode == 'i') {
            current_mode = 'a';
            priority_mode = 'A';
          } else if (current_mode == 'a') {
            current_mode = 's';
            priority_mode = 'S';
          } else if (current_mode == 's') {
            current_mode = 'i';
          }
          update_parameter_colors();
          update_and_recalculate();
        }
        
        // 检查滚动键 - 为匹配Python版本，仅使用点/分号进行滚动
        if (M5Cardputer.Keyboard.isKeyPressed(UP_KEY_STR)) {
          scroll_list(-1);  // 向上滚动
        } else if (M5Cardputer.Keyboard.isKeyPressed(DOWN_KEY_STR)) {
          scroll_list(1);  // 向下滚动
        }
      }
    
    // --- 传感器更新 ---
    // 仅在需要时更新电池显示以减少闪烁
    static unsigned long last_battery_update = 0;
    static uint8_t last_battery_level = 0;
    uint8_t current_battery_level = M5Cardputer.Power.getBatteryLevel();
    if (millis() - last_battery_update > 1000 || abs(current_battery_level - last_battery_level) > 1) {
      last_battery_update = millis();
      last_battery_level = current_battery_level;
      // 仅清除电池区域
      //M5Cardputer.Display.fillRect(BATTERY_LABEL_X, 0, BATTERY_AREA_WIDTH, TITLE_BAR_HEIGHT, COLOR_TITLE_BG);
      M5Cardputer.Display.fillRect(BATTERY_PERCENT_X, 0, BATTERY_AREA_WIDTH, TITLE_BAR_HEIGHT, COLOR_TITLE_BG);
      
      // 使用与标题栏相同的字体设置
      M5Cardputer.Display.setTextColor(COLOR_DEFAULT, COLOR_TITLE_BG);
      M5Cardputer.Display.setTextSize(TITLE_BAR_FONT_SIZE); // 使用标题栏字体大小常量
      M5Cardputer.Display.setFont(TITLE_BAR_FONT);          // 使用标题栏字体类型常量
      
      //M5Cardputer.Display.setCursor(BATTERY_LABEL_X, BATTERY_LABEL_Y);
      //M5Cardputer.Display.print("电量:");
      M5Cardputer.Display.setCursor(BATTERY_PERCENT_X, BATTERY_PERCENT_Y);
      M5Cardputer.Display.print(String(current_battery_level) + "%");
    }
    
    // 降低传感器更新频率以减少闪烁
    static unsigned long last_sensor_update = 0;
    if (millis() - last_sensor_update > 200) {  // 每200ms更新一次传感器
      last_sensor_update = millis();
      if (dlight_0) {
        try {
          // getLUX返回的是uint16_t类型，需要正确处理
          uint16_t raw_lux = dlight_0->getLUX();
          // 转换为float以便计算
          float current_lux = static_cast<float>(raw_lux);
          // 始终更新传感器值
          float previous_lux = last_lux_value;
          last_lux_value = current_lux;
          // 更新调试信息，包含LUX值和调试信息
          debug_message = "LUX: " + String(current_lux) + " (RAW: " + String(raw_lux) + ")";
          // 只有当有明显变化时才触发完全重新计算
          if (abs(current_lux - previous_lux) > 1) {
            update_and_recalculate();
          } else {
            // 仅更新光照度显示，无需完全重新计算
            draw_left_panel();
          }
        } catch (...) {
            // 更新调试信息以显示错误
            debug_message = "DLight read error";
            if (last_lux_value != 0) {
            last_lux_value = 0;
            draw_left_panel();
          }
        }
      } else {
        // 传感器未初始化，显示状态信息
        debug_message = "DLight sensor not found";
      }
    }
  }
  
  delay(20);
}