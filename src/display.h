#ifndef DISPLAY_H
#define DISPLAY_H

#include "SDL2/SDL.h"
#include <string>
#include <thread>
#include <atomic>
extern "C" { // based on: https://stackoverflow.com/questions/24487203/ffmpeg-undefined-reference-to-avcodec-register-all-does-not-link
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

class Display
{
    SDL_Renderer* renderer = NULL;
    SDL_Window     *screen = NULL;
    SDL_Rect        rect;
    SDL_Event       event;

    int width;
    int height;
    std::string title;

    std::thread display_thread;
    void runDisplay();
    std::atomic_bool stop_flag;
public:
    Display(const std::string& _title)
        : title(_title), stop_flag(false){}
    ~Display();

    int init(int pos_x, int pos_y, int _width, int _height);
    void startDisplay();
    void stopDisplay();

    void displayFrame(AVFrame* frame);
};


#endif  /* DISPLAY_H */
