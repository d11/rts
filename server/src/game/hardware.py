import logging

class Hardware( object ):

   def __init__( self, name ):
      self.name = name
      logging.info( "hardware: " + name )

   def description( self ):
      return self.name
