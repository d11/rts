import logging

class Unit( object ):

   def addHardware( self, hardwareType, name ):
      hw = hardwareType( name )
      desc = hw.description()
      logging.info( "adding hardware " + desc )
      self.hw[ name ] = hw

   def description( self ):
      for name, hw in self.hw.iteritems():
         print( name + " -> " + hw.description() )

   @classmethod
   def description_getter( cls ):
      return lambda unit: unit.description()

   def __init__( self, name ):
      logging.info( "initting unit: " + name )
      self.hw = dict()

