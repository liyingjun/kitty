//========================================================================
// GLFW 3.4 XIM - www.glfw.org
//------------------------------------------------------------------------
// Copyright (c) 2024 Custom XIM Implementation for Kitty
//
// XIM (X Input Method) 支持实现
// 用于支持 fcitx 4.x 等使用 XIM 协议的输入法
//
// 使用方法:
//   设置环境变量 GLFW_IM_MODULE=xim
//   然后启动 kitty
//
//========================================================================

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>

#include "internal.h"
#include "xim_glfw.h"

#define debug(...) if (_glfw.hints.init.debugKeyboard) printf(__VA_ARGS__);

// 检查环境变量
static bool test_env_var(const char *name, const char *val) {
    const char *q = getenv(name);
    return (q && strcmp(q, val) == 0);
}

// XIM 回调函数：预编辑开始
static int preedit_start_callback(XIC ic UNUSED, XPointer client_data UNUSED, XPointer call_data UNUSED) {
    debug("XIM: Preedit start\n");
    return -1;  // 返回 -1 表示没有最大长度限制
}

// XIM 回调函数：预编辑完成
static void preedit_done_callback(XIC ic UNUSED, XPointer client_data UNUSED, XPointer call_data UNUSED) {
    debug("XIM: Preedit done\n");
}

// XIM 回调函数：预编辑绘制（显示正在输入的内容）
static void preedit_draw_callback(XIC ic UNUSED, XPointer client_data UNUSED, XIMPreeditDrawCallbackStruct *call_data) {
    debug("XIM: Preedit draw\n");
    
    if (!call_data || !call_data->text) return;
    
    // 获取预编辑文本
    if (call_data->text->string.multi_byte) {
        debug("XIM: Preedit text: %s\n", call_data->text->string.multi_byte);
        
        // 这里可以通过回调将预编辑文本发送到 kitty
        // 目前简化处理，只打印调试信息
    }
}

// XIM 回调函数：预编辑光标移动
static void preedit_caret_callback(XIC ic UNUSED, XPointer client_data UNUSED, XIMPreeditCaretCallbackStruct *call_data UNUSED) {
    debug("XIM: Preedit caret\n");
}

// 初始化 XIM
void glfw_xim_init(_GLFWXIMData *xim, Display *display) {
    // 确保结构体被初始化为零
    if (!xim) return;
    
    // 如果已经初始化，直接返回
    if (xim->inited) return;
    
    // 清零结构体（防止未初始化的成员）
    memset(xim, 0, sizeof(_GLFWXIMData));
    
    // 检查是否启用 XIM
    if (!test_env_var("GLFW_IM_MODULE", "xim")) {
        debug("XIM not enabled (GLFW_IM_MODULE != xim)\n");
        return;
    }
    
    debug("Initializing XIM...\n");
    
    // 设置 locale（XIM 需要正确的 locale）
    if (!setlocale(LC_CTYPE, "")) {
        _glfwInputError(GLFW_PLATFORM_ERROR, "XIM: Failed to set locale");
        return;
    }
    
    // 确保 X11 支持当前 locale
    if (!XSupportsLocale()) {
        _glfwInputError(GLFW_PLATFORM_ERROR, "XIM: X11 does not support current locale");
        return;
    }
    
    // 设置 locale 修饰符（从环境变量读取）
    XSetLocaleModifiers("");
    
    // 打开输入法
    xim->im = XOpenIM(display, NULL, NULL, NULL);
    if (!xim->im) {
        _glfwInputError(GLFW_PLATFORM_ERROR, "XIM: Failed to open X Input Method");
        debug("XIM: XOpenIM failed. Make sure your input method (fcitx) is running.\n");
        return;
    }
    
    // 查询支持的输入样式
    XIMStyles *styles = NULL;
    if (XGetIMValues(xim->im, XNQueryInputStyle, &styles, NULL) || !styles) {
        _glfwInputError(GLFW_PLATFORM_ERROR, "XIM: Failed to query input styles");
        XCloseIM(xim->im);
        xim->im = NULL;
        return;
    }
    
    // 选择最佳的输入样式
    // 优先顺序: PreeditPosition > PreeditNothing > Root
    XIMStyle best_style = 0;
    XIMStyle preferred_styles[] = {
        XIMPreeditPosition | XIMStatusNothing,
        XIMPreeditNothing | XIMStatusNothing,
        XIMPreeditNone | XIMStatusNone,
    };
    
    for (size_t i = 0; i < sizeof(preferred_styles) / sizeof(preferred_styles[0]); i++) {
        for (int j = 0; j < styles->count_styles; j++) {
            if (styles->supported_styles[j] == preferred_styles[i]) {
                best_style = preferred_styles[i];
                goto style_found;
            }
        }
    }
    
    // 如果没有找到首选样式，使用第一个可用的
    if (styles->count_styles > 0) {
        best_style = styles->supported_styles[0];
    }
    
style_found:
    XFree(styles);
    
    if (best_style == 0) {
        _glfwInputError(GLFW_PLATFORM_ERROR, "XIM: No suitable input style found");
        XCloseIM(xim->im);
        xim->im = NULL;
        return;
    }
    
    xim->style = best_style;
    xim->inited = true;
    xim->enabled = true;
    xim->ic = NULL;
    xim->window = None;
    xim->spot_x = 0;
    xim->spot_y = 0;
    
    debug("XIM initialized successfully (style: 0x%lx)\n", best_style);
}

