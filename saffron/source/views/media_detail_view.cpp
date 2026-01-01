#include "views/media_detail_view.hpp"
#include "views/player_activity.hpp"
#include "views/tag_media_view.hpp"
#include "core/plex_server.hpp"
#include "core/plex_api.hpp"
#include "core/settings_manager.hpp"
#include "util/image_loader.hpp"
#include "view/recycling_grid.hpp"
#include "view/cast_card_cell.hpp"
#include "styles/plex.hpp"

static std::string formatFileSize(int64_t bytes) {
    if (bytes <= 0) return "";
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    int unitIndex = 0;
    double size = static_cast<double>(bytes);
    while (size >= 1024.0 && unitIndex < 4) {
        size /= 1024.0;
        unitIndex++;
    }
    char buf[32];
    if (unitIndex >= 2) {
        snprintf(buf, sizeof(buf), "%.1f %s", size, units[unitIndex]);
    } else {
        snprintf(buf, sizeof(buf), "%.0f %s", size, units[unitIndex]);
    }
    return buf;
}

static std::string extractFilename(const std::string& path) {
    if (path.empty()) return "";
    size_t lastSlash = path.find_last_of("/\\");
    std::string filename = (lastSlash != std::string::npos) ? path.substr(lastSlash + 1) : path;
    size_t lastDot = filename.find_last_of('.');
    if (lastDot != std::string::npos) {
        filename = filename.substr(0, lastDot);
    }
    return filename;
}

static std::string buildVideoInfo(const plex::Media& media) {
    std::string info;
    if (!media.videoCodec.empty()) {
        std::string codec = media.videoCodec;
        if (codec == "hevc") codec = "H.265";
        else if (codec == "h264" || codec == "avc") codec = "H.264";
        else if (codec == "av1") codec = "AV1";
        info += codec;
    }
    if (!media.videoResolution.empty()) {
        if (!info.empty()) info += " ";
        std::string res = media.videoResolution;
        if (res == "4k" || res == "4K") info += "4K";
        else if (res == "1080" || res == "1080p") info += "1080p";
        else if (res == "720" || res == "720p") info += "720p";
        else info += res;
    }
    if (!media.parts.empty() && !media.parts[0].streams.empty()) {
        for (const auto& stream : media.parts[0].streams) {
            if (stream.streamType == 1) {
                if (!stream.colorSpace.empty() && stream.colorSpace != "bt709") {
                    if (!info.empty()) info += " ";
                    if (stream.colorSpace == "bt2020nc" || stream.colorSpace == "bt2020") {
                        info += "HDR";
                    }
                }
                if (stream.bitDepth > 8) {
                    if (!info.empty()) info += " ";
                    info += std::to_string(stream.bitDepth) + "-bit";
                }
                break;
            }
        }
    }
    return info;
}

static std::string buildAudioInfo(const plex::Media& media) {
    std::string info;
    if (!media.audioCodec.empty()) {
        std::string codec = media.audioCodec;
        if (codec == "aac") codec = "AAC";
        else if (codec == "ac3") codec = "AC3";
        else if (codec == "eac3") codec = "EAC3";
        else if (codec == "truehd") codec = "TrueHD";
        else if (codec == "dts") codec = "DTS";
        else if (codec == "dca") codec = "DTS";
        else if (codec == "flac") codec = "FLAC";
        info += codec;
    }
    if (media.audioChannels > 0) {
        if (!info.empty()) info += " ";
        if (media.audioChannels == 8) info += "7.1";
        else if (media.audioChannels == 6) info += "5.1";
        else if (media.audioChannels == 2) info += "Stereo";
        else if (media.audioChannels == 1) info += "Mono";
        else info += std::to_string(media.audioChannels) + "ch";
    }
    if (!media.parts.empty() && !media.parts[0].streams.empty()) {
        for (const auto& stream : media.parts[0].streams) {
            if (stream.streamType == 2) {
                if (stream.displayTitle.find("Atmos") != std::string::npos) {
                    info += " Atmos";
                } else if (stream.displayTitle.find("DTS:X") != std::string::npos) {
                    info += " DTS:X";
                }
                break;
            }
        }
    }
    return info;
}

