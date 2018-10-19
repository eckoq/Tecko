#-*- coding:utf-8 -*-

import sys
import os

sys.path.append("../")

from video.h265.x265 import Cx265
from video.h265.t265 import Ct265


src_dir = "/data/download/"
dst_dir = "/data/download/trans/"

def run(file_name):
    fd = open(file_name, "r")
    all_line = fd.readlines()

    #t265
    #for line in all_line:
    #    vid = line.strip()       
    #    src_path = src_dir + vid + ".f0.mp4"
    #    
    #    dst_path = dst_dir + vid + ".t265.mp4"
    #    result_file = "./t265.result"
    #    print src_path, dst_path, result_file
    #    obj = Ct265(src_path, dst_path, result_file, "60", "vfr", "23.5")
    #    obj.t265_encode()
    #    obj.psnr()

    #x265
    crfs = [
            #20,
            #21,
            #22,
            22.5,
            23,
            23.5,
            24,
            24.5,
            25
            ]
    for crf in crfs:
        for line in all_line:
            vid = line.strip()       
            src_path = src_dir + vid + ".f0.mp4"
            
            dst_path = dst_dir + vid + ".x265." + str(crf) +  ".mp4"
            result_file = "./x265." + str(crf) + ".result"
            
            print src_path, dst_path, result_file

            obj = Cx265(src_path, dst_path, result_file, "30", "cfr", str(crf), 0)
            obj.x265_encode()
            obj.psnr()


if __name__ == "__main__":
    run(sys.argv[1])

