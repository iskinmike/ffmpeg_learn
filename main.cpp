


// tutorial05.c
// A pedagogical video player that really works!
//
// Code based on FFplay, Copyright (c) 2003 Fabrice Bellard,
// and a tutorial by Martin Bohme (boehme@inb.uni-luebeckREMOVETHIS.de)
// Tested on Gentoo, CVS version 5/01/07 compiled with GCC 4.1.1
// With updates from https://github.com/chelyaev/ffmpeg-tutorial
// Updates tested on:
// LAVC 54.59.100, LAVF 54.29.104, LSWS 2.1.101, SDL 1.2.15
// on GCC 4.7.2 in Debian February 2015
// Use
//
//gcc -g -o tutorial05 ffmpeg_tutorial05.c -lavformat -lavcodec -lswscale -lavutil -lswresample -lz -ldl `sdl2-config --cflags --libs`
// to build (assuming libavformat and libavcodec are correctly installed,
// and assuming you have sdl-config. Please refer to SDL docs for your installation.)
//
// Run using
// tutorial04 myvideofile.mpg
//
// to play the video stream on your screen.

/*
useful to understand:
    https://ffmpeg.zeranoe.com/forum/viewtopic.php?t=604
    https://stackoverflow.com/questions/17816532/creating-a-video-from-images-using-ffmpeg-libav-and-libx264
    https://stackoverflow.com/questions/16564870/ffmpeg-format-not-available
*/

extern "C" { // based on: https://stackoverflow.com/questions/24487203/ffmpeg-undefined-reference-to-avcodec-register-all-does-not-link
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
//#include <avdevice.h> // header included from downloaded source fmpeg-2.18
#include <libavutil/imgutils.h>
#include <libavdevice/avdevice.h> // installed libavdevice-dev
}

#include <SDL2/SDL.h>
#include <SDL2/SDL_thread.h>

#include <stdio.h>
#include <assert.h>
#include <math.h>
#include <string>
#include <vector>
#include <algorithm>
#include <iostream>

#define FF_REFRESH_EVENT (SDL_USEREVENT)
#define FF_QUIT_EVENT (SDL_USEREVENT + 1)

void SaveFrame(AVFrame *pFrame, int width, int height, int iFrame) {
  FILE *pFile;
  char szFilename[64];
  int  y;

  // Open file
  sprintf(szFilename, "tmp/frame%d.ppm", iFrame);
  pFile=fopen(szFilename, "wb");
  if(pFile==NULL)
    return;

  // Write header
  fprintf(pFile, "P6\n%d %d\n255\n", width, height);

  // Write pixel data
  for(y=0; y<height; y++)
    fwrite(pFrame->data[0]+y*pFrame->linesize[0], 1, width*3, pFile);

  // Close file
  fclose(pFile);
}