static std::string buildSourceLabel(const plex::Media& media, int index) {
    if (!media.parts.empty() && !media.parts[0].file.empty()) {
        return extractFilename(media.parts[0].file);
    }
    if (!media.editionTitle.empty()) {
        return media.editionTitle;
    }
    if (index == 0) return "Default";
    return "Source " + std::to_string(index + 1);
}

MediaDetailView* MediaDetailView::s_instance = nullptr;

MediaDetailView::MediaDetailView(PlexServer* server, const plex::MediaItem& item)
    : m_server(server), m_item(item) {

    s_instance = this;

    this->setAxis(brls::Axis::COLUMN);
    this->setWidthPercentage(100);
    this->setHeightPercentage(100);

    // Register B button to go back
    this->registerAction("Back", brls::BUTTON_B, [](brls::View*) {
        brls::Application::popActivity();
        return true;
    });

    setupUI();
    updateUI();
}

MediaDetailView::~MediaDetailView() {
    if (s_instance == this) {
        s_instance = nullptr;
    }
}

void MediaDetailView::willAppear(bool resetState) {
    brls::Box::willAppear(resetState);
    m_isVisible = true;
    loadFullMetadata();
}

void MediaDetailView::willDisappear(bool resetState) {
    Box::willDisappear(resetState);
    m_isVisible = false;
    ImageQueue::instance().clear();
    ImageLoader::cancel(m_posterImage);
    ImageLoader::cancel(m_backdropImage);
    if (m_castGrid) {
        m_castGrid->cancelAllPendingImages();
    }
}

brls::View* MediaDetailView::getDefaultFocus() {
    return m_playButton;
}

