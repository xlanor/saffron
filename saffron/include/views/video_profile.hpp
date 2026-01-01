#ifndef SAFFRON_VIDEO_PROFILE_HPP
#define SAFFRON_VIDEO_PROFILE_HPP

#include <borealis.hpp>
#include "models/plex_types.hpp"

class VideoProfile : public brls::Box {
public:
    VideoProfile();
    void update(const plex::Media* media = nullptr);

private:
    BRLS_BIND(brls::Label, labelSourceFile, "profile/source/file");
    BRLS_BIND(brls::Label, labelSourceRes, "profile/source/res");
    BRLS_BIND(brls::Label, labelSourceVideo, "profile/source/video");
    BRLS_BIND(brls::Label, labelSourceAudio, "profile/source/audio");
    BRLS_BIND(brls::Label, labelSourceBitrate, "profile/source/bitrate");
    BRLS_BIND(brls::Label, labelVideoRes, "profile/video/res");
    BRLS_BIND(brls::Label, labelVideoCodec, "profile/video/codec");
    BRLS_BIND(brls::Label, labelVideoHW, "profile/video/hwdec");
    BRLS_BIND(brls::Label, labelVideoBitrate, "profile/video/bitrate");
    BRLS_BIND(brls::Label, labelVideoFps, "profile/video/fps");
    BRLS_BIND(brls::Label, labelVideoDrop, "profile/video/drop");
    BRLS_BIND(brls::Label, labelVideoSync, "profile/video/avsync");
    BRLS_BIND(brls::Label, labelAudioCodec, "profile/audio/codec");
    BRLS_BIND(brls::Label, labelAudioChannel, "profile/audio/channel");
    BRLS_BIND(brls::Label, labelCache, "profile/cache");
};

#endif
