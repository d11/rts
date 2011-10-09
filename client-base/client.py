#!/usr/bin/env python2
import socket
import sys
import time

serverIP   = '127.0.0.1'
serverPort = 9999

server = socket.socket( socket.AF_INET, socket.SOCK_STREAM )
server.connect( (serverIP, serverPort) )

def quit():
   print 'Quitting'
   server.close()
   sys.exit( 0 )

lines = [
      'hi there',
      'BeginCode',
      'int main( argc ) {',
      'cout << foo',
      'return 0;',
      '}',
      'EndCode',
      'yo yo'
      ]

try:
   for message in lines:
      server.send( message + "\n")
      time.sleep( 0.1 )
except KeyboardInterrupt, e:
   print( e )
   quit()

#bufferSize = 1024
#data = s.recv( bufferSize )
#print "received data:", data


class ProxyCommandCentre( object ):

   def __init__( self ):
      pass
