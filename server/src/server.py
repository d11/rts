import logging
import stackless
import socket

from game.commandCentre import *
from game.unit import *

class Server( object ):

   def __init__( self, ip, port ):
      logging.info( "starting server" )
      self.ip = ip
      self.port = port

      self.socket = socket.socket( socket.AF_INET, socket.SOCK_STREAM )
      self.socket.bind( (self.ip, self.port) )
      self.bufferSize = 10
      self.socket.listen( 1 )


   def start( self ):

      playerCount = 1
      
      centres = [ CommandCentre() for x in range( 0, playerCount ) ]
      print( map( Unit.description_getter(), centres ) )

      units = centres
      #units.append( 

      def runUnit(unit):
         while 1:
            print("schedule ", unit.name)

      stackless.tasklet(

server = Server( '127.0.0.1', 5005 )
server.start()


stackless.tasklet(InfiniteLoop)(1)
stackless.tasklet(InfiniteLoop)(2)

# The current main thread of execution counts as one running tasklet, so
# we know that if it is the only one remaining, we can exit.
while stackless.getruncount() != 1:
   t = stackless.run(1000)
   # If we got a tasklet back, it was the one that was interrupted.
   # we need to reinsert it for rescheduling.
   if t:
       t.insert()

