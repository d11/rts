package rts

import scala.actors.Actor
import scala.actors.Actor._

import java.net.ServerSocket
import java.net.Socket
import java.io.IOException
import com.weiglewilczek.slf4s._

class PlayerCreator( port: Int ) extends Actor with Logging {

  implicit def showSocket(socket: Socket) = { socket.getInetAddress().getHostAddress }

  def act() {
    try {
      val serverSocket = new ServerSocket( port );
      var playerIndex = 0
      loop {
        logger.info( "PlayerCreator waiting for connections" )
        val clientSocket = serverSocket.accept;
        logger.info( "PlayerCreator got connection from " + clientSocket )
        var playerActor = new TcpReceiverActor( playerIndex, clientSocket )
        playerActor.start
        playerIndex += 1
      }
      serverSocket.close()
    }
  }
}
