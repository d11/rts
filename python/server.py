import logging
from CommandCentre import CommandCentre
from unit import Unit

class Server( object ):

   def __init__( self ):
      logging.info( "starting server" )

   def start( self ):

      playerCount = 1
      
      centres = [ CommandCentre() for x in range( 0, playerCount ) ]
      print( map( Unit.description_getter(), centres ) )

      f = open( "commands.temp", 'r' )
      for l in f.readlines():
         print(l)
         

server = Server()
server.start()

