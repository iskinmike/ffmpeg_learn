
#include "frame_keeper.h"

FrameKeeper::~FrameKeeper(){
    av_frame_free(&frame);
}

void FrameKeeper::assigNewFrame(AVFrame* new_frame)
{
    std::lock_guard<std::mutex> lock(mtx);
    av_frame_free(&frame);
    frame = av_frame_clone(new_frame);
    cond_flag = true;
    cond.notify_all(); // Signal for waiter that we have new Frame
}

AVFrame *FrameKeeper::getFrame()
{
    std::lock_guard<std::mutex> lock(mtx);
    cond_listeners_count.fetch_add(-1);
    if (cond_listeners_count == 0) {
        cond_flag = false;
    }
    return av_frame_clone(frame); // Пока попробуем так чтобы ничего не терять и не освободить данные случайно
}

std::condition_variable *FrameKeeper::getCondVar()
{
    return &cond;
}

std::mutex *FrameKeeper::getMtx()
{
    return &mtx;
}

void FrameKeeper::waitData()
{
    cond_listeners_count.fetch_add(1);
    std::unique_lock<std::mutex> lck(flag_mtx);
    while (!cond_flag) cond.wait_for(lck, std::chrono::seconds(4));
}
