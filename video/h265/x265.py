#-*- coding:utf-8 -*-

import sys
import os
import subprocess
import json
import datetime
import shutil
import math
from codec import Ccodec

class Cx265(Ccodec):
    def __init__(self, src_path, dst_path, log_path, frame_rate, frame_sync, crf, is_twopass):
        Ccodec.__init__(self, src_path, log_path)
        self._psnr_tools = "/data/eckoqzhang/workspace/Tecko/bin/psnr_yuv611"
        self._ffmpeg_tools = "/data/eckoqzhang/workspace/Tecko/bin/ffmpeg"
        self._dst_path = dst_path
        self._frame_rate = frame_rate
        self._frame_sync = frame_sync
        self._crf = crf
        self._is_twopass = is_twopass
    
    def psnr(self):
        src_path = self._src_path
        dst_path = self._dst_path

        width = self._src_info["width"]
        height = self._src_info["height"]

        self._src_yuv_path = src_path + ".yuv"
        self._dst_yuv_path = dst_path + ".yuv"
        
        self.decode(1, src_path, self._src_yuv_path)
        self.decode(0, dst_path, self._dst_yuv_path)


        self._psnr_log = dst_path + ".psnr"
        self._psnr_cmd = self._psnr_tools + " " + str(width) + " " + str(height) + " "
        self._psnr_cmd += self._src_yuv_path + " " + self._dst_yuv_path + " 1>" + self._psnr_log

        self.run_cmd(self._psnr_cmd)

        psnr_fd = open(self._psnr_log)
        psnr = float(psnr_fd.readlines()[0])
        psnr_fd.close()

        os.remove(self._src_yuv_path)
        os.remove(self._dst_yuv_path)
        os.remove(self._psnr_log)

        msg = "%s\t%s\t%s\t%s\t%s" %(str(self._src_path), str(self._src_info["bitrate"]), str(self._trans_info["bitrate"]), self._trans_info["trans_fps"] ,str(psnr))
        self.write_log(msg)
        
    def decode(self, flag, src_path, dst_path):
        self._decode_cmd = self._ffmpeg_tools + " -i "  + src_path + " -vsync 0 -an -y " + dst_path + " 2> /dev/null" 
        if flag == 1:
            self._decode_cmd = self._ffmpeg_tools + " -noautorotate -i "  + src_path + " -s " + str(self._trans_info["width"]) + "x" + str(self._trans_info["height"]) 
            self._decode_cmd += " -vsync " + str(self._frame_sync) + " -r " + str(self._frame_rate) + " -an -pix_fmt yuv420p -y " + dst_path + " 2> /dev/null"
        
        print self._decode_cmd
        self.run_cmd(self._decode_cmd) 
    
    def x265_encode(self):
        src_path = self._src_path
        dst_path = self._dst_path
        width = self._src_info["width"]
        height = self._src_info["height"]
        pass1 = dst_path + ".2pass"
        
        #pass1
        self._x265_cmd = self._ffmpeg_tools + " -threads 3 -noautorotate  -max_error_rate 0.99 -fflags +genpts  -y -loglevel error "
        self._x265_cmd += " -i " + src_path + " -r " + str(self._frame_rate) + " -vsync " + str(self._frame_sync)
        self._x265_cmd += " -filter_complex \"[0]scale=w=" + str(width) + ":h=" + str(height) + "[scale_1_out]\" "
        self._x265_cmd += " -map [scale_1_out]:v? -map 0:a? "
        
        if self._is_twopass == 1:
            self._x265_cmd += " -pix_fmt  yuv420p  -vcodec libx265  -preset veryslow -x265-params pass=1:stats=" + pass1 + ":crf=" + str(self._crf) + ":vbv-maxrate=4000:vbv-bufsize=8000:level-idc=4.1:ref=5:trellis=2:direct=auto:rc_lookahead=50  -acodec copy "
        else:
            self._x265_cmd += " -pix_fmt  yuv420p  -vcodec libx265  -preset veryslow -x265-params crf=" + str(self._crf) + ":vbv-maxrate=4000:vbv-bufsize=8000:level-idc=4.1:ref=5:trellis=2:direct=auto:rc_lookahead=50  -acodec copy "

        self._x265_cmd += " -bsf:a aac_adtstoasc  -f mp4 -movflags +faststart -threads 6 " + dst_path 
        
        print self._x265_cmd
        retmsg = self.run_cmd(self._x265_cmd) 

        spend_time_us = retmsg["spend_time_us"]
        self._trans_info = self.ffprobe(dst_path)

        if self._is_twopass != 1:
            self._trans_info["trans_fps"] = int(self._trans_info["frames"]) * 1000000.0 / spend_time_us
            return 

        bitrate = int(self._trans_info["bitrate"]) / 1000

        #pass2
        self._x265_cmd = self._ffmpeg_tools + " -threads 3 -noautorotate  -max_error_rate 0.99 -fflags +genpts  -y -loglevel error "
        self._x265_cmd += " -i " + src_path + " -r " + str(self._frame_rate) + " -vsync " + str(self._frame_sync)
        self._x265_cmd += " -filter_complex \"[0]scale=w=" + str(width) + ":h=" + str(height) + "[scale_1_out]\" "
        self._x265_cmd += " -map [scale_1_out]:v? -map 0:a? "
        self._x265_cmd += " -pix_fmt  yuv420p  -vcodec libx265  -preset veryslow -x265-params pass=2:stats=" + pass1 + ":bitrate=" + str(bitrate) + ":vbv-maxrate=4000:vbv-bufsize=8000:level-idc=4.1:ref=5:trellis=2:direct=auto:rc_lookahead=50  -acodec copy "
        self._x265_cmd += " -bsf:a aac_adtstoasc  -f mp4 -movflags +faststart -threads 6 " + dst_path 
        
        print self._x265_cmd
        retmsg = self.run_cmd(self._x265_cmd) 

        spend_time_us += retmsg["spend_time_us"]
        self._trans_info = self.ffprobe(dst_path)
        self._trans_info["trans_fps"] = int(self._trans_info["frames"]) * 1000000.0 / spend_time_us
    

if __name__ == "__main__":
    obj = Cx265("/data/eckoqzhang/video/t265/seqs/tjg_674267130_1047_76c6a727cda64d25bcaaef5f8a89vide.f0.mp4", "/data/eckoqzhang/workspace/Tecko/video/h265/tjg_674267130_1047_76c6a727cda64d25bcaaef5f8a89vide.x265.mp4", "result.log", "30", "cfr", "24", 1)
    obj.x265_encode()
    print obj.psnr()
