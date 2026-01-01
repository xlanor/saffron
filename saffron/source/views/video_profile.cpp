#include "views/video_profile.hpp"
#include "core/mpv_core.hpp"
#include <fmt/format.h>

VideoProfile::VideoProfile() {
    this->inflateFromXMLRes("xml/view/video_profile.xml");
    this->setVisibility(brls::Visibility::INVISIBLE);
    this->setPositionType(brls::PositionType::ABSOLUTE);
    this->setPositionTop(25);
    this->setPositionLeft(25);
}

void VideoProfile::update(const plex::Media* media) {
    auto* mpv = MPVCore::getInstance();
    if (!mpv->isValid()) return;

    if (media) {
        std::string filename;
        if (!media->parts.empty() && !media->parts[0].file.empty()) {
            filename = media->parts[0].file;
            size_t pos = filename.rfind('/');
            if (pos == std::string::npos) pos = filename.rfind('\\');
            if (pos != std::string::npos) filename = filename.substr(pos + 1);
        }
        labelSourceFile->setText(filename.empty() ? "N/A" : filename);
        labelSourceRes->setText(fmt::format("{}x{} ({})", media->width, media->height, media->videoResolution));
        labelSourceVideo->setText(media->videoCodec);
        labelSourceAudio->setText(fmt::format("{} {}ch", media->audioCodec, media->audioChannels));
        labelSourceBitrate->setText(fmt::format("{} kbps", media->bitrate));
    } else {
        labelSourceFile->setText("N/A");
        labelSourceRes->setText("N/A");
        labelSourceVideo->setText("N/A");
        labelSourceAudio->setText("N/A");
        labelSourceBitrate->setText("N/A");
    }

    labelVideoRes->setText(fmt::format("{}x{}@{:.0f}fps",
        mpv->getInt("video-params/w"),
        mpv->getInt("video-params/h"),
        mpv->getDouble("container-fps")));
    labelVideoCodec->setText(mpv->getString("video-codec"));

    std::string hwdec = mpv->getString("hwdec-current");
    labelVideoHW->setText(hwdec.empty() ? "no (software)" : hwdec);

    int64_t mpvBitrate = mpv->getInt("video-bitrate") / 1024;
    if (mpvBitrate > 0) {
        labelVideoBitrate->setText(fmt::format("{} kbps", mpvBitrate));
    } else if (media && media->bitrate > 0) {
        labelVideoBitrate->setText(fmt::format("~{} kbps", media->bitrate));
    } else {
        labelVideoBitrate->setText("N/A");
    }

    labelVideoFps->setText(fmt::format("{:.2f}", mpv->getDouble("estimated-vf-fps")));
    labelVideoDrop->setText(fmt::format("{} (dec) {} (out)",
        mpv->getInt("decoder-frame-drop-count"),
        mpv->getInt("frame-drop-count")));
    labelVideoSync->setText(fmt::format("{:.3f}", mpv->getDouble("avsync")));

    labelAudioCodec->setText(mpv->getString("audio-codec"));
    labelAudioChannel->setText(std::to_string(mpv->getInt("audio-params/channel-count")));

    labelCache->setText(fmt::format("{:.1f}s", mpv->getDouble("demuxer-cache-duration")));
}
