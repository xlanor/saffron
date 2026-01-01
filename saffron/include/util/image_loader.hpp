// Ported from Switchfin: https://github.com/dragonflylee/switchfin
#ifndef SAFFRON_IMAGE_LOADER_HPP
#define SAFFRON_IMAGE_LOADER_HPP

#include <borealis.hpp>
#include <borealis/core/cache_helper.hpp>
#include <curl/curl.h>
#include <nanovg.h>
#include <borealis/extern/nanovg/stb_image.h>

#include <string>
#include <vector>
#include <list>
#include <queue>
#include <unordered_map>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <memory>
#include <functional>

class ImageQueue {
public:
    static constexpr int MAX_CONCURRENT = 3;

    static ImageQueue& instance() {
        static ImageQueue queue;
        return queue;
    }

    void enqueue(std::function<void()> task) {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_queue.push(std::move(task));
        }
        processQueue();
    }

    void onTaskComplete() {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_activeCount > 0) {
                m_activeCount--;
            }
        }
        processQueue();
    }

    void clear() {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::queue<std::function<void()>> empty;
        std::swap(m_queue, empty);
    }

    static void shutdown() {
        auto& queue = instance();
        queue.clear();
        std::lock_guard<std::mutex> lock(queue.m_mutex);
        queue.m_activeCount = 0;
    }

private:
    ImageQueue() = default;

    void processQueue() {
        std::function<void()> task;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_activeCount >= MAX_CONCURRENT || m_queue.empty()) {
                return;
            }
            task = std::move(m_queue.front());
            m_queue.pop();
            m_activeCount++;
        }
        brls::async(std::move(task));
    }

    std::queue<std::function<void()>> m_queue;
    std::mutex m_mutex;
    int m_activeCount = 0;
};

class CurlPool {
public:
    static CurlPool& instance() {
        static CurlPool pool;
        return pool;
    }

    CURL* acquire() {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_pool.empty()) {
            CURL* curl = curl_easy_init();
            if (curl && m_share) {
                curl_easy_setopt(curl, CURLOPT_SHARE, m_share);
            }
            return curl;
        }
        CURL* curl = m_pool.back();
        m_pool.pop_back();
        return curl;
    }

    void release(CURL* curl) {
        if (!curl) return;
        curl_easy_reset(curl);
        if (m_share) {
            curl_easy_setopt(curl, CURLOPT_SHARE, m_share);
        }
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_pool.size() < 8) {
            m_pool.push_back(curl);
        } else {
            curl_easy_cleanup(curl);
        }
    }

    static void shutdown() {
        auto& pool = instance();
        std::lock_guard<std::mutex> lock(pool.m_mutex);
        for (CURL* curl : pool.m_pool) {
            curl_easy_cleanup(curl);
        }
        pool.m_pool.clear();
        if (pool.m_share) {
            curl_share_cleanup(pool.m_share);
            pool.m_share = nullptr;
        }
    }

private:
    CurlPool() {
        m_share = curl_share_init();
        if (m_share) {
            curl_share_setopt(m_share, CURLSHOPT_SHARE, CURL_LOCK_DATA_SSL_SESSION);
            curl_share_setopt(m_share, CURLSHOPT_SHARE, CURL_LOCK_DATA_DNS);
            curl_share_setopt(m_share, CURLSHOPT_LOCKFUNC, lockCallback);
            curl_share_setopt(m_share, CURLSHOPT_UNLOCKFUNC, unlockCallback);
            curl_share_setopt(m_share, CURLSHOPT_USERDATA, this);
        }
    }

    static void lockCallback(CURL*, curl_lock_data, curl_lock_access, void* userptr) {
        static_cast<CurlPool*>(userptr)->m_shareMutex.lock();
    }

    static void unlockCallback(CURL*, curl_lock_data, void* userptr) {
        static_cast<CurlPool*>(userptr)->m_shareMutex.unlock();
    }

    std::vector<CURL*> m_pool;
    std::mutex m_mutex;
    std::mutex m_shareMutex;
    CURLSH* m_share = nullptr;
};

class ImageLoader {
public:
    using Cancel = std::shared_ptr<std::atomic_bool>;
    using Ref = std::shared_ptr<ImageLoader>;

    ImageLoader() {
        m_isCancel = std::make_shared<std::atomic_bool>(false);
    }

    static void pause() { s_paused.store(true); }
    static void resume() { s_paused.store(false); }
    static bool isPaused() { return s_paused.load(); }

    static void load(brls::Image* view, const std::string& url) {
        if (url.empty() || !view) return;
        if (s_paused.load()) return;

        int oldTex = view->getTexture();
        if (oldTex > 0) {
            brls::TextureCache::instance().removeCache(oldTex);
        }

        int tex = brls::TextureCache::instance().getCache(url);
        if (tex > 0) {
            view->setFreeTexture(false);
            view->innerSetImage(tex);
            return;
        }

        Ref item;
        {
            std::lock_guard<std::mutex> lock(s_requestMutex);

            if (s_pool.empty()) {
                item = std::make_shared<ImageLoader>();
            } else {
                item = s_pool.front();
                s_pool.pop_front();
            }

            auto it = s_requests.find(view);
            if (it != s_requests.end()) {
                it->second->m_isCancel->store(true);
                s_pool.push_back(it->second);
                s_requests.erase(it);
            }

            s_requests[view] = item;
        }

        item->m_image = view;
        item->m_url = url;
        item->m_isCancel = std::make_shared<std::atomic_bool>(false);
        view->ptrLock();
        view->setFreeTexture(false);

        ImageQueue::instance().enqueue([item]() {
            item->doRequest(item);
        });
    }

