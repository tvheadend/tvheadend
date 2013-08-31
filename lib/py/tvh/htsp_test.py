#!/usr/bin/env python
#
# Copyright (C) 2013 Jon Davies <daviesjpuk@gmail.com>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, version 3 of the License.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#
"""
This is a simple test suite for the HTSP client library written by Adam Sutton
see https://tvheadend.org/projects/tvheadend/wiki/Htsp
"""
import random
import unittest
import socket
import time
import htsp         # provides Global HTSP_PROTO_VERSION

class Test_htsp(unittest.TestCase):

    clientaddress = '127.0.0.1'
    clientport =  9982
    clientname = "htsp test suite"
    
    event_args_reqd={ 'eventId' : int, 'channelId' : int, 'start' : int, 'stop' : int}
    event_args_opt={ 
                    'title': str, 
                    'summary': str,
                    'description': str,
                    'episodeOnscreen': str,
                    'image': str,
                    'firstAired':int,
                    'serieslinkId':int,
                    'episodeId':int,
                    'seasonId':int,
                    'brandId':int,
                    'contentType':int,
                    'ageRating':int,
                    'starRating':int,
                    'seasonNumber':int,
                    'seasonCount':int,
                    'episodeNumber':int,
                    'episodeCount':int,
                    'partNumber':int,
                    'partCount':int,
                    'dvrId':int,
                    'nextEventId':int,
                    }        
    dvrEntry_args_reqd={
                        'id':int,
                        'channel':int,
                        'start':int,
                        'stop':int,
                        'state':str
                        }
    dvrEntry_args_opt={
                        'eventId':int,
                        'title':str,
                        'summary':str,
                        'description':str,
                        'error':str,
                        'path':str,
                        }
    tagAdd_args_reqd={
                        'tagId':int,
                        'tagName':str,
                         }
    tagAdd_args_opt={
                        'tagIcon':str,
                        'tagTitledIcon':int,
                        'members':list,
                        }
    channelAdd_args_reqd={
                        'channelId':int,
                        'channelNumber':int,
                        'channelName':str,
                         }
    channelAdd_args_opt={
                        'channelIcon':str,
                        'eventId':int,
                        'nextEventId':int,
                        'tags':list,
                        'services':list,  
                        }
    epgObject_args_opt={
                        'is_subtitled': int,
                        'updated': int,
                        'episode': str,
                        'description': dict,
                        'stop': int,
                        'start': int,
                        'grabber': str,
                        'is_repeat': int,
                        'type': int,
                        'id': int,
                        'channel': int
                        }
    def check_obj(self,call,objname,obj,args_reqd,args_opt):
        for field,fieldtype in args_reqd.items():
            self.assertIn(field,obj,                    
                            'Response to %s : %s missing required field %s' % (call, objname, field))
            self.assertIsInstance(obj[field],fieldtype,
                            'Response to %s : %s["%s"] is not %s' % (call, objname, field,fieldtype))

        for field,fieldtype in args_opt.items():
            if field in obj:
                self.assertIsInstance(obj[field],fieldtype,
                            'Response to %s : %s["%s"] is not %s' % (call, objname, field,fieldtype))
                           
        allargs = dict(args_reqd.items() + args_opt.items())
        for field in obj:
            self.assertIn(field,allargs,
                            'Response to %s : %s["%s"] is not an expected field' % (call, objname, field))
        

    @classmethod
    def setUpClass(cls):
        pass

    @classmethod
    def tearDownClass(cls):
        pass

    def setUp(self):
        self.client = htsp.HTSPClient((self.clientaddress, self.clientport), self.clientname)

        
    def  tearDown(self):
        self.client._sock.close()
        

    def test_hello_response(self):
        result=self.client.hello()
        self.check_obj(
                        call='hello',
                        objname='result',
                        obj=result,
                        args_reqd={'htspversion':int, 'servername':str, 'serverversion' : str,
                                   'challenge':str, 'servercapability':list},
                        args_opt={'webroot':str}
                        )
        
    def test_hello_values(self):
        pass
                
    def test_getSysTime_response(self):
        result=self.client.getSysTime()
        self.check_obj(
                        call='getSysTime',
                        objname='result',
                        obj=result,
                        args_reqd={'time':int, 'timezone':int},
                        args_opt={}
                        )
        
    def test_getSysTime_values(self):
        result=self.client.getSysTime()
        self.assertTrue((result['time']>=0),                    'time is negative')
        self.assertTrue((result['timezone']>=0),                'timezone is negative')

    def test_getDiskSpace_response(self):
        result=self.client.getDiskSpace()
        self.check_obj(
                        call='getDiskSpace',
                        objname='result',
                        obj=result,
                        args_reqd={'freediskspace':int, 'totaldiskspace':int},
                        args_opt={}
                        )
        
    def test_getDiskSpace_values(self):
        result=self.client.getDiskSpace()
        self.assertTrue((result['freediskspace']>=0),                    'freediskspace is negative')
        self.assertTrue((result['totaldiskspace']>=0),                'totaldiskspace is negative')

    def test_getEvents_response(self):
        result=self.client.getEvents(numFollowing=1)
        self.check_obj(
                        call='getEvents',
                        objname='result',
                        obj=result,
                        args_reqd={'events':list},
                        args_opt={}
                        )

    def test_getEvents_values(self):
        result=self.client.getEvents(numFollowing=1)
        self.check_obj('getEvents','event',result['events'][0],self.event_args_reqd,self.event_args_opt)

    def test_getEvent_response(self):
        result=self.client.getEvents(numFollowing=1)
        eventId=result['events'][0]['eventId']
        result=self.client.getEvent(eventId=eventId)
        self.check_obj('getEvent','result',result,self.event_args_reqd,self.event_args_opt)

    def test_getEvent_values(self):
        pass
                
    def test_enableAsyncMetadata_response(self):
        result = self.client.enableAsyncMetadata()
        self.check_obj(
                        call='enableAsyncMetadata',
                        objname='result',
                        obj=result,
                        args_reqd={},
                        args_opt={}
                        )
        
    def test_enableAsyncMetadata_values(self):
        methods ={"initialSyncCompleted","dvrEntryAdd","tagAdd","tagUpdate","channelAdd"}
        self.client.enableAsyncMetadata()
        while True:
            item = self.client.recv()
            method = item.get('method')
            del item['method']
            self.assertIn(method,methods,
                            'Response after enableAsyncMetadata : unknown method %s' % (method))
            if method == "initialSyncCompleted":
                break
            elif method == "dvrEntryAdd":
                self.check_obj(
                    call='enableAsyncMetadata',
                    objname='dvrEntry',
                    obj=item,
                    args_reqd=self.dvrEntry_args_reqd,
                    args_opt=self.dvrEntry_args_opt
                    )
            elif method == "tagAdd":
                self.check_obj(
                    call='enableAsyncMetadata',
                    objname='tagAdd',
                    obj=item,
                    args_reqd=self.tagAdd_args_reqd,
                    args_opt=self.tagAdd_args_opt
                    )
            elif method == "tagUpdate":
                self.check_obj(
                    call='enableAsyncMetadata',
                    objname='tagUpdate',
                    obj=item,
                    args_reqd={},
                    args_opt=dict(self.tagAdd_args_reqd.items() + self.tagAdd_args_opt.items()),
                    )
            elif method == "channelAdd":
                self.check_obj(
                    call='enableAsyncMetadata',
                    objname='channelAdd',
                    obj=item,
                    args_reqd=self.channelAdd_args_reqd,
                    args_opt=self.channelAdd_args_opt
                    )

    def test_enableAsyncMetadata_values_epg(self):
        methods ={"initialSyncCompleted","eventAdd","dvrEntryAdd","tagAdd","tagUpdate","channelAdd"}
        self.client.enableAsyncMetadata(epg=1)
        while True:
            item = self.client.recv()
            method = item.get('method')
            del item['method']
            self.assertIn(method,methods,
                            'Response after enableAsyncMetadata : unknown method %s' % (method))
            if method == "initialSyncCompleted":
                break
            elif method == "eventAdd":
                self.check_obj(
                    call='enableAsyncMetadata',
                    objname='event',
                    obj=item,
                    args_reqd=self.event_args_reqd,
                    args_opt=self.event_args_opt
                    )
            elif method == "dvrEntryAdd":
                self.check_obj(
                    call='enableAsyncMetadata',
                    objname='dvrEntry',
                    obj=item,
                    args_reqd=self.dvrEntry_args_reqd,
                    args_opt=self.dvrEntry_args_opt
                    )
            elif method == "tagAdd":
                self.check_obj(
                    call='enableAsyncMetadata',
                    objname='tagAdd',
                    obj=item,
                    args_reqd=self.tagAdd_args_reqd,
                    args_opt=self.tagAdd_args_opt
                    )
            elif method == "tagUpdate":
                self.check_obj(
                    call='enableAsyncMetadata',
                    objname='tagUpdate',
                    obj=item,
                    args_reqd={},
                    args_opt=dict(self.tagAdd_args_reqd.items() + self.tagAdd_args_opt.items()),
                    )
            elif method == "channelAdd":
                self.check_obj(
                    call='enableAsyncMetadata',
                    objname='channelAdd',
                    obj=item,
                    args_reqd=self.channelAdd_args_reqd,
                    args_opt=self.channelAdd_args_opt
                    )
                    
    def test_epgQuery_response(self):
        result=self.client.epgQuery('')
        self.check_obj(
                        call='epgQuery',
                        objname='result',
                        obj=result,
                        args_reqd={},
                        args_opt={'eventIds':list}
                        )

    def test_epgQuery_response_values(self):
        result=self.client.epgQuery('')
        self.assertIsInstance(result['eventIds'][0],int, 'eventId is not an integer')

    def test_epgQuery_full_response(self):
        result=self.client.epgQuery('',full=1)
        self.check_obj(
                        call='epgQuery full',
                        objname='result',
                        obj=result,
                        args_reqd={},
                        args_opt={'events':list}
                        )

    def test_epgQuery_full_values(self):
        result=self.client.epgQuery('',full=1)
        self.check_obj('epgQuery full','event',result['events'][0],self.event_args_reqd,self.event_args_opt)

    def test_getEpgObject_response(self):
        result=self.client.getEvents(numFollowing=1)
        eventId=result['events'][0]['eventId']
        result=self.client.getEpgObject(id=eventId)
        self.check_obj('getEpgObject','result',result,{},self.epgObject_args_opt)
        
    def test_addDvrEntry__eventId_response(self):
        result=self.client.getEvents(numFollowing=1)
        i=-1
        while i>-len(result['events']):
            event=result['events'][i]
            if not 'dvrId' in event:
                result=self.client.addDvrEntry(eventId=event['eventId'])
                self.assertTrue((result['success']==1),                    'freediskspace is negative')
                if result.get('success',0)==1:
                    result=self.client.deleteDvrEntry(id=result['id'])                                
                break
            i -=1   

    def test_deleteDvrEntry__eventId_response(self):
        result=self.client.getEvents(numFollowing=1)
        i=-1
        while i>-len(result['events']):
            event=result['events'][i]
            if not 'dvrId' in event:
                result=self.client.addDvrEntry(eventId=event['eventId'])
                if result.get('success',0)==1:
                    result=self.client.deleteDvrEntry(id=result['id'])                                
                    self.assertTrue((result['success']==1),                    'freediskspace is negative')
                break
            i -=1   


if __name__ == '__main__':
    unittest.main()
