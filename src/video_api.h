#ifndef VIDEO_API_H
#define VIDEO_API_H

#include "decoder.h"
#include "encoder.h"
#include "display.h"
#include <iostream>
#include <memory>


class VideoAPI
{
    std::shared_ptr<Decoder> decoder;
    std::shared_ptr<Encoder> encoder;
    std::shared_ptr<Display> display;//"video4linux2"

    std::string input_file, output_file, format;

public:
    VideoAPI(std::string in, std::string out, std::string fmt, std::string title);
    ~VideoAPI(){}
    void startVideoRecord();
    void startVideoDisplay(int pos_x, int pos_y);
    void stopVideoRecord();
    void stopVideoDisplay();
};







#endif  /* VIDEO_API_H */
