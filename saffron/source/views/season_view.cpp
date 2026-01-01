#include "views/season_view.hpp"
#include "views/media_detail_view.hpp"
#include "core/plex_server.hpp"
#include "core/plex_api.hpp"
#include "util/image_loader.hpp"

SeasonView* SeasonView::s_instance = nullptr;

SeasonView::SeasonView(PlexServer* server, const plex::MediaItem& season)
    : m_server(server), m_season(season) {

    s_instance = this;

    this->setAxis(brls::Axis::COLUMN);
    this->setWidthPercentage(100);
    this->setHeightPercentage(100);

    this->registerAction("Back", brls::BUTTON_B, [](brls::View*) {
        brls::Application::popActivity();
        return true;
    });

    setupUI();
}

SeasonView::~SeasonView() {
    if (s_instance == this) {
        s_instance = nullptr;
    }
}

void SeasonView::willAppear(bool resetState) {
    brls::Box::willAppear(resetState);
    m_isVisible = true;
    if (!m_episodesLoaded) {
        loadEpisodes();
    }
}

void SeasonView::willDisappear(bool resetState) {
    Box::willDisappear(resetState);
    m_isVisible = false;
    for (auto* img : m_episodeThumbs) {
        ImageLoader::cancel(img);
    }
}

brls::View* SeasonView::getDefaultFocus() {
    if (m_episodesList && m_episodesList->getChildren().size() > 0) {
        return m_episodesList->getChildren()[0];
    }
    return this;
}

void SeasonView::setupUI() {
    auto* headerBox = new brls::Box();
    headerBox->setAxis(brls::Axis::COLUMN);
    headerBox->setWidthPercentage(100);
    headerBox->setPadding(30);
    headerBox->setBackgroundColor(nvgRGBA(30, 30, 30, 255));
    this->addView(headerBox);

    m_headerLabel = new brls::Label();
    m_headerLabel->setFontSize(24);
    std::string header = m_season.parentTitle;
    if (!header.empty()) {
        header += " - ";
    }
    header += "Season " + std::to_string(m_season.index);
    m_headerLabel->setText(header);
    headerBox->addView(m_headerLabel);

    auto* scrollView = new brls::ScrollingFrame();
    scrollView->setWidthPercentage(100);
    scrollView->setGrow(1);
    this->addView(scrollView);

    m_episodesList = new brls::Box();
    m_episodesList->setAxis(brls::Axis::COLUMN);
    m_episodesList->setWidthPercentage(100);
    m_episodesList->setPadding(20);
    scrollView->setContentView(m_episodesList);
}

