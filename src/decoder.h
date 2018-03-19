#ifndef DECODER_H
#define DECODER_H

extern "C" { // based on: https://stackoverflow.com/questions/24487203/ffmpeg-undefined-reference-to-avcodec-register-all-does-not-link
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <libavdevice/avdevice.h> // installed libavdevice-dev
}

#include "frame_keeper.h"
#include <string>
#include <stdio.h>
#include <iostream>
#include <thread>
#include <atomic>

void start_decode_video(std::string file_name);

class Decoder
{
    std::string      filename;//(argv[1]);
    std::string      format;//("video4linux2");
    AVFormatContext *pFormatCtx;
    AVInputFormat   *file_iformat;
    AVCodecContext*  pCodecCtx;
    AVCodec*         pCodec;
    AVDictionary    *optionsDict;
    AVFrame         *pFrame;
    AVFrame         *pFrameOut;

    struct SwsContext      *sws_ctx;
    int             numBytes;
    uint8_t         *buffer;

    AVPacket        packet;
    int             frameFinished;
    int videoStream;

    std::thread decoder_thread;

    int bit_rate;
    int width;
    int height;
    void runDecoding();

    std::atomic_bool stop_flag;
public:
  Decoder(std::string in, std::string _format)
      : filename(in), format(_format),
        pFormatCtx(NULL),file_iformat(NULL),pCodecCtx(NULL),
        pCodec(NULL), optionsDict(NULL), pFrame(NULL),
        pFrameOut(NULL), sws_ctx(NULL), buffer(NULL), stop_flag(false) {}
  ~Decoder();

  int init();

  void startDecoding();
  void stopDecoding();

  int getBitRate();
  int getWidth();
  int getHeight();

};

#endif  /* DECODER_H */
