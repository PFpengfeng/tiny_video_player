
/**
 * fpeng
 * A journey of a thousand miles begins with a single step.
 */

extern "C"
{
    #include "libavcodec/avcodec.h"
    #include "libavformat/avformat.h"
    #include "libavutil/pixfmt.h"
    #include "libswscale/swscale.h"
    #include "SDL2/SDL.h"
}

#include <stdio.h>

//Refresh Event
#define SFM_REFRESH_EVENT  (SDL_USEREVENT + 1)

#define SFM_BREAK_EVENT  (SDL_USEREVENT + 2)

int thread_exit=0;
int thread_pause=0;

int sfp_refresh_thread(void *opaque){
    thread_exit=0;
    thread_pause=0;
    while (!thread_exit) {
        if(!thread_pause){
            SDL_Event event;
            event.type = SFM_REFRESH_EVENT;
            SDL_PushEvent(&event);
        }
        SDL_Delay(40);
    }
    thread_exit=0;
    thread_pause=0;
    //Break
    SDL_Event event;
    event.type = SFM_BREAK_EVENT;
    SDL_PushEvent(&event);
    
    return 0;
}



int main(int argc, char *argv[])
{
    char *file_path = "../../testmedia/Johnny.mp4";
    AVFormatContext *pFormatCtx;
    AVCodecContext *pCodecCtx;
    AVCodec *pCodec;
    AVFrame *pFrame, *pFrameYUV;
    AVPacket *packet;
    uint8_t *out_buffer;

    int pixel_w,pixel_h;

    static struct SwsContext *img_convert_ctx;

    int videoStream, i, numBytes;
    int ret, got_picture;

    // SDL
    int screen_w=0,screen_h=0;
    SDL_Window *screen;
    SDL_Renderer* sdlRenderer;
    SDL_Texture* sdlTexture;
    SDL_Rect sdlRect;
    SDL_Thread *video_tid;
    SDL_Event event;

    av_register_all(); //初始化 FFmpeg，只有调用该函数，才能使用 复用器、编解码器

    //Allocate an AVFormatContext.
    // AVFormatContext 包含码流参数的结构体， pFormatCtx 用于保存码率信息
    pFormatCtx = avformat_alloc_context();

    // 打开码流文件
    if (avformat_open_input(&pFormatCtx, file_path, NULL, NULL) != 0) {
        printf("can't open the file. \n");
        return -1;
    }

    // avformat_find_stream_info 给 AVstream 结构体赋值，例如寻找解码器
    if (avformat_find_stream_info(pFormatCtx, NULL) < 0) {
        printf("Could't find stream infomation.\n");
        return -1;
    }

    videoStream = -1;

    ///循环查找视频中包含的流信息，直到找到视频类型的流
    ///便将其记录下来 保存到videoStream变量中
    ///这里我们现在只处理视频流  音频流先不管
    for (i = 0; i < pFormatCtx->nb_streams; i++) {
        if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoStream = i;
        }
    }

    ///如果videoStream为-1 说明没有找到视频流
    if (videoStream == -1) {
        printf("Didn't find a video stream.\n");
        return -1;
    }

    ///查找解码器
    pCodecCtx = pFormatCtx->streams[videoStream]->codec;
    pCodec = avcodec_find_decoder(pCodecCtx->codec_id);
    if (pCodec == NULL) {
        printf("Codec not found.\n");
        return -1;
    }

    ///打开解码器
    if (avcodec_open2(pCodecCtx, pCodec, NULL) < 0) {
        printf("Could not open codec.\n");
        return -1;
    }

    pFrame = av_frame_alloc();
    pFrameYUV = av_frame_alloc();

    pixel_w = pCodecCtx->width;
    pixel_h = pCodecCtx->height;

    // sws_getContext 进行图形缩放
    img_convert_ctx = sws_getContext(pCodecCtx->width, pCodecCtx->height,
            pCodecCtx->pix_fmt, pCodecCtx->width, pCodecCtx->height,
            AV_PIX_FMT_YUV420P, SWS_BICUBIC, NULL, NULL, NULL);

    // 图片需要的 bytes 大小
    numBytes = avpicture_get_size(AV_PIX_FMT_YUV420P, pCodecCtx->width,pCodecCtx->height);

    out_buffer = (uint8_t *) av_malloc(numBytes * sizeof(uint8_t));
    avpicture_fill((AVPicture *) pFrameYUV, out_buffer, AV_PIX_FMT_YUV420P,
            pCodecCtx->width, pCodecCtx->height);

    int y_size = pCodecCtx->width * pCodecCtx->height;

    packet = (AVPacket *) malloc(sizeof(AVPacket)); //分配一个packet
    av_new_packet(packet, y_size); //分配packet的数据
    av_dump_format(pFormatCtx, 0, file_path, 0); //输出视频信息到控制台

    int index = 0;
    // SDL init
    screen_h = pCodecCtx->height;
    screen_w = pCodecCtx->width;
    screen = SDL_CreateWindow("tiny video player",SDL_WINDOWPOS_UNDEFINED,
                SDL_WINDOWPOS_UNDEFINED,screen_w,screen_h,SDL_WINDOW_OPENGL);
    
    if(!screen){
        printf("SDL: could not create window - exiting: %s \n",SDL_GetError());
        return -1;
    }
    sdlRenderer = SDL_CreateRenderer(screen,-1,0);
    sdlTexture = SDL_CreateTexture(sdlRenderer,SDL_PIXELFORMAT_IYUV,SDL_TEXTUREACCESS_STREAMING,
          pCodecCtx->width,pCodecCtx->height);
    sdlRect.x=0;
    sdlRect.y=0;
    sdlRect.w=screen_w;
    sdlRect.h=screen_h;
    //SDL init End----------------------

    video_tid = SDL_CreateThread(sfp_refresh_thread,NULL,NULL);

    while (1)
    {
        SDL_WaitEvent(&event);
        if(event.type == SFM_REFRESH_EVENT){
            while(1){
                if(av_read_frame(pFormatCtx,packet) < 0){
                    printf("no packet!\n");
                    thread_exit = 1;
                }
                if(packet->stream_index == videoStream){
                    break;
                }
                ret = avcodec_decode_video2(pCodecCtx, pFrame, &got_picture,packet);
                if(ret < 0){
                    printf("Decode Error.\n");
                    return -1;
                }
            }
            if(got_picture){
                sws_scale(img_convert_ctx,
                        (uint8_t const * const *) pFrame->data,
                        pFrame->linesize, 0, pCodecCtx->height, pFrameYUV->data,
                        pFrameYUV->linesize);

                // // SDL display 
                SDL_UpdateYUVTexture(sdlTexture, &sdlRect,
                        pFrameYUV->data[0], pFrameYUV->linesize[0],
                        pFrameYUV->data[1], pFrameYUV->linesize[1],
                        pFrameYUV->data[2], pFrameYUV->linesize[2]);
                SDL_RenderClear(sdlRenderer);  
                SDL_RenderCopy( sdlRenderer, sdlTexture,  NULL, &sdlRect);  
                SDL_RenderPresent( sdlRenderer ); 
            }
            av_free_packet(packet);
        }
        else if(event.type == SDL_KEYDOWN){
            if(event.key.keysym.sym == SDLK_SPACE){
                thread_pause = !thread_pause;
            }
        }
        else if(event.type == SDL_QUIT){
            thread_exit = 1;
        }
        else if(event.type = SFM_BREAK_EVENT){
            break;
        }
    }
    sws_freeContext(img_convert_ctx);
    SDL_Quit();
    av_free(out_buffer);
    av_frame_free(&pFrame);
    av_frame_free(&pFrameYUV);
    avcodec_close(pCodecCtx);
    avformat_close_input(&pFormatCtx);
    return 0;
}

