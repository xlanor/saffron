#ifndef SAFFRON_SHARED_VIEW_HOLDER_HPP
#define SAFFRON_SHARED_VIEW_HOLDER_HPP

#include <memory>
#include <mutex>
#include <unordered_map>

class SharedViewHolder {
public:
    template<typename T, typename... Args>
    static std::shared_ptr<T> holdNew(Args&&... args);

    template<typename T>
    static void hold(std::shared_ptr<T> view);

    static void release(void* view);
    static bool isHeld(void* view);

private:
    static std::mutex& getMutex();
    static std::unordered_map<void*, std::shared_ptr<void>>& getHeldViews();
};

template<typename T, typename... Args>
std::shared_ptr<T> SharedViewHolder::holdNew(Args&&... args) {
    T* rawPtr = new T(std::forward<Args>(args)...);
    auto ptr = std::shared_ptr<T>(rawPtr, [](T*) {});
    std::lock_guard<std::mutex> lock(getMutex());
    getHeldViews()[static_cast<void*>(rawPtr)] = ptr;
    return ptr;
}

template<typename T>
void SharedViewHolder::hold(std::shared_ptr<T> view) {
    std::lock_guard<std::mutex> lock(getMutex());
    getHeldViews()[static_cast<void*>(view.get())] = view;
}

#endif