void MediaDetailView::setupUI() {
    auto* scrollView = new brls::ScrollingFrame();
    scrollView->setWidthPercentage(100);
    scrollView->setHeightPercentage(100);
    this->addView(scrollView);

    auto* scrollContent = new brls::Box();
    scrollContent->setAxis(brls::Axis::COLUMN);
    scrollContent->setWidthPercentage(100);
    scrollView->setContentView(scrollContent);

    auto* headerContainer = new brls::Box();
    headerContainer->setWidthPercentage(100);
    headerContainer->setHeight(400);
    scrollContent->addView(headerContainer);

    m_backdropImage = new brls::Image();
    m_backdropImage->setWidthPercentage(100);
    m_backdropImage->setHeight(400);
    m_backdropImage->setScalingType(brls::ImageScalingType::FILL);
    m_backdropImage->setPositionType(brls::PositionType::ABSOLUTE);
    m_backdropImage->setPositionTop(0);
    m_backdropImage->setPositionLeft(0);
    headerContainer->addView(m_backdropImage);

    auto* overlay = new brls::Box();
    overlay->setWidthPercentage(100);
    overlay->setHeight(400);
    overlay->setBackgroundColor(nvgRGBA(0, 0, 0, 180));
    overlay->setPositionType(brls::PositionType::ABSOLUTE);
    overlay->setPositionTop(0);
    headerContainer->addView(overlay);

    auto* contentBox = new brls::Box();
    contentBox->setAxis(brls::Axis::ROW);
    contentBox->setWidthPercentage(100);
    contentBox->setHeight(brls::View::AUTO);
    contentBox->setPadding(40);
    contentBox->setPositionType(brls::PositionType::ABSOLUTE);
    contentBox->setPositionTop(20);
    contentBox->setAlignItems(brls::AlignItems::FLEX_START);
    headerContainer->addView(contentBox);

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

    auto* titleRow = new brls::Box();
    titleRow->setAxis(brls::Axis::ROW);
    titleRow->setAlignItems(brls::AlignItems::CENTER);
    titleRow->setMarginBottom(10);
    infoBox->addView(titleRow);

    m_titleLabel = new brls::Label();
    m_titleLabel->setFontSize(28);
    titleRow->addView(m_titleLabel);

    m_resolutionBadge = new brls::Box();
    m_resolutionBadge->setPadding(4, 10, 4, 10);
    m_resolutionBadge->setCornerRadius(4);
    m_resolutionBadge->setMarginLeft(10);
    m_resolutionBadge->setVisibility(brls::Visibility::GONE);
    titleRow->addView(m_resolutionBadge);

    m_resolutionLabel = new brls::Label();
    m_resolutionLabel->setFontSize(12);
    m_resolutionLabel->setTextColor(nvgRGBA(30, 30, 30, 255));
    m_resolutionBadge->addView(m_resolutionLabel);

    auto* infoChip = new brls::Box();
    infoChip->setAxis(brls::Axis::ROW);
    infoChip->setPadding(6, 12, 6, 12);
    infoChip->setMarginLeft(10);
    infoChip->setCornerRadius(14);
    infoChip->setFocusable(true);
    infoChip->setAlignItems(brls::AlignItems::CENTER);

    auto* infoLabel = new brls::Label();
    infoLabel->setFontSize(12);
    infoLabel->setText("Info");
    infoLabel->setTextColor(nvgRGBA(229, 160, 13, 255));
    infoChip->addView(infoLabel);

    infoChip->registerClickAction([this](brls::View*) {
        showMediaInfo();
        return true;
    });
    infoChip->getFocusEvent()->subscribe([infoChip, infoLabel](brls::View*) {
        infoChip->setBackgroundColor(nvgRGBA(229, 160, 13, 255));
        infoLabel->setTextColor(nvgRGBA(0, 0, 0, 255));
    });
    infoChip->getFocusLostEvent()->subscribe([infoChip, infoLabel](brls::View*) {
        infoChip->setBackgroundColor(nvgRGBA(0, 0, 0, 0));
        infoLabel->setTextColor(nvgRGBA(229, 160, 13, 255));
    });
    titleRow->addView(infoChip);

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
    m_studioLabel->setVisibility(brls::Visibility::GONE);
    metaRow->addView(m_studioLabel);

    m_taglineLabel = new brls::Label();
    m_taglineLabel->setFontSize(16);
    m_taglineLabel->setTextColor(nvgRGBA(180, 180, 180, 255));
    m_taglineLabel->setMarginBottom(15);
    m_taglineLabel->setVisibility(brls::Visibility::GONE);
    infoBox->addView(m_taglineLabel);

    auto* buttonRow = new brls::Box();
    buttonRow->setAxis(brls::Axis::ROW);
    buttonRow->setMarginBottom(15);
    buttonRow->setAlignItems(brls::AlignItems::CENTER);
    infoBox->addView(buttonRow);

    m_playButton = new brls::Button();
    plex::style::applyPlayButton(m_playButton);
    m_playButton->setMarginRight(15);
    m_playButton->setText("Loading...");
    m_playButton->setState(brls::ButtonState::DISABLED);
    m_playButton->registerClickAction([this](brls::View*) {
        if (!m_metadataLoaded) return true;
        playMedia();
        return true;
    });
    buttonRow->addView(m_playButton);

    m_sourceButton = new brls::Button();
    m_sourceButton->setFontSize(16);
    m_sourceButton->setText("Version");
    m_sourceButton->setVisibility(brls::Visibility::GONE);
    m_sourceButton->registerClickAction([this](brls::View*) {
        showSourceSelector();
        return true;
    });
    buttonRow->addView(m_sourceButton);

    m_videoInfoLabel = new brls::Label();
    m_videoInfoLabel->setFontSize(14);
    m_videoInfoLabel->setTextColor(nvgRGBA(150, 150, 150, 255));
    m_videoInfoLabel->setMarginBottom(5);
    m_videoInfoLabel->setVisibility(brls::Visibility::GONE);
    infoBox->addView(m_videoInfoLabel);

    m_audioInfoLabel = new brls::Label();
    m_audioInfoLabel->setFontSize(14);
    m_audioInfoLabel->setTextColor(nvgRGBA(150, 150, 150, 255));
    m_audioInfoLabel->setMarginBottom(5);
    m_audioInfoLabel->setVisibility(brls::Visibility::GONE);
    infoBox->addView(m_audioInfoLabel);

    m_fileInfoLabel = new brls::Label();
    m_fileInfoLabel->setFontSize(14);
    m_fileInfoLabel->setTextColor(nvgRGBA(120, 120, 120, 255));
    m_fileInfoLabel->setMarginBottom(10);
    m_fileInfoLabel->setVisibility(brls::Visibility::GONE);
    infoBox->addView(m_fileInfoLabel);

    auto* progressBox = new brls::Box();
    progressBox->setAxis(brls::Axis::COLUMN);
    progressBox->setWidth(300);
    progressBox->setMarginBottom(15);
    progressBox->setVisibility(brls::Visibility::GONE);
    infoBox->addView(progressBox);

    m_progressBar = new brls::Box();
    m_progressBar->setWidthPercentage(100);
    m_progressBar->setHeight(4);
    m_progressBar->setBackgroundColor(nvgRGBA(80, 80, 80, 255));
    m_progressBar->setCornerRadius(2);
    m_progressBar->setMarginBottom(5);
    progressBox->addView(m_progressBar);

    m_progressFill = new brls::Box();
    m_progressFill->setHeight(4);
    m_progressFill->setBackgroundColor(nvgRGBA(229, 160, 13, 255));
    m_progressFill->setCornerRadius(2);
    m_progressBar->addView(m_progressFill);

    m_progressLabel = new brls::Label();
    m_progressLabel->setFontSize(12);
    m_progressLabel->setTextColor(nvgRGBA(150, 150, 150, 255));
    progressBox->addView(m_progressLabel);

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
    scrollContent->addView(m_genresScrollFrame);

    m_genresRow = new brls::Box();
    m_genresRow->setAxis(brls::Axis::ROW);
    m_genresRow->setHeight(40);
    m_genresRow->setPaddingLeft(40);
    m_genresRow->setPaddingRight(40);
    m_genresScrollFrame->setContentView(m_genresRow);

    m_castSection = new brls::Box();
    m_castSection->setAxis(brls::Axis::COLUMN);
    m_castSection->setWidthPercentage(100);
    m_castSection->setMarginTop(30);
    m_castSection->setPaddingLeft(40);
    m_castSection->setPaddingRight(40);
    m_castSection->setPaddingBottom(40);
    scrollContent->addView(m_castSection);

}

