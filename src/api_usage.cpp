
#include "decoder.h"
#include "encoder.h"
#include "display.h"
#include <iostream>
#include <unistd.h>
#include "video_api.h"

using namespace std;

int main(int argc, char const *argv[])
{
    /* code */
//    Decoder dec(argv[1], "video4linux2");
//    dec.init();

//    Encoder enc(argv[2]);
//    enc.init(dec.getBitRate(), dec.getWidth(), dec.getHeight());

//    Display display(std::string("TTTTITLE"));
//    display.init(200,200, dec.getWidth(), dec.getHeight());

//    dec.startDecoding();
//    enc.startEncoding();
//    display.startDisplay();

    VideoAPI api(argv[1], argv[2], "video4linux2", "CAM VIDEO");

    api.startVideoRecord();
    api.startVideoDisplay(200,200);

    for (int i = 0; i <5; ++i){
        sleep(1);
        cout << "sleep: " << i << endl;
    }

    api.stopVideoDisplay();
    api.stopVideoRecord();

//    enc.stopEncoding();
//    display.stopDisplay();
//    dec.stopDecoding();

    return 0;
}

