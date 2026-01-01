#include "views/show_detail_view.hpp"
#include "views/season_view.hpp"
#include "views/tag_media_view.hpp"
#include "core/plex_server.hpp"
#include "core/plex_api.hpp"
#include "util/image_loader.hpp"
#include "view/recycling_grid.hpp"
#include "view/cast_card_cell.hpp"
#include "styles/plex.hpp"

ShowDetailView* ShowDetailView::s_instance = nullptr;

ShowDetailView::ShowDetailView(PlexServer* server, const plex::MediaItem& show)
    : m_server(server), m_show(show) {

    s_instance = this;

    this->setAxis(brls::Axis::COLUMN);
    this->setWidthPercentage(100);
    this->setHeightPercentage(100);

    this->registerAction("Back", brls::BUTTON_B, [](brls::View*) {
        brls::Application::popActivity();
        return true;
    });

    setupUI();
    updateUI();
}

ShowDetailView::~ShowDetailView() {
    if (s_instance == this) {
        s_instance = nullptr;
    }
}

void ShowDetailView::willAppear(bool resetState) {
    brls::Box::willAppear(resetState);
    m_isVisible = true;
    if (!m_metadataLoaded) {
        loadFullMetadata();
    }
    if (!m_seasonsLoaded) {
        loadSeasons();
    }
}

void ShowDetailView::willDisappear(bool resetState) {
    Box::willDisappear(resetState);
    m_isVisible = false;
    ImageQueue::instance().clear();
    ImageLoader::cancel(m_posterImage);
    ImageLoader::cancel(m_backdropImage);
    if (m_castGrid) {
        m_castGrid->cancelAllPendingImages();
    }
}

brls::View* ShowDetailView::getDefaultFocus() {
    return m_seasonButton;
}

brls::View* ShowDetailView::getNextFocus(brls::FocusDirection direction, brls::View* currentView) {
    if (direction == brls::FocusDirection::UP && m_castGrid) {
        brls::View* parent = currentView;
        while (parent) {
            if (parent == m_castGrid) {
                return m_seasonButton;
            }
            parent = parent->getParent();
        }
    }

    if (direction == brls::FocusDirection::DOWN && currentView == m_seasonButton && m_castGrid) {
        return m_castGrid;
    }

    return Box::getNextFocus(direction, currentView);
}

