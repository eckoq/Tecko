#-*-coding:utf-8


import sys
import json
import random

class Ccommon(object):
    def __init__(self, data):
        self._data = data
   
    def random_list(self, datas):
        random.shuffle(datas)

    def video_subject_test(self, name):
        subject_test = {
          "task_name": name, 
          "report_list": "liusong,altonzhu,eckoqzhang,faithehuang,feelwang,flyllehuang,jonwu,kunqian,lianjiepeng,lmzeng,lonsonhuang,natuschen,nickxiong,steveyan,yangyyang,zeyalu,zongshenguo", 
          "test_type": 3, 
          "test_list": "liusong,altonzhu,eckoqzhang,faithehuang,feelwang,flyllehuang,jonwu,kunqian,lianjiepeng,lmzeng,lonsonhuang,natuschen,nickxiong,steveyan,yangyyang,zeyalu,zongshenguo", 
          "owner": "eckoqzhang", 
          "url_list": [
          ]
        }

        file_name = self._data["file_name"]
        fd = open(file_name)
        lines = fd.readlines()
        for line in lines:
            vid = line.strip()
            item = {}
            f30_url = "http://100.115.131.35:8080/dstest/" + vid + ".f30.mp4"
            item["url_a"] = f30_url
            f66_url = "http://9.73.158.18:8080/dst_video/" + vid + ".f66.mp4"
            item["url_b"] = f66_url
            subject_test["url_list"].append(item)
        print json.dumps(subject_test, indent=2)
            
                 
           
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

    obj = Ccommon({"file_name": "./raw.vid"})
    obj.video_subject_test("ds_test_20180925")
     
