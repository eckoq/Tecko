#-*- coding:utf-8 -*-

import sys
import os
import subprocess
import json
import datetime
import shutil
import math

class Cx265():

    def __init__(self, src_path, dst_path, log_path):
        self._src_path = src_path
        self._dst_path = dst_path
        
        self._log_fd = open(log_path, "a")

        self._src_info = self.ffprobe(src_path)
    
    def write_log(self, msg):
        self._log_fd.write(str(msg) + "\n")

    def run_cmd(self, cmd):
        start_time = datetime.datetime.now()
        retcode = subprocess.call(cmd, shell=True)
        end_time = datetime.datetime.now()
        passed_time = end_time - start_time
        passed_time_us = passed_time.seconds * 1000000 + passed_time.microseconds
        return {"retcode" : retcode, "spend_time_us": passed_time_us}

    def ffprobe(self, video_path):
        self._ffprobe_log = video_path + ".ffprobe"
        self._ffprobe_cmd = "./ffprobe -i " + video_path + " -show_streams -select_streams v -print_format json "
        self._ffprobe_cmd += " >" + self._ffprobe_log + " 2>/dev/null"

        self.run_cmd(self._ffprobe_cmd)

        ffprobe_fd = open(self._ffprobe_log, "r")
        ffprobe_data = json.load(ffprobe_fd)
        
        videoinfo = {}
        videoinfo["width"] = ffprobe_data["streams"][0]["width"]
        videoinfo["height"] = ffprobe_data["streams"][0]["height"]
        videoinfo["bitrate"] = ffprobe_data["streams"][0]["bit_rate"]
        videoinfo["frames"] = ffprobe_data["streams"][0]["nb_frames"]
        if "tags" in ffprobe_data["streams"][0].keys():
            if "rotate" in ffprobe_data["streams"][0]["tags"].keys():
                rotate = int(ffprobe_data["streams"][0]["tags"]["rotate"]);
                if rotate == 90 or rotate == 270:
                    videoinfo["width"] = ffprobe_data["streams"][0]["height"]
                    videoinfo["height"] = ffprobe_data["streams"][0]["width"]

        ffprobe_fd.close()
        return videoinfo
    
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
        self._psnr_cmd = "./psnr_yuv611 " + str(width) + " " + str(height) + " "
        self._psnr_cmd += self._src_yuv_path + " " + self._dst_yuv_path + " 1>" + self._psnr_log
        #print self._psnr_cmd
        self.run_cmd(self._psnr_cmd)
        psnr_fd = open(self._psnr_log)
        psnr = float(psnr_fd.readlines()[0])
        psnr_fd.close()

        os.remove(self._src_yuv_path)
        os.remove(self._dst_yuv_path)

        msg = "%s\t%s\t%s\t%s\t%s" %(str(self._src_path), str(self._src_info["bitrate"]), str(self._trans_info["bitrate"]), self._trans_info["trans_fps"] ,str(psnr))
        self.write_log(msg)
        
    def decode(self, flag, src_path, dst_path):
        self._decode_cmd = "./ffmpeg -i "  + src_path + " -vsync 0 -an -y " + dst_path + " 2> /dev/null" 
        if flag == 1:
            self._decode_cmd = "./ffmpeg -noautorotate -i "  + src_path + " -s " + str(self._trans_info["width"]) + "x" + str(self._trans_info["height"]) + " -vsync cfr -r 30  -an -y " + dst_path + " 2> /dev/null"
        
        print self._decode_cmd
        self.run_cmd(self._decode_cmd) 
    
    def x265_encode(self):
        src_path = self._src_path
        dst_path = self._dst_path
        width = self._src_info["width"]
        height = self._src_info["height"]
        pass1 = dst_path + ".2pass"
        
        #pass1
        self._t265_cmd = "./ffmpeg -threads 3 -noautorotate  -max_error_rate 0.99 -fflags +genpts  -y -loglevel error "
        self._t265_cmd += " -i " + src_path + " -r 30 -vsync cfr "
        self._t265_cmd += " -filter_complex \"[0]scale=w=" + str(width) + ":h=" + str(height) + "[scale_1_out]\" "
        self._t265_cmd += " -map [scale_1_out]:v? -map 0:a? "
        self._t265_cmd += " -pix_fmt  yuv420p  -vcodec libx265  -preset veryslow -x265-params pass=1:stats=" + pass1 + ":crf=23:vbv-maxrate=4000:vbv-bufsize=8000:level-idc=4.1:ref=5:trellis=2:direct=auto:rc_lookahead=50  -acodec copy "
        self._t265_cmd += " -bsf:a aac_adtstoasc  -f mp4 -movflags +faststart -threads 6 " + dst_path 
        #print "pass1 %s"  %(self._t265_cmd)
        retmsg = self.run_cmd(self._t265_cmd) 
        spend_time_us = retmsg["spend_time_us"]
        #print "pass1 spend_time:%d" %(spend_time_us)

        self._trans_info = self.ffprobe(dst_path)

        bitrate = int(self._trans_info["bitrate"]) / 1000

        #pass2
        self._t265_cmd = "./ffmpeg -threads 3 -noautorotate  -max_error_rate 0.99 -fflags +genpts  -y -loglevel error "
        self._t265_cmd += " -i " + src_path + " -r 30 -vsync cfr "
        self._t265_cmd += " -filter_complex \"[0]scale=w=" + str(width) + ":h=" + str(height) + "[scale_1_out]\" "
        self._t265_cmd += " -map [scale_1_out]:v? -map 0:a? "
        self._t265_cmd += " -pix_fmt  yuv420p  -vcodec libx265  -preset veryslow -x265-params pass=2:stats=" + pass1 + ":bitrate=" + str(bitrate) + ":vbv-maxrate=4000:vbv-bufsize=8000:level-idc=4.1:ref=5:trellis=2:direct=auto:rc_lookahead=50  -acodec copy "
        self._t265_cmd += " -bsf:a aac_adtstoasc  -f mp4 -movflags +faststart -threads 6 " + dst_path 
        
        #print "pass2 %s"  %(self._t265_cmd)
        retmsg = self.run_cmd(self._t265_cmd) 
        spend_time_us += retmsg["spend_time_us"]
        #print "pass1 and pass2 spend_time:%d frames:%d" %(spend_time_us, int(self._trans_info["frames"]))

        self._trans_info = self.ffprobe(dst_path)


        self._trans_info["trans_fps"] = int(self._trans_info["frames"]) * 1000000.0 / spend_time_us
    

if __name__ == "__main__":
    obj = Cx265("/data/eckoqzhang/video/t265/seqs/tjg_674267130_1047_76c6a727cda64d25bcaaef5f8a89vide.f0.mp4", "/data/eckoqzhang/video/t265/trans/tjg_674267130_1047_76c6a727cda64d25bcaaef5f8a89vide.x265.mp4", "result.log")
    obj.x265_encode()
    print obj.psnr()
