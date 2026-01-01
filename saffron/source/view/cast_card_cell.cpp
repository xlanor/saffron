#include "view/cast_card_cell.hpp"
#include "core/plex_server.hpp"
#include "util/image_loader.hpp"

CastCardCell::CastCardCell() {
    this->setAxis(brls::Axis::COLUMN);
    this->setAlignItems(brls::AlignItems::CENTER);
    this->setPadding(10);
    this->setCornerRadius(8);
    this->setBackgroundColor(brls::Application::getTheme().getColor("color/card"));

    m_image = new brls::Image();
    m_image->setWidth(120);
    m_image->setHeight(120);
    m_image->setScalingType(brls::ImageScalingType::FILL);
    m_image->setCornerRadius(60);
    m_image->setMarginBottom(10);
    this->addView(m_image);

    m_nameLabel = new brls::Label();
    m_nameLabel->setFontSize(14);
    m_nameLabel->setHorizontalAlign(brls::HorizontalAlign::CENTER);
    m_nameLabel->setMarginBottom(4);
    this->addView(m_nameLabel);
    registerFocusableLabel(m_nameLabel);

    m_roleLabel = new brls::Label();
    m_roleLabel->setFontSize(12);
    m_roleLabel->setTextColor(nvgRGBA(150, 150, 150, 255));
    m_roleLabel->setHorizontalAlign(brls::HorizontalAlign::CENTER);
    this->addView(m_roleLabel);
    registerFocusableLabel(m_roleLabel, true);
}

RecyclingGridItem* CastCardCell::create() {
    return new CastCardCell();
}

void CastCardCell::setActor(const plex::Tag& actor, PlexServer* server) {
    m_nameLabel->setText(actor.tag);

    if (!actor.role.empty()) {
        m_roleLabel->setText(actor.role);
        m_roleLabel->setVisibility(brls::Visibility::VISIBLE);
    } else {
        m_roleLabel->setVisibility(brls::Visibility::GONE);
    }

    if (!actor.thumb.empty() && server) {
        std::string thumbUrl = server->getTranscodePictureUrl(actor.thumb, 120, 120);
        ImageLoader::load(m_image, thumbUrl);
    }
}

void CastCardCell::prepareForReuse() {
    m_image->clear();
    m_nameLabel->setText("");
    m_roleLabel->setText("");
}

void CastCardCell::cacheForReuse() {
    ImageLoader::cancel(m_image);
    m_image->clear();
}

void CastCardCell::cancelPendingRequests() {
    ImageLoader::cancel(m_image);
}

CastDataSource::CastDataSource(const std::vector<plex::Tag>& cast, PlexServer* server)
    : m_cast(cast), m_server(server) {}

size_t CastDataSource::getItemCount() {
    return m_cast.size();
}

RecyclingGridItem* CastDataSource::cellForRow(RecyclingView* recycler, size_t index) {
    CastCardCell* cell = dynamic_cast<CastCardCell*>(recycler->dequeueReusableCell("CastCard"));
    if (cell && index < m_cast.size()) {
        cell->setActor(m_cast[index], m_server);
    }
    return cell;
}

void CastDataSource::onItemSelected(brls::Box* recycler, size_t index) {
    if (m_onActorClick && index < m_cast.size()) {
        m_onActorClick(m_cast[index]);
    }
}

void CastDataSource::clearData() {
    m_cast.clear();
}

void CastDataSource::setOnActorClick(std::function<void(const plex::Tag&)> callback) {
    m_onActorClick = callback;
}