void MediaDetailView::updateUI() {
    std::string displayTitle = m_item.title;
    if (!m_item.editionTitle.empty()) {
        displayTitle += " - " + m_item.editionTitle + " Edition";
    }
    m_titleLabel->setText(displayTitle);

    if (m_item.year > 0) {
        m_yearLabel->setText(std::to_string(m_item.year));
    }

    if (m_item.contentRating.length() > 0) {
        m_ratingLabel->setText(m_item.contentRating);
    }

    if (m_item.duration > 0) {
        m_durationLabel->setText(formatDuration(m_item.duration));
    }

    if (!m_item.studio.empty()) {
        m_studioLabel->setText(m_item.studio);
        m_studioLabel->setVisibility(brls::Visibility::VISIBLE);
    }

    if (!m_item.tagline.empty()) {
        m_taglineLabel->setText("\"" + m_item.tagline + "\"");
        m_taglineLabel->setVisibility(brls::Visibility::VISIBLE);
    }

    m_summaryLabel->setText(m_item.summary);

    if (m_metadataLoaded) {
        if (m_item.viewOffset > 0) {
            m_playButton->setText("Resume");
            auto* progressBox = m_progressBar->getParent();
            if (progressBox) {
                progressBox->setVisibility(brls::Visibility::VISIBLE);
            }
            float progress = static_cast<float>(m_item.viewOffset) / static_cast<float>(m_item.duration);
            m_progressFill->setWidth(progress * 300);
            m_progressLabel->setText(formatProgress());
        } else {
            m_playButton->setText("Play");
        }
        m_playButton->invalidate();

        if (m_item.media.size() > 1) {
            std::string editionLabel = buildSourceLabel(m_item.media[m_selectedMediaIndex], m_selectedMediaIndex);
            m_sourceButton->setText(editionLabel);
            m_sourceButton->setVisibility(brls::Visibility::VISIBLE);
        }

        updateTechnicalInfo();

        if (!m_item.genres.empty() && m_genresRow) {
            m_genresRow->clearViews();
            for (const auto& genre : m_item.genres) {
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
                int sectionId = m_item.librarySectionId;
                chip->registerClickAction([this, genreId, genreName, sectionId](brls::View*) {
                    auto* tagView = new TagMediaView(m_server, TagType::Genre, genreId, genreName, "", sectionId);
                    brls::Application::pushActivity(new brls::Activity(tagView));
                    return true;
                });

                m_genresRow->addView(chip);
            }
            m_genresScrollFrame->setVisibility(brls::Visibility::VISIBLE);
        }
    }

    brls::Logger::info("MediaDetailView: thumb='{}', art='{}'", m_item.thumb, m_item.art);
    if (!m_item.thumb.empty() && m_server) {
        std::string posterUrl = m_server->getTranscodePictureUrl(m_item.thumb, 200, 300);
        ImageLoader::load(m_posterImage, posterUrl);
    }

    std::string artKey = m_item.art.empty() ? m_item.thumb : m_item.art;
    if (!artKey.empty() && m_server) {
        std::string backdropUrl = m_server->getTranscodePictureUrl(artKey, 1280, 720);
        ImageLoader::load(m_backdropImage, backdropUrl);
    }

    setupCastSection(m_castSection);
}

