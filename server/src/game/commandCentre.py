from game.unit import Unit
from game.transmitter import Transmitter
from game.receiver import Receiver
from game.builder import Builder

class CommandCentre( Unit ):
   
   def __init__( self ):

      super( CommandCentre, self ).__init__( "CommandCentre" )
      self.addHardware( Transmitter, "transmitter" )
      self.addHardware( Receiver, "receiver" )
      self.addHardware( Builder, "builder" )
