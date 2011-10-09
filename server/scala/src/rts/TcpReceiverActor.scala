package rts
import scala.actors.Actor
import scala.actors.Actor._
import scala.collection.mutable.ArrayBuffer
import scala.collection.mutable.Buffer
import java.net.Socket
import java.io.IOException
import java.io.BufferedReader
import java.io.InputStreamReader
import com.weiglewilczek.slf4s._

/* To do 
sending
*/

class TcpReceiverActor( playerIndex : Int, clientSocket : Socket ) extends Actor with Logging {
  
  var inCode : Boolean  = false
  var codeBuffer : Buffer[String] = new ArrayBuffer[String]

  def beginCode = {
    inCode = true
  }
  def endCode = {
    inCode = false
    var codeStr : String = codeBuffer.mkString("\n")
    logger.info( "Creating unit with code:" )
    logger.info( codeStr )
  }
  def processCode( codeLine : String ) = {
    codeBuffer += codeLine
  }
  
  def die = {
    logger.error( "Player " + playerIndex + " connection lost" )
    exit()
  }

  def act = {
    try {
      val reader = new BufferedReader( new InputStreamReader( clientSocket.getInputStream ) )
      loop {
        val str = reader.readLine
        if ( str == null ) {
          die
        }
        str match {
          case "BeginCode" => beginCode
          case "EndCode"   => endCode
          case _           => inCode match {
            case true => processCode( str )
            case false => { logger.debug( "[unknown] " + str ) }
          }
        }
      }
    }
    catch {
      case e: IOException =>
        logger.error("Player " + playerIndex + " connection error");
    }
  }
}
