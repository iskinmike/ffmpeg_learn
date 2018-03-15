


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
*/

extern "C" { // based on: https://stackoverflow.com/questions/24487203/ffmpeg-undefined-reference-to-avcodec-register-all-does-not-link
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <avdevice.h> // header included from downloaded source fmpeg-2.18
#include <libavutil/imgutils.h>
}

#include <SDL2/SDL.h>
#include <SDL2/SDL_thread.h>

#include <stdio.h>
#include <assert.h>
#include <math.h>
#include <string>
#include <vector>
#include <algorithm>

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
    std::string      out_file("out.video");
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

    printf("Need options %d", av_dict_count(optionsDict));

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
    numBytes=avpicture_get_size(PIX_FMT_RGB32, pCodecCtx->width,
                    pCodecCtx->height);
    buffer=(uint8_t *)av_malloc(numBytes*sizeof(uint8_t));

    sws_ctx = sws_getContext
        (
            pCodecCtx->width,
            pCodecCtx->height,
            pCodecCtx->pix_fmt,
            pCodecCtx->width,
            pCodecCtx->height,
            PIX_FMT_RGB32,
            SWS_BILINEAR,
            NULL,
            NULL,
            NULL
        );

    // Assign appropriate parts of buffer to image planes in pFrameRGB
    // Note that pFrameRGB is an AVFrame, but AVFrame is a superset
    // of AVPicture
    avpicture_fill((AVPicture *)pFrameRGB, buffer, PIX_FMT_RGB32,
           pCodecCtx->width, pCodecCtx->height);


    // Try to encode and write file
    FILE *pFile;
    // Open file
    pFile=fopen(out_file.c_str(), "wb");
    printf("file oened\n");
    outbuf = new uint8_t[numBytes*sizeof(uint8_t)];

    pEncodeCodec = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (!pEncodeCodec) {
        fprintf(stderr, "codec not found\n");
        exit(1);
    }
    pEncodeCodecCtx = avcodec_alloc_context3(pEncodeCodec);
    pEncodeCodecCtx->codec_id = AV_CODEC_ID_H264;
    pEncodeCodecCtx->codec_type = AVMEDIA_TYPE_VIDEO;
    pEncodeCodecCtx->gop_size = 10;//pCodecCtx->gop_size;
    pEncodeCodecCtx->bit_rate = pCodecCtx->bit_rate;
    pEncodeCodecCtx->width = pCodecCtx->width;
    pEncodeCodecCtx->height = pCodecCtx->height;
    pEncodeCodecCtx->time_base = pCodecCtx->time_base;
    pEncodeCodecCtx->max_b_frames = 1;
    pEncodeCodecCtx->pix_fmt = AV_PIX_FMT_YUV420P;
//    pEncodeCodecCtx->

    printf("Settings: \n\
        width:[%d]\n\
        height:[%d]\n\
    \n",
    pEncodeCodecCtx->width,
    pEncodeCodecCtx->height);

    if(avcodec_open2(pEncodeCodecCtx, pEncodeCodec, NULL)<0) {
        printf("Can't open codec to encode\n");
        return -1; // Could not open codec
    }

    AVFrame* frame = av_frame_alloc();//avcodec_alloc_frame();
    if (!frame) {
        fprintf(stderr, "Could not allocate video frame\n");
        return -1;
    }
    frame->format = pEncodeCodecCtx->pix_fmt;
    frame->width  = pEncodeCodecCtx->width;
    frame->height = pEncodeCodecCtx->height;
//    av_frame_get_buffer(frame, 32); // get buffer

    /* the image can be allocated by any means and av_image_alloc() is
          * just the most convenient way if av_malloc() is to be used */

    int ret = av_image_alloc(frame->data, frame->linesize, pEncodeCodecCtx->width, pEncodeCodecCtx->height, pEncodeCodecCtx->pix_fmt, 32);
    if (ret < 0) {
     printf("Could not allocate raw picture buffer\n");
     return -1;
    }

    int out_size = 0;

