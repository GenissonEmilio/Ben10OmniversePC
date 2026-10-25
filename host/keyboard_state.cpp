// SDL events are owned by the video thread; guest pad reads use this snapshot.
#include "keyboard_state.h"
#include <array>
#include <atomic>
#include <algorithm>
#include <cstring>
namespace {
std::array<std::atomic<uint8_t>,512> keys{};
std::atomic<bool> focused{false};
}
extern "C" void ben10_keyboard_focus(int value) {
    if (!value) {
        focused.store(false,std::memory_order_release);
        for (auto& key:keys) key.store(0,std::memory_order_relaxed);
    } else focused.store(true,std::memory_order_release);
}
extern "C" void ben10_keyboard_event(unsigned code,int pressed) {
    if (code<keys.size()) keys[code].store(pressed?1:0,std::memory_order_relaxed);
}
extern "C" int ben10_keyboard_snapshot(uint8_t* output,size_t count) {
    if (!output) return 0;
    std::memset(output,0,count);
    if (!focused.load(std::memory_order_acquire)) return 0;
    for (size_t i=0;i<std::min(count,keys.size());++i)
        output[i]=keys[i].load(std::memory_order_relaxed);
    return 1;
}
