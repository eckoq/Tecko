#-*- coding:utf-8 -*-

import sys
import os
import subprocess
import json
import datetime
import shutil
import math

class Ccodec():
    def __init__(self, src_path, log_path):
        self._src_path = src_path
        self._log_fd = open(log_path, "a")
        self._ffprobe_tools = "../../bin/ffprobe"
        self._src_info = self.ffprobe(src_path)
    
    def write_log(self, msg):
        self._log_fd.write(str(msg) + "\n")
        self._log_fd.close()

    def run_cmd(self, cmd):
        start_time = datetime.datetime.now()
        retcode = subprocess.call(cmd, shell=True)
        end_time = datetime.datetime.now()
        passed_time = end_time - start_time
        passed_time_us = passed_time.seconds * 1000000 + passed_time.microseconds

        return {
                    "retcode" : retcode, 
                    "spend_time_us": passed_time_us
            }

    def ffprobe(self, video_path):
        self._ffprobe_log = video_path + ".ffprobe"
        self._ffprobe_cmd = self._ffprobe_tools + " -i " + video_path + " -show_streams -select_streams v -print_format json "
        self._ffprobe_cmd += " >" + self._ffprobe_log + " 2>/dev/null"

        self.run_cmd(self._ffprobe_cmd)

        ffprobe_fd = open(self._ffprobe_log, "r")
        ffprobe_data = json.load(ffprobe_fd)
        
        videoinfo = {}
        videoinfo["width"] = ffprobe_data["streams"][0]["width"]
        videoinfo["height"] = ffprobe_data["streams"][0]["height"]
        videoinfo["bitrate"] = ffprobe_data["streams"][0]["bit_rate"]
        videoinfo["frames"] = ffprobe_data["streams"][0]["nb_frames"]
        videoinfo["pix_fmt"] = ffprobe_data["streams"][0]["pix_fmt"]
        if "tags" in ffprobe_data["streams"][0].keys():
            if "rotate" in ffprobe_data["streams"][0]["tags"].keys():
                rotate = int(ffprobe_data["streams"][0]["tags"]["rotate"]);
                if rotate == 90 or rotate == 270:
                    videoinfo["width"] = ffprobe_data["streams"][0]["height"]
                    videoinfo["height"] = ffprobe_data["streams"][0]["width"]

        ffprobe_fd.close()
        os.remove(self._ffprobe_log)
        return videoinfo
        
if __name__ == "__main__":
    obj = Ccodec("/data/eckoqzhang/video/t265/seqs/tjg_746285709_1047_bc809c56307d4ee19964b7aab922vide.f0.mp4", "result.log")
    print obj.ffprobe("/data/eckoqzhang/video/t265/seqs/tjg_746285709_1047_bc809c56307d4ee19964b7aab922vide.f0.mp4")
