/**
 * @file Win32IMEService.h
 * @brief IMEService 的 Win32 占位实现（M0 阶段存根）。
 */

#pragma once

#include <mine/platform/IMEService.h>

namespace mine::platform::win32 {

/**
 * @brief Win32 输入法服务存根实现。
 *
 * M0 阶段暂不实现 IME 功能，所有方法为空操作。
 * 待 mine.text 模块（L3）实现后补充完整逻辑。
 */
class Win32IMEService final : public IMEService {
public:
    Win32IMEService() = default;
    ~Win32IMEService() override = default;

    void enable(math::Rect /*composition_rect*/) override {
        // TODO(IME): M0 阶段暂未实现
    }

    void disable() override {
        // TODO(IME): M0 阶段暂未实现
    }

    [[nodiscard]] bool is_enabled() const override {
        return false;
    }

    void set_composition_rect(math::Rect /*composition_rect*/) override {
        // TODO(IME): M0 阶段暂未实现
    }
};

} // namespace mine::platform::win32