// 为窗口创建输入上下文
void glfw_xim_create_ic(_GLFWXIMData *xim, Display *display UNUSED, Window window) {
    if (!xim || !xim->inited || !xim->im) {
        debug("XIM: Cannot create IC - not initialized (inited=%d, im=%p)\n", 
              xim ? xim->inited : 0, xim ? (void*)xim->im : NULL);
        return;
    }
    
    if (xim->ic) {
        // 如果窗口改变了，需要重新创建 IC
        if (xim->window != window) {
            debug("XIM: Window changed (old=0x%lx, new=0x%lx), destroying old IC\n", 
                  xim->window, window);
            glfw_xim_destroy_ic(xim);
        } else {
            debug("XIM: IC already exists for this window\n");
            return;
        }
    }
    
    debug("XIM: Creating input context for window 0x%lx\n", window);
    
    xim->window = window;
    
    // 根据样式创建输入上下文
    XVaNestedList preedit_attr = NULL;
    
    if (xim->style & XIMPreeditPosition) {
        // 设置预编辑位置
        XPoint spot = {0, 0};
        
        // 创建回调结构 - 使用联合类型避免函数指针类型转换警告
        XIMCallback preedit_start_cb;
        preedit_start_cb.client_data = NULL;
        preedit_start_cb.callback = (XIMProc)(void (*)(void))preedit_start_callback;
        
        XIMCallback preedit_done_cb;
        preedit_done_cb.client_data = NULL;
        preedit_done_cb.callback = (XIMProc)(void (*)(void))preedit_done_callback;
        
        XIMCallback preedit_draw_cb;
        preedit_draw_cb.client_data = NULL;
        preedit_draw_cb.callback = (XIMProc)(void (*)(void))preedit_draw_callback;
        
        XIMCallback preedit_caret_cb;
        preedit_caret_cb.client_data = NULL;
        preedit_caret_cb.callback = (XIMProc)(void (*)(void))preedit_caret_callback;
        
        preedit_attr = XVaCreateNestedList(0,
            XNSpotLocation, &spot,
            XNPreeditStartCallback, &preedit_start_cb,
            XNPreeditDoneCallback, &preedit_done_cb,
            XNPreeditDrawCallback, &preedit_draw_cb,
            XNPreeditCaretCallback, &preedit_caret_cb,
            NULL);
    }
    
    // 创建输入上下文（尝试更简单的样式）
    if (preedit_attr) {
        xim->ic = XCreateIC(xim->im,
            XNInputStyle, xim->style,
            XNClientWindow, window,
            XNFocusWindow, window,
            XNPreeditAttributes, preedit_attr,
            NULL);
        XFree(preedit_attr);
    }
    
    // 如果上面失败，尝试不带 preedit 属性的简单样式
    if (!xim->ic) {
        debug("XIM: Trying simpler IC creation without preedit attributes\n");
        xim->ic = XCreateIC(xim->im,
            XNInputStyle, XIMPreeditNothing | XIMStatusNothing,
            XNClientWindow, window,
            XNFocusWindow, window,
            NULL);
        if (xim->ic) {
            xim->style = XIMPreeditNothing | XIMStatusNothing;
        }
    }
    
    if (!xim->ic) {
        _glfwInputError(GLFW_PLATFORM_ERROR, "XIM: Failed to create input context");
        debug("XIM: XCreateIC failed for all styles\n");
        return;
    }
    
    // 获取 XIM 需要的事件掩码
    long im_event_mask = 0;
    if (XGetICValues(xim->ic, XNFilterEvents, &im_event_mask, NULL) == NULL) {
        // 设置窗口事件掩码（需要在窗口创建时处理）
        debug("XIM: IC created, event mask: 0x%lx\n", im_event_mask);
    }
    
    debug("XIM: Input context created successfully\n");
}