    static void cancel(brls::Image* view) {
        if (!view) return;

        std::lock_guard<std::mutex> lock(s_requestMutex);
        auto it = s_requests.find(view);
        if (it != s_requests.end()) {
            it->second->m_isCancel->store(true);
            it->second->m_image->ptrUnlock();
            it->second->m_image = nullptr;
            s_pool.push_back(it->second);
            s_requests.erase(it);
        }
        view->clear();
    }

    static void cancelAll() {
        ImageQueue::instance().clear();
        std::lock_guard<std::mutex> lock(s_requestMutex);
        for (auto& [view, item] : s_requests) {
            item->m_isCancel->store(true);
            if (item->m_image) {
                item->m_image->ptrUnlock();
                item->m_image = nullptr;
            }
        }
        s_requests.clear();
    }

private:
    brls::Image* m_image = nullptr;
    std::string m_url;
    Cancel m_isCancel;

    inline static std::list<Ref> s_pool;
    inline static std::unordered_map<brls::Image*, Ref> s_requests;
    inline static std::mutex s_requestMutex;
    inline static std::atomic<bool> s_paused{false};

    static size_t writeCallback(char* ptr, size_t size, size_t nmemb, void* userdata) {
        std::string* response = reinterpret_cast<std::string*>(userdata);
        response->append(ptr, size * nmemb);
        return size * nmemb;
    }

    void doRequest(Ref self) {
        Cancel cancelFlag = m_isCancel;
        brls::Image* imagePtr = m_image;
        std::string url = m_url;

        if (cancelFlag->load()) {
            clear(imagePtr, self);
            ImageQueue::instance().onTaskComplete();
            return;
        }

        CURL* curl = CurlPool::instance().acquire();
        if (!curl) {
            clear(imagePtr, self);
            ImageQueue::instance().onTaskComplete();
            return;
        }

        std::string data;
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &data);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, 1L);

        CURLcode res = curl_easy_perform(curl);

        long httpCode = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
        CurlPool::instance().release(curl);

        if (res != CURLE_OK) {
            brls::Logger::error("ImageLoader: curl failed for {} - {}", url, curl_easy_strerror(res));
            clear(imagePtr, self);
            ImageQueue::instance().onTaskComplete();
            return;
        }

        if (httpCode != 200) {
            brls::Logger::debug("ImageLoader: HTTP {} for {}", httpCode, url);
            clear(imagePtr, self);
            ImageQueue::instance().onTaskComplete();
            return;
        }

        if (cancelFlag->load()) {
            clear(imagePtr, self);
            ImageQueue::instance().onTaskComplete();
            return;
        }

        int imageW = 0, imageH = 0, n = 0;
        uint8_t* imageData = stbi_load_from_memory(
            (unsigned char*)data.c_str(), data.size(), &imageW, &imageH, &n, 4);

        if (!imageData) {
            brls::Logger::error("ImageLoader: stbi decode failed for {}", url);
            clear(imagePtr, self);
            ImageQueue::instance().onTaskComplete();
            return;
        }

        if (cancelFlag->load()) {
            stbi_image_free(imageData);
            clear(imagePtr, self);
            ImageQueue::instance().onTaskComplete();
            return;
        }
        brls::sync([imageData, imageW, imageH, cancelFlag, imagePtr, url, self]() {
            if (!cancelFlag->load() && imagePtr && !s_paused.load()) {
                int tex = brls::TextureCache::instance().getCache(url);
                if (tex == 0) {
                    NVGcontext* vg = brls::Application::getNVGContext();
                    tex = nvgCreateImageRGBA(vg, imageW, imageH, 0, imageData);
                    brls::TextureCache::instance().addCache(url, tex);
                }
                if (tex > 0) {
                    brls::Logger::debug("ImageLoader: Loaded {} ({}x{})", url, imageW, imageH);
                    imagePtr->innerSetImage(tex);
                } else {
                    brls::Logger::error("ImageLoader: nvgCreateImageRGBA failed for {}", url);
                }
                clear(imagePtr, self);
            }
            stbi_image_free(imageData);
            ImageQueue::instance().onTaskComplete();
        });
    }

    static void clear(brls::Image* view, Ref self) {
        if (!view) return;

        std::lock_guard<std::mutex> lock(s_requestMutex);
        auto it = s_requests.find(view);
        // Only clean up if this is still OUR request, not a newer one
        if (it != s_requests.end() && it->second == self) {
            it->second->m_image->ptrUnlock();
            it->second->m_image = nullptr;
            s_pool.push_back(it->second);
            s_requests.erase(it);
        }
    }
};

#endif
