#-*- coding:utf-8 -*-

import sys
import os
import subprocess
import json
import datetime
import shutil
import math
from codec import Ccodec

class Ct265(Ccodec):
    def __init__(self, src_path, dst_path, log_path):
        Ccodec.__init__(self, src_path, log_path)
        self._psnr_tools = "../../bin/psnr_yuv611"
        self._ffmpeg_tools = "../../bin/ffmpeg_t265"
        self._dst_path = dst_path
    
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
            self._decode_cmd = self._ffmpeg_tools + " -noautorotate -i "  + src_path + " -s " + str(self._trans_info["width"]) + "x" + str(self._trans_info["height"]) + " -vsync cfr -r 30 -an  -pix_fmt yuv420p -y " + dst_path + " 2> /dev/null"
        
        print self._decode_cmd
        self.run_cmd(self._decode_cmd) 
    
    def t265_encode(self):
        src_path = self._src_path
        dst_path = self._dst_path
        width = self._src_info["width"]
        height = self._src_info["height"]

        self._t265_cmd = self._ffmpeg_tools + " -threads 3 -noautorotate  -max_error_rate 0.99 -fflags +genpts  -y -loglevel error "
        self._t265_cmd += " -i " + src_path + " -r 30 -vsync cfr "
        self._t265_cmd += " -filter_complex \"[0]scale=w=" + str(width) + ":h=" + str(height) + "[scale_1_out]\" "
        self._t265_cmd += " -map [scale_1_out]:v? -map 0:a? "
        self._t265_cmd += " -pix_fmt  yuv420p  -vcodec libt265  -t265-params crf=23.5:preset=0:vbv-maxrate=4000:vbv-bufsize=8000 -acodec copy "
        self._t265_cmd += " -bsf:a aac_adtstoasc  -f mp4 -movflags +faststart -threads 6 " + dst_path 
        
        print self._t265_cmd
        retmsg = self.run_cmd(self._t265_cmd) 
        spend_time_us = retmsg["spend_time_us"]

        self._trans_info = self.ffprobe(dst_path)
        self._trans_info["trans_fps"] = int(self._trans_info["frames"]) * 1000000.0 / spend_time_us
    
    def x265_encode(self):
        pass

if __name__ == "__main__":
    obj = Ct265("/data/eckoqzhang/video/t265/seqs/tjg_746285709_1047_bc809c56307d4ee19964b7aab922vide.f0.mp4", "/data/eckoqzhang/workspace/Tecko/video/h265/tjg_746285709_1047_bc809c56307d4ee19964b7aab922vide.t265.mp4", "result.log")
    obj.t265_encode()
    print obj.psnr()