void ShowDetailView::setupUI() {
    m_backdropImage = new brls::Image();
    m_backdropImage->setWidthPercentage(100);
    m_backdropImage->setHeight(400);
    m_backdropImage->setScalingType(brls::ImageScalingType::FILL);
    this->addView(m_backdropImage);

    auto* overlay = new brls::Box();
    overlay->setWidthPercentage(100);
    overlay->setHeight(400);
    overlay->setBackgroundColor(nvgRGBA(0, 0, 0, 180));
    overlay->setPositionType(brls::PositionType::ABSOLUTE);
    overlay->setPositionTop(0);
    this->addView(overlay);

    auto* contentBox = new brls::Box();
    contentBox->setAxis(brls::Axis::ROW);
    contentBox->setWidthPercentage(100);
    contentBox->setHeight(brls::View::AUTO);
    contentBox->setPadding(40);
    contentBox->setPositionType(brls::PositionType::ABSOLUTE);
    contentBox->setPositionTop(20);
    contentBox->setAlignItems(brls::AlignItems::FLEX_START);
    this->addView(contentBox);

    m_posterImage = new brls::Image();
    m_posterImage->setWidth(200);
    m_posterImage->setHeight(300);
    m_posterImage->setScalingType(brls::ImageScalingType::FILL);
    m_posterImage->setCornerRadius(8);
    m_posterImage->setMarginRight(30);
    contentBox->addView(m_posterImage);

    auto* infoBox = new brls::Box();
    infoBox->setAxis(brls::Axis::COLUMN);
    infoBox->setGrow(1);
    infoBox->setShrink(1);
    contentBox->addView(infoBox);

    m_titleLabel = new brls::Label();
    m_titleLabel->setFontSize(28);
    m_titleLabel->setMarginBottom(10);
    infoBox->addView(m_titleLabel);

    auto* metaRow = new brls::Box();
    metaRow->setAxis(brls::Axis::ROW);
    metaRow->setMarginBottom(15);
    metaRow->setAlignItems(brls::AlignItems::CENTER);
    infoBox->addView(metaRow);

    m_yearLabel = new brls::Label();
    m_yearLabel->setFontSize(16);
    m_yearLabel->setTextColor(nvgRGBA(200, 200, 200, 255));
    m_yearLabel->setMarginRight(20);
    metaRow->addView(m_yearLabel);

    m_ratingLabel = new brls::Label();
    m_ratingLabel->setFontSize(16);
    m_ratingLabel->setTextColor(nvgRGBA(200, 200, 200, 255));
    m_ratingLabel->setMarginRight(20);
    metaRow->addView(m_ratingLabel);

    m_durationLabel = new brls::Label();
    m_durationLabel->setFontSize(16);
    m_durationLabel->setTextColor(nvgRGBA(200, 200, 200, 255));
    m_durationLabel->setMarginRight(20);
    metaRow->addView(m_durationLabel);

    m_studioLabel = new brls::Label();
    m_studioLabel->setFontSize(16);
    m_studioLabel->setTextColor(nvgRGBA(200, 200, 200, 255));
    m_studioLabel->setMarginRight(20);
    m_studioLabel->setVisibility(brls::Visibility::GONE);
    metaRow->addView(m_studioLabel);

    m_episodeCountLabel = new brls::Label();
    m_episodeCountLabel->setFontSize(16);
    m_episodeCountLabel->setTextColor(nvgRGBA(200, 200, 200, 255));
    m_episodeCountLabel->setVisibility(brls::Visibility::GONE);
    metaRow->addView(m_episodeCountLabel);

    m_taglineLabel = new brls::Label();
    m_taglineLabel->setFontSize(16);
    m_taglineLabel->setTextColor(nvgRGBA(180, 180, 180, 255));
    m_taglineLabel->setMarginBottom(15);
    m_taglineLabel->setVisibility(brls::Visibility::GONE);
    infoBox->addView(m_taglineLabel);

    m_seasonButton = new brls::Button();
    m_seasonButton->setStyle(&plex::style::BUTTONSTYLE_PLEX);
    m_seasonButton->setMarginBottom(15);
    m_seasonButton->setText("Loading...");
    m_seasonButton->setState(brls::ButtonState::DISABLED);
    m_seasonButton->registerClickAction([this](brls::View*) {
        if (m_seasonsLoaded) {
            showSeasonDropdown();
        }
        return true;
    });
    infoBox->addView(m_seasonButton);

    auto* summaryScroll = new brls::ScrollingFrame();
    summaryScroll->setWidthPercentage(100);
    summaryScroll->setHeight(100);
    infoBox->addView(summaryScroll);

    m_summaryLabel = new brls::Label();
    m_summaryLabel->setFontSize(14);
    m_summaryLabel->setTextColor(nvgRGBA(180, 180, 180, 255));
    summaryScroll->setContentView(m_summaryLabel);

    m_genresScrollFrame = new brls::HScrollingFrame();
    m_genresScrollFrame->setWidthPercentage(100);
    m_genresScrollFrame->setHeight(50);
    m_genresScrollFrame->setMarginTop(20);
    m_genresScrollFrame->setVisibility(brls::Visibility::GONE);
    this->addView(m_genresScrollFrame);

    m_genresRow = new brls::Box();
    m_genresRow->setAxis(brls::Axis::ROW);
    m_genresRow->setHeight(40);
    m_genresRow->setPaddingLeft(40);
    m_genresRow->setPaddingRight(40);
    m_genresScrollFrame->setContentView(m_genresRow);

    m_castSection = new brls::Box();
    m_castSection->setAxis(brls::Axis::COLUMN);
    m_castSection->setWidthPercentage(100);
    m_castSection->setMarginTop(20);
    m_castSection->setPaddingLeft(40);
    m_castSection->setPaddingRight(40);
    this->addView(m_castSection);
}

