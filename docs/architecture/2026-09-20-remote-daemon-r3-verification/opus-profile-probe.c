#include <opus.h>
#include <math.h>
#include <stdio.h>
#include <time.h>
/* R-R3-23: the product profile. The coded bandwidth follows the target
   bitrate as bandwidthForBitrate() in OpusAudioCodec.cpp forces it:
   24000 bit/s -> wideband (1103), 48000 bit/s -> fullband (1105).
   Fixture: 1 kHz + 15 kHz on every channel, so a fullband packet has
   something above 8 kHz to carry. */
static int bandwidthForBitrate(int bitrate) {
 return bitrate==48000 ? OPUS_BANDWIDTH_FULLBAND : OPUS_BANDWIDTH_WIDEBAND;
}
int main(void) {
 for(int channels=1;channels<=2;channels++) {
  for(int bitrate=24000;bitrate<=48000;bitrate+=24000) {
   int err=0; OpusEncoder* enc=opus_encoder_create(48000,channels,OPUS_APPLICATION_AUDIO,&err);
   if(!enc||err)return 2;
   int forced=bandwidthForBitrate(bitrate);
   if(opus_encoder_ctl(enc,OPUS_SET_SIGNAL(OPUS_SIGNAL_MUSIC))!=OPUS_OK
    ||opus_encoder_ctl(enc,OPUS_SET_BANDWIDTH(forced))!=OPUS_OK
    ||opus_encoder_ctl(enc,OPUS_SET_BITRATE(bitrate))!=OPUS_OK
    ||opus_encoder_ctl(enc,OPUS_SET_VBR(1))!=OPUS_OK
    ||opus_encoder_ctl(enc,OPUS_SET_VBR_CONSTRAINT(1))!=OPUS_OK
    ||opus_encoder_ctl(enc,OPUS_SET_COMPLEXITY(10))!=OPUS_OK
    ||opus_encoder_ctl(enc,OPUS_SET_INBAND_FEC(0))!=OPUS_OK
    ||opus_encoder_ctl(enc,OPUS_SET_DTX(0))!=OPUS_OK)return 4;
   float pcm[1920*2]; unsigned char packet[1276]; long bytes=0; int minbw=1105,maxbw=0,minch=2,maxch=0; clock_t start=clock();
   for(int frame=0;frame<250;frame++) {
    for(int i=0;i<1920;i++){double t=(double)(frame*1920+i)/48000.0; float v=(float)(0.25*sin(2*3.141592653589793*1000.0*t)+0.25*sin(2*3.141592653589793*15000.0*t)); for(int c=0;c<channels;c++) pcm[i*channels+c]=v;}
    int n=opus_encode_float(enc,pcm,1920,packet,sizeof(packet)); if(n<0)return 3;
    bytes+=n;int bw=opus_packet_get_bandwidth(packet);int ch=opus_packet_get_nb_channels(packet);
    if(bw<minbw)minbw=bw;if(bw>maxbw)maxbw=bw;if(ch<minch)minch=ch;if(ch>maxch)maxch=ch;
   }
   double cpu_ms=1000.0*(clock()-start)/CLOCKS_PER_SEC;
   printf("input_channels=%d bitrate=%d forced_bandwidth=%d packet_bandwidth=%d..%d output_channels=%d..%d payload_bps=%.0f encode_cpu_ms=%.1f audio_seconds=10 cpu_ms_per_audio_second=%.2f\n",channels,bitrate,forced,minbw,maxbw,minch,maxch,bytes*8/10.0,cpu_ms,cpu_ms/10.0);
   opus_encoder_destroy(enc);
  }
 }
 return 0;
}
