package rts
import com.weiglewilczek.slf4s._

object Util {
  def addShutdownHook(body: => Unit) = 
    Runtime.getRuntime.addShutdownHook(new Thread {
      override def run { body }
    })
}

class Server extends Object with Logging {

  def quit = { 
    logger.info( "Terminating ..." )
    // todo message actors
  }

  def run = {
    logger.info("Starting server")

    Util.addShutdownHook { quit }

    var playerCreator = new PlayerCreator( 9999 )
    playerCreator.start

    logger.info("Finishing")
  }
}