void ShowDetailView::updateUI() {
    m_titleLabel->setText(m_show.title);

    if (m_show.year > 0) {
        m_yearLabel->setText(std::to_string(m_show.year));
    }

    if (!m_show.contentRating.empty()) {
        m_ratingLabel->setText(m_show.contentRating);
    }

    if (m_show.childCount > 0) {
        m_durationLabel->setText(std::to_string(m_show.childCount) + " Seasons");
    }

    if (!m_show.studio.empty()) {
        m_studioLabel->setText(m_show.studio);
        m_studioLabel->setVisibility(brls::Visibility::VISIBLE);
    }

    if (m_show.leafCount > 0) {
        std::string episodeText = std::to_string(m_show.leafCount) + " Episodes";
        if (m_show.viewedLeafCount > 0 && m_show.viewedLeafCount < m_show.leafCount) {
            episodeText += " (" + std::to_string(m_show.viewedLeafCount) + " watched)";
        }
        m_episodeCountLabel->setText(episodeText);
        m_episodeCountLabel->setVisibility(brls::Visibility::VISIBLE);
    }

    if (!m_show.tagline.empty()) {
        m_taglineLabel->setText("\"" + m_show.tagline + "\"");
        m_taglineLabel->setVisibility(brls::Visibility::VISIBLE);
    }

    m_summaryLabel->setText(m_show.summary);

    if (!m_show.thumb.empty() && m_server) {
        std::string posterUrl = m_server->getTranscodePictureUrl(m_show.thumb, 200, 300);
        ImageLoader::load(m_posterImage, posterUrl);
    }

    std::string artKey = m_show.art.empty() ? m_show.thumb : m_show.art;
    if (!artKey.empty() && m_server) {
        std::string backdropUrl = m_server->getTranscodePictureUrl(artKey, 1280, 720);
        ImageLoader::load(m_backdropImage, backdropUrl);
    }

    if (!m_show.genres.empty() && m_genresRow) {
        m_genresRow->clearViews();
        for (const auto& genre : m_show.genres) {
            auto* chip = new brls::Box();
            chip->setAxis(brls::Axis::ROW);
            chip->setPadding(8, 16, 8, 16);
            chip->setMarginRight(10);
            chip->setCornerRadius(15);
            chip->setBackgroundColor(nvgRGBA(60, 60, 60, 255));
            chip->setFocusable(true);
            chip->setAlignItems(brls::AlignItems::CENTER);

            auto* label = new brls::Label();
            label->setFontSize(14);
            label->setText(genre.tag);
            chip->addView(label);

            int genreId = genre.id;
            std::string genreName = genre.tag;
            int sectionId = m_show.librarySectionId;
            chip->registerClickAction([this, genreId, genreName, sectionId](brls::View*) {
                auto* tagView = new TagMediaView(m_server, TagType::Genre, genreId, genreName, "", sectionId);
                brls::Application::pushActivity(new brls::Activity(tagView));
                return true;
            });

            m_genresRow->addView(chip);
        }
        m_genresScrollFrame->setVisibility(brls::Visibility::VISIBLE);
    }

    setupCastSection(m_castSection);
}

void ShowDetailView::loadFullMetadata() {
    if (!m_server) return;

    int requestId = ++m_metadataRequestId;
    brls::Application::blockInputs();

    PlexApi::getMetadata(
        m_server,
        m_show.ratingKey,
        [requestId](plex::MediaItem item) {
            brls::Application::unblockInputs();
            if (!s_instance) return;
            if (s_instance->m_metadataRequestId != requestId) {
                brls::Logger::debug("ShowDetailView: Stale metadata request #{} ignored", requestId);
                return;
            }
            if (!s_instance->m_isVisible) {
                brls::Logger::debug("ShowDetailView: View not visible, skipping updateUI");
                return;
            }

            s_instance->m_show = item;
            s_instance->m_metadataLoaded = true;
            s_instance->updateUI();
        },
        [requestId](const std::string& error) {
            brls::Application::unblockInputs();
            brls::Logger::error("Failed to load show metadata: {}", error);
        }
    );
}