int main(int argc, char *argv[]){
    if(argc < 2) {
        printf("Please provide a movie file\n");
        return -1;
    }

    std::string      filename(argv[1]);
    std::string      out_file("out.mpg");
    std::string      format("video4linux2");
    AVFormatContext *pFormatCtx = NULL;
    AVInputFormat   *file_iformat = NULL;
    AVCodecContext*  pCodecCtx = NULL;
    AVCodec*         pCodec = NULL;
    AVDictionary    *optionsDict = NULL;
    AVFrame         *pFrame = NULL;
    AVFrame         *pFrameRGB = NULL;

    struct SwsContext      *sws_ctx = NULL;
    int             numBytes;
    uint8_t         *buffer = NULL;

    AVPacket        packet;
    int             frameFinished;

    AVCodecContext*  pEncodeCodecCtx = NULL;
    AVCodec*         pEncodeCodec = NULL;

    int outbuf_size = 100000;
    uint8_t* outbuf = NULL;// malloc(outbuf_size);

//    SDL_Overlay     *bmp = NULL;
    SDL_Renderer* renderer = NULL;
    SDL_Window     *screen = NULL;
    SDL_Rect        rect;
    SDL_Event       event;

    if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER)) {
      fprintf(stderr, "Could not initialize SDL - %s\n", SDL_GetError());
      exit(1);
    }

    // Register all formats and codecs
    av_register_all();
    avdevice_register_all();

    // determine format context
    pFormatCtx = avformat_alloc_context();
    if (!pFormatCtx) {
        printf("Memory error\n");
        return -1;
    }

    file_iformat = av_find_input_format(format.c_str());
    if (file_iformat == NULL) {
        printf("Unknown input format: '%s'\n", format.c_str());
        return -1; // Couldn't open file
    }

    // Open video file
    if(avformat_open_input(&pFormatCtx, filename.c_str(), file_iformat, NULL)!=0){
      printf("Can't Open video file with format [%s] try to autodetect...\n", format.c_str());
      if(avformat_open_input(&pFormatCtx, filename.c_str(), NULL, NULL)!=0){
          printf("Can't Open video file anyway\n");
          return -1; // Couldn't open file
      }
    } 

    // Retrieve stream information
    if(avformat_find_stream_info(pFormatCtx, NULL)<0) {
      printf("Can't Retrieve stream information\n");
      return -1; // Couldn't find stream information
    }

    // Dump information about file onto standard error
    av_dump_format(pFormatCtx, 0, argv[1], 0);

    std::cout << "format bit_rate  = " << pFormatCtx->bit_rate  << std::endl;


    // Find the first video stream
    int videoStream=-1;
    for(int i=0; i<pFormatCtx->nb_streams; i++)
    if(pFormatCtx->streams[i]->codec->codec_type==AVMEDIA_TYPE_VIDEO) {
      videoStream=i;
      break;
    }
    if(videoStream==-1) {
        printf("Can't find streams\n");
        return -1; // Didn't find a video stream
    }
    printf("find %d streams\n", videoStream + 1);

    // Get a pointer to the codec context for the video stream
    pCodecCtx=pFormatCtx->streams[videoStream]->codec;

    // Find the decoder for the video stream
    pCodec=avcodec_find_decoder(pCodecCtx->codec_id);
    if(pCodec==NULL) {
      printf("Unsupported codec!\n");
      return -1; // Codec not found
    }
    printf("Find codec [%s]\n", pCodec->long_name);

    // Open codec
    if(avcodec_open2(pCodecCtx, pCodec, &optionsDict)<0) {
        printf("Can't open codec\n");
        return -1; // Could not open codec
    }

    printf("Need options %d\n", av_dict_count(optionsDict));

    // Allocate video frame
    pFrame=av_frame_alloc();

    // Allocate an AVFrame structure
    pFrameRGB=av_frame_alloc();
    if(pFrameRGB==NULL){
        return -1;
    }

    screen = SDL_CreateWindow("FFmpeg Tutorial",
                              SDL_WINDOWPOS_UNDEFINED,
                              SDL_WINDOWPOS_UNDEFINED, pCodecCtx->width, pCodecCtx->height, 0);
    if(!screen) {
        fprintf(stderr, "SDL: could not set video mode - exiting\n");
        exit(1);
    }

    renderer = SDL_CreateRenderer(screen, -1, 0);
    if (!renderer) {
        fprintf(stderr, "SDL: could not create renderer - exiting\n");
        exit(1);
    }

    // Determine required buffer size and allocate buffer
    numBytes=avpicture_get_size(AV_PIX_FMT_YUV420P, pCodecCtx->width,
                    pCodecCtx->height);
    buffer=(uint8_t *)av_malloc(numBytes*sizeof(uint8_t));

    sws_ctx = sws_getContext
        (
            pCodecCtx->width,
            pCodecCtx->height,
            pCodecCtx->pix_fmt,
            pCodecCtx->width,
            pCodecCtx->height,
            AV_PIX_FMT_YUV420P,
            SWS_BILINEAR,
            NULL,
            NULL,
            NULL
        );

    // Assign appropriate parts of buffer to image planes in pFrameRGB
    // Note that pFrameRGB is an AVFrame, but AVFrame is a superset
    // of AVPicture
    avpicture_fill((AVPicture *)pFrameRGB, buffer, AV_PIX_FMT_YUV420P,
           pCodecCtx->width, pCodecCtx->height);


    // Try to encode and write file
    FILE *pFile;
    // Open file
    pFile=fopen(out_file.c_str(), "wb");
    printf("file opened\n");
    outbuf = new uint8_t[numBytes*sizeof(uint8_t)];

