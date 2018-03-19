#ifndef FRAME_KEEPER_H
#define FRAME_KEEPER_H

#include <mutex>
#include <condition_variable>
#include <atomic>
#include <chrono>

extern "C" { // based on: https://stackoverflow.com/questions/24487203/ffmpeg-undefined-reference-to-avcodec-register-all-does-not-link
#include <libavcodec/avcodec.h>
}

// Try to do it as Meyerce's Singletone [https://ru.stackoverflow.com/questions/327136/singleton-%D0%B8-%D1%80%D0%B5%D0%B0%D0%BB%D0%B8%D0%B7%D0%B0%D1%86%D0%B8%D1%8F]
class FrameKeeper
{
    AVFrame *frame;
    std::mutex mtx;
    std::mutex flag_mtx;
    std::atomic_int cond_listeners_count;
    bool cond_flag;
    std::condition_variable cond;
    FrameKeeper(): cond_flag(false){}
    ~FrameKeeper();
public:
    static FrameKeeper& Instance()
    {
        // согласно стандарту, этот код ленивый и потокобезопасный
        static FrameKeeper fk;
        return fk;
    }
    FrameKeeper(FrameKeeper const&) = delete;
    FrameKeeper& operator= (FrameKeeper const&) = delete;
  
  void assigNewFrame(AVFrame *new_frame);
  AVFrame* getFrame();
  std::condition_variable* getCondVar();
//  bool getFlag();
  std::mutex *getMtx();

  void waitData();
};
#endif  /* FRAME_KEEPER_H */
