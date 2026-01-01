#include "util/shared_view_holder.hpp"

std::mutex& SharedViewHolder::getMutex() {
    static std::mutex mutex;
    return mutex;
}

std::unordered_map<void*, std::shared_ptr<void>>& SharedViewHolder::getHeldViews() {
    static std::unordered_map<void*, std::shared_ptr<void>> held_views;
    return held_views;
}

void SharedViewHolder::release(void* view) {
    std::lock_guard<std::mutex> lock(getMutex());
    getHeldViews().erase(view);
}

bool SharedViewHolder::isHeld(void* view) {
    std::lock_guard<std::mutex> lock(getMutex());
    return getHeldViews().find(view) != getHeldViews().end();
}
