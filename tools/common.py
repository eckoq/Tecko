#-*-coding:utf-8


import sys
import random

class Ccommon(object):
    def __init__(self, data):
        self._data = data
   
    def random_list(self, datas):
        random.shuffle(datas)

    def video_subject_test(self):
        file_name = self._data["file_name"]
        fd = open(file_name)
        lines = fd.readlines()
        for line in lines:
            vid = line.strip()
                 
           
if __name__ == "__main__":

    #file_name = sys.argv[1]
    #fd = open(file_name)
    #lines = fd.readlines()
    #data = {}
    #feedids = []
    #for line in lines:
    #    type = line.strip().split()[0]
    #    feedid = line.strip().split()[1]
    #    data[feedid] = type
    #    feedids.append(feedid)

    #obj = Ccommon()
    #obj.random_list(feedids)

      