void SeasonView::loadEpisodes() {
    if (!m_server) return;

    int requestId = ++m_requestId;

    PlexApi::getChildren(
        m_server,
        m_season.ratingKey,
        [requestId](std::vector<plex::MediaItem> episodes) {
            if (!s_instance) return;
            if (s_instance->m_requestId != requestId) {
                brls::Logger::debug("SeasonView: Stale request #{} ignored", requestId);
                return;
            }
            if (!s_instance->m_isVisible) {
                brls::Logger::debug("SeasonView: View not visible, skipping episode update");
                return;
            }

            s_instance->m_episodes = episodes;
            s_instance->m_episodesLoaded = true;
            s_instance->m_episodesList->clearViews();
            s_instance->m_episodeThumbs.clear();

            bool isFirst = true;
            for (const auto& episode : episodes) {
                auto* episodeBox = new brls::Box();
                episodeBox->setAxis(brls::Axis::ROW);
                episodeBox->setWidthPercentage(100);
                episodeBox->setHeight(80);
                episodeBox->setPadding(10);
                episodeBox->setMarginBottom(10);
                episodeBox->setBackgroundColor(isFirst ? nvgRGBA(229, 160, 13, 255) : nvgRGBA(50, 50, 50, 255));
                episodeBox->setCornerRadius(8);
                episodeBox->setFocusable(true);
                episodeBox->setHideHighlightBackground(true);
                episodeBox->addGestureRecognizer(new brls::TapGestureRecognizer(episodeBox));

                auto* thumb = new brls::Image();
                thumb->setWidth(120);
                thumb->setHeight(68);
                thumb->setScalingType(brls::ImageScalingType::FILL);
                thumb->setCornerRadius(4);
                thumb->setMarginRight(15);
                episodeBox->addView(thumb);
                s_instance->m_episodeThumbs.push_back(thumb);

                if (!episode.thumb.empty() && s_instance->m_server) {
                    std::string thumbUrl = s_instance->m_server->getTranscodePictureUrl(episode.thumb, 240, 135);
                    ImageLoader::load(thumb, thumbUrl);
                }

                auto* textBox = new brls::Box();
                textBox->setAxis(brls::Axis::COLUMN);
                textBox->setGrow(1);
                textBox->setShrink(1);
                textBox->setJustifyContent(brls::JustifyContent::CENTER);
                episodeBox->addView(textBox);

                auto* titleLabel = new brls::Label();
                titleLabel->setFontSize(18);
                titleLabel->setSingleLine(true);
                std::string title = std::to_string(episode.index) + ". " + episode.title;
                titleLabel->setText(title);
                if (isFirst) titleLabel->setTextColor(nvgRGBA(0, 0, 0, 255));
                textBox->addView(titleLabel);

                auto* durationLabel = new brls::Label();
                durationLabel->setFontSize(14);
                durationLabel->setTextColor(isFirst ? nvgRGBA(0, 0, 0, 255) : nvgRGBA(150, 150, 150, 255));
                if (episode.duration > 0) {
                    int64_t mins = episode.duration / 60000;
                    durationLabel->setText(std::to_string(mins) + " min");
                }
                textBox->addView(durationLabel);

                brls::Box* progressFill = nullptr;
                brls::Box* progressBox = nullptr;
                if (episode.viewOffset > 0 && episode.duration > 0) {
                    progressBox = new brls::Box();
                    progressBox->setWidthPercentage(100);
                    progressBox->setHeight(3);
                    progressBox->setBackgroundColor(isFirst ? nvgRGBA(0, 0, 0, 80) : nvgRGBA(80, 80, 80, 255));
                    progressBox->setCornerRadius(2);
                    progressBox->setMarginTop(5);
                    textBox->addView(progressBox);

                    float progress = static_cast<float>(episode.viewOffset) / static_cast<float>(episode.duration);
                    progressFill = new brls::Box();
                    progressFill->setWidthPercentage(progress * 100.0f);
                    progressFill->setHeight(3);
                    progressFill->setBackgroundColor(isFirst ? nvgRGBA(0, 0, 0, 255) : nvgRGBA(229, 160, 13, 255));
                    progressFill->setCornerRadius(2);
                    progressBox->addView(progressFill);
                }

                episodeBox->getFocusEvent()->subscribe([episodeBox, titleLabel, durationLabel, progressFill, progressBox](brls::View*) {
                    episodeBox->setBackgroundColor(nvgRGBA(229, 160, 13, 255));
                    titleLabel->setTextColor(nvgRGBA(0, 0, 0, 255));
                    durationLabel->setTextColor(nvgRGBA(0, 0, 0, 255));
                    if (progressFill) progressFill->setBackgroundColor(nvgRGBA(0, 0, 0, 255));
                    if (progressBox) progressBox->setBackgroundColor(nvgRGBA(0, 0, 0, 80));
                });
                episodeBox->getFocusLostEvent()->subscribe([episodeBox, titleLabel, durationLabel, progressFill, progressBox](brls::View*) {
                    episodeBox->setBackgroundColor(nvgRGBA(50, 50, 50, 255));
                    titleLabel->setTextColor(nvgRGBA(255, 255, 255, 255));
                    durationLabel->setTextColor(nvgRGBA(150, 150, 150, 255));
                    if (progressFill) progressFill->setBackgroundColor(nvgRGBA(229, 160, 13, 255));
                    if (progressBox) progressBox->setBackgroundColor(nvgRGBA(80, 80, 80, 255));
                });

                episodeBox->registerClickAction([episode](brls::View*) {
                    if (s_instance) {
                        s_instance->onEpisodeSelected(episode);
                    }
                    return true;
                });

                s_instance->m_episodesList->addView(episodeBox);
                isFirst = false;
            }

            if (!episodes.empty()) {
                brls::Application::giveFocus(s_instance->m_episodesList->getChildren()[0]);
            }
        },
        [requestId](const std::string& error) {
            brls::Logger::error("Failed to load episodes: {}", error);
        }
    );
}

void SeasonView::onEpisodeSelected(const plex::MediaItem& episode) {
    auto* detailView = new MediaDetailView(m_server, episode);
    brls::Application::pushActivity(new brls::Activity(detailView));
}