//    pEncodeCodec = avcodec_find_encoder(AV_CODEC_ID_H264);
    pEncodeCodec = avcodec_find_encoder(AV_CODEC_ID_MPEG4);
    if (!pEncodeCodec) {
        fprintf(stderr, "codec not found\n");
        exit(1);
    }
    pEncodeCodecCtx = avcodec_alloc_context3(pEncodeCodec);
//    pEncodeCodecCtx->codec_id = AV_CODEC_ID_H264;
    pEncodeCodecCtx->codec_type = AVMEDIA_TYPE_VIDEO;
    pEncodeCodecCtx->gop_size = 0;//pCodecCtx->gop_size;
    pEncodeCodecCtx->bit_rate = pFormatCtx->bit_rate;
    pEncodeCodecCtx->width = pCodecCtx->width;
    pEncodeCodecCtx->height = pCodecCtx->height;
    pEncodeCodecCtx->time_base = (AVRational){1,25};//pCodecCtx->time_base;//
    pEncodeCodecCtx->max_b_frames = 0;
    pEncodeCodecCtx->pix_fmt = AV_PIX_FMT_YUV420P;
    pEncodeCodecCtx->bit_rate_tolerance = pFormatCtx->bit_rate;

    std::cout << "codec_id     = " << pEncodeCodecCtx->codec_id     << std::endl;
    std::cout << "codec_type   = " << pEncodeCodecCtx->codec_type   << std::endl;
    std::cout << "gop_size     = " << pEncodeCodecCtx->gop_size     << std::endl;
    std::cout << "bit_rate     = " << pEncodeCodecCtx->bit_rate     << std::endl;
    std::cout << "width        = " << pEncodeCodecCtx->width        << std::endl;
    std::cout << "height       = " << pEncodeCodecCtx->height       << std::endl;
    std::cout << "max_b_frames = " << pEncodeCodecCtx->max_b_frames << std::endl;
    std::cout << "refcounted_frames = " << pEncodeCodecCtx->refcounted_frames << std::endl;
    std::cout << "pix_fmt           = " << pCodecCtx->pix_fmt << std::endl;
    std::cout << "bit_rate_tolerance = " << pEncodeCodecCtx->bit_rate_tolerance << std::endl;


    if(avcodec_open2(pEncodeCodecCtx, pEncodeCodec, NULL)<0) {
        printf("Can't open codec to encode\n");
        return -1; // Could not open codec
    }

//    AVFrame* frame = av_frame_alloc();//avcodec_alloc_frame();
//    if (!frame) {
//        fprintf(stderr, "Could not allocate video frame\n");
//        return -1;
//    }
//    frame->format = pEncodeCodecCtx->pix_fmt;
//    frame->width  = pEncodeCodecCtx->width;
//    frame->height = pEncodeCodecCtx->height;
////    av_frame_get_buffer(frame, 32); // get buffer

//    /* the image can be allocated by any means and av_image_alloc() is
//          * just the most convenient way if av_malloc() is to be used */

//    int ret = av_image_alloc(frame->data, frame->linesize, pEncodeCodecCtx->width, pEncodeCodecCtx->height, pEncodeCodecCtx->pix_fmt, 32);
//    if (ret < 0) {
//     printf("Could not allocate raw picture buffer\n");
//     return -1;
//    }

    int out_size = 0;

