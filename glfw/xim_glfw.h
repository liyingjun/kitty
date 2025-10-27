//========================================================================
// GLFW 3.4 XIM - www.glfw.org
//------------------------------------------------------------------------
// Copyright (c) 2024 Custom XIM Implementation for Kitty
//
// This software is provided 'as-is', without any express or implied
// warranty. In no event will the authors be held liable for any damages
// arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would
//    be appreciated but is not required.
//
// 2. Altered source versions must be plainly marked as such, and must not
//    be misrepresented as being the original software.
//
// 3. This notice may not be removed or altered from any source
//    distribution.
//
//========================================================================

#pragma once

#include "internal.h"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xlocale.h>

// XIM 数据结构
typedef struct {
    XIM im;                      // X Input Method
    XIC ic;                      // X Input Context
    bool inited;                 // 是否已初始化
    bool enabled;                // 是否启用
    XIMStyle style;              // 输入样式
    Window window;               // 关联的窗口
    int spot_x, spot_y;          // 光标位置
} _GLFWXIMData;

// 初始化 XIM
void glfw_xim_init(_GLFWXIMData *xim, Display *display);

// 为窗口创建输入上下文
void glfw_xim_create_ic(_GLFWXIMData *xim, Display *display, Window window);

// 销毁输入上下文
void glfw_xim_destroy_ic(_GLFWXIMData *xim);

// 终止 XIM
void glfw_xim_terminate(_GLFWXIMData *xim);

// 设置焦点
void glfw_xim_set_focus(_GLFWXIMData *xim, bool focused);

// 过滤 X 事件（返回 true 表示已处理）
bool glfw_xim_filter_event(_GLFWXIMData *xim, XEvent *event);

// 处理按键事件并查找字符串
int glfw_xim_lookup_string(_GLFWXIMData *xim, XKeyPressedEvent *event,
                            char *buffer, int buffer_size,
                            KeySym *keysym, Status *status);

// 设置光标位置（用于预编辑窗口）
void glfw_xim_set_spot_location(_GLFWXIMData *xim, int x, int y);

// 启用/禁用 XIM
void glfw_xim_set_enabled(_GLFWXIMData *xim, bool enabled);