//    std::vector<AVPacket> pkt_queue;

    printf("Ready to code/decode video\n");
    for (int i=0; i< 1000; ++i){
        av_read_frame(pFormatCtx, &packet);
        printf("av_read_frame complete\n");
        //    while(av_read_frame(pFormatCtx, &packet)>=0) {
        // Is this a packet from the video stream?
        if(packet.stream_index==videoStream) {
            // Decode video frame
            printf("try to decode\n");
            avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished, &packet);
            printf("decoded\n");
            // Did we get a video frame?
            if(frameFinished) {
                // Convert the image from its native format to RGB
                sws_scale
                (
                    sws_ctx,
                    (uint8_t const * const *)pFrame->data,
                    pFrame->linesize,
                    0,
                    pCodecCtx->height,
                    pFrameRGB->data,
                    pFrameRGB->linesize
                );

                /* prepare a dummy image */
                         /* Y */
                for(int y=0;y<pEncodeCodecCtx->height;y++) {
                 for(int x=0;x<pEncodeCodecCtx->width;x++) {
                     frame->data[0][y * frame->linesize[0] + x] = x + y + i * 3;
                 }
                }

                /* Cb and Cr */
                for(int y=0;y<pEncodeCodecCtx->height/2;y++) {
                 for(int x=0;x<pEncodeCodecCtx->width/2;x++) {
                     frame->data[1][y * frame->linesize[1] + x] = 128 + y + i * 2;
                     frame->data[2][y * frame->linesize[2] + x] = 64 + x + i * 5;
                 }
                }

//                frame->pts = pFrame->pts;
//                av_frame_copy(frame, pFrame);
                printf("frame settings: \n\
                    data:[%p]\n\
                    data[0]:[%p]\n\
                    data[1]:[%p]\n\
                    data[2]:[%p]\n\
                \n",
                (uint8_t*)frame->data,
                (uint8_t*)frame->data[0],
                (uint8_t*)frame->data[1],
                (uint8_t*)frame->data[2]);

                printf("pFrame settings: \n\
                    data:[%p]\n\
                    data[0]:[%p]\n\
                    data[1]:[%p]\n\
                    data[2]:[%p]\n\
                    linesize[0]:[%d]\n\
                    linesize[1]:[%d]\n\
                    linesize[2]:[%d]\n\
                \n",
                (uint8_t*) pFrame->data,
                (uint8_t*) pFrame->data[0],
                (uint8_t*) pFrame->data[1],
                (uint8_t*) pFrame->data[2],
                pFrame->linesize[0],
                pFrame->linesize[1],
                pFrame->linesize[2]
                );


                int got_pack = 0;
//                int out_size = avcodec_encode_video(pEncodeCodecCtx, outbuf, outbuf_size, pFrame);
                AVPacket tmp_pack;
                av_init_packet(&tmp_pack);
                tmp_pack.data = NULL; // for autoinit
                tmp_pack.size = 0;
//                do {
//                    out_size = avcodec_encode_video2(pEncodeCodecCtx, &tmp_pack, frame, &got_pack);
//                } while (got_pack != 1);

//                pkt_queue.push_back(tmp_pack);
//                if (tmp_pack.stream_index==videoStream) {
//                    printf("encoding frame %3d (size=%5d)  ---  pack size [%x]\n", i, out_size, tmp_pack.size);
//                    fwrite(tmp_pack.data, 1, tmp_pack.size, pFile);
//                }

                out_size = avcodec_encode_video(pEncodeCodecCtx, outbuf, outbuf_size, frame);
                printf("encoding frame %3d (size=%5d)\n", i, out_size);
                fwrite(outbuf, 1, out_size, pFile);

                SDL_Texture *texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_YV12,
                        SDL_TEXTUREACCESS_STREAMING, pCodecCtx->width, pCodecCtx->height);

                SDL_UpdateYUVTexture(
                    texture,
                    NULL,
                    pFrame->data[0], //vp->yPlane,
                    pFrame->linesize[0],
                    pFrame->data[1], //vp->yPlane,
                    pFrame->linesize[1],
//                        NULL, 0
                    pFrame->data[2], //vp->yPlane,
                    pFrame->linesize[2]
//                    NULL,//pFrameRGB->buf[0]->data, //vp->uPlane,
//                    0,//(pFrameRGB->buf[0]->data != NULL) ? (pCodecCtx->width / 2) : 0,    //vp->uvPitch,
//                    NULL,//pFrameRGB->buf[0]->data, //vp->vPlane,
//                    0//(pFrameRGB->buf[0]->data != NULL) ? (pCodecCtx->width / 2) : 0    //vp->uvPitch
                );
                SDL_RenderClear(renderer);
                SDL_RenderCopy(renderer, texture, NULL, NULL);
                SDL_RenderPresent(renderer);
                SaveFrame(pFrameRGB, pCodecCtx->width, pCodecCtx->height, i);
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

    int i = 0;
    for(; out_size; i++) {
//        fflush(stdout);

//        int got_pack = 0;
//        int out_size = avcodec_encode_video(pEncodeCodecCtx, outbuf, outbuf_size, pFrame);
//        AVPacket tmp_pack;
//        av_init_packet(&tmp_pack);
//        tmp_pack.data = NULL; // for autoinit
//        tmp_pack.size = 0;
//        do {
//            out_size = avcodec_encode_video2(pEncodeCodecCtx, &tmp_pack, NULL, &got_pack);
//        } while (got_pack != 1);

//        printf("encoding frame %3d (size=%5d)  ---  pack size [%x]\n", i, out_size, tmp_pack.size);
//        fwrite(tmp_pack.data, 1, tmp_pack.size, pFile);
        out_size = avcodec_encode_video(pEncodeCodecCtx, outbuf, outbuf_size, NULL);
        printf("encoding frame %3d (size=%5d)\n", i, out_size);
        fwrite(outbuf, 1, out_size, pFile);
    }

    outbuf[0] = 0x00;
    outbuf[1] = 0x00;
    outbuf[2] = 0x01;
    outbuf[3] = 0xb7;
    fwrite(outbuf, 1, 4, pFile);
    fclose(pFile);
}