//    std::vector<AVPacket> pkt_queue;

    int i=0;
    printf("Ready to code/decode video\n");
    while(av_read_frame(pFormatCtx, &packet)>=0) {
//    for ( i=0; i< 100; ++i){
//        av_read_frame(pFormatCtx, &packet);

        // Is this a packet from the video stream?
        if(packet.stream_index==videoStream) {
            // Decode video frame
            avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished, &packet);
            // Did we get a video frame?
            if(frameFinished) {
//                if (i < 4) { // skip couple of frames
//                    continue;
//                }
                // Convert the image from its native format to AV_PIX_FMT_YUV420P
                sws_scale(sws_ctx,
                    ((AVPicture*)pFrame)->data, ((AVPicture*)pFrame)->linesize, 0,
                    pCodecCtx->height, ((AVPicture *)pFrameRGB)->data,
                    ((AVPicture *)pFrameRGB)->linesize);

                pFrameRGB->pts = pFrame->best_effort_timestamp;

                pFrameRGB->format = AV_PIX_FMT_YUV420P;
                pFrameRGB->width = pCodecCtx->width;
                pFrameRGB->height = pCodecCtx->height;

                int got_pack = 0;
//                int out_size = avcodec_encode_video(pEncodeCodecCtx, outbuf, outbuf_size, pFrame);
                AVPacket tmp_pack;
                av_init_packet(&tmp_pack);
                tmp_pack.data = NULL; // for autoinit
                tmp_pack.size = 0;
//                do {
                    out_size = avcodec_encode_video2(pEncodeCodecCtx, &tmp_pack, pFrameRGB, &got_pack);
//                } while (got_pack != 1);

//                pkt_queue.push_back(tmp_pack);
                printf("encoding frame %3d (size=%5d)  ---  pack size [%x] --- [%d]\n", i++, out_size, tmp_pack.size, got_pack);
                if (got_pack) {
                    fwrite(tmp_pack.data, 1, tmp_pack.size, pFile);
                }

//                out_size = avcodec_encode_video(pEncodeCodecCtx, outbuf, outbuf_size, frame);
//                printf("encoding frame %3d (size=%5d)\n", i, out_size);
//                fwrite(outbuf, 1, out_size, pFile);

                SDL_Texture *texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_YV12,
                        SDL_TEXTUREACCESS_STREAMING, pCodecCtx->width, pCodecCtx->height);

                SDL_UpdateYUVTexture(
                    texture,
                    NULL,
                    pFrameRGB->data[0], //vp->yPlane,
                    pFrameRGB->linesize[0],
                    pFrameRGB->data[1], //vp->yPlane,
                    pFrameRGB->linesize[1],
                    pFrameRGB->data[2], //vp->yPlane,
                    pFrameRGB->linesize[2]
                );
                SDL_RenderClear(renderer);
                SDL_RenderCopy(renderer, texture, NULL, NULL);
                SDL_RenderPresent(renderer);

                SDL_PollEvent(&event);
                switch(event.type) {
                case SDL_SCANCODE_Q:
                case SDL_QUIT:
                  SDL_Quit();
                  exit(0);
                  break;
                default:
                  break;
                }
//                SaveFrame(pFrameRGB, pCodecCtx->width, pCodecCtx->height, i);
            }
        }

        // Free the packet that was allocated by av_read_frame
        av_free_packet(&packet);
    }

    // try to sort
//    std::sort(pkt_queue.begin(), pkt_queue.end(), [] (AVPacket a, AVPacket b) {
//        if (a.pts < b.pts) {
//            return true;
//        }
//    } );

//    for (auto el : pkt_queue) {
//        fwrite(el.data, 1, el.size, pFile);
//    }

//    out_size=1;
//    for(; out_size; i++) {
////        fflush(stdout);

//        int got_pack = 0;
//        AVPacket tmp_pack;
//        av_init_packet(&tmp_pack);
//        tmp_pack.data = NULL; // for autoinit
//        tmp_pack.size = 0;
////        do {
//            out_size = avcodec_encode_video2(pEncodeCodecCtx, &tmp_pack, NULL, &got_pack);
////        } while (got_pack != 1);

//        printf("encoding frame %3d (size=%5d)  ---  pack size [%x]\n", i, out_size, tmp_pack.size);
//        fwrite(tmp_pack.data, 1, tmp_pack.size, pFile);
//        if (out_size != 0) {
//            break;
//        }
//        out_size = avcodec_encode_video(pEncodeCodecCtx, outbuf, outbuf_size, NULL);
//        printf("encoding frame %3d (size=%5d)\n", i, out_size);
//        fwrite(outbuf, 1, out_size, pFile);
//    }

    outbuf[0] = 0x00;
    outbuf[1] = 0x00;
    outbuf[2] = 0x01;
    outbuf[3] = 0xb7;
    fwrite(outbuf, 1, 4, pFile);
    fclose(pFile);
}


