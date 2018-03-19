
#include "video_api.h"

VideoAPI::VideoAPI(std::string in, std::string out, std::string fmt, std::string title)
{
    decoder = std::shared_ptr<Decoder> (new Decoder(in, fmt));
    encoder = std::shared_ptr<Encoder> (new Encoder(out));
    display = std::shared_ptr<Display> (new Display(title));
}

void VideoAPI::startVideoRecord()
{
    decoder->init();
    encoder->init(decoder->getBitRate(), decoder->getWidth(), decoder->getHeight());

    decoder->startDecoding();
    encoder->startEncoding();
}

void VideoAPI::startVideoDisplay(int pos_x, int pos_y)
{
    display->init(pos_x, pos_y, decoder->getWidth(), decoder->getHeight());
    display->startDisplay();
}

void VideoAPI::stopVideoRecord()
{
    encoder->stopEncoding();
    decoder->stopDecoding();
}

void VideoAPI::stopVideoDisplay()
{
    display->stopDisplay();
}
