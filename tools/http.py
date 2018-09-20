#-*- coding:utf-8 -*-

import urllib2
import json
import sys

def http_get(ip_port, cgi, data):
    try:    
        retcode = -1
        errmsg = "exception catched"
        res = []
    
        url    = "http://" + ip_port + cgi + "?" + data;
        header = {"Content-type":"text/plain"}

        req = urllib2.Request(url)
        req.get_method = lambda:'GET'
        response = urllib2.urlopen(req, timeout=600)
        result = json.loads(response.read(), 'latin_1')
        return result 

    except Exception,e:
        return {
                "retcode": -10000,
                "errmsg" : "except:%s" %e
            }

if __name__ == "__main__":
    fd = open(sys.argv[1])
    out_fd = open(sys.argv[2], "w")
    all_lines = fd.readlines()
    for line in all_lines:
        line = line.strip()
        retmsg = http_get("9.25.161.90", "/api/getVidViaFeedid", "feedid=%s" %(line))
        if retmsg["errno"] == 0:
            out_fd.write("%s\t%s\n" %(line, retmsg["payload"]["vid_list"][0]))
