import unit
from Transmitter import Transmitter
from Receiver import Receiver
from Builder import Builder

class CommandCentre( unit.Unit ):
   
   def __init__( self ):

      super( CommandCentre, self ).__init__( "CommandCentre" )
      self.addHardware( Transmitter, "transmitter" )
      self.addHardware( Receiver, "receiver" )
      self.addHardware( Builder, "builder" )

