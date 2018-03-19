#ifndef ENCODER_H
#define ENCODER_H

extern "C" { // based on: https://stackoverflow.com/questions/24487203/ffmpeg-undefined-reference-to-avcodec-register-all-does-not-link
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}
#include <stdio.h>
#include <string>
#include <thread>
#include <atomic>
#include <iostream>

class Encoder
{
    AVCodecContext*  pEncodeCodecCtx;
    AVCodec*         pEncodeCodec;

    std::string      out_file;//("out.mpg");

    int bit_rate;
    int width;
    int height;

    FILE *pFile;
    std::thread encoder_thread;
    void runEncoding();
    std::atomic_bool stop_flag;
public:
  Encoder(std::string out)
      : pEncodeCodecCtx(NULL), pEncodeCodec(NULL), out_file(out) {}
  ~Encoder();

  int init(int _bit_rate, int _width, int _height);
  void startEncoding();
  void stopEncoding();

  void encodeFrame(AVFrame* frame);
  void closeFile();
};



#endif  /* ENCODER_H */