void MediaDetailView::updateTechnicalInfo() {
    if (m_item.media.empty() || m_selectedMediaIndex >= static_cast<int>(m_item.media.size())) {
        return;
    }

    const auto& media = m_item.media[m_selectedMediaIndex];

    std::string res = media.videoResolution;
    if (res == "4k" || res == "4K") {
        m_resolutionLabel->setText("4K");
        m_resolutionBadge->setBackgroundColor(nvgRGBA(250, 128, 114, 255));
        m_resolutionBadge->setVisibility(brls::Visibility::VISIBLE);
    } else if (res == "1080" || res == "1080p") {
        m_resolutionLabel->setText("1080p");
        m_resolutionBadge->setBackgroundColor(nvgRGBA(255, 179, 71, 255));
        m_resolutionBadge->setVisibility(brls::Visibility::VISIBLE);
    } else if (res == "720" || res == "720p") {
        m_resolutionLabel->setText("720p");
        m_resolutionBadge->setBackgroundColor(nvgRGBA(251, 206, 177, 255));
        m_resolutionBadge->setVisibility(brls::Visibility::VISIBLE);
    } else {
        m_resolutionBadge->setVisibility(brls::Visibility::GONE);
    }

    std::string videoInfo = buildVideoInfo(media);
    std::string audioInfo = buildAudioInfo(media);

    if (!videoInfo.empty()) {
        m_videoInfoLabel->setText("Video: " + videoInfo);
        m_videoInfoLabel->setVisibility(brls::Visibility::VISIBLE);
    } else {
        m_videoInfoLabel->setVisibility(brls::Visibility::GONE);
    }

    if (!audioInfo.empty()) {
        m_audioInfoLabel->setText("Audio: " + audioInfo);
        m_audioInfoLabel->setVisibility(brls::Visibility::VISIBLE);
    } else {
        m_audioInfoLabel->setVisibility(brls::Visibility::GONE);
    }

    std::string fileInfo;
    if (!media.parts.empty()) {
        if (media.parts[0].size > 0) {
            fileInfo = formatFileSize(media.parts[0].size);
        }
        if (!media.container.empty()) {
            if (!fileInfo.empty()) fileInfo += " | ";
            std::string container = media.container;
            for (auto& c : container) c = static_cast<char>(toupper(c));
            fileInfo += container;
        }
    }
    if (!fileInfo.empty()) {
        m_fileInfoLabel->setText(fileInfo);
        m_fileInfoLabel->setVisibility(brls::Visibility::VISIBLE);
    } else {
        m_fileInfoLabel->setVisibility(brls::Visibility::GONE);
    }
}