// 销毁输入上下文
void glfw_xim_destroy_ic(_GLFWXIMData *xim) {
    if (!xim) return;
    if (!xim->ic) return;  // 已经销毁，直接返回
    
    debug("XIM: Destroying input context (window=0x%lx)\n", xim->window);
    
    // 先保存IC指针，然后设置为NULL，避免重复释放
    XIC ic = xim->ic;
    xim->ic = NULL;
    xim->window = None;
    
    // 销毁IC
    XDestroyIC(ic);
    
    debug("XIM: Input context destroyed\n");
}

// 终止 XIM
void glfw_xim_terminate(_GLFWXIMData *xim) {
    if (!xim) return;
    if (!xim->inited) return;  // 未初始化，无需终止
    
    debug("XIM: Terminating (im=%p, ic=%p)\n", (void*)xim->im, (void*)xim->ic);
    
    // 先标记为未初始化，避免并发问题
    xim->inited = false;
    xim->enabled = false;
    
    // 销毁输入上下文
    glfw_xim_destroy_ic(xim);
    
    // 关闭输入法连接
    if (xim->im) {
        XIM im = xim->im;
        xim->im = NULL;
        XCloseIM(im);
        debug("XIM: Input method closed\n");
    }
    
    debug("XIM: Terminated successfully\n");
}

// 设置焦点
void glfw_xim_set_focus(_GLFWXIMData *xim, bool focused) {
    if (!xim) {
        debug("XIM: set_focus called but xim is NULL\n");
        return;
    }
    if (!xim->ic) {
        debug("XIM: set_focus called but IC is NULL\n");
        return;
    }
    
    if (focused) {
        debug("XIM: Focus in\n");
        XSetICFocus(xim->ic);
    } else {
        debug("XIM: Focus out\n");
        XUnsetICFocus(xim->ic);
    }
}

// 过滤 X 事件
bool glfw_xim_filter_event(_GLFWXIMData *xim, XEvent *event) {
    if (!xim || !xim->ic || !xim->enabled) return false;
    
    // 关键：XFilterEvent 的第二个参数应该传 None，让 XIM 自己判断
    // 而不是传具体的窗口 ID
    Bool filtered = XFilterEvent(event, None);
    
    if (filtered) {
        // 事件被 XIM 过滤（例如输入法切换、预编辑等）
        if (event->type == KeyPress || event->type == KeyRelease) {
            debug("XIM: Keyboard event filtered (IM switch or preedit)\n");
        } else {
            debug("XIM: Non-keyboard event filtered\n");
        }
        return true;
    }
    
    return false;
}

// 处理按键事件并查找字符串
int glfw_xim_lookup_string(_GLFWXIMData *xim, XKeyPressedEvent *event,
                            char *buffer, int buffer_size,
                            KeySym *keysym, Status *status) {
    if (!xim->ic || !xim->enabled) {
        // XIM 未启用，使用标准查找
        return XLookupString(event, buffer, buffer_size, keysym, NULL);
    }
    
    // 使用 XIM 查找字符串（支持输入法）
    int len = Xutf8LookupString(xim->ic, event, buffer, buffer_size, keysym, status);
    
    if (*status == XBufferOverflow) {
        debug("XIM: Buffer overflow (needed %d bytes)\n", len);
        return 0;
    }
    
    if (*status == XLookupChars || *status == XLookupBoth) {
        buffer[len] = '\0';
        debug("XIM: Lookup result: '%s' (len=%d, status=%d)\n", buffer, len, *status);
    } else if (*status == XLookupKeySym) {
        debug("XIM: Lookup keysym only: 0x%lx\n", *keysym);
    }
    
    return len;
}

// 设置光标位置
void glfw_xim_set_spot_location(_GLFWXIMData *xim, int x, int y) {
    if (!xim || !xim->ic) return;
    if (!(xim->style & XIMPreeditPosition)) return;
    
    // 只在位置改变时更新
    if (xim->spot_x == x && xim->spot_y == y) return;
    
    xim->spot_x = x;
    xim->spot_y = y;
    
    XPoint spot = {x, y};
    XVaNestedList preedit_attr = XVaCreateNestedList(0, XNSpotLocation, &spot, NULL);
    XSetICValues(xim->ic, XNPreeditAttributes, preedit_attr, NULL);
    XFree(preedit_attr);
    
    debug("XIM: Spot location updated to (%d, %d)\n", x, y);
}

// 启用/禁用 XIM
void glfw_xim_set_enabled(_GLFWXIMData *xim, bool enabled) {
    if (xim->enabled != enabled) {
        xim->enabled = enabled;
        debug("XIM: %s\n", enabled ? "Enabled" : "Disabled");
    }
}