void ShowDetailView::loadSeasons() {
    if (!m_server) return;

    int requestId = ++m_seasonsRequestId;

    PlexApi::getChildren(
        m_server,
        m_show.ratingKey,
        [requestId](std::vector<plex::MediaItem> seasons) {
            if (!s_instance) return;
            if (s_instance->m_seasonsRequestId != requestId) {
                brls::Logger::debug("ShowDetailView: Stale seasons request #{} ignored", requestId);
                return;
            }
            if (!s_instance->m_isVisible) {
                brls::Logger::debug("ShowDetailView: View not visible, skipping season update");
                return;
            }

            s_instance->m_seasons = seasons;
            s_instance->m_seasonsLoaded = true;

            if (seasons.size() == 1) {
                s_instance->m_seasonButton->setText("Season " + std::to_string(seasons[0].index));
            } else if (seasons.size() > 1) {
                s_instance->m_seasonButton->setText("Select Season");
            } else {
                s_instance->m_seasonButton->setText("No Seasons");
            }
            s_instance->m_seasonButton->setState(brls::ButtonState::ENABLED);
            s_instance->m_seasonButton->invalidate();
            brls::Application::giveFocus(s_instance->m_seasonButton);
        },
        [requestId](const std::string& error) {
            brls::Logger::error("Failed to load seasons: {}", error);
            if (s_instance && s_instance->m_seasonsRequestId == requestId && s_instance->m_isVisible) {
                s_instance->m_seasonButton->setText("Error");
            }
        }
    );
}

void ShowDetailView::showSeasonDropdown() {
    if (m_seasons.empty()) return;

    if (m_seasons.size() == 1) {
        onSeasonSelected(m_seasons[0]);
        return;
    }

    std::vector<std::string> seasonNames;
    for (const auto& season : m_seasons) {
        std::string name = "Season " + std::to_string(season.index);
        if (season.leafCount > 0) {
            name += " (" + std::to_string(season.leafCount) + " episodes)";
        }
        seasonNames.push_back(name);
    }

    brls::Dropdown* dropdown = new brls::Dropdown(
        "Select Season",
        seasonNames,
        [](int) {},
        0,
        [this](int selected) {
            if (selected >= 0 && selected < static_cast<int>(m_seasons.size())) {
                onSeasonSelected(m_seasons[selected]);
            }
        }
    );
    brls::Application::pushActivity(new brls::Activity(dropdown));
}

void ShowDetailView::onSeasonSelected(const plex::MediaItem& season) {
    auto* seasonView = new SeasonView(m_server, season);
    brls::Application::pushActivity(new brls::Activity(seasonView));
}

void ShowDetailView::setupCastSection(brls::Box* parent) {
    if (m_show.cast.empty()) return;

    if (m_castGrid) {
        auto* dataSource = new CastDataSource(m_show.cast, m_server);
        dataSource->setOnActorClick([this](const plex::Tag& actor) {
            auto* tagView = new TagMediaView(m_server, TagType::Actor, actor.id, actor.tag, actor.thumb);
            brls::Application::pushActivity(new brls::Activity(tagView));
        });
        m_castGrid->setDataSource(dataSource);
        return;
    }

    parent->clearViews();

    auto* titleLabel = new brls::Label();
    titleLabel->setFontSize(20);
    titleLabel->setText("Cast & Crew");
    titleLabel->setMarginBottom(15);
    parent->addView(titleLabel);

    m_castGrid = new RecyclingGrid();
    m_castGrid->setWidthPercentage(100);
    m_castGrid->setHeight(500);
    m_castGrid->spanCount = 5;
    m_castGrid->estimatedRowHeight = 180;
    m_castGrid->estimatedRowSpace = 10;
    m_castGrid->registerCell("CastCard", CastCardCell::create);

    auto* dataSource = new CastDataSource(m_show.cast, m_server);
    dataSource->setOnActorClick([this](const plex::Tag& actor) {
        auto* tagView = new TagMediaView(m_server, TagType::Actor, actor.id, actor.tag, actor.thumb);
        brls::Application::pushActivity(new brls::Activity(tagView));
    });
    m_castGrid->setDataSource(dataSource);

    parent->addView(m_castGrid);
}

std::string ShowDetailView::formatDuration(int64_t ms) {
    int64_t totalMinutes = ms / 60000;
    int hours = static_cast<int>(totalMinutes / 60);
    int minutes = static_cast<int>(totalMinutes % 60);

    if (hours > 0) {
        return std::to_string(hours) + "h " + std::to_string(minutes) + "m";
    }
    return std::to_string(minutes) + " min";
}