void MediaDetailView::setupCastSection(brls::Box* parent) {
    if (m_item.cast.empty()) return;

    if (m_castGrid) {
        auto* dataSource = new CastDataSource(m_item.cast, m_server);
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

    auto* dataSource = new CastDataSource(m_item.cast, m_server);
    dataSource->setOnActorClick([this](const plex::Tag& actor) {
        auto* tagView = new TagMediaView(m_server, TagType::Actor, actor.id, actor.tag, actor.thumb);
        brls::Application::pushActivity(new brls::Activity(tagView));
    });
    m_castGrid->setDataSource(dataSource);

    parent->addView(m_castGrid);
}

void MediaDetailView::loadFullMetadata() {
    if (!m_server) return;

    m_metadataLoaded = false;
    m_playButton->setText("Loading...");
    m_playButton->setState(brls::ButtonState::DISABLED);

    int requestId = ++m_requestId;
    brls::Logger::debug("MediaDetailView: Loading metadata for ratingKey={} (request #{})", m_item.ratingKey, requestId);
    brls::Application::blockInputs();

    PlexApi::getMetadata(
        m_server,
        m_item.ratingKey,
        [requestId](plex::MediaItem item) {
            brls::Application::unblockInputs();
            brls::Logger::debug("MediaDetailView: Metadata loaded, media count={}", item.media.size());
            if (!s_instance) {
                brls::Logger::debug("MediaDetailView: s_instance is null - view was destroyed");
                return;
            }
            if (s_instance->m_requestId != requestId) {
                brls::Logger::debug("MediaDetailView: Stale request #{} ignored (current #{})", requestId, s_instance->m_requestId);
                return;
            }
            if (!s_instance->m_isVisible) {
                brls::Logger::debug("MediaDetailView: View not visible, skipping updateUI");
                return;
            }
            brls::Logger::debug("MediaDetailView: s_instance valid, updating UI");
            s_instance->m_item = item;
            s_instance->m_metadataLoaded = true;
            s_instance->updateUI();
            s_instance->m_playButton->setState(brls::ButtonState::ENABLED);
            s_instance->m_playButton->invalidate();
            brls::Application::giveFocus(s_instance->m_playButton);
            brls::Logger::debug("MediaDetailView: Button updated and focus given");
        },
        [requestId](const std::string& error) {
            brls::Application::unblockInputs();
            brls::Logger::error("Failed to load metadata: {}", error);
            if (s_instance && s_instance->m_requestId == requestId && s_instance->m_isVisible) {
                s_instance->m_playButton->setText("Unavailable");
                s_instance->m_playButton->setState(brls::ButtonState::DISABLED);
                s_instance->m_playButton->invalidate();
            }
        }
    );
}

void MediaDetailView::playMedia() {
    if (!m_server) {
        brls::Application::notify("No server selected");
        return;
    }

    if (m_item.media.empty()) {
        brls::Application::notify("No media available");
        return;
    }

    if (m_item.viewOffset > 0) {
        showResumeDialog();
    } else {
        startPlaybackFlow();
    }
}

void MediaDetailView::showResumeDialog() {
    std::string resumeTime = formatDuration(m_item.viewOffset);

    brls::Dialog* dialog = new brls::Dialog("Resume Playback");
    dialog->addButton("Resume from " + resumeTime, [this]() {
        startPlaybackFlow();
    });
    dialog->addButton("Start from beginning", [this]() {
        m_item.viewOffset = 0;
        startPlaybackFlow();
    });
    dialog->open();
}

void MediaDetailView::startPlaybackFlow() {
    brls::Application::notify("Checking playback options...");

    PlexApi::getPlaybackDecision(m_server, m_item.ratingKey, 0, "", false, 0, m_selectedMediaIndex,
        [this](const plex::PlaybackInfo& info) {
            if (!s_instance) return;
            s_instance->showQualityMenu(info);
        },
        [](const std::string& error) {
            brls::Logger::error("Failed to check playback options: {}", error);
            brls::Application::notify("Failed to check playback options");
        }
    );
}

void MediaDetailView::showSourceSelector() {
    if (m_item.media.size() <= 1) return;

    std::vector<std::string> options;
    for (size_t i = 0; i < m_item.media.size(); i++) {
        options.push_back(buildSourceLabel(m_item.media[i], static_cast<int>(i)));
    }

    brls::Dropdown* dropdown = new brls::Dropdown(
        "Select Source",
        options,
        [](int) {},
        m_selectedMediaIndex,
        [this](int selected) {
            if (selected < 0 || selected >= static_cast<int>(m_item.media.size())) return;
            m_selectedMediaIndex = selected;
            std::string editionLabel = buildSourceLabel(m_item.media[m_selectedMediaIndex], m_selectedMediaIndex);
            m_sourceButton->setText(editionLabel);
            updateTechnicalInfo();
        }
    );
    brls::Application::pushActivity(new brls::Activity(dropdown));
}

void MediaDetailView::showQualityMenu(const plex::PlaybackInfo& info) {
    int mediaIndex = m_selectedMediaIndex;
    if (mediaIndex >= static_cast<int>(m_item.media.size())) mediaIndex = 0;
    int sourceHeight = m_item.media[mediaIndex].height;

    // Plex recommended bitrate/resolution pairings
    struct QualityPreset {
        int bitrate;
        int height;
        const char* label;
    };
    static const QualityPreset presets[] = {
        {20000, 1080, "20 Mbps 1080p"},
        {12000, 1080, "12 Mbps 1080p"},
        {10000, 1080, "10 Mbps 1080p"},
        {8000, 1080, "8 Mbps 1080p"},
        {4000, 720, "4 Mbps 720p"},
        {3000, 720, "3 Mbps 720p"},
        {2000, 480, "2 Mbps 480p"},
        {1500, 480, "1.5 Mbps 480p"},
        {720, 360, "720 Kbps 360p"},
    };

    std::vector<std::string> options;
    std::vector<std::pair<int, int>> qualityPairs;

    // Always offer Original: libmpv+FFmpeg can decode any format the Switch hardware supports.
    // Plex's decision API tells us what the server would do, but we bypass it for direct play.
    options.push_back("Original");
    qualityPairs.push_back({0, 0});

    // Add transcode options based on source resolution
    for (const auto& preset : presets) {
        if (preset.height > sourceHeight) continue;

        options.push_back(preset.label);
        qualityPairs.push_back({preset.bitrate, preset.height});
    }

    if (options.empty()) {
        brls::Application::notify("No playback options available");
        return;
    }

    brls::Dropdown* dropdown = new brls::Dropdown(
        "Select Quality",
        options,
        [](int) {},
        0,
        [this, qualityPairs](int selected) {
            if (selected < 0 || selected >= static_cast<int>(qualityPairs.size())) return;
            auto quality = qualityPairs[selected];
            std::string resolution = quality.second > 0 ? std::to_string(quality.second) + "p" : "";
            brls::Application::pushActivity(new PlayerActivity(m_server, m_item, quality.first, resolution, m_selectedMediaIndex));
        }
    );
    brls::Application::pushActivity(new brls::Activity(dropdown));
}

std::string MediaDetailView::formatDuration(int64_t ms) {
    int64_t totalMinutes = ms / 60000;
    int hours = static_cast<int>(totalMinutes / 60);
    int minutes = static_cast<int>(totalMinutes % 60);

    if (hours > 0) {
        return std::to_string(hours) + "h " + std::to_string(minutes) + "m";
    }
    return std::to_string(minutes) + " min";
}

std::string MediaDetailView::formatProgress() {
    std::string watched = formatDuration(m_item.viewOffset);
    std::string total = formatDuration(m_item.duration);
    int64_t remaining = m_item.duration - m_item.viewOffset;
    std::string remainingStr = formatDuration(remaining);
    return remainingStr + " remaining";
}

void MediaDetailView::showMediaInfo() {
    if (m_selectedMediaIndex >= static_cast<int>(m_item.media.size())) return;
    const auto& media = m_item.media[m_selectedMediaIndex];

    auto* view = new brls::Box();
    view->setWidthPercentage(100);
    view->setHeightPercentage(100);
    view->setBackgroundColor(nvgRGBA(20, 20, 20, 255));

    view->registerAction("Back", brls::BUTTON_B, [](brls::View*) {
        brls::Application::popActivity();
        return true;
    });

    auto* scroll = new brls::ScrollingFrame();
    scroll->setWidthPercentage(100);
    scroll->setHeightPercentage(100);
    view->addView(scroll);

    auto* content = new brls::Box();
    content->setAxis(brls::Axis::COLUMN);
    content->setWidthPercentage(100);
    content->setPadding(40);
    scroll->setContentView(content);

    auto addHeader = [content](const std::string& text) {
        auto* label = new brls::Label();
        label->setText(text);
        label->setFontSize(20);
        label->setTextColor(nvgRGB(229, 160, 13));
        label->setMarginTop(20);
        label->setMarginBottom(10);
        content->addView(label);
    };

    auto addRow = [content](const std::string& label, const std::string& value) {
        if (value.empty()) return;
        auto* row = new brls::Box();
        row->setAxis(brls::Axis::ROW);
        row->setWidthPercentage(100);
        row->setMarginBottom(5);

        auto* labelView = new brls::Label();
        labelView->setText(label);
        labelView->setFontSize(14);
        labelView->setTextColor(nvgRGBA(150, 150, 150, 255));
        labelView->setWidth(200);
        row->addView(labelView);

        auto* valueView = new brls::Label();
        valueView->setText(value);
        valueView->setFontSize(14);
        valueView->setGrow(1);
        row->addView(valueView);

        content->addView(row);
    };

    auto* title = new brls::Label();
    title->setText("Media Information");
    title->setFontSize(24);
    title->setMarginBottom(20);
    content->addView(title);

    addHeader("MEDIA");
    addRow("ID", std::to_string(media.id));
    addRow("Container", media.container);
    addRow("Duration", std::to_string(media.duration));
    addRow("Bitrate", std::to_string(media.bitrate));
    addRow("Width", std::to_string(media.width));
    addRow("Height", std::to_string(media.height));
    addRow("Aspect Ratio", media.aspectRatio);
    addRow("Video Codec", media.videoCodec);
    addRow("Video Resolution", media.videoResolution);
    addRow("Video Frame Rate", media.videoFrameRate);
    addRow("Video Profile", media.videoProfile);
    addRow("Audio Codec", media.audioCodec);
    addRow("Audio Channels", std::to_string(media.audioChannels));
    addRow("Audio Profile", media.audioProfile);
    addRow("Optimized", media.optimizedForStreaming ? "true" : "false");
    addRow("Edition", media.editionTitle);

    for (size_t p = 0; p < media.parts.size(); p++) {
        const auto& part = media.parts[p];
        addHeader("PART " + std::to_string(p + 1));
        addRow("ID", std::to_string(part.id));
        addRow("File", part.file);
        addRow("Size", std::to_string(part.size));
        addRow("Container", part.container);
        addRow("Duration", std::to_string(part.duration));
        addRow("Video Profile", part.videoProfile);
        addRow("Audio Profile", part.audioProfile);

        int videoCount = 0, audioCount = 0, subCount = 0;
        for (const auto& stream : part.streams) {
            std::string streamHeader;
            if (stream.streamType == 1) {
                videoCount++;
                streamHeader = "VIDEO STREAM " + std::to_string(videoCount);
            } else if (stream.streamType == 2) {
                audioCount++;
                streamHeader = "AUDIO STREAM " + std::to_string(audioCount);
            } else if (stream.streamType == 3) {
                subCount++;
                streamHeader = "SUBTITLE " + std::to_string(subCount);
            } else {
                streamHeader = "STREAM (type " + std::to_string(stream.streamType) + ")";
            }

            addHeader(streamHeader);
            addRow("ID", std::to_string(stream.id));
            addRow("Stream Type", std::to_string(stream.streamType));
            addRow("Codec", stream.codec);
            addRow("Display Title", stream.displayTitle);
            addRow("Language", stream.language);
            addRow("Language Code", stream.languageCode);
            addRow("Profile", stream.profile);
            addRow("Selected", stream.selected ? "true" : "false");
            addRow("Default", stream.isDefault ? "true" : "false");

            if (stream.streamType == 1) {
                addRow("Width", std::to_string(stream.width));
                addRow("Height", std::to_string(stream.height));
                addRow("Frame Rate", std::to_string(stream.frameRate));
                addRow("Bit Depth", std::to_string(stream.bitDepth));
                addRow("Bitrate", std::to_string(stream.bitrate));
                addRow("Color Space", stream.colorSpace);
                addRow("Color Range", stream.colorRange);
                addRow("Color Primaries", stream.colorPrimaries);
            } else if (stream.streamType == 2) {
                addRow("Channels", std::to_string(stream.channels));
                addRow("Channel Layout", stream.audioChannelLayout);
                addRow("Bitrate", std::to_string(stream.bitrate));
                addRow("Bit Depth", std::to_string(stream.bitDepth));
            }
        }
    }

    brls::Application::pushActivity(new brls::Activity(view));
}
